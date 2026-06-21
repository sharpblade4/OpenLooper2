#include "OpenLooper2/LoopBufferManager.h"

namespace OpenLooper2 {

LoopBufferManager::LoopBufferManager()
{
}

LoopBufferManager::~LoopBufferManager()
{
}

void LoopBufferManager::initialize(double sr, int maxCh, float maxLengthSeconds)
{
    this->sampleRate = sr;
    this->maxChannels = maxCh;
    this->maxBufferSize = static_cast<int>(sr * maxLengthSeconds);
    
    circularBuffer.initialize(maxCh, maxBufferSize);
    
    loopLengthSamples.store(0, std::memory_order_release);
    initialized.store(true, std::memory_order_release);
}

void LoopBufferManager::writeAudio(const juce::AudioBuffer<float>& input, int startSample, int numSamples)
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    // For recording, we write sequentially
    circularBuffer.write(input, startSample, numSamples);
}

void LoopBufferManager::readAudio(juce::AudioBuffer<float>& output, int startSample, int numSamples, int positionInSamples)
{
    if (!initialized.load(std::memory_order_acquire))
    {
        output.clear(startSample, numSamples);
        return;
    }
    
    const int currentLoopLength = loopLengthSamples.load(std::memory_order_acquire);
    if (currentLoopLength <= 0)
    {
        output.clear(startSample, numSamples);
        return;
    }
    
    // loopStartPosition is the absolute buffer position where the loop begins
    const int loopStart = loopStartPosition.load(std::memory_order_acquire);
    
    // Read from (loopStart + positionInSamples), wrapping handled by bufferMask inside
    const int absoluteReadPosition = loopStart + positionInSamples;
    circularBuffer.readAtPosition(output, startSample, numSamples, absoluteReadPosition);
}

void LoopBufferManager::writeBackAudio(const juce::AudioBuffer<float>& input, int startSample, int numSamples, int positionInSamples)
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const int currentLoopLength = loopLengthSamples.load(std::memory_order_acquire);
    if (currentLoopLength <= 0)
        return;
    
    const int loopStart = loopStartPosition.load(std::memory_order_acquire);
    const int absoluteWritePosition = loopStart + positionInSamples;
    circularBuffer.writeAtPosition(input, startSample, numSamples, absoluteWritePosition);
}

void LoopBufferManager::setLoopLength(int lengthInSamples)
{
    if (lengthInSamples >= 0 && lengthInSamples <= maxBufferSize)
    {
        loopLengthSamples.store(lengthInSamples, std::memory_order_release);
        
        // Capture where the loop starts in the circular buffer.
        // writeHead is at the end of recording, so loop starts at (writeHead - length).
        // Use unsigned arithmetic to handle wrapping correctly with bufferMask.
        const int currentWriteHead = circularBuffer.getWritePosition();
        const int bufMask = circularBuffer.getBufferSize() - 1;
        const int startPos = (currentWriteHead - lengthInSamples) & bufMask;
        loopStartPosition.store(startPos, std::memory_order_release);
        
        DBG("setLoopLength: len=" << lengthInSamples << " writeHead=" << currentWriteHead << " startPos=" << startPos);
    }
}

float LoopBufferManager::getLoopLengthSeconds() const
{
    const int lengthSamples = loopLengthSamples.load(std::memory_order_acquire);
    return static_cast<float>(lengthSamples / sampleRate);
}

void LoopBufferManager::clear()
{
    if (initialized.load(std::memory_order_acquire))
    {
        circularBuffer.clear();
        loopLengthSamples.store(0, std::memory_order_release);
        loopStartPosition.store(0, std::memory_order_release);
    }
}

} // namespace OpenLooper2