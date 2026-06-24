# OpenLooper2

A JUCE-based audio looper VST3 plugin for Ableton Live. Records, plays back, and overdubs audio loops with support for multi-instrument routing.

## Features

- Single Record/Overdub button with context-sensitive behavior
- Waveform display with playback position indicator
- Monitor passthrough toggle for multi-track routing
- Overdub modifiers: Wait Loop Begin, One Loop Record
- Feedback control for gradual loop decay during overdub
- Volume control for loop playback level
- Input level meter
- MIDI looper mode with piano roll display
- Host transport sync support
- Loop export as WAV file

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

Then restart Ableton Live (or rescan plugins) to reload.

## Usage in Ableton Live

### Audio Looper (single track)

1. Drag the plugin onto an **Audio track**
2. Arm the track, set monitoring to **Auto**
3. Press the main button to Record, press again to loop, press again to Overdub

### Multi-Instrument Routing

Use this to loop audio from multiple instruments (e.g., piano + guitar) onto one looper track:

1. Put instruments on **MIDI tracks** 1, 2, etc.
2. Create an **Audio track** (track 3) and add OpenLooper2
3. On tracks 1 and 2, set **"Audio To"** → Track 3
4. On track 3, set monitoring to **"In"**
5. In the plugin, enable the **Monitor** toggle (top-right)
6. Record with one instrument, switch to another, overdub

With Monitor ON, you always hear the live instruments plus the loop playback.

### MIDI Looper

1. Drag the plugin onto a **MIDI track** (after the instrument)
2. Press Record, play your MIDI controller, press Record again to loop
3. The plugin shows "MIDI" in the top-left when in MIDI mode
4. Incoming MIDI always passes through to the instrument
5. Use Overdub to layer more notes (pure additive)
6. **Feedback** controls velocity decay per loop cycle
7. **Volume** scales playback note velocity

## Controls

### Main Button (Record / Overdub)

Context-sensitive — label changes based on state:

| Current State | Button Label | Action |
|---|---|---|
| Stopped | Record | Start recording (clears previous loop) |
| Recording | Recording... | Stop recording, begin playback |
| Playing | Overdub | Start overdubbing |
| Overdubbing | Overdubbing... | Stop overdub, continue playing |

### Overdub Modifier Checkboxes

- **Wait Loop Begin** — overdub starts at the next loop boundary instead of immediately
- **One Loop Record** — overdub automatically stops after one full loop cycle

These compose: both checked means "wait for loop start, then record exactly one loop."

### Secondary Controls

- **Play** — start loop playback (if a loop exists)
- **Stop** — stop playback/recording
- **Export** — save loop as 24-bit WAV file

### Parameters

- **Feedback** — how much existing content is preserved per overdub pass (1.0 = full, 0.0 = replace)
- **Volume** — loop playback output level

### Toggles

- **Monitor** — pass input audio through to output (OFF by default to prevent feedback with mic monitoring; turn ON for multi-track routing)
- **DBG** — toggle debug logging to system console
