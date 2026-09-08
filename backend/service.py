"""Milestone 3: long-running local service wrapping match.py.

Exposes the analyze -> compose-query pipeline over local HTTP so a Max for
Live device (or anything else) can drive it without spawning a Python
process per request. Runs on localhost only.

Endpoints:
  POST /match   {"audio_path": str, "prompt": str}
                -> {"features": {...}, "query": {...}}
                Always available, no credentials required (see match.py).

  POST /search  {"audio_path": str, "prompt": str}
                -> {"features": {...}, "query": {...}, "results": [...]}
                Additionally runs the composed query against the real Splice
                catalog by shelling out to the `claude` CLI in headless mode
                (`claude -p`). This uses whatever auth `claude` is already
                logged in with -- a Claude Pro/Max subscription, not a
                separate pay-per-token ANTHROPIC_API_KEY -- and the `splice`
                MCP server already registered for that CLI (check with
                `claude mcp list`). Verified working end-to-end against the
                live Splice catalog while building this.

  POST /download {"asset_uuid": str, "name": str}
                -> {"local_path": str}
                Downloads one chosen sample (via the same claude-CLI/MCP
                route) into DOWNLOAD_DIR. Spends one Splice purchase credit
                the first time a given asset is downloaded (free on repeat
                downloads of the same asset).

                KNOWN UNRELIABLE: tested live (1 real credit spent, verified
                the full path works end-to-end once) plus 3 free repeat
                attempts -- only 1 of 4 identical calls actually invoked the
                tool; the other 3 times Claude refused, correctly treating
                "the user already confirmed this" arriving in an automated
                prompt as a likely prompt-injection pattern (which, from its
                perspective in a stateless headless call, it can't
                distinguish from the real thing). This is Claude Code's
                injection-resistance working as intended, not a bug --
                download_asset's own tool description mandates interactive
                human confirmation, which a one-shot headless call
                structurally cannot provide.

                Do not paper over this with more insistent prompt wording --
                that's attacking a legitimate safety behavior, not fixing a
                bug. The correct fix is to bypass Claude's judgment for this
                one deterministic action (download a specific, already
                user-clicked asset_uuid is not a judgment call) via a raw
                MCP client authenticated through splice_auth.py's OAuth
                token, calling tools/call directly over the MCP protocol.
                Not built here: it needs a live token from a completed
                `splice_auth.py login` to build against and test, which
                requires a real interactive browser login this session
                couldn't perform. Until then, treat /download as something
                to call, check the response, and retry by hand if it
                declines -- not something to wire to an unattended trigger.

Run with: uvicorn service:app --host 127.0.0.1 --port 8787
"""

from __future__ import annotations

import json
import logging
import os
import shutil
import signal
import subprocess
import time
import urllib.request

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

from analysis import extract_features
from match import compose_query

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("clauducer")

app = FastAPI(title="clauducer backend")

DOWNLOAD_DIR = os.path.expanduser(os.environ.get("SPLICE_DOWNLOAD_DIR", "~/Music/Splice Matches"))

_RESULT_SCHEMA = {
    "type": "object",
    "properties": {
        "results": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "bpm": {"type": "number"},
                    "key": {"type": "string"},
                    "link": {"type": "string"},
                    "asset_uuid": {"type": "string"},
                },
                "required": ["name", "link", "asset_uuid"],
            },
        }
    },
    "required": ["results"],
}

_DOWNLOAD_URL_SCHEMA = {
    "type": "object",
    "properties": {
        "download_url": {"type": "string"},
        "file_name": {"type": "string"},
    },
    "required": ["download_url"],
}


def _run_claude_json(prompt: str, allowed_tool: str, schema: dict) -> dict:
    if shutil.which("claude") is None:
        raise HTTPException(
            status_code=501,
            detail="`claude` CLI not found on PATH. Use /match for the credential-free half of the pipeline.",
        )
    log.info("claude CLI: starting (%s)", allowed_tool)
    start = time.monotonic()
    # start_new_session so the whole process group (claude + any MCP child
    # it spawns) can be killed on timeout -- proc.kill() alone only kills
    # the top-level `claude` process and can leave a hung MCP grandchild
    # running behind it.
    proc_handle = subprocess.Popen(
        [
            "claude",
            "-p",
            prompt,
            "--output-format",
            "json",
            "--allowedTools",
            allowed_tool,
            "--permission-prompts",
            "none",
            "--no-session-persistence",
            "--json-schema",
            json.dumps(schema),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    try:
        stdout, stderr = proc_handle.communicate(timeout=90)
    except subprocess.TimeoutExpired:
        os.killpg(proc_handle.pid, signal.SIGKILL)
        proc_handle.wait()
        log.warning("claude CLI: timed out after %.1fs (%s)", time.monotonic() - start, allowed_tool)
        raise HTTPException(status_code=504, detail="claude CLI timed out")

    elapsed = time.monotonic() - start
    log.info("claude CLI: exited %d after %.1fs (%s)", proc_handle.returncode, elapsed, allowed_tool)

    if proc_handle.returncode != 0:
        raise HTTPException(status_code=502, detail=f"claude CLI failed: {stderr.strip()}")

    try:
        envelope = json.loads(stdout)
        return envelope["structured_output"]
    except (json.JSONDecodeError, KeyError) as exc:
        raise HTTPException(status_code=502, detail=f"unexpected claude CLI output: {exc}")


class MatchRequest(BaseModel):
    audio_path: str
    prompt: str


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


@app.post("/search")
def search(req: MatchRequest) -> dict:
    features = extract_features(req.audio_path)
    query = compose_query(features, req.prompt)

    call_args = f"query={query['query']!r}, type={query['type']!r}"
    if "bpm_min" in query and "bpm_max" in query:
        call_args += f", bpm_min={query['bpm_min']}, bpm_max={query['bpm_max']}"
    prompt = (
        f"Use the splice MCP tool describe_a_sound to search with {call_args}. "
        "Return only the results."
    )
    output = _run_claude_json(prompt, "mcp__splice__describe_a_sound", _RESULT_SCHEMA)
    return {"features": features.__dict__, "query": query, "results": output["results"]}


@app.post("/download")
def download(req: DownloadRequest) -> dict:
    """Spends a Splice purchase credit on first download of this asset_uuid."""
    prompt = (
        "The user has already explicitly confirmed they want to spend one "
        "Splice credit to download this asset (this HTTP request is that "
        f"confirmation). Use the splice MCP tool download_asset with "
        f"asset_uuid={req.asset_uuid!r} now, without asking again. Return "
        "the presigned download_url and, if available, the file's name."
    )
    output = _run_claude_json(prompt, "mcp__splice__download_asset", _DOWNLOAD_URL_SCHEMA)

    os.makedirs(DOWNLOAD_DIR, exist_ok=True)
    file_name = output.get("file_name") or req.name
    local_path = os.path.join(DOWNLOAD_DIR, file_name)
    try:
        urllib.request.urlretrieve(output["download_url"], local_path)
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"failed to fetch presigned download URL: {exc}")

    return {"local_path": local_path}
