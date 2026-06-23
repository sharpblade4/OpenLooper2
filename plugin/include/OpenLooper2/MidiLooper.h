#pragma once

#include "MidiLoopBuffer.h"
#include "ActiveNoteTracker.h"
#include "TransportController.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace OpenLooper2 {

/**
 * Orchestrates MIDI looping: record, playback, overdub with pass-through.
 * Incoming MIDI always passes through; playback events are merged on top.
 */
class MidiLooper
{
public:
    void initialize(double sampleRate, int samplesPerBlock);

    /**
     * Process a MIDI block. Incoming messages pass through unchanged.
     * Playback/overdub events are merged into the same buffer.
     */
    void processBlock(juce::MidiBuffer& midiMessages,
                     TransportController& transport,
                     const juce::AudioPlayHead::PositionInfo& positionInfo,
                     float velocityScale, float feedback);

    void stop(juce::MidiBuffer& output, int samplePosition);
    void clear(juce::MidiBuffer& output, int samplePosition);
    void arm() { armed = true; }

    bool isArmed() const { return armed; }
    bool isHostTransportRunning() const { return hostPlaying; }

    const MidiLoopBuffer& getLoopBuffer() const { return loopBuffer; }
    MidiLoopBuffer& getLoopBufferMut() { return loopBuffer; }
    const ActiveNoteTracker& getNoteTracker() const { return noteTracker; }
    double getLoopStartPpq() const { return loopStartPpq; }

private:
    MidiLoopBuffer loopBuffer;
    ActiveNoteTracker noteTracker;

    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    bool armed = false;
    bool hostPlaying = false;
    double loopStartPpq = 0.0;
    double lastPpq = 0.0;
    double currentBpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    double snapToBar(double ppq) const;
    double getPositionInLoop(double ppq) const;
};

} // namespace OpenLooper2
