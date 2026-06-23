#include "OpenLooper2/MidiLooper.h"
#include <cmath>

namespace OpenLooper2 {

void MidiLooper::initialize(double newSampleRate, int newSamplesPerBlock)
{
    sampleRate = newSampleRate;
    samplesPerBlock = newSamplesPerBlock;
}

void MidiLooper::processBlock(juce::MidiBuffer& midiMessages,
                              TransportController& transport,
                              const juce::AudioPlayHead::PositionInfo& positionInfo,
                              float velocityScale, float feedback)
{
    // Extract host transport info
    hostPlaying = positionInfo.getIsPlaying();
    currentBpm = positionInfo.getBpm().orFallback(120.0);

    if (auto ts = positionInfo.getTimeSignature())
    {
        timeSigNumerator = ts->numerator;
        timeSigDenominator = ts->denominator;
    }

    const double ppq = positionInfo.getPpqPosition().orFallback(0.0);
    const auto state = transport.getCurrentState();

    // If armed and transport just started, begin recording
    if (armed && hostPlaying)
    {
        armed = false;
        loopStartPpq = snapToBar(ppq);
        transport.startRecording();
        // Re-read state after transition
        return processBlock(midiMessages, transport, positionInfo, velocityScale, feedback);
    }

    // If we need transport but it's not running, just pass through
    if (!hostPlaying && state != TransportController::State::Stopped)
    {
        // Transport stopped mid-loop — stop cleanly
        stop(midiMessages, 0);
        transport.stopPlayback();
        lastPpq = ppq;
        return;
    }

    const double beatsPerBlock = (static_cast<double>(samplesPerBlock) * currentBpm) / (sampleRate * 60.0);

    switch (state)
    {
        case TransportController::State::Stopped:
            // Just pass through — nothing to do
            break;

        case TransportController::State::Recording:
        {
            // Store incoming note events with PPQ relative to loop start
            for (const auto metadata : midiMessages)
            {
                auto msg = metadata.getMessage();
                double eventPpq = ppq + (static_cast<double>(metadata.samplePosition) * currentBpm) / (sampleRate * 60.0);
                double relPpq = eventPpq - loopStartPpq;
                if (relPpq >= 0.0)
                    loopBuffer.recordEvent(msg, relPpq);
            }
            break;
        }

        case TransportController::State::Playing:
        {
            double loopLen = loopBuffer.getLoopLengthBeats();
            if (loopLen > 0.0)
            {
                double startInLoop = getPositionInLoop(ppq);
                double endInLoop = startInLoop + beatsPerBlock;

                // Handle wrap
                if (endInLoop > loopLen)
                    endInLoop -= loopLen;

                // Apply velocity decay once per loop cycle
                double prevInLoop = getPositionInLoop(lastPpq);
                if (prevInLoop > startInLoop && feedback < 1.0f)
                    loopBuffer.applyVelocityDecay(feedback);

                loopBuffer.getEventsInRange(startInLoop, endInLoop, midiMessages,
                                           0.0, sampleRate, currentBpm, velocityScale);

                // Track active notes from playback
                for (const auto metadata : midiMessages)
                {
                    auto msg = metadata.getMessage();
                    if (msg.isNoteOn())
                        noteTracker.noteOn(msg.getChannel() - 1, msg.getNoteNumber());
                    else if (msg.isNoteOff())
                        noteTracker.noteOff(msg.getChannel() - 1, msg.getNoteNumber());
                }
            }
            break;
        }

        case TransportController::State::Overdubbing:
        {
            double loopLen = loopBuffer.getLoopLengthBeats();
            if (loopLen > 0.0)
            {
                double startInLoop = getPositionInLoop(ppq);
                double endInLoop = startInLoop + beatsPerBlock;
                if (endInLoop > loopLen)
                    endInLoop -= loopLen;

                // Apply velocity decay at loop boundary
                double prevInLoop = getPositionInLoop(lastPpq);
                if (prevInLoop > startInLoop && feedback < 1.0f)
                    loopBuffer.applyVelocityDecay(feedback);

                // Record incoming events (additive)
                for (const auto metadata : midiMessages)
                {
                    auto msg = metadata.getMessage();
                    double eventPpq = ppq + (static_cast<double>(metadata.samplePosition) * currentBpm) / (sampleRate * 60.0);
                    double relPpq = std::fmod(eventPpq - loopStartPpq, loopLen);
                    if (relPpq < 0.0) relPpq += loopLen;
                    loopBuffer.recordEvent(msg, relPpq);
                }

                // Playback existing events
                loopBuffer.getEventsInRange(startInLoop, endInLoop, midiMessages,
                                           0.0, sampleRate, currentBpm, velocityScale);

                // Track active notes
                for (const auto metadata : midiMessages)
                {
                    auto msg = metadata.getMessage();
                    if (msg.isNoteOn())
                        noteTracker.noteOn(msg.getChannel() - 1, msg.getNoteNumber());
                    else if (msg.isNoteOff())
                        noteTracker.noteOff(msg.getChannel() - 1, msg.getNoteNumber());
                }
            }
            break;
        }
    }

    lastPpq = ppq;
}

void MidiLooper::stop(juce::MidiBuffer& output, int samplePosition)
{
    noteTracker.sendAllNotesOff(output, samplePosition);
    armed = false;
}

void MidiLooper::clear(juce::MidiBuffer& output, int samplePosition)
{
    noteTracker.sendAllNotesOff(output, samplePosition);
    loopBuffer.clear();
    armed = false;
}

double MidiLooper::snapToBar(double ppq) const
{
    double beatsPerBar = static_cast<double>(timeSigNumerator) * (4.0 / timeSigDenominator);
    return std::floor(ppq / beatsPerBar) * beatsPerBar;
}

double MidiLooper::getPositionInLoop(double ppq) const
{
    double loopLen = loopBuffer.getLoopLengthBeats();
    if (loopLen <= 0.0) return 0.0;
    double pos = std::fmod(ppq - loopStartPpq, loopLen);
    if (pos < 0.0) pos += loopLen;
    return pos;
}

} // namespace OpenLooper2
