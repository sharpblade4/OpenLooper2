#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace OpenLooper2 {

struct TimestampedMidiEvent {
    double ppqPosition;        // beats relative to loop start
    juce::MidiMessage message;
};

/**
 * Stores MIDI events positioned in beats (PPQ).
 * Supports record, additive overdub, playback with velocity scaling and decay.
 */
class MidiLoopBuffer
{
public:
    void recordEvent(const juce::MidiMessage& msg, double ppqRelativeToLoopStart);
    void getEventsInRange(double startPpq, double endPpq, juce::MidiBuffer& output,
                         double blockStartSampleTime, double sampleRate, double bpm,
                         float velocityScale);
    void applyVelocityDecay(float feedback);
    void clear();
    bool isEmpty() const { return events.empty(); }
    int getEventCount() const { return static_cast<int>(events.size()); }

    void setLoopLengthBeats(double lengthInBeats) { loopLengthBeats = lengthInBeats; }
    double getLoopLengthBeats() const { return loopLengthBeats; }

    const std::vector<TimestampedMidiEvent>& getEvents() const { return events; }

private:
    std::vector<TimestampedMidiEvent> events; // sorted by ppqPosition
    double loopLengthBeats = 0.0;

    void sortEvents();
    int ppqToSampleOffset(double ppq, double rangeStartPpq, double sampleRate, double bpm) const;
};

} // namespace OpenLooper2
