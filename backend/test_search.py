"""Unit tests for service._parse_search_results.

describe_a_sound has no output schema and replies in markdown; the plugin's
BackendClient expects {name, bpm?, key?, link, asset_uuid} per result, and
asset_uuid is what /download spends a credit on -- so a parse that silently
drops or mangles it breaks the whole pick-and-download flow.
Samples below are trimmed from real live responses (loop and oneshot).
Run with: python3 -m unittest test_search.py -v
"""

from __future__ import annotations

import unittest

from service import _parse_search_results

LOOP_TEXT = """## Search results for: "warm analog pad"
Showing 10 of 7422 total results

### 1. DYL_90_kit_groovin_synth_stabs_Emin.wav
BPM: 90 | Key: e minor | Duration: 10.7s | Type: loop
**Pack:** Sample Magic
**Tags:** synth, stabs, indie, indie pop
**Link:** https://splice.com/sounds/sample/553e/dyl-90-kit-groovin-synth-stabs-emin-wav
**Asset UUID:** 2821c85c-893d-47fe-8bb1-652a95684c0c

### 2. PL_VEN_88_Piano_Loop_Distance_A#m.wav
BPM: 88 | Key: a# minor | Duration: 10.9s | Type: loop
**Pack:** Prime Loops
**Link:** https://splice.com/sounds/sample/08d5/pl-ven-88-piano-loop-distance-a-m-wav
**Asset UUID:** 789cc7bc-3d43-4274-b491-e84ef1f40248
"""

ONESHOT_TEXT = """## Search results for: "punchy kick drum"
Showing 10 of 15428 total results

### 1. ESM_NDS_drum_kick_one_shot_punchy_round_sustain.wav
Duration: 0.6s | Type: oneshot
**Pack:** Epic Stock Media
**Link:** https://splice.com/sounds/sample/858e/esm-nds-drum-kick
**Asset UUID:** 461d86f4-2ef4-4a76-be15-e452e5d4bd3a
"""


class ParseSearchResultsTests(unittest.TestCase):
    def test_loop_results_keep_uuid_bpm_and_key_per_block(self):
        results = _parse_search_results(LOOP_TEXT)
        self.assertEqual(len(results), 2)
        # uuid must belong to its own block, not a neighbour's
        self.assertEqual(results[1]["asset_uuid"], "789cc7bc-3d43-4274-b491-e84ef1f40248")
        self.assertEqual(results[1]["name"], "PL_VEN_88_Piano_Loop_Distance_A#m.wav")
        self.assertEqual(results[0]["bpm"], 90.0)
        self.assertEqual(results[0]["key"], "e minor")

    def test_oneshot_has_no_bpm_or_key_rather_than_a_wrong_one(self):
        (result,) = _parse_search_results(ONESHOT_TEXT)
        self.assertNotIn("bpm", result)
        self.assertNotIn("key", result)
        self.assertEqual(result["asset_uuid"], "461d86f4-2ef4-4a76-be15-e452e5d4bd3a")

    def test_block_without_uuid_is_skipped(self):
        text = "### 1. broken.wav\nBPM: 90\n**Link:** https://x\n"
        self.assertEqual(_parse_search_results(text), [])


if __name__ == "__main__":
    unittest.main()
