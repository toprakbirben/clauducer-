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
    # The plugin logs this as "Feeling: <words>" -- a handful of mood tags,
    # not a paragraph.
    def test_mood_words_pass_and_are_normalised(self):
        self.assertEqual(match._checked_feeling("Brooding, restless, laid-back"), "brooding, restless, laid-back")

    def test_sentence_rejected(self):
        with self.assertRaises(ValueError):
            match._checked_feeling("This dark arp feels brooding, restless, and tense.")

    def test_too_few_or_too_many_words_rejected(self):
        with self.assertRaises(ValueError):
            match._checked_feeling("moody, dark")
        with self.assertRaises(ValueError):
            match._checked_feeling("moody, dark, tense, restless, cold, heavy")

    def test_template_feeling_is_valid_words(self):
        for tempo in (0.0, 80.0, 128.0, 170.0):
            feeling = match._template_feeling(_features(tempo))
            self.assertEqual(match._checked_feeling(feeling), feeling)


class ConcreteQueryTests(unittest.TestCase):
    # describe_a_sound only receives the text; "the reference" means nothing
    # to it and just adds noise to the search.
    def test_query_mentioning_reference_rejected(self):
        with self.assertRaises(ValueError):
            match._checked_query(
                {"query": "bass that matches the reference track's energy", "type": "loop"}, _features(92.0)
            )

    def test_template_query_never_mentions_reference(self):
        for tempo in (0.0, 92.0):
            query = match._template_query(_features(tempo), "gritty bass")["query"]
            self.assertNotIn("reference", query.lower())
            self.assertIn("A minor", query)


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
