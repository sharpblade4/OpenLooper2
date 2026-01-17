#pragma once

#include "CircularAudioBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace OpenLooper2 {

/**
 * Manages loop buffer storage and retrieval using a circular buffer.
 * Handles dynamic loop length management and efficient audio I/O.
 */
class LoopBufferManager
{
public:
    LoopBufferManager();
    ~LoopBufferManager();

    /**
     * Initialize the buffer manager with audio specifications.
     * @param sampleRate The audio sample rate
     * @param maxChannels Maximum number of audio channels
     * @param maxLengthSeconds Maximum loop length in seconds
     */
    void initialize(double sampleRate, int maxChannels, float maxLengthSeconds);

    /**
     * Write audio data to the loop buffer.
     * @param input The input audio buffer
     * @param startSample Starting sample in the input buffer
     * @param numSamples Number of samples to write
     */
    void writeAudio(const juce::AudioBuffer<float>& input, int startSample, int numSamples);

    /**
     * Read audio data from the loop buffer at a specific position.
     * @param output The output audio buffer
     * @param startSample Starting sample in the output buffer
     * @param numSamples Number of samples to read
     * @param position Normalized position in the loop (0.0 to 1.0)
     */
    void readAudio(juce::AudioBuffer<float>& output, int startSample, int numSamples, float position);

    /**
     * Set the current loop length in samples.
     * @param lengthInSamples The loop length in samples
     */
    void setLoopLength(int lengthInSamples);

    /**
     * Get the current loop length in samples.
     */
    int getLoopLength() const { return loopLengthSamples.load(std::memory_order_acquire); }

    /**
     * Get the current loop length in seconds.
     */
    float getLoopLengthSeconds() const;

    /**
     * Clear the loop buffer.
     */
    void clear();

    /**
     * Check if the buffer is initialized.
     */
    bool isInitialized() const { return initialized.load(std::memory_order_acquire); }

    /**
     * Get the maximum buffer size in samples.
     */
    int getMaxBufferSize() const { return maxBufferSize; }

private:
    CircularAudioBuffer circularBuffer;
    std::atomic<int> loopLengthSamples{0};
    std::atomic<bool> initialized{false};
    
    double sampleRate{44100.0};
    int maxBufferSize{0};
    int maxChannels{2};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopBufferManager)
};

} // namespace OpenLooper2