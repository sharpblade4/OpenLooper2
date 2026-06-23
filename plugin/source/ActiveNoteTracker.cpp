#include "OpenLooper2/ActiveNoteTracker.h"

namespace OpenLooper2 {

void ActiveNoteTracker::noteOn(int channel, int noteNumber)
{
    activeNotes.set(index(channel, noteNumber));
}

void ActiveNoteTracker::noteOff(int channel, int noteNumber)
{
    activeNotes.reset(index(channel, noteNumber));
}

void ActiveNoteTracker::sendAllNotesOff(juce::MidiBuffer& output, int samplePosition)
{
    for (int ch = 0; ch < 16; ++ch)
        for (int note = 0; note < 128; ++note)
            if (activeNotes.test(index(ch, note)))
                output.addEvent(juce::MidiMessage::noteOff(ch + 1, note), samplePosition);

    activeNotes.reset();
}

void ActiveNoteTracker::clear()
{
    activeNotes.reset();
}

bool ActiveNoteTracker::hasActiveNotes() const
{
    return activeNotes.any();
}

} // namespace OpenLooper2
