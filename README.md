# Clauducer

Find Splice samples that fit a sound you already have.

Give Clauducer a reference sound and a short prompt ("gritty bass line").
The reference can be a few seconds of a track playing in Ableton, or an audio
file in the Standalone app. Clauducer then:

1. analyses the audio (tempo, key, brightness, loudness, timbre) and
   describes its mood in a few words,
2. turns that plus your prompt into a search query for Splice's catalog,
3. shows the matching samples. Click one to hear a preview; drag it onto a
   track to download the real WAV.

It has two parts:

- **`backend/`**: a small local Python server that does the analysis, the
  query writing and the Splice calls.
- **`plugin/`**: a JUCE audio plugin (VST3, AU and a Standalone app) that is
  the user interface. It talks to the backend over HTTP on
  `127.0.0.1:8787`.

---

## Contents

- [How it works](#how-it-works)
- [Current status](#current-status)
- [Requirements](#requirements)
- [Set it up yourself](#set-it-up-yourself)
- [Using it](#using-it)
- [Configuration](#configuration)
- [Tech stack and how to swap each part](#tech-stack-and-how-to-swap-each-part)
- [How previews work](#how-previews-work)
- [Backend HTTP API](#backend-http-api)
- [Development](#development)
- [Troubleshooting](#troubleshooting)

---

## How it works

```
 Plugin (JUCE)                              Backend (FastAPI, 127.0.0.1:8787)
 ───────────────                            ─────────────────────────────────
 reference audio ──► POST /analyze ───────► librosa features
 (4 s capture or                             + mood words (Claude / Ollama / template)
  a dropped file)  ◄── features, feeling ───

 prompt ─────────► POST /search ──────────► query (Claude / Ollama / template)
                                             └─► Splice MCP describe_a_sound
                   ◄── 10 results ─────────     (OAuth, no LLM involved)

 click a row ────► hidden web view plays the sample on its public Splice page
 drag a row ─────► POST /download ────────► Splice MCP download_asset
                   ◄── local WAV path ──────  (spends 1 credit the first time)
                   └─► dropped onto your track
```

- **The only model-driven steps** are writing the search query and the mood
  words. Everything else (analysis, search, download, preview) is plain
  code.
- **Splice is called directly** over its public MCP server
  (`https://mcp.splice.com/mcp`). The backend logs in with OAuth 2.0 (PKCE,
  dynamic client registration), so you don't need a developer key or the
  Claude app.
- **With no model configured**, both steps fall back to a fixed template
  built from the audio features. The whole app works with no API key and no
  local model.

---

## Current status

Tested on macOS (Apple Silicon, M4) only.

| Area | Status |
|---|---|
| Backend unit tests (24) | Passing |
| `/analyze`, `/search` against live Splice | Verified with real results |
| Splice login from the app (overlay → browser → token saved) | Verified |
| Local model (Ollama `qwen3.5:4b`) for query and mood words | Verified live |
| Claude API path (`ANTHROPIC_API_KEY`) | **Not tested** (no key was available) |
| Plugin builds (VST3, AU, Standalone) | Builds cleanly on macOS |
| Preview playback technique | Verified in a hidden WebKit view against live pages; **not yet confirmed by clicking inside the plugin** |
| Drag a result onto a track (`/download`) | **Not yet confirmed end-to-end** |
| Plugin inside Ableton Live | **Not yet tested** |
| Windows / Linux | **Not tested** (see [Platforms](#platforms)) |

---

## Requirements

| What | Version used | Needed for |
|---|---|---|
| macOS | 26 (Darwin 25) | Tested platform |
| Python | 3.12 (3.10+ should work) | Backend |
| `librosa`, `numpy` | librosa 0.11 | Audio analysis |
| `fastapi`, `uvicorn`, `pydantic` | fastapi 0.116, uvicorn 0.35 | Backend server |
| `anthropic` | any recent | *Optional*: only for the Claude API path |
| [Ollama](https://ollama.com) | any recent | *Optional*: local model path |
| CMake | 3.22 or newer | Building the plugin |
| Xcode command line tools (C++17) | any recent | Building the plugin |
| Git | any | CMake downloads JUCE 8.0.4 from GitHub |
| A Splice account | with credits for downloads | Search is free; each first download of a sample costs 1 credit |

---

## Set it up yourself

### 1. Get the code

```bash
git clone <this repo> clauducer
cd clauducer
```

### 2. Backend

```bash
cd backend
python3 -m venv .venv && source .venv/bin/activate   # optional but recommended
pip install librosa numpy fastapi uvicorn pydantic
# optional, only for the Claude API path:
pip install anthropic
```

Start it and leave it running:

```bash
uvicorn service:app --host 127.0.0.1 --port 8787
```

### 3. Log in to Splice (once)

Either:

- **From the app:** open the plugin or Standalone app. If you're not logged in, a
  "Log in to Splice" overlay appears; click it, approve in the browser, and
  the app continues once the login completes. Or:
- **From the terminal:** `python3 splice_auth.py login` in `backend/`.

The token is saved to `~/.clauducer/splice_token.json` (readable only by
you) and refreshed automatically.

### 4. Pick how queries and mood words are written (optional)

Pick one; the backend checks them in this order:

| Option | How | Cost |
|---|---|---|
| **Claude API** | `export ANTHROPIC_API_KEY=sk-ant-...` before starting uvicorn | Pay per use on the Anthropic API. A Claude Pro/Max subscription does **not** include API access. |
| **Local model via Ollama** | `brew install ollama`, `ollama serve &`, `ollama pull qwen3.5:4b`, then start uvicorn with `OLLAMA_MODEL=qwen3.5:4b` | Free, runs offline, ~3.4 GB download, ~1–2 s per call on Apple Silicon |
| **Template (default)** | Set neither | Free and instant, but less nuanced |

Example with the local model:

```bash
OLLAMA_MODEL=qwen3.5:4b uvicorn service:app --host 127.0.0.1 --port 8787
```

### 5. Build the plugin

```bash
cd plugin
cmake -B build -S .
cmake --build build
```

The first configure downloads JUCE 8.0.4 into `plugin/build/_deps/`, which
takes a few minutes and ~300 MB. A successful build also **installs** the
plugins to your user plugin folders (`COPY_PLUGIN_AFTER_BUILD`):

| Output | Built at | Installed to |
|---|---|---|
| Standalone app | `plugin/build/Clauducer_artefacts/Standalone/Clauducer.app` | — |
| VST3 | `plugin/build/Clauducer_artefacts/VST3/Clauducer.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| AU | `plugin/build/Clauducer_artefacts/AU/Clauducer.component` | `~/Library/Audio/Plug-Ins/Components/` |

Clean rebuild: `rm -rf plugin/build` and repeat.

---

## Using it

**Start the backend first** (step 2). The plugin shows an error with the
exact command if it can't reach it.

### Standalone app (no DAW needed)

```bash
open plugin/build/Clauducer_artefacts/Standalone/Clauducer.app
```

1. Drop an audio file (wav, aif, mp3, flac) on the window, or click the orb
   to choose one.
2. Type what you want, e.g. "gritty bass line".
3. Click the orb. The log panel shows each step: the file loaded, its
   features, its mood words ("Feeling: moody, shadowy, forceful, driving"),
   the query sent to Splice and how many results came back.
4. Results replace the search view (**← Back** returns). Each row shows
   name, BPM (loops only), key and duration.

### Inside a DAW (Ableton Live etc.)

1. In Ableton: Preferences → Plug-Ins → rescan. Then put **Clauducer** as an
   audio effect on the track you want to match.
2. Play the track. The plugin passes audio through unchanged and keeps the
   last 10 s in memory.
3. Type a prompt and click the orb. It uses the **last 4 seconds** that
   played as the reference.

### Results

- **Click a row** to hear the preview; the row shows ▶ while it plays.
  Click it again to stop. It stops by itself when the preview ends.
- **Hover over a row** before clicking. That loads its preview in the
  background, so the click plays in about 0.7 s instead of about 2 s.
- **Drag a row** onto a track (or into Finder) to download the full WAV
  into `~/Music/Splice Matches/` and drop it there. This **spends 1 Splice
  credit** the first time you download that sample; after that it's free.

### The orb

Its ripple reacts to how you press it: a quick tap makes a small, sharp
splash, and holding makes the surface swell and then release a bigger,
slower splash (full strength at 1 s).

---

## Configuration

| Setting | Where | Default | Effect |
|---|---|---|---|
| `ANTHROPIC_API_KEY` | env var for uvicorn | unset | Use Claude (`claude-opus-5`) for the query and mood words |
| `OLLAMA_MODEL` | env var for uvicorn | unset | Use this local Ollama model (only if no Anthropic key) |
| `OLLAMA_URL` | env var for uvicorn | `http://localhost:11434` | Where Ollama runs |
| `SPLICE_DOWNLOAD_DIR` | env var for uvicorn | `~/Music/Splice Matches` | Where dragged samples are saved |
| Backend port | `uvicorn --port` **and** `plugin/Source/BackendClient.h` (`http://127.0.0.1:8787`) | 8787 | Change both, then rebuild the plugin |
| Splice token | `backend/splice_auth.py` (`TOKEN_PATH`) | `~/.clauducer/splice_token.json` | Delete it to log out |
| OAuth callback port | `backend/splice_auth.py` (`CALLBACK_PORT`) | 3119 | Change if something else uses 3119 |
| Capture length (DAW) | `plugin/Source/PluginEditor.cpp` (`captureReferenceToTempFile(4.0)`) | 4 s (buffer holds 10 s) | How much recent audio becomes the reference |
| Claude model | `backend/match.py` (`_CLAUDE_MODEL`) | `claude-opus-5` | Any Claude model id |

---

## Tech stack and how to swap each part

Each row is one part of the app: what it uses today, and what changes if you swap it.

### Query and mood-word writing (`backend/match.py`)

- **Now:** Claude API (`anthropic` SDK, `claude-opus-5`), or a local model
  over Ollama's `/api/chat`, or a fixed template.
- **Ollama details:** thinking is turned off (`"think": false`), the query is
  constrained to a JSON schema, and the mood words are constrained to a
  3–5-item word list.
- **Safety checks in code (for every source):** a query that mentions "the
  reference" is rejected, since Splice only sees text; a made-up BPM range
  is dropped for sounds with no tempo; mood words must be 3–5 single words.
  If a check fails, it falls back to the template.
- **Swap to another Ollama model:** set `OLLAMA_MODEL` to any model you've
  pulled, e.g. `llama3.2:3b`, `gemma3:4b`, `phi4-mini`, `qwen3.5:2b`. No
  code change.
- **Swap to LM Studio, llama.cpp server, vLLM or another hosted API:** add a
  branch next to `_ollama_chat()` that calls that server and returns the
  reply text. Keep the `_checked_query` / `_checked_feeling` checks and the
  template fallback.
- **Swap the Claude model:** change `_CLAUDE_MODEL`.

### Audio analysis (`backend/analysis.py`)

- **Now:** `librosa`, which gives:
  - tempo (beat tracking);
  - key and mode (Krumhansl–Schmuckler key profiles on chroma);
  - brightness (spectral centroid);
  - loudness (RMS);
  - timbre (MFCC tilt).
- **Swap:** Essentia or aubio can replace `extract_features()`. Keep the
  `AudioFeatures` fields the same and nothing else needs to change.

### Sample catalog (`backend/splice_auth.py`, `backend/mcp_client.py`, `backend/service.py`)

- **Now:** Splice's MCP server. `describe_a_sound` handles search and
  `download_asset` handles downloads. A small MCP client (`initialize` +
  `tools/call` over Streamable HTTP) handles the connection, with OAuth 2.0
  PKCE and dynamic client registration. Search results come back as
  markdown, which `_parse_search_results` parses.
- **Swap to another catalog:** replace:
  - `call_tool("describe_a_sound", …)` and the parser in `/search`;
  - `download_asset` in `/download`.

  The plugin only needs each result as
  `{name, bpm?, key?, duration_sec?, link, asset_uuid}`. `asset_uuid` can be
  any id your `/download` understands. Previews are Splice-specific (see
  below) and would need their own approach.

### Backend server (`backend/service.py`)

- **Now:** FastAPI + uvicorn on `127.0.0.1:8787`, local-only.
- **Swap:** any HTTP server works if it keeps the same routes and JSON
  shapes ([API](#backend-http-api)). To change the port, see
  [Configuration](#configuration).

### Plugin framework (`plugin/`)

- **Now:** JUCE 8.0.4 (fetched by CMake), C++17, VST3 + AU + Standalone.
- **Swap JUCE version:** change `GIT_TAG` in `plugin/CMakeLists.txt`.
- **Other formats:**
  - **CLAP:** can be added with the third-party `clap-juce-extensions`
    (not set up).
  - **AAX (Pro Tools):** needs Avid's SDK and signing.

### Graphics

- **Now:** OpenGL (`juce_opengl`) for the ripple orb (`RippleSphere`,
  `CircularCaptureButton`). Everything else is normal JUCE components.
- **Note:** the orb's OpenGL view draws on top of other components, so
  overlays hide it while they're showing.

### Preview playback (`plugin/Source/PreviewPlayer.cpp`)

- **Now:** `juce::WebBrowserComponent` (WKWebView on macOS) running
  Splice's own web player, invisibly.
- **Swap:** see [How previews work](#how-previews-work).

### Platforms

- **macOS:** the only tested platform.
- **Windows:** VST3 and Standalone should build, but it's untested:
  - the preview web view needs Microsoft's WebView2 runtime and JUCE's
    WebView2 backend (`JUCE_USE_WIN_WEBVIEW2=1`, and
    `WebBrowserComponent::Options::withBackend(webview2)`);
  - the backend runs anywhere Python and librosa do.
- **Linux:** JUCE's web view needs WebKitGTK. Untested.
- **AU:** macOS only.
- **CI:** `.github/workflows/` contains starter CMake and Python workflows.
  They haven't been checked against this project.

---

## How previews work

Splice's MCP tools don't return any preview audio. Splice's public
sample pages do have a preview, but the audio file is **scrambled**, and
only Splice's own web player can play it. Clauducer does not download,
unscramble or store preview audio. Doing so would mean getting around
Splice's protection and is against their
[Terms of Use](https://splice.com/terms).

Instead, the plugin plays previews the way you would in a browser:

1. `PreviewPlayer` keeps one Splice sample page open in a **hidden** web
   view.
2. A small injected script presses that page's own play button
   (`button[data-qa="play-button"]`). It reads the button's "Play"/"Pause"
   label to know whether audio is playing, and presses it again to stop.
3. To switch samples, the script clicks a link to the other sample, so
   Splice's site switches pages without a full reload.
4. To keep clicks fast, the first result is loaded as soon as results
   arrive, and hovering a row loads that sample ahead of the click.

**Measured on live pages** (hidden WKWebView, M4):

| Case | Click → sound |
|---|---|
| Full page load (old approach) | ~3.5–4.5 s |
| Hovered ~2 s, then click | ~0.7 s |
| Clicked without hovering | ~1.2–2 s |
| After pressing Play, fetching the preview file | ~0.2–0.65 s (Splice only fetches it when Play is pressed) |

**Limits:**
- Each open Splice page costs about 214 MB of memory, so only one is kept
  open.
- Hovering while a preview plays does nothing, because it would cut off
  the sound.
- The script depends on Splice's page layout. If they rename that button,
  previews stop until the selector in `PreviewPlayer.cpp` is updated.
- It automates their own player. Check Splice's Terms of Use before
  distributing this beyond personal use.

**Alternatives:**
- **Official download on click:** always-instant replays and a real WAV,
  but 1 credit per new sample.
- **Several preloaded pages:** faster first clicks, more memory.
- **Open the page in your browser:** zero code, least convenient.

---

## Backend HTTP API

All routes are on `http://127.0.0.1:8787`.

| Method & path | Body | Returns |
|---|---|---|
| `POST /analyze` | `{"audio_path"}` | `{"features": {...}, "feeling": "moody, shadowy, forceful, driving"}` |
| `POST /search` | `{"audio_path", "prompt", "features"?}` | `{"query": {...}, "results": [{name, bpm?, key?, duration_sec?, link, asset_uuid}]}`. Pass `features` from `/analyze` to skip re-analysing. |
| `POST /download` | `{"asset_uuid", "name"}` | `{"local_path"}`. **Spends 1 credit** the first time per sample. |
| `GET /auth/status` | — | `{"authorized", "login_in_progress", "error"}` |
| `POST /auth/login` | — | `{"started"}`. Opens the browser login; poll `/auth/status`. |
| `POST /match` | `{"audio_path", "prompt"}` | Features + the composed query, without calling Splice (for debugging) |

Errors:
- **400:** the audio file couldn't be analysed.
- **401:** not logged in to Splice (the plugin then shows the login
  overlay).
- **502:** Splice or the network failed.

CLI (no server):

```bash
python3 backend/match.py path/to/audio.wav "a warm pad to sit under this vocal"
```

---

## Development

```
backend/
  analysis.py      audio features (librosa)
  match.py         query + mood words (Claude / Ollama / template) and their checks
  service.py       FastAPI app: routes, Splice result parsing
  splice_auth.py   OAuth login, token storage/refresh, MCP tool calls
  mcp_client.py    minimal MCP Streamable HTTP client
  test_*.py        unit tests (network mocked)
plugin/
  CMakeLists.txt   JUCE fetch, plugin formats, sources
  Source/
    PluginProcessor.*        audio passthrough, 10 s capture buffer, search thread
    PluginEditor.*           layout, search/results views, file drop
    BackendClient.*          HTTP client for the backend
    CircularCaptureButton.*  the orb (OpenGL), press-duration dynamics
    RippleSphere.*           orb shader/mesh
    ResultsListComponent.*   result rows, click-to-preview, drag-to-download
    PreviewPlayer.*          hidden Splice web player
    LogPanel.*, LoginOverlay.*, AppFont.h, BackgroundGradient.h, RoundedLookAndFeel.h
```

Run the backend tests:

```bash
cd backend
python3 -m unittest test_match.py test_search.py test_download.py -v
```

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| "Could not reach backend at http://127.0.0.1:8787" | Start it: `cd backend && uvicorn service:app --host 127.0.0.1 --port 8787` |
| Login overlay keeps appearing / 401 errors | Log in again from the overlay, or `python3 backend/splice_auth.py login`. To reset, delete `~/.clauducer/splice_token.json`. |
| Login never finishes | Something else may be using port 3119 (the OAuth callback). Free it or change `CALLBACK_PORT`. |
| Mood words look like the template although `OLLAMA_MODEL` is set | Check the uvicorn log for `warning: Ollama ...`. Is `ollama serve` running and the model pulled? `ANTHROPIC_API_KEY` takes priority if set. |
| First search after starting Ollama is slow | The model loads on first use (~5 s); it then stays loaded for 30 min. |
| Clicking a row plays nothing | The Splice page may have changed its play button. Check `button[data-qa="play-button"]` in `PreviewPlayer.cpp`. |
| Plugin doesn't show in Ableton | Rescan plug-ins. Check `~/Library/Audio/Plug-Ins/VST3/Clauducer.vst3` and `~/Library/Audio/Plug-Ins/Components/Clauducer.component` exist. |
| mcp.splice.com returns 403 | It blocks Python's default User-Agent; `splice_auth.py` sends its own. Keep it if you change those HTTP calls. |
