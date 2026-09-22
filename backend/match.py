"""Milestone 2 prototype: audio analysis -> a ready-to-run Splice search.

Takes a local audio file (standing in for "the clip/track already in your
Ableton set") and a free-text prompt, extracts audio features with
analysis.py, and composes the arguments for Splice MCP's `describe_a_sound`
tool: a natural-language query plus a bpm range.

This script does NOT call Splice MCP itself. Splice's `describe_a_sound` is
only reachable right now through an authenticated MCP connection (this Claude
Code session already has one). Wiring a fully standalone backend requires its
own OAuth grant against mcp.splice.com, which is an open item -- see
backend/README.md. Until that's resolved, the intended flow is: run this
script, then hand its printed query/bpm range to `describe_a_sound`.

Query composition uses the Claude API when ANTHROPIC_API_KEY is set (richer,
folds in nuance from the prompt); otherwise it falls back to a deterministic
template built purely from the extracted features, so this stays runnable
with zero credentials.
"""

from __future__ import annotations

import argparse
import json
import os
import sys

from analysis import AudioFeatures, extract_features

_SYSTEM_PROMPT = """You turn a short user prompt plus extracted audio features \
into a single natural-language search query for Splice's sample catalog, plus \
an optional BPM range. The query should read like a producer's request \
(mention key, timbre/mood words, and how the sample should relate to the \
reference audio), not a list of raw numbers. If tempo_bpm is 0 or missing, \
the reference has no reliable tempo (a sustained pad/texture/one-shot) -- \
omit bpm_min/bpm_max entirely rather than inventing a narrow range around 0. \
Respond with strict JSON: {"query": str, "bpm_min": int (optional), \
"bpm_max": int (optional), "type": "loop"|"oneshot"}."""


_CLAUDE_MODEL = "claude-opus-5"

_FEELING_SYSTEM_PROMPT = """You describe how a piece of reference audio feels \
to a music producer, given only extracted audio features. Write 1-2 short \
sentences about its mood and feel (e.g. brooding, restless, laid-back). No raw \
numbers, no preamble -- reply with the sentences only."""


_MIN_PLAUSIBLE_BPM = 20  # below this, treat tempo detection as having failed
# (e.g. sustained pads/textures/one-shots with no rhythmic onsets to lock onto --
# librosa.beat.beat_track legitimately returns 0 for these, it's not a bug)


def _template_query(features: AudioFeatures, prompt: str) -> dict:
    descriptors = ", ".join(features.timbre_descriptors)
    has_tempo = features.tempo_bpm >= _MIN_PLAUSIBLE_BPM

    if has_tempo:
        query = (
            f"{prompt}. Should sit well with a reference in {features.key} "
            f"{features.mode} at {features.tempo_bpm:.0f} BPM, with a "
            f"{descriptors} character."
        )
    else:
        # No reliable tempo to anchor to (sustained/non-rhythmic reference) --
        # describe the key and timbre only, and don't constrain bpm at all.
        query = (
            f"{prompt}. Should sit well with a reference in {features.key} "
            f"{features.mode} (no fixed tempo -- a sustained/non-rhythmic "
            f"sound), with a {descriptors} character."
        )

    result = {"query": query, "type": "loop"}
    if has_tempo:
        margin = 4
        result["bpm_min"] = max(1, round(features.tempo_bpm) - margin)
        result["bpm_max"] = round(features.tempo_bpm) + margin
    return result


def _claude_query(features: AudioFeatures, prompt: str) -> dict:
    import anthropic  # local import: only required for this code path

    client = anthropic.Anthropic()
    user_content = (
        f"User prompt: {prompt}\n\nExtracted audio features:\n"
        f"{json.dumps(features.__dict__, indent=2)}"
    )
    response = client.messages.create(
        model=_CLAUDE_MODEL,
        max_tokens=4096,
        system=_SYSTEM_PROMPT,
        messages=[{"role": "user", "content": user_content}],
    )
    text = "".join(block.text for block in response.content if block.type == "text")
    return json.loads(text)


def compose_query(features: AudioFeatures, prompt: str) -> dict:
    if os.environ.get("ANTHROPIC_API_KEY"):
        try:
            return _claude_query(features, prompt)
        except Exception as exc:  # fall back rather than hard-fail the prototype
            print(f"warning: Claude query composition failed ({exc}); using template", file=sys.stderr)
    return _template_query(features, prompt)


_MODE_MOOD = {"major": "open, uplifting", "minor": "moody, introspective"}
_BRIGHTNESS_MOOD = {"dark": "shadowy", "warm": "warm", "bright": "clear", "airy": "airy, shimmering"}
_DYNAMICS_MOOD = {"quiet": "restrained", "moderate": "steady", "punchy": "forceful"}


def _tempo_feel(bpm: float) -> str:
    if bpm < _MIN_PLAUSIBLE_BPM:
        return "free-floating, without a fixed pulse"
    if bpm < 90:
        return "slow and unhurried"
    if bpm < 120:
        return "mid-tempo and grooving"
    if bpm < 140:
        return "driving"
    return "fast and urgent"


def _template_feeling(features: AudioFeatures) -> str:
    mode = _MODE_MOOD.get(features.mode, features.mode)
    tone = _BRIGHTNESS_MOOD.get(features.brightness, features.brightness)
    energy = _DYNAMICS_MOOD.get(features.dynamics, features.dynamics)
    return (
        f"A {mode} sound in {features.key} {features.mode}, {_tempo_feel(features.tempo_bpm)}. "
        f"The tone is {tone} and the energy {energy}."
    )


def _claude_feeling(features: AudioFeatures) -> str:
    import anthropic  # local import: only required for this code path

    client = anthropic.Anthropic()
    response = client.messages.create(
        model=_CLAUDE_MODEL,
        max_tokens=4096,
        system=_FEELING_SYSTEM_PROMPT,
        messages=[{"role": "user", "content": json.dumps(features.__dict__, indent=2)}],
    )
    text = "".join(block.text for block in response.content if block.type == "text").strip()
    if not text:
        raise ValueError(f"empty response (stop_reason={response.stop_reason})")
    return text


def describe_feeling(features: AudioFeatures) -> str:
    if os.environ.get("ANTHROPIC_API_KEY"):
        try:
            return _claude_feeling(features)
        except Exception as exc:  # fall back rather than hard-fail
            print(f"warning: Claude feeling description failed ({exc}); using template", file=sys.stderr)
    return _template_feeling(features)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audio_path", help="Path to the reference audio file")
    parser.add_argument("prompt", help="Free-text description of the sample you want")
    args = parser.parse_args()

    features = extract_features(args.audio_path)
    query = compose_query(features, args.prompt)

    print("--- extracted features ---")
    print(json.dumps(features.__dict__, indent=2))
    print("\n--- describe_a_sound call ---")
    print(json.dumps(query, indent=2))


if __name__ == "__main__":
    main()
