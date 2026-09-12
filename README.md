# clauducer-

Analyzes a reference sound from an Ableton Live track plus a text prompt,
searches Splice's catalog for a match, and lets you drag the result straight
into a track.

Delivered as a standalone audio-effect plugin (VST3/AU, `plugin/`), not a Max
for Live device: dragging a file out of an M4L device's own UI onto an
Ableton track has no supported API in Max for Live (confirmed against
Cycling '74's docs and forums -- `live.drop` and the Max SDK's drag/drop
hooks only support accepting drops *into* Max, not originating a drag out of
it). A standalone plugin has a first-class API for exactly this
(JUCE's `DragAndDropContainer::performExternalDragDropOfFiles`), the same
mechanism real sample-browser plugins use, and as an audio-effect insert it
also gets the track's live audio directly, same as M4L would have.

## Layout

- `backend/` -- Python service: audio analysis, Splice search, Splice
  download (see `backend/README.md`).
- `plugin/` -- JUCE-based VST3/AU plugin UI (see below).

## Plugin

CMake + JUCE (fetched automatically). Requires the backend running locally
first (`uvicorn service:app --host 127.0.0.1 --port 8787` from `backend/`).

```
cmake -B build -S plugin
cmake --build build
```

Not yet built or run in a real DAW in this environment (no CMake/JUCE/Xcode
plugin toolchain available here) -- verify by loading the built VST3/AU on
an Ableton track before relying on it. See the plan file / design notes for
the full verification checklist (passthrough transparency, capture/search
round trip, a real drag-and-drop landing on another track with the Splice
credit balance decreasing by exactly one).

No in-plugin audio preview in v1: confirmed live against Splice's real MCP
tools that `describe_a_sound` returns no preview-audio URL, only a link to
the sound's Splice webpage -- there's no audio source to play before
dragging a result in.
