#include "OpenLooper2/LoopBufferManager.h"

namespace OpenLooper2 {

LoopBufferManager::LoopBufferManager()
{
}

LoopBufferManager::~LoopBufferManager()
{
}

void LoopBufferManager::initialize(double sampleRate, int maxChannels, float maxLengthSeconds)
{
    this->sampleRate = sampleRate;
    this->maxChannels = maxChannels;
    this->maxBufferSize = static_cast<int>(sampleRate * maxLengthSeconds);
    
    circularBuffer.initialize(maxChannels, maxBufferSize);
    
    loopLengthSamples.store(0, std::memory_order_release);
    initialized.store(true, std::memory_order_release);
}

void LoopBufferManager::writeAudio(const juce::AudioBuffer<float>& input, int startSample, int numSamples)
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    circularBuffer.write(input, startSample, numSamples);
}

void LoopBufferManager::readAudio(juce::AudioBuffer<float>& output, int startSample, int numSamples, float position)
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
    
    // Calculate read offset based on position within the loop
    const int readOffset = static_cast<int>(position * currentLoopLength);
    circularBuffer.read(output, startSample, numSamples, readOffset);
}

void LoopBufferManager::setLoopLength(int lengthInSamples)
{
    if (lengthInSamples >= 0 && lengthInSamples <= maxBufferSize)
    {
        loopLengthSamples.store(lengthInSamples, std::memory_order_release);
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
    }
}

} // namespace OpenLooper2