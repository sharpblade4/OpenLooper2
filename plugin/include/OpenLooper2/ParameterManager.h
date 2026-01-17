#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace OpenLooper2 {

/**
 * Manages all plugin parameters and automation for the audio looper.
 * Provides thread-safe parameter access and VST3 automation support.
 */
class ParameterManager
{
public:
    // Parameter IDs
    static constexpr const char* RECORD_ID = "record";
    static constexpr const char* PLAY_ID = "play";
    static constexpr const char* STOP_ID = "stop";
    static constexpr const char* OVERDUB_ID = "overdub";
    static constexpr const char* FEEDBACK_ID = "feedback";
    static constexpr const char* VOLUME_ID = "volume";

    ParameterManager();
    ~ParameterManager();

    /**
     * Create and add parameters to the AudioProcessorValueTreeState.
     * @param apvts The AudioProcessorValueTreeState to add parameters to
     */
    void createParameters(juce::AudioProcessorValueTreeState& apvts);

    /**
     * Update internal state from parameter values.
     * Call this from the audio thread to get latest parameter values.
     * @param apvts The AudioProcessorValueTreeState containing current parameter values
     */
    void updateFromParameters(const juce::AudioProcessorValueTreeState& apvts);

    /**
     * Check if a transport button was triggered and reset the trigger state.
     */
    bool wasRecordTriggered();
    bool wasPlayTriggered();
    bool wasStopTriggered();
    bool wasOverdubTriggered();

    /**
     * Get current continuous parameter values.
     */
    float getFeedbackLevel() const { return feedbackLevel.load(std::memory_order_acquire); }
    float getVolumeLevel() const { return volumeLevel.load(std::memory_order_acquire); }

    /**
     * Set parameter values programmatically.
     */
    void setFeedbackLevel(float level);
    void setVolumeLevel(float level);

    /**
     * Create parameter layout for the AudioProcessorValueTreeState.
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // Trigger states for transport buttons
    std::atomic<bool> recordTriggered{false};
    std::atomic<bool> playTriggered{false};
    std::atomic<bool> stopTriggered{false};
    std::atomic<bool> overdubTriggered{false};
    
    // Continuous parameter values
    std::atomic<float> feedbackLevel{0.8f};
    std::atomic<float> volumeLevel{1.0f};
    
    // Previous button states for edge detection
    std::atomic<bool> prevRecordState{false};
    std::atomic<bool> prevPlayState{false};
    std::atomic<bool> prevStopState{false};
    std::atomic<bool> prevOverdubState{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterManager)
};

} // namespace OpenLooper2