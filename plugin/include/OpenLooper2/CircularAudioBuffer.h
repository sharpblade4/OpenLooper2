#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace OpenLooper2 {

/**
 * Lock-free circular audio buffer for real-time audio processing.
 * Uses atomic operations to ensure thread safety without blocking.
 */
class CircularAudioBuffer
{
public:
    CircularAudioBuffer();
    ~CircularAudioBuffer();

    /**
     * Initialize the buffer with specified parameters.
     * Buffer size will be rounded up to the next power of 2 for efficiency.
     */
    void initialize(int numChannels, int bufferSizeInSamples);

    /**
     * Write audio data to the buffer.
     * This is lock-free and safe to call from the audio thread.
     */
    void write(const juce::AudioBuffer<float>& input, int startSample, int numSamples);

    /**
     * Read audio data from the buffer at a specific offset.
     * This is lock-free and safe to call from the audio thread.
     */
    void read(juce::AudioBuffer<float>& output, int startSample, int numSamples, int readOffset = 0);

    /**
     * Clear the buffer contents.
     */
    void clear();

    /**
     * Get the current write position.
     */
    int getWritePosition() const { return writeHead.load(std::memory_order_acquire); }

    /**
     * Get the buffer size (always a power of 2).
     */
    int getBufferSize() const { return bufferSize; }

    /**
     * Check if the buffer is initialized.
     */
    bool isInitialized() const { return initialized.load(std::memory_order_acquire); }

private:
    juce::AudioBuffer<float> buffer;
    std::atomic<int> writeHead{0};
    std::atomic<bool> initialized{false};
    
    int bufferSize{0};
    int bufferMask{0};  // bufferSize - 1 for fast modulo operations
    int numChannels{0};

    /**
     * Round up to the next power of 2.
     */
    static int nextPowerOfTwo(int value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularAudioBuffer)
};

} // namespace OpenLooper2