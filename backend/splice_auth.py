"""Standalone OAuth (PKCE + dynamic client registration) against Splice MCP.

Resolves the open item from README.md: a backend process separate from a
Claude Code session needs its own authenticated connection to
https://mcp.splice.com/mcp. Splice's server publishes standard OAuth
discovery metadata (confirmed via `curl
https://mcp.splice.com/.well-known/oauth-authorization-server`) including a
`registration_endpoint`, so this performs RFC 7591 dynamic client
registration followed by an Authorization Code + PKCE flow -- the same shape
Cursor/VS Code/etc use per Splice's own docs
(https://support.splice.com/en/articles/14442749). No pre-shared client_id
or Splice-side developer registration is required.

Usage:
    python3 splice_auth.py login      # one-time: opens a browser, stores tokens
    python3 splice_auth.py refresh    # refresh an expired access token
    python3 splice_auth.py token      # print the current access token (refreshing if needed)

Tokens are stored in ~/.clauducer/splice_token.json (owner-read/write only).
"""

from __future__ import annotations

import base64
import hashlib
import http.server
import json
import os
import secrets
import sys
import threading
import time
import urllib.parse
import urllib.request
import webbrowser

ISSUER = "https://mcp.splice.com"
CALLBACK_PORT = 3119  # distinct from the port Claude Code's own flow uses (3118)
REDIRECT_URI = f"http://localhost:{CALLBACK_PORT}/callback"
SCOPES = "openid profile email offline_access"
TOKEN_PATH = os.path.expanduser("~/.clauducer/splice_token.json")
# mcp.splice.com 403s the default "Python-urllib/x.y" User-Agent (bot-blocking,
# confirmed via curl comparison), so every request needs a real-looking one.
USER_AGENT = "clauducer-backend/0.1"


def _discover() -> dict:
    req = urllib.request.Request(
        f"{ISSUER}/.well-known/oauth-authorization-server",
        headers={"User-Agent": USER_AGENT},
    )
    with urllib.request.urlopen(req) as resp:
        return json.load(resp)


def _register_client(metadata: dict) -> str:
    body = json.dumps(
        {
            "client_name": "clauducer-backend",
            "redirect_uris": [REDIRECT_URI],
            "grant_types": ["authorization_code", "refresh_token"],
            "response_types": ["code"],
            "token_endpoint_auth_method": "none",  # public client, PKCE-secured
        }
    ).encode()
    req = urllib.request.Request(
        metadata["registration_endpoint"],
        data=body,
        headers={"Content-Type": "application/json", "User-Agent": USER_AGENT},
        method="POST",
    )
    with urllib.request.urlopen(req) as resp:
        return json.load(resp)["client_id"]


class _CallbackHandler(http.server.BaseHTTPRequestHandler):
    result: dict = {}

    def do_GET(self) -> None:  # noqa: N802 (stdlib method name)
        params = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        _CallbackHandler.result = {k: v[0] for k, v in params.items()}
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        self.wfile.write(b"<html><body>Splice login complete, you can close this tab.</body></html>")

    def log_message(self, *args) -> None:  # silence default request logging
        pass


def _await_callback() -> dict:
    server = http.server.HTTPServer(("localhost", CALLBACK_PORT), _CallbackHandler)
    thread = threading.Thread(target=server.handle_request)
    thread.start()
    thread.join(timeout=180)
    server.server_close()
    return _CallbackHandler.result


def _save_tokens(tokens: dict) -> None:
    os.makedirs(os.path.dirname(TOKEN_PATH), exist_ok=True)
    tokens = {**tokens, "obtained_at": time.time()}
    with open(TOKEN_PATH, "w") as f:
        json.dump(tokens, f)
    os.chmod(TOKEN_PATH, 0o600)


def _load_tokens() -> dict:
    with open(TOKEN_PATH) as f:
        return json.load(f)


def login() -> None:
    metadata = _discover()
    client_id = _register_client(metadata)

    verifier = base64.urlsafe_b64encode(secrets.token_bytes(32)).rstrip(b"=").decode()
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    state = secrets.token_urlsafe(16)

    auth_url = metadata["authorization_endpoint"] + "?" + urllib.parse.urlencode(
        {
            "response_type": "code",
            "client_id": client_id,
            "redirect_uri": REDIRECT_URI,
            "code_challenge": challenge,
            "code_challenge_method": "S256",
            "state": state,
            "scope": SCOPES,
            "resource": f"{ISSUER}/mcp",
        }
    )

    print(f"Opening browser for Splice login:\n{auth_url}\n")
    webbrowser.open(auth_url)
    result = _await_callback()

    if result.get("state") != state:
        sys.exit("error: OAuth state mismatch (possible CSRF or stale callback); aborting")
    if "code" not in result:
        sys.exit(f"error: no authorization code returned: {result}")

    token_req = urllib.request.Request(
        metadata["token_endpoint"],
        data=urllib.parse.urlencode(
            {
                "grant_type": "authorization_code",
                "code": result["code"],
                "redirect_uri": REDIRECT_URI,
                "client_id": client_id,
                "code_verifier": verifier,
            }
        ).encode(),
        headers={"Content-Type": "application/x-www-form-urlencoded", "User-Agent": USER_AGENT},
        method="POST",
    )
    with urllib.request.urlopen(token_req) as resp:
        tokens = json.load(resp)
    _save_tokens({**tokens, "client_id": client_id, "token_endpoint": metadata["token_endpoint"]})
    print(f"Logged in. Tokens saved to {TOKEN_PATH}")


def refresh() -> dict:
    tokens = _load_tokens()
    req = urllib.request.Request(
        tokens["token_endpoint"],
        data=urllib.parse.urlencode(
            {
                "grant_type": "refresh_token",
                "refresh_token": tokens["refresh_token"],
                "client_id": tokens["client_id"],
            }
        ).encode(),
        headers={"Content-Type": "application/x-www-form-urlencoded", "User-Agent": USER_AGENT},
        method="POST",
    )
    with urllib.request.urlopen(req) as resp:
        new_tokens = json.load(resp)
    merged = {**tokens, **new_tokens}
    _save_tokens(merged)
    return merged


def get_access_token() -> str:
    tokens = _load_tokens()
    expires_in = tokens.get("expires_in")
    if expires_in is not None and time.time() > tokens["obtained_at"] + expires_in - 60:
        tokens = refresh()
    return tokens["access_token"]


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else "login"
    if command == "login":
        login()
    elif command == "refresh":
        refresh()
        print("Refreshed.")
    elif command == "token":
        print(get_access_token())
    else:
        sys.exit(f"unknown command: {command} (expected login|refresh|token)")
