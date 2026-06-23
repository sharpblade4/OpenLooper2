# MIDI Looper Plan

## Goal
Support MIDI recording/playback when the plugin is on a MIDI track, while keeping existing audio looper behavior on audio tracks.

## Design Decisions

- **Single plugin** — no split into separate audio/MIDI plugins. One binary handles both modes.
- **Pure additive overdub** — overdub always layers new notes on top; never replaces existing events.
- **Beat-based timing** — MIDI events stored in PPQ (beats), not samples or ticks. Immune to tempo changes.
- **MIDI pass-through always** — incoming MIDI always passes through to the instrument downstream, regardless of looper state. You can always play and hear your instrument.
- **Host transport required for record/play** — recording and playback only activate when the DAW transport is running (provides PPQ and BPM). If Record is triggered without transport, the looper arms and waits — UI shows "armed, waiting for transport." Pass-through is unaffected.
- **All-notes-off on stop** — on any state transition to Stopped (or Clear), send note-off for every currently active note to prevent stuck notes.

## Changes Required

### 1. Plugin Configuration (CMakeLists.txt)
- Set `NEEDS_MIDI_INPUT TRUE` and `NEEDS_MIDI_OUTPUT TRUE`
- Keep `IS_MIDI_EFFECT FALSE` (preserves audio bus for audio looper mode)
- Keep `IS_SYNTH FALSE`

### 2. Mode Detection
- In `prepareToPlay`: check `getTotalNumInputChannels() == 0` → set `midiMode = true`
- This is called by the host whenever bus layout changes, so it stays current
- Document assumption: if audio channels exist, it's audio mode regardless of MIDI routing

### 3. MidiLoopBuffer (new class, separate testable component)

Stores MIDI events positioned in **beats (PPQ)**:

```cpp
struct TimestampedMidiEvent {
    double ppqPosition;          // position in beats relative to loop start
    juce::MidiMessage message;
};
```

Interface:
```cpp
class MidiLoopBuffer {
public:
    void recordEvent(const juce::MidiMessage& msg, double ppqRelativeToLoopStart);
    void overdubEvent(const juce::MidiMessage& msg, double ppqRelativeToLoopStart); // same as record (additive)
    void getEventsInRange(double startPpq, double endPpq, juce::MidiBuffer& output, double blockStartTime, double sampleRate, double bpm);
    void clear();
    bool isEmpty() const;
    int getEventCount() const;
    
    void setLoopLengthBeats(double lengthInBeats);
    double getLoopLengthBeats() const;

private:
    std::vector<TimestampedMidiEvent> events; // sorted by ppqPosition
    double loopLengthBeats = 0.0;
};
```

Key behaviors:
- Events sorted by ppqPosition for efficient range queries
- `getEventsInRange` converts PPQ positions to sample offsets within the current audio block using BPM
- Loop wraps: if endPpq > loopLength, query wraps around to the beginning

### 4. ActiveNoteTracker (new class)

Tracks which notes are currently sounding to enable clean stop:

```cpp
class ActiveNoteTracker {
public:
    void noteOn(int channel, int noteNumber);
    void noteOff(int channel, int noteNumber);
    void sendAllNotesOff(juce::MidiBuffer& output, int samplePosition);
    void clear();
    bool hasActiveNotes() const;

private:
    // Bitfield: 16 channels × 128 notes
    std::bitset<2048> activeNotes;
};
```

Used in:
- **Playback**: track notes being emitted
- **Stop/Clear**: call `sendAllNotesOff()` to kill stuck notes
- **Loop boundary**: send note-off for notes that started in previous cycle but shouldn't ring into next cycle (unless held across boundary intentionally)

### 5. MidiLooper (new class, orchestrates MIDI processing)

```cpp
class MidiLooper {
public:
    void initialize(double sampleRate, int samplesPerBlock);
    void processBlock(juce::MidiBuffer& midiMessages, 
                     const TransportController& transport,
                     const juce::AudioPlayHead::PositionInfo& positionInfo);
    
    void startRecording(double currentPpq, double bpm, int timeSigNumerator, int timeSigDenominator);
    void stopRecording(double currentPpq);
    void startOverdub();
    void stopOverdub();
    void stop(juce::MidiBuffer& output, int samplePosition);
    void clear(juce::MidiBuffer& output, int samplePosition);
    
    bool isHostTransportRunning() const;
    bool needsHostTransport() const { return true; }

private:
    MidiLoopBuffer loopBuffer;
    ActiveNoteTracker activeNoteTracker;
    double loopStartPpq = 0.0;
    double sampleRate = 44100.0;
    bool hostTransportRunning = false;
};
```

### 6. Loop Length / Beat Quantization
- When recording starts: snap `loopStartPpq` to the nearest bar boundary (using time signature from host)
- When recording stops: snap loop end to nearest bar boundary
- Loop length stored in beats (e.g., 4.0 = one bar in 4/4)
- Minimum loop length: 1 beat; maximum: 64 bars

### 7. Integration in processBlock

```cpp
void Looper::processBlock(juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midiMessages,  // NEW parameter
                          const juce::AudioProcessorValueTreeState& apvts)
{
    if (midiMode) {
        buffer.clear(); // no audio in MIDI mode
        // midiMessages already contains incoming MIDI — it always passes through.
        // MidiLooper ADDS playback events to the same buffer (merge, not replace).
        midiLooper.processBlock(midiMessages, transportController, lastPositionInfo);
    } else {
        midiMessages.clear(); // no MIDI in audio mode
        processAudioForCurrentState(buffer);
    }
}
```

**MIDI pass-through behavior by state:**
| State | Incoming MIDI | Loop playback | Stored |
|-------|--------------|---------------|--------|
| Stopped | passes through | — | — |
| Armed (waiting for transport) | passes through | — | — |
| Recording | passes through | — | ✓ stored in buffer |
| Playing | passes through | merged into output | — |
| Overdubbing | passes through | merged into output | ✓ stored in buffer |

### 8. Note-Off Strategy (detailed)

| Event | Action |
|-------|--------|
| Stop pressed | `activeNoteTracker.sendAllNotesOff(output)` |
| Clear pressed | `activeNoteTracker.sendAllNotesOff(output)` then `loopBuffer.clear()` |
| Loop boundary (playback) | Send note-off for notes whose duration doesn't cross the boundary |
| Recording stops mid-note | Insert a note-off at loop end for any notes still held |
| Plugin bypassed/removed | `sendAllNotesOff` in `releaseResources()` |

### 9. What Changes in Existing Code

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add MIDI flags, add new source files |
| `PluginProcessor.cpp` | Pass `midiMessages` to looper, add mode detection in `prepareToPlay` |
| `Looper.h/.cpp` | Add `midiMode` flag, `MidiLooper` member, branching in `processBlock` |
| `PluginEditor.cpp` | Show "MIDI mode" indicator, "armed — start transport" status when waiting |

### 10. What Stays the Same
- State machine (Record/Play/Stop/Overdub transitions)
- UI buttons (same controls for both modes)
- TransportController (position tracking, state management)
- ParameterManager (volume param becomes velocity scaling in MIDI mode, feedback = note decay/probability)

## Effort Estimate

| Component | Effort |
|-----------|--------|
| CMake flags + bus config | 15 min |
| MidiLoopBuffer class + tests | 2-3 hours |
| ActiveNoteTracker class + tests | 1 hour |
| MidiLooper class (orchestration) | 2-3 hours |
| Mode detection + processBlock routing | 30 min |
| Beat-quantized loop boundaries | 1-2 hours |
| Note-off edge cases | 1-2 hours |
| UI updates (mode indicator, transport status) | 1 hour |
| Integration testing in Ableton | 1-2 hours |

**Total: ~2-3 days**

## Testing Strategy

- **MidiLoopBuffer**: unit test recording/playback/overdub with known PPQ sequences
- **ActiveNoteTracker**: unit test note tracking and all-notes-off generation
- **MidiLooper**: integration test full record→play→overdub→stop cycle
- **Edge cases**: loop boundary wrapping, tempo changes mid-loop, stop-while-recording with held notes
- **Manual**: load in Ableton on MIDI track, verify with a virtual instrument downstream

## Resolved Design Decisions (parameters & UI)

1. **Velocity scaling (volume param)** — scales velocity of playback output. Incoming pass-through and recording are unaffected.
2. **Feedback = velocity decay** — each loop cycle, note velocities are multiplied by the feedback value (0.0–1.0). Notes whose velocity drops below a threshold (e.g., 1) are removed from the buffer. At feedback=1.0, notes loop forever.
3. **Visualization** — piano-roll style display showing note positions and durations within the loop, with a playback cursor.
