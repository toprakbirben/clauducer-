// Node for Max script for the M4L device (Milestones 4-5).
// Listens for "match" and "download" messages from the Max patch, POSTs to
// the local backend service (backend/service.py, must be running on
// 127.0.0.1:8787), and outputs parsed responses back into the patch.
//
// Wiring in Max: a [node.script match-client.js] object.
//   - Send "match <audio_path> <prompt text...>" to search (~20-30s, since
//     it's a real headless Claude + Splice call).
//   - Send "download <asset_uuid> <name>" to download one chosen result.
//     NOTE: this spends a Splice purchase credit the first time a given
//     asset is downloaded -- only wire this to an explicit per-result
//     "Download" button, never to something that fires automatically.

const Max = require("max-api");
const http = require("http");

const MIN_PLAUSIBLE_BPM = 20; // mirrors backend/match.py's _MIN_PLAUSIBLE_BPM

let lastResults = [];

function formatFeatures(features) {
  const parts = [];
  if (features.duration_sec != null) {
    parts.push(`${features.duration_sec.toFixed(1)}s`);
  }
  if (features.tempo_bpm != null && features.tempo_bpm >= MIN_PLAUSIBLE_BPM) {
    parts.push(`${Math.round(features.tempo_bpm)} BPM`);
  } else {
    parts.push("no fixed tempo");
  }
  if (features.key && features.mode) {
    const conf =
      features.key_confidence != null
        ? ` (${Math.round(features.key_confidence * 100)}% conf.)`
        : "";
    parts.push(`${features.key} ${features.mode}${conf}`);
  }
  const line1 = parts.join(" · ");

  const descriptors =
    Array.isArray(features.timbre_descriptors) && features.timbre_descriptors.length
      ? features.timbre_descriptors.join(", ")
      : null;
  const loudness =
    features.loudness_rms_db != null ? `${features.loudness_rms_db.toFixed(0)}dB RMS` : null;
  const line2 = [descriptors, loudness].filter(Boolean).join(" · ");

  return line2 ? `${line1}\n${line2}` : line1;
}

function formatWanted(query) {
  if (!query || !query.query) return "";
  let text = query.query;
  if (query.bpm_min != null && query.bpm_max != null) {
    text += ` (${query.bpm_min}-${query.bpm_max} BPM)`;
  }
  return text;
}

function formatResults(results) {
  if (!Array.isArray(results) || results.length === 0) return "no results";
  return results
    .map((r, i) => {
      const bits = [];
      if (r.bpm != null) bits.push(`${r.bpm} BPM`);
      if (r.key) bits.push(r.key);
      const suffix = bits.length ? ` — ${bits.join(", ")}` : "";
      return `${i + 1}. ${r.name}${suffix}`;
    })
    .join("\n");
}

function postJson(path, payload, timeoutMs, onResult) {
  const body = JSON.stringify(payload);
  const req = http.request(
    {
      hostname: "127.0.0.1",
      port: 8787,
      path,
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Content-Length": Buffer.byteLength(body),
      },
      timeout: timeoutMs,
    },
    (res) => {
      let responseBody = "";
      res.on("data", (chunk) => (responseBody += chunk));
      res.on("end", () => {
        if (res.statusCode !== 200) {
          Max.post(`match-client: backend returned ${res.statusCode}: ${responseBody}`);
          Max.outlet("error", responseBody);
          return;
        }
        try {
          onResult(JSON.parse(responseBody));
        } catch (err) {
          Max.post(`match-client: failed to parse response: ${err}`);
          Max.outlet("error", String(err));
        }
      });
    }
  );

  req.on("timeout", () => req.destroy(new Error("request timed out")));
  req.on("error", (err) => {
    Max.post(`match-client: request failed: ${err.message}`);
    Max.outlet("error", err.message);
  });

  req.write(body);
  req.end();
}

function toPosixPath(path) {
  // Max's [opendialog] can hand back a legacy Mac volume-prefixed path
  // (e.g. "Macintosh HD:/Users/you/file.wav") instead of a plain POSIX
  // path -- strip a leading "VolumeName:" if present, since Python/Node
  // file APIs only understand the POSIX form. Already-POSIX paths (no
  // colon before the first slash) pass through unchanged.
  return path.replace(/^[^/]+:(?=\/)/, "");
}

Max.addHandler("match", (audioPath, ...promptWords) => {
  if (!audioPath) {
    Max.post("match-client: missing audio_path");
    return;
  }
  audioPath = toPosixPath(audioPath);
  const prompt = promptWords.join(" ");
  postJson("/search", { audio_path: audioPath, prompt }, 120000, (parsed) => {
    lastResults = Array.isArray(parsed.results) ? parsed.results : [];
    Max.outlet("analyzed", formatFeatures(parsed.features));
    Max.outlet("wanted", formatWanted(parsed.query));
    Max.outlet("results_text", formatResults(lastResults));
  });
});

Max.addHandler("select_result", (index) => {
  const i = Number(index);
  const result = lastResults[i];
  if (!result) {
    Max.post(`match-client: no result at index ${index}`);
    return;
  }
  Max.outlet("selected_uuid", result.asset_uuid);
  Max.outlet("selected_name", result.name);
});

Max.addHandler("download", (assetUuid, ...nameWords) => {
  if (!assetUuid) {
    Max.post("match-client: missing asset_uuid");
    return;
  }
  const name = nameWords.join(" ");
  postJson("/download", { asset_uuid: assetUuid, name }, 30000, (parsed) => {
    Max.outlet("downloaded", parsed.local_path);
  });
});

Max.post("match-client.js loaded");
