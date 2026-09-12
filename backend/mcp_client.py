"""Minimal MCP Streamable HTTP client for calling a single tool directly.

Built to replace the `claude -p` route for `/download` in service.py: that
route asks an LLM to decide whether to call `download_asset`, which is the
wrong shape for a deterministic, already-user-confirmed action (see
backend/README.md, "/download is unreliable"). This talks JSON-RPC 2.0
directly to Splice's MCP server (https://mcp.splice.com/mcp) with no LLM in
the loop -- one HTTP round trip per call, always deterministic.

Only implements the subset of the MCP Streamable HTTP transport needed for
`initialize` + `tools/call`: https://modelcontextprotocol.io/docs/concepts/transports
Not a general-purpose MCP client (no resources, prompts, or SSE server-push
handling beyond parsing a single response stream).
"""

from __future__ import annotations

import json
import re
import urllib.error
import urllib.request

USER_AGENT = "clauducer-backend/0.1"
PROTOCOL_VERSION = "2025-06-18"


class MCPError(Exception):
    """A JSON-RPC error, or a tool call that returned isError=true."""


class MCPAuthError(MCPError):
    """The access token was rejected (HTTP 401)."""


class MCPClient:
    def __init__(self, endpoint: str, access_token: str):
        self.endpoint = endpoint
        self.access_token = access_token
        self.session_id: str | None = None
        self._next_id = 1

    def _headers(self) -> dict:
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {self.access_token}",
            "User-Agent": USER_AGENT,
            "MCP-Protocol-Version": PROTOCOL_VERSION,
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        return headers

    def _post(self, payload: dict) -> dict | None:
        req = urllib.request.Request(
            self.endpoint,
            data=json.dumps(payload).encode(),
            headers=self._headers(),
            method="POST",
        )
        try:
            with urllib.request.urlopen(req) as resp:
                body = resp.read().decode()
                # resp.headers is an email.message.Message (via
                # http.client.HTTPMessage) -- .get() is case-insensitive,
                # unlike a plain dict built from .items(). Splice's server
                # sends "mcp-session-id" lowercase, so a dict lookup here
                # silently missed it (confirmed against the live server).
                session_id = resp.headers.get("Mcp-Session-Id")
                content_type = resp.headers.get("Content-Type", "")
        except urllib.error.HTTPError as exc:
            body = exc.read().decode()
            if exc.code == 401:
                raise MCPAuthError(f"HTTP 401 from MCP server: {body}") from exc
            raise MCPError(f"HTTP {exc.code} from MCP server: {body}") from exc

        if session_id:
            self.session_id = session_id

        if not body.strip():
            return None

        if "text/event-stream" in content_type:
            return _parse_sse_json(body)
        return json.loads(body)

    def _request(self, method: str, params: dict | None = None) -> dict:
        request_id = self._next_id
        self._next_id += 1
        envelope = self._post(
            {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}}
        )
        if envelope is None:
            raise MCPError(f"empty response to {method!r}")
        if "error" in envelope:
            raise MCPError(f"{method!r} failed: {envelope['error']}")
        return envelope.get("result", {})

    def _notify(self, method: str, params: dict | None = None) -> None:
        self._post({"jsonrpc": "2.0", "method": method, "params": params or {}})

    def initialize(self) -> None:
        self._request(
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "clauducer-backend", "version": "0.1"},
            },
        )
        self._notify("notifications/initialized")

    def call_tool(self, name: str, arguments: dict) -> dict:
        result = self._request("tools/call", {"name": name, "arguments": arguments})
        if result.get("isError"):
            raise MCPError(f"tool {name!r} returned an error: {result.get('content')}")
        return result


def _parse_sse_json(body: str) -> dict | None:
    """Extract the last `data: {...}` JSON-RPC message from an SSE response."""
    last = None
    for line in body.splitlines():
        if line.startswith("data:"):
            chunk = line[len("data:") :].strip()
            if chunk:
                last = json.loads(chunk)
    return last


_URL_RE = re.compile(r"https?://\S+")


def extract_tool_result(result: dict) -> dict:
    """Pull structured data out of a tools/call result.

    Tries, in order:
      1. `structuredContent` (present when the tool declares an output schema
         -- confirmed live that Splice's tools currently don't).
      2. Parsing a text content block as JSON.
      3. Regex-extracting the first URL in a text content block as
         `download_url` -- confirmed live that Splice's `describe_a_sound`
         returns prose/markdown rather than JSON, so `download_asset` (which
         also has no output schema) likely does too. A trailing `)`, `.`,
         `,`, or `'`/`"` right after the URL is stripped, since prose often
         punctuates or wraps a URL right up against it.
    """
    if "structuredContent" in result:
        return result["structuredContent"]

    for block in result.get("content", []):
        if block.get("type") != "text":
            continue
        text = block["text"]
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            pass

        match = _URL_RE.search(text)
        if match:
            url = match.group(0).rstrip(").,'\"")
            return {"download_url": url}

    raise MCPError(f"could not extract structured data from tool result: {result}")
