# Building the plugin

CMake + JUCE. JUCE itself is not vendored in this repo -- CMake fetches it
automatically (via `FetchContent`, pinned to tag `8.0.4`) the first time you
configure the build, into `build/_deps/`. `build/` is gitignored: it's ~450MB
of fetched source and compiled objects, none of which belongs in version
control.

## Prerequisites

- CMake >= 3.22
- A C++17 compiler (Xcode command line tools on macOS)
- Git (JUCE is fetched from GitHub over `https://`)

## Build

From `plugin/`:

```
cmake -B build -S .
cmake --build build
```

The first configure step clones JUCE into `build/_deps/juce-src` -- expect
it to take a few minutes and ~300MB of disk on a clean checkout.

## Output

`COPY_PLUGIN_AFTER_BUILD` is on, so a successful build also installs the
plugin to your user plugin folders automatically. The built artifacts also
land here:

```
build/Clauducer_artefacts/VST3/Clauducer.vst3
build/Clauducer_artefacts/AU/Clauducer.component
```

## Using it

The plugin talks to the local backend over HTTP -- start that first
(`uvicorn service:app --host 127.0.0.1 --port 8787` from `backend/`, see
`backend/README.md`), then load `Clauducer.vst3`/`.component` as an
audio-effect insert in your DAW.

## Clean rebuild

```
rm -rf build
cmake -B build -S .
cmake --build build
```
