# Changelog

## 2026-06-22 — Add MIDI looper support

### Feature
The plugin now supports MIDI looping when loaded on a MIDI track. Audio looper behavior is unchanged on audio tracks.

### MIDI Mode Behavior
- **Auto-detected**: MIDI mode activates when the plugin has no audio input channels (i.e., placed on a MIDI track)
- **Pass-through always**: incoming MIDI flows to the instrument downstream regardless of looper state
- **Beat-based timing**: events stored in PPQ (beats), immune to tempo changes
- **Bar-quantized loops**: loop boundaries snap to bar lines
- **Armed state**: if Record is pressed without host transport running, the looper arms and waits
- **Velocity scaling**: volume parameter scales playback velocity
- **Velocity decay**: feedback parameter decays note velocities each loop cycle (notes fade out over time)
- **All-notes-off on stop**: prevents stuck notes on any state transition to Stopped
- **Pure additive overdub**: new notes layer on top, never replace existing events
- **Piano-roll UI**: waveform area shows MIDI notes with velocity-based brightness

### New Files
- `ActiveNoteTracker.h/.cpp` — bitset-based note tracking, sends note-off for all active notes
- `MidiLoopBuffer.h/.cpp` — PPQ-based event storage with sorted recording and range playback
- `MidiLooper.h/.cpp` — orchestrates MIDI record/play/overdub with pass-through

### Modified Files
- `plugin/CMakeLists.txt` — NEEDS_MIDI_INPUT/OUTPUT TRUE, new source files
- `Looper.h/.cpp` — midiMode flag, MidiLooper integration, processBlock branching
- `PluginProcessor.cpp` — passes MidiBuffer, detects MIDI mode in prepareToPlay
- `PluginEditor.h/.cpp` — MIDI indicator, armed state, piano-roll visualization
- `tests/CMakeLists.txt` — new MIDI sources linked to LooperTest
- `tests/LooperTest.cpp` — updated processBlock signature

## 2026-06-21 — Fix audio monitoring feedback issues in Ableton Live

### Problem
When adding the plugin to an Ableton Live channel with the track armed:
- Monitor OFF: Plugin's input meter showed no signal (Ableton doesn't route audio through the plugin chain unless transport is running)
- Monitor IN/Auto: Input meter worked but caused audio feedback loops

### Root Cause
1. **Stopped state** passed audio through unchanged (`buffer` left untouched), so live input was output directly — creating a feedback loop when Ableton's monitoring was set to Auto/In.
2. **Overdubbing state** output the mixed result (loop + live input) via `buffer.makeCopyOf(loopBuffer)`. With Auto monitoring, this live input in the output fed back through the mic.

### Fix
- **Stopped state**: Clear the output buffer instead of passing through. A looper has nothing to output when stopped.
- **Overdubbing state**: Output only the pre-existing loop content (re-read from buffer manager), not the mixed result containing live input. The overdub still writes the mix back to the loop buffer, so new input is captured — it's just not echoed back immediately.

### Usage
Set Ableton's track monitoring to **Auto**. The plugin now works without feedback in all states:
- Stopped: output silent, input meter active
- Recording: captures input, output silent
- Playing: outputs loop content only
- Overdubbing: captures input mixed into loop, outputs only existing loop content

### Files Modified
- `plugin/source/Looper.cpp` — `processAudioForCurrentState()` Stopped and Overdubbing cases
