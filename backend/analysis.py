"""Audio feature extraction for the Splice sample-matching pipeline.

Extracts the features described in the project plan (tempo, key, timbre/
spectral descriptors) from a local audio file, to be folded into a natural
language query for Splice's `describe_a_sound` tool.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass

import librosa
import numpy as np

# Krumhansl-Schmuckler key profiles, used to estimate musical key from a
# chroma vector by correlating against every major/minor rotation.
_MAJOR_PROFILE = np.array(
    [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
)
_MINOR_PROFILE = np.array(
    [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]
)
_PITCH_CLASSES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


@dataclass
class AudioFeatures:
    path: str
    duration_sec: float
    tempo_bpm: float
    key: str
    mode: str
    key_confidence: float
    brightness: str
    spectral_centroid_hz: float
    loudness_rms_db: float
    dynamics: str
    timbre_descriptors: list[str]


def _estimate_key(chroma: np.ndarray) -> tuple[str, str, float]:
    chroma_mean = chroma.mean(axis=1)
    best_score = -np.inf
    best_tonic = 0
    best_mode = "major"
    for shift in range(12):
        major_score = np.corrcoef(chroma_mean, np.roll(_MAJOR_PROFILE, shift))[0, 1]
        minor_score = np.corrcoef(chroma_mean, np.roll(_MINOR_PROFILE, shift))[0, 1]
        if major_score > best_score:
            best_score, best_tonic, best_mode = major_score, shift, "major"
        if minor_score > best_score:
            best_score, best_tonic, best_mode = minor_score, shift, "minor"
    return _PITCH_CLASSES[best_tonic], best_mode, float(best_score)


def _brightness_label(centroid_hz: float) -> str:
    if centroid_hz < 1200:
        return "dark"
    if centroid_hz < 2500:
        return "warm"
    if centroid_hz < 4500:
        return "bright"
    return "airy"


def _dynamics_label(rms_db: float) -> str:
    if rms_db < -35:
        return "quiet"
    if rms_db < -20:
        return "moderate"
    return "punchy"


def _timbre_descriptors(mfcc: np.ndarray, centroid_hz: float, rms_db: float) -> list[str]:
    descriptors = [_brightness_label(centroid_hz), _dynamics_label(rms_db)]
    # MFCC[1] mean tracks the balance between low- and high-frequency energy;
    # a strong positive value tends to correlate with a "thin"/nasal timbre,
    # a strong negative value with a "full"/round one.
    tilt = float(mfcc[1].mean())
    if tilt > 20:
        descriptors.append("thin")
    elif tilt < -20:
        descriptors.append("full")
    return descriptors


def extract_features(path: str) -> AudioFeatures:
    y, sr = librosa.load(path, sr=None, mono=True)
    duration = librosa.get_duration(y=y, sr=sr)

    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    tempo_bpm = float(np.atleast_1d(tempo)[0])

    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    tonic, mode, confidence = _estimate_key(chroma)

    centroid = librosa.feature.spectral_centroid(y=y, sr=sr)
    centroid_hz = float(centroid.mean())

    rms = librosa.feature.rms(y=y)
    rms_db = float(librosa.amplitude_to_db(rms).mean())

    mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13)

    return AudioFeatures(
        path=path,
        duration_sec=round(duration, 2),
        tempo_bpm=round(tempo_bpm, 1),
        key=tonic,
        mode=mode,
        key_confidence=round(confidence, 3),
        brightness=_brightness_label(centroid_hz),
        spectral_centroid_hz=round(centroid_hz, 1),
        loudness_rms_db=round(rms_db, 1),
        dynamics=_dynamics_label(rms_db),
        timbre_descriptors=_timbre_descriptors(mfcc, centroid_hz, rms_db),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract audio features for Splice matching")
    parser.add_argument("audio_path", help="Path to a local audio file")
    args = parser.parse_args()
    print(json.dumps(asdict(extract_features(args.audio_path)), indent=2))


if __name__ == "__main__":
    main()
