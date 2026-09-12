# Backend prototype (Milestone 2)

Validates the `analyze → compose query → Splice search` pipeline.

## Files

- `analysis.py` — extracts tempo, key/mode, brightness, loudness, and timbre
  descriptors from a local audio file with librosa.
- `match.py` — combines `analysis.py`'s output with a free-text prompt into
  the arguments for Splice MCP's `describe_a_sound` tool (a natural-language
  query + bpm range). Uses the Claude API to write the query if
  `ANTHROPIC_API_KEY` is set, otherwise falls back to a deterministic
  template — no credentials required to run it.
- `service.py` — Milestone 3: wraps the same pipeline as a long-running local
  FastAPI service (`POST /match`, `POST /search`), so a Max for Live device
  can call it over HTTP instead of spawning a Python process per request.

## Usage

CLI:
```
pip install librosa numpy          # anthropic is optional, see below
python3 match.py path/to/audio.wav "a warm layering pad to sit under this vocal"
```

Service:
```
pip install librosa numpy fastapi uvicorn
uvicorn service:app --host 127.0.0.1 --port 8787
curl -X POST http://127.0.0.1:8787/match -H "Content-Type: application/json" \
  -d '{"audio_path": "path/to/audio.wav", "prompt": "a warm layering pad to sit under this vocal"}'
```

`/match` prints the extracted features and the ready-to-run `describe_a_sound`
call — no credentials required. `/search` additionally executes that query
against the live Splice catalog by shelling out to the `claude` CLI in
headless mode (`claude -p ... --output-format json --json-schema ...`). This
uses whatever the `claude` CLI is already logged in with — **a Claude Pro/Max
subscription, not a separate pay-per-token API key** — plus the `splice` MCP
server already registered for that CLI. Check both with:

```
claude mcp list          # should show: splice  https://mcp.splice.com/mcp (HTTP) - ✔ Connected
```

If `splice` isn't listed, register it once (`claude mcp add --transport http splice https://mcp.splice.com/mcp`)
and complete the browser login the first time you use it interactively —
same as any other MCP server. Verified end-to-end against the live Splice
catalog while building this (see "Validated so far" below).

`splice_auth.py`'s OAuth login (`python3 splice_auth.py login`, one-time,
opens a browser) is required for `/download`, which now uses it directly
(see "`/download`" below). It's not required for `/match` or `/search`,
which still go through the `claude` CLI/subscription.

## Validated so far

Full pipeline (`/search`, not just query composition) run end-to-end against
a real local file (86.1 BPM, C# minor, dark/thin timbre) with the prompt "a
warm layering pad to sit under this vocal": `POST /search` extracted
features, composed a query, ran it through `claude -p` against the real
`splice` MCP server, and returned 10 real, schema-validated results — all at
90 BPM, mostly in closely related keys (D#/C#/D major, D/F/A minor — several
off by a relative-major shift rather than an exact minor match). Good enough
to confirm the whole pipeline works; the query wording could be tuned later
to weight key-matching harder if exact-key results matter more than they do
here.

**Cost/usage note:** each `/search` call is a full `claude -p` invocation —
in testing it used ~55k tokens of fresh context (mostly cache-eligible) and
took ~25s. Under a subscription this doesn't cost money per call, but it
does count against your plan's usage/rate limits like any other Claude Code
session, so calling `/search` on every keystroke or in a tight loop isn't
free in that sense — fine for a human clicking "Match" occasionally, worth
knowing before wiring it to something more automated.

## Standalone Splice authentication (resolved)

Splice's MCP server (docs: https://splice.com/tools/mcp-server and
https://support.splice.com/en/articles/14442749) publishes standard OAuth
discovery metadata at `https://mcp.splice.com/.well-known/oauth-authorization-server`,
including a `registration_endpoint` — meaning it supports RFC 7591 dynamic
client registration. Any standards-compliant client (ours included) can
self-register and run an Authorization Code + PKCE flow with no pre-shared
`client_id` or Splice-side developer partnership needed; this is the same
mechanism Cursor/VS Code/etc use per Splice's own docs.

`splice_auth.py` implements this: `login` registers a client, opens a
browser for the user to authorize, and stores the resulting tokens (with
automatic refresh) in `~/.clauducer/splice_token.json`. Discovery and
dynamic registration were tested directly against the live server and work;
the interactive browser/login leg needs a real browser session to test,
which is on you to run locally.

Note: `mcp.splice.com` 403s the default Python `urllib` User-Agent (bot
blocking) — `splice_auth.py` sends a custom one; keep that if you touch its
HTTP calls.

## `/download`

Calls Splice's `download_asset` MCP tool directly over JSON-RPC
(`mcp_client.py`, wired up in `splice_auth.download_asset`), authenticated
with the OAuth access token from `splice_auth.py login` — no LLM in the
loop.

This replaces an earlier `claude -p`-based implementation that was
unreliable by design: asking an LLM to decide whether to call
`download_asset` on every request is the wrong shape for a deterministic,
already user-confirmed action (deciding what to search for is a genuine
judgment call; downloading a specific, already user-clicked `asset_uuid` is
not). `download_asset`'s tool description mandates human confirmation
before spending a credit, and a stateless one-shot `claude -p` call has no
way to prove to Claude that a real confirmation happened, so Claude
correctly refused about 3 of 4 identical automated attempts in testing —
that was Claude's injection-resistance working as intended, not a bug to
route around with more insistent prompt wording.

`mcp_client.py` implements the minimal subset of the MCP Streamable HTTP
transport needed for `initialize` + `tools/call`. It was built and unit
tested (`test_download.py`, all HTTP mocked) without a live Splice OAuth
token, so the exact shape of `download_asset`'s success response
(`structuredContent` vs. a JSON-in-text content block) is handled
defensively in `mcp_client.extract_tool_result` but not yet confirmed
against the real server — verify this against a real account (`python3
splice_auth.py login` then a real `/download` call) before relying on it
unattended.
