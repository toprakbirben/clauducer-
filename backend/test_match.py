"""Unit tests for the Ollama path in match.py.

A small local model can ignore the system prompt, so match.py enforces the
rules that matter in code: a sound with no detectable tempo must not get a
bpm range (it would filter out every sustained/one-shot match), a rambling
"feeling" must not reach the plugin log, and an unreachable Ollama must fall
back to the template instead of failing /analyze or /search.
Run with: python3 -m unittest test_match.py -v
"""

from __future__ import annotations

import os
import unittest
from unittest import mock

import match
from analysis import AudioFeatures


def _features(tempo_bpm: float) -> AudioFeatures:
    return AudioFeatures(
        path="ref.wav", duration_sec=8.0, tempo_bpm=tempo_bpm, key="A", mode="minor",
        key_confidence=0.8, brightness="dark", spectral_centroid_hz=900.0,
        loudness_rms_db=-18.0, dynamics="punchy", timbre_descriptors=["dark", "punchy"],
    )


class CheckedQueryTests(unittest.TestCase):
    def test_invented_bpm_range_dropped_when_no_tempo(self):
        result = match._checked_query(
            {"query": "airy pad", "type": "loop", "bpm_min": 0, "bpm_max": 4}, _features(0.0)
        )
        self.assertNotIn("bpm_min", result)
        self.assertNotIn("bpm_max", result)

    def test_bpm_range_kept_when_tempo_detected(self):
        result = match._checked_query(
            {"query": "dark drums", "type": "loop", "bpm_min": 88, "bpm_max": 96}, _features(92.0)
        )
        self.assertEqual((result["bpm_min"], result["bpm_max"]), (88, 96))

    def test_reversed_bpm_range_is_swapped(self):
        # Splice would return nothing for min > max.
        result = match._checked_query(
            {"query": "dark drums", "type": "loop", "bpm_min": 96, "bpm_max": 88}, _features(92.0)
        )
        self.assertEqual((result["bpm_min"], result["bpm_max"]), (88, 96))

    def test_empty_query_rejected(self):
        with self.assertRaises(ValueError):
            match._checked_query({"query": "  ", "type": "loop"}, _features(92.0))


class CheckedFeelingTests(unittest.TestCase):
    def test_short_feeling_passes(self):
        self.assertEqual(match._checked_feeling("Brooding and restless."), "Brooding and restless.")

    def test_rambling_feeling_rejected(self):
        with self.assertRaises(ValueError):
            match._checked_feeling("Sure! " + "very moody " * 40)
        with self.assertRaises(ValueError):
            match._checked_feeling("Line one.\nLine two.\nLine three.")


class FallbackTests(unittest.TestCase):
    def test_unreachable_ollama_falls_back_to_template(self):
        env = {"OLLAMA_MODEL": "qwen3.5:4b", "OLLAMA_URL": "http://127.0.0.1:9"}
        with mock.patch.dict(os.environ, env, clear=False), \
                mock.patch.dict(os.environ, {"ANTHROPIC_API_KEY": ""}), \
                mock.patch.object(match, "_OLLAMA_URL", env["OLLAMA_URL"]):
            features = _features(92.0)
            self.assertEqual(match.describe_feeling(features), match._template_feeling(features))
            self.assertEqual(
                match.compose_query(features, "dark drums"), match._template_query(features, "dark drums")
            )


if __name__ == "__main__":
    unittest.main()
