# MIDI Looper Plan

## Goal
Support MIDI recording/playback when the plugin is on a MIDI track, while keeping existing audio looper behavior on audio tracks.

## Changes Required

### 1. Plugin Configuration
- Set `NEEDS_MIDI_INPUT TRUE` and `NEEDS_MIDI_OUTPUT TRUE` in CMakeLists.txt

### 2. MidiLoopBuffer (new component)
- Stores timestamped MIDI events: `{ int tickPosition, juce::MidiMessage message }`
- Record: capture incoming events with position relative to loop start
- Playback: emit events whose tick falls within the current block's time window
- Overdub: append new events to existing list
- Clear: remove all events
- Needs sorting by timestamp for efficient playback

### 3. Mode Detection
- Check `getTotalNumInputChannels() == 0` → MIDI mode (MIDI-only tracks in Ableton have no audio buses)
- Or check if audio buffer has no channels vs midiMessages being routed
- Decision made once in `prepareToPlay`, stored as a bool

### 4. MIDI processBlock Logic
```cpp
if (midiMode)
{
    // Recording: store incoming midiMessages with timestamps
    // Playing: inject stored events into midiMessages output
    // Overdub: store new + replay existing
    buffer.clear(); // no audio output in MIDI mode
}
else
{
    // existing audio looper logic
}
```

### 5. Loop Length / Beat Quantization
- MIDI loops should be beat-quantized (e.g., 1/2/4/8 bars)
- Use host BPM and PPQ position to define loop boundaries
- Snap loop start/end to nearest bar boundary
- Existing `TransportController` host sync code can be extended for this

## Effort Estimate

| Component | Effort |
|-----------|--------|
| CMake flags + bus config | 10 min |
| MidiLoopBuffer class | 2-3 hours |
| MIDI record/play/overdub in processBlock | 2-3 hours |
| Mode detection + routing | 30 min |
| Beat-quantized loop length | 1-2 hours |
| Testing | 2 hours |

**Total: ~1-2 days**

## What Stays the Same
- State machine (Record/Play/Stop/Overdub transitions)
- UI (buttons, waveform display could show MIDI note density)
- TransportController (position tracking, state management)
- ParameterManager (volume becomes velocity scaling, feedback = note decay)
- OverdubEngine concept (merge new events with existing)
