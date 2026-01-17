#include "OpenLooper2/CircularAudioBuffer.h"

namespace OpenLooper2 {

CircularAudioBuffer::CircularAudioBuffer()
{
}

CircularAudioBuffer::~CircularAudioBuffer()
{
}

void CircularAudioBuffer::initialize(int numChannels, int bufferSizeInSamples)
{
    this->numChannels = numChannels;
    this->bufferSize = nextPowerOfTwo(bufferSizeInSamples);
    this->bufferMask = bufferSize - 1;
    
    buffer.setSize(numChannels, bufferSize);
    buffer.clear();
    
    writeHead.store(0, std::memory_order_release);
    initialized.store(true, std::memory_order_release);
}

void CircularAudioBuffer::write(const juce::AudioBuffer<float>& input, int startSample, int numSamples)
{
    if (!initialized.load(std::memory_order_acquire) || numSamples <= 0)
        return;

    const int currentWriteHead = writeHead.load(std::memory_order_acquire);
    
    for (int channel = 0; channel < juce::jmin(numChannels, input.getNumChannels()); ++channel)
    {
        const float* inputData = input.getReadPointer(channel, startSample);
        float* bufferData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < numSamples; ++i)
        {
            const int writeIndex = (currentWriteHead + i) & bufferMask;
            bufferData[writeIndex] = inputData[i];
        }
    }
    
    // Update write head atomically
    const int newWriteHead = (currentWriteHead + numSamples) & bufferMask;
    writeHead.store(newWriteHead, std::memory_order_release);
}

void CircularAudioBuffer::read(juce::AudioBuffer<float>& output, int startSample, int numSamples, int readOffset)
{
    if (!initialized.load(std::memory_order_acquire) || numSamples <= 0)
    {
        output.clear(startSample, numSamples);
        return;
    }

    const int currentWriteHead = writeHead.load(std::memory_order_acquire);
    const int readHead = (currentWriteHead - readOffset) & bufferMask;
    
    for (int channel = 0; channel < juce::jmin(numChannels, output.getNumChannels()); ++channel)
    {
        const float* bufferData = buffer.getReadPointer(channel);
        float* outputData = output.getWritePointer(channel, startSample);
        
        for (int i = 0; i < numSamples; ++i)
        {
            const int readIndex = (readHead + i) & bufferMask;
            outputData[i] = bufferData[readIndex];
        }
    }
}

void CircularAudioBuffer::clear()
{
    if (initialized.load(std::memory_order_acquire))
    {
        buffer.clear();
        writeHead.store(0, std::memory_order_release);
    }
}

int CircularAudioBuffer::nextPowerOfTwo(int value)
{
    if (value <= 0)
        return 1;
    
    int result = 1;
    while (result < value)
        result <<= 1;
    
    return result;
}

} // namespace OpenLooper2