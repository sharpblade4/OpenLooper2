#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace OpenLooper2 {

/**
 * Manages playback state and timing for the audio looper.
 * Provides thread-safe state management using atomic operations.
 */
class TransportController
{
public:
    enum class State
    {
        Stopped,
        Recording,
        Playing,
        Overdubbing
    };

    TransportController();
    ~TransportController();

    /**
     * Initialize the transport controller with audio specifications.
     * @param sampleRate The audio sample rate
     * @param samplesPerBlock Expected samples per audio block
     */
    void initialize(double sampleRate, int samplesPerBlock);

    /**
     * Process a block of audio samples, updating timing and position.
     * @param numSamples Number of samples in the current block
     */
    void processBlock(int numSamples);

    /**
     * Transport control methods.
     */
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void startOverdub();
    void stopOverdub();

    /**
     * Get the current transport state.
     */
    State getCurrentState() const { return currentState.load(std::memory_order_acquire); }

    /**
     * Get the current playback position as a normalized value (0.0 to 1.0).
     */
    float getPlaybackPosition() const { return playbackPosition.load(std::memory_order_acquire); }

    /**
     * Get the current playback position in samples.
     */
    int getPlaybackPositionSamples() const { return playbackPositionSamples.load(std::memory_order_acquire); }

    /**
     * Set the loop length for position calculations.
     * @param lengthInSamples The loop length in samples
     */
    void setLoopLength(int lengthInSamples);

    /**
     * Get the current loop length in samples.
     */
    int getLoopLength() const { return loopLengthSamples.load(std::memory_order_acquire); }

    /**
     * Reset the playback position to the beginning.
     */
    void resetPosition();

    /**
     * Check if the transport is initialized.
     */
    bool isInitialized() const { return initialized.load(std::memory_order_acquire); }

private:
    std::atomic<State> currentState{State::Stopped};
    std::atomic<float> playbackPosition{0.0f};
    std::atomic<int> playbackPositionSamples{0};
    std::atomic<int> loopLengthSamples{0};
    std::atomic<bool> initialized{false};
    
    double sampleRate{44100.0};
    int samplesPerBlock{512};

    /**
     * Update the playback position based on the number of samples processed.
     */
    void updatePosition(int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportController)
};

} // namespace OpenLooper2