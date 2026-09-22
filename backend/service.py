"""Milestone 3: long-running local service wrapping match.py.

Exposes the analyze -> compose-query pipeline over local HTTP so a Max for
Live device (or anything else) can drive it without spawning a Python
process per request. Runs on localhost only.

Endpoints:
  POST /match   {"audio_path": str, "prompt": str}
                -> {"features": {...}, "query": {...}}
                Always available, no credentials required (see match.py).

  POST /analyze {"audio_path": str}
                -> {"features": {...}, "feeling": str}
                Extracts features and a 1-2 sentence mood description (Claude
                when ANTHROPIC_API_KEY is set, otherwise a template).

  POST /search  {"audio_path": str, "prompt": str, "features": {...} (optional)}
                -> {"features": {...}, "query": {...}, "results": [...]}
                Additionally runs the composed query against the real Splice
                catalog by calling Splice's `describe_a_sound` MCP tool
                directly (splice_auth.call_tool) and parsing its markdown
                reply. Pass `features` from a prior /analyze to skip
                re-analyzing the audio.

  GET  /auth/status -> {"authorized": bool, "login_in_progress": bool, "error": str|null}
  POST /auth/login  -> {"started": bool}
                Starts the browser OAuth flow (splice_auth.login) on a
                background thread and returns immediately; poll /auth/status.

  POST /download {"asset_uuid": str, "name": str}
                -> {"local_path": str}
                Downloads one chosen sample into DOWNLOAD_DIR. Spends one
                Splice purchase credit the first time a given asset is
                downloaded (free on repeat downloads of the same asset).

                Calls Splice's `download_asset` MCP tool directly via a raw
                MCP client (see mcp_client.py, splice_auth.download_asset),
                authenticated with the OAuth token from `splice_auth.py
                login` -- no LLM in the loop. This replaces an earlier
                `claude -p` based implementation that was unreliable by
                design (see git history / backend/README.md): asking an LLM
                to decide whether to call download_asset each time was the
                wrong shape for a deterministic, already user-confirmed
                action, since the tool's own description correctly demands
                human confirmation that a stateless headless call can't
                prove happened. Requires `python3 splice_auth.py login` to
                have been run once first.

Run with: uvicorn service:app --host 127.0.0.1 --port 8787
"""

from __future__ import annotations

import logging
import os
import re
import threading
import urllib.request

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

import splice_auth
from analysis import AudioFeatures, extract_features
from match import compose_query, describe_feeling

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("clauducer")

app = FastAPI(title="clauducer backend")

DOWNLOAD_DIR = os.path.expanduser(os.environ.get("SPLICE_DOWNLOAD_DIR", "~/Music/Splice Matches"))

_login_lock = threading.Lock()
_login_state: dict = {"in_progress": False, "error": None}


def _run_login() -> None:
    try:
        splice_auth.login()
        _login_state["error"] = None
    except Exception as exc:
        log.warning("splice login failed: %s", exc)
        _login_state["error"] = str(exc)
    finally:
        _login_state["in_progress"] = False


_RESULT_BLOCK_RE = re.compile(r"^### \d+\. (.+)$", re.MULTILINE)
_BPM_RE = re.compile(r"BPM: (\d+(?:\.\d+)?)")
_KEY_RE = re.compile(r"Key: ([^|\n]+)")
_LINK_RE = re.compile(r"\*\*Link:\*\* (\S+)")
_UUID_RE = re.compile(r"\*\*Asset UUID:\*\* (\S+)")


def _parse_search_results(text: str) -> list[dict]:
    """Parse describe_a_sound's markdown reply into {name, bpm?, key?, link, asset_uuid}.

    The tool has no output schema; each hit is a `### N. <file name>` block
    with `BPM: .. | Key: ..` (both absent for one-shots), `**Link:**` and
    `**Asset UUID:**` lines. Blocks missing a link or uuid are skipped.
    """
    headers = list(_RESULT_BLOCK_RE.finditer(text))
    results = []
    for i, header in enumerate(headers):
        block = text[header.end() : headers[i + 1].start() if i + 1 < len(headers) else len(text)]
        link, uuid = _LINK_RE.search(block), _UUID_RE.search(block)
        if not (link and uuid):
            continue
        item = {"name": header.group(1).strip(), "link": link.group(1), "asset_uuid": uuid.group(1)}
        if bpm := _BPM_RE.search(block):
            item["bpm"] = float(bpm.group(1))
        if key := _KEY_RE.search(block):
            item["key"] = key.group(1).strip()
        results.append(item)
    return results


class MatchRequest(BaseModel):
    audio_path: str
    prompt: str


class AnalyzeRequest(BaseModel):
    audio_path: str


class SearchRequest(MatchRequest):
    features: dict | None = None


class DownloadRequest(BaseModel):
    asset_uuid: str
    name: str


@app.post("/match")
def match(req: MatchRequest) -> dict:
    try:
        features = extract_features(req.audio_path)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"could not analyze audio: {exc}")
    query = compose_query(features, req.prompt)
    return {"features": features.__dict__, "query": query}


@app.get("/auth/status")
def auth_status() -> dict:
    return {
        "authorized": splice_auth.is_authorized(),
        "login_in_progress": _login_state["in_progress"],
        "error": _login_state["error"],
    }


@app.post("/auth/login")
def auth_login() -> dict:
    with _login_lock:
        if _login_state["in_progress"]:
            return {"started": False}
        _login_state["in_progress"] = True
        _login_state["error"] = None
    threading.Thread(target=_run_login, daemon=True).start()
    return {"started": True}


@app.post("/analyze")
def analyze(req: AnalyzeRequest) -> dict:
    try:
        features = extract_features(req.audio_path)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"could not analyze audio: {exc}")
    return {"features": features.__dict__, "feeling": describe_feeling(features)}


@app.post("/search")
def search(req: SearchRequest) -> dict:
    try:
        features = AudioFeatures(**req.features) if req.features else extract_features(req.audio_path)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"could not analyze audio: {exc}")
    query = compose_query(features, req.prompt)

    args = {"query": query["query"], "type": query.get("type", "loop")}
    if "bpm_min" in query and "bpm_max" in query:
        args["bpm_min"], args["bpm_max"] = query["bpm_min"], query["bpm_max"]
    try:
        output = splice_auth.call_tool("describe_a_sound", args)
    except splice_auth.SpliceAuthError as exc:
        raise HTTPException(status_code=401, detail=f"Splice authentication failed: {exc}")
    except FileNotFoundError:
        raise HTTPException(status_code=401, detail="Not logged in to Splice")
    except Exception as exc:
        log.warning("search: describe_a_sound failed: %s", exc)
        raise HTTPException(status_code=502, detail=f"Splice search failed: {exc}")

    text = "".join(b.get("text", "") for b in output.get("content", []) if b.get("type") == "text")
    return {"features": features.__dict__, "query": query, "results": _parse_search_results(text)}


@app.post("/download")
def download(req: DownloadRequest) -> dict:
    """Spends a Splice purchase credit on first download of this asset_uuid."""
    try:
        output = splice_auth.download_asset(req.asset_uuid)
    except splice_auth.SpliceAuthError as exc:
        log.warning("download %s: auth failed: %s", req.asset_uuid, exc)
        raise HTTPException(status_code=401, detail=f"Splice authentication failed: {exc}")
    except Exception as exc:
        # Log the full detail server-side -- the client only shows a
        # truncated version of this message in its UI, and this endpoint
        # spends a real Splice credit on success, so a failure here needs to
        # be fully diagnosable without re-spending a credit to reproduce it.
        log.warning("download %s: failed: %s", req.asset_uuid, exc)
        raise HTTPException(status_code=502, detail=f"Splice download failed: {exc}")

    download_url = output.get("download_url")
    if not download_url:
        log.warning("download %s: no download_url in response: %s", req.asset_uuid, output)
        raise HTTPException(status_code=502, detail=f"no download_url in Splice response: {output}")

    os.makedirs(DOWNLOAD_DIR, exist_ok=True)
    file_name = output.get("file_name") or req.name
    local_path = os.path.join(DOWNLOAD_DIR, file_name)
    try:
        urllib.request.urlretrieve(download_url, local_path)
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"failed to fetch presigned download URL: {exc}")

    return {"local_path": local_path}
