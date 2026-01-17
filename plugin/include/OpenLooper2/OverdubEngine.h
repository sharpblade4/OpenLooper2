#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace OpenLooper2 {

/**
 * Handles audio layering and feedback processing for overdub operations.
 * Provides high-quality mixing with configurable feedback levels.
 */
class OverdubEngine
{
public:
    OverdubEngine();
    ~OverdubEngine();

    /**
     * Initialize the overdub engine with audio specifications.
     * @param sampleRate The audio sample rate
     * @param samplesPerBlock Expected samples per audio block
     */
    void initialize(double sampleRate, int samplesPerBlock);

    /**
     * Process overdub mixing, combining input audio with existing buffer content.
     * @param buffer The audio buffer containing existing loop content (will be modified)
     * @param input The new input audio to be mixed in
     * @param feedbackLevel The feedback level for the existing content (0.0 to 1.0)
     */
    void processOverdub(juce::AudioBuffer<float>& buffer, 
                       const juce::AudioBuffer<float>& input,
                       float feedbackLevel);

    /**
     * Set the feedback level for overdub operations.
     * @param level Feedback level (0.0 to 1.0)
     */
    void setFeedbackLevel(float level);

    /**
     * Get the current feedback level.
     */
    float getFeedbackLevel() const { return currentFeedbackLevel.load(std::memory_order_acquire); }

    /**
     * Set the overdub gain for new input audio.
     * @param gain Overdub gain (0.0 to 2.0)
     */
    void setOverdubGain(float gain);

    /**
     * Get the current overdub gain.
     */
    float getOverdubGain() const { return currentOverdubGain.load(std::memory_order_acquire); }

    /**
     * Check if the engine is initialized.
     */
    bool isInitialized() const { return initialized.load(std::memory_order_acquire); }

private:
    std::atomic<float> currentFeedbackLevel{0.8f};
    std::atomic<float> currentOverdubGain{1.0f};
    std::atomic<bool> initialized{false};
    
    juce::dsp::Gain<float> feedbackGain;
    juce::dsp::Gain<float> overdubGain;
    
    double sampleRate{44100.0};
    int samplesPerBlock{512};

    /**
     * Apply smooth parameter changes to prevent audio artifacts.
     */
    void updateGainParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverdubEngine)
};

} // namespace OpenLooper2