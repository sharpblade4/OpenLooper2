#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <bitset>

namespace OpenLooper2 {

/**
 * Tracks currently active (sounding) MIDI notes to enable clean note-off
 * on stop, clear, or loop boundaries. Prevents stuck notes.
 */
class ActiveNoteTracker
{
public:
    void noteOn(int channel, int noteNumber);
    void noteOff(int channel, int noteNumber);
    void sendAllNotesOff(juce::MidiBuffer& output, int samplePosition);
    void clear();
    bool hasActiveNotes() const;

private:
    // 16 channels × 128 notes = 2048 bits
    std::bitset<2048> activeNotes;

    static int index(int channel, int noteNumber) { return channel * 128 + noteNumber; }
};

} // namespace OpenLooper2
