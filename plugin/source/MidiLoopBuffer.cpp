#include "OpenLooper2/MidiLoopBuffer.h"
#include <algorithm>

namespace OpenLooper2 {

void MidiLoopBuffer::recordEvent(const juce::MidiMessage& msg, double ppqRelativeToLoopStart)
{
    events.push_back({ppqRelativeToLoopStart, msg});
    sortEvents();
}

void MidiLoopBuffer::getEventsInRange(double startPpq, double endPpq, juce::MidiBuffer& output,
                                      double blockStartSampleTime, double sampleRate, double bpm,
                                      float velocityScale)
{
    if (events.empty() || loopLengthBeats <= 0.0)
        return;

    const double samplesPerBeat = (sampleRate * 60.0) / bpm;

    for (const auto& event : events)
    {
        bool inRange = false;
        double eventPpq = event.ppqPosition;

        if (startPpq <= endPpq)
            inRange = (eventPpq >= startPpq && eventPpq < endPpq);
        else // wrap around loop boundary
            inRange = (eventPpq >= startPpq || eventPpq < endPpq);

        if (inRange)
        {
            double offsetBeats = eventPpq - startPpq;
            if (offsetBeats < 0.0)
                offsetBeats += loopLengthBeats;

            int sampleOffset = static_cast<int>(offsetBeats * samplesPerBeat);
            auto msg = event.message;

            // Apply velocity scaling to note-on messages
            if (msg.isNoteOn() && velocityScale < 1.0f)
            {
                int vel = static_cast<int>(msg.getVelocity() * velocityScale);
                msg = juce::MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(),
                                                static_cast<juce::uint8>(juce::jmax(1, vel)));
            }

            output.addEvent(msg, sampleOffset);
        }
    }
}

void MidiLoopBuffer::applyVelocityDecay(float feedback)
{
    if (feedback >= 1.0f)
        return;

    events.erase(
        std::remove_if(events.begin(), events.end(),
            [feedback](TimestampedMidiEvent& event) {
                if (event.message.isNoteOn())
                {
                    int vel = static_cast<int>(event.message.getVelocity() * feedback);
                    if (vel < 1)
                        return true; // remove dead note
                    event.message = juce::MidiMessage::noteOn(
                        event.message.getChannel(), event.message.getNoteNumber(),
                        static_cast<juce::uint8>(vel));
                }
                return false;
            }),
        events.end());
}

void MidiLoopBuffer::clear()
{
    events.clear();
    loopLengthBeats = 0.0;
}

void MidiLoopBuffer::sortEvents()
{
    std::sort(events.begin(), events.end(),
              [](const TimestampedMidiEvent& a, const TimestampedMidiEvent& b) {
                  return a.ppqPosition < b.ppqPosition;
              });
}

int MidiLoopBuffer::ppqToSampleOffset(double ppq, double rangeStartPpq, double sampleRate, double bpm) const
{
    double offsetBeats = ppq - rangeStartPpq;
    if (offsetBeats < 0.0)
        offsetBeats += loopLengthBeats;
    return static_cast<int>((offsetBeats * sampleRate * 60.0) / bpm);
}

} // namespace OpenLooper2
