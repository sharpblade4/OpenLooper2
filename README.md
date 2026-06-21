# OpenLooper2

A JUCE-based audio looper VST3 plugin for Ableton Live. Records, plays back, and overdubs audio loops.

## Features

- Record/Play/Stop/Overdub with single-button state machine
- Waveform display of recorded loop content
- Feedback control for gradual loop decay during overdub
- Volume control for playback level
- Input level meter
- Host transport sync support

## Build

Requires CMake 3.22+ and a C++17 compiler.

```bash
cmake -S . -B build
cmake --build build
```

### Run unit tests

```bash
cmake --build build --target LoopBufferTest
cmake --build build --target LooperTest
```

## Deploy to Ableton Live

```bash
rm -rf ~/Documents/VST_For_Ableton/OpenLooper2.vst3 && cp -r build/plugin/OpenLooper2Plugin_artefacts/Debug/VST3/OpenLooper2.vst3 ~/Documents/VST_For_Ableton/
```

Then restart Ableton Live to reload the plugin.

## Usage in Ableton Live

1. Drag the plugin onto an **Audio track**
2. **Arm** the track
3. Set track monitoring to **Auto**
4. Press Ableton's **Play** (transport must be running for audio to flow through the plugin)
5. Press **Record** in the plugin UI, perform, then press **Record** again (or Stop/Play) to finish

### Controls

- **Record** — start/stop recording (second press transitions to playback)
- **Play** — play back the recorded loop
- **Stop** — stop playback (also finalizes recording if pressed during record)
- **Overdub** — layer new audio onto the existing loop
- **Feedback** — how much existing content is preserved per overdub pass (1.0 = full, 0.0 = replace)
- **Volume** — loop playback output level
- **DBG** — toggle debug logging to system console
