#include "OpenLooper2/ParameterManager.h"

namespace OpenLooper2 {

ParameterManager::ParameterManager()
{
}

ParameterManager::~ParameterManager()
{
}

void ParameterManager::createParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // The parameters are created through the AudioProcessorValueTreeState constructor
    // This method can be used for additional parameter setup if needed
}

juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Transport control buttons (trigger parameters)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        RECORD_ID, "Record", false));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(
        PLAY_ID, "Play", false));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(
        STOP_ID, "Stop", false));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(
        OVERDUB_ID, "Overdub", false));

    // Continuous parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        FEEDBACK_ID, "Feedback", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        VOLUME_ID, "Volume", 
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    return layout;
}

void ParameterManager::updateFromParameters(const juce::AudioProcessorValueTreeState& apvts)
{
    // Get current button states
    const bool currentRecord = *apvts.getRawParameterValue(RECORD_ID) > 0.5f;
    const bool currentPlay = *apvts.getRawParameterValue(PLAY_ID) > 0.5f;
    const bool currentStop = *apvts.getRawParameterValue(STOP_ID) > 0.5f;
    const bool currentOverdub = *apvts.getRawParameterValue(OVERDUB_ID) > 0.5f;
    
    // Detect button press edges (transition from false to true)
    const bool prevRecord = prevRecordState.load(std::memory_order_acquire);
    const bool prevPlay = prevPlayState.load(std::memory_order_acquire);
    const bool prevStop = prevStopState.load(std::memory_order_acquire);
    const bool prevOverdub = prevOverdubState.load(std::memory_order_acquire);
    
    if (currentRecord && !prevRecord)
        recordTriggered.store(true, std::memory_order_release);
    
    if (currentPlay && !prevPlay)
        playTriggered.store(true, std::memory_order_release);
    
    if (currentStop && !prevStop)
        stopTriggered.store(true, std::memory_order_release);
    
    if (currentOverdub && !prevOverdub)
        overdubTriggered.store(true, std::memory_order_release);
    
    // Update previous states
    prevRecordState.store(currentRecord, std::memory_order_release);
    prevPlayState.store(currentPlay, std::memory_order_release);
    prevStopState.store(currentStop, std::memory_order_release);
    prevOverdubState.store(currentOverdub, std::memory_order_release);
    
    // Update continuous parameters
    const float newFeedback = *apvts.getRawParameterValue(FEEDBACK_ID);
    const float newVolume = *apvts.getRawParameterValue(VOLUME_ID);
    
    feedbackLevel.store(newFeedback, std::memory_order_release);
    volumeLevel.store(newVolume, std::memory_order_release);
}

bool ParameterManager::wasRecordTriggered()
{
    return recordTriggered.exchange(false, std::memory_order_acq_rel);
}

bool ParameterManager::wasPlayTriggered()
{
    return playTriggered.exchange(false, std::memory_order_acq_rel);
}

bool ParameterManager::wasStopTriggered()
{
    return stopTriggered.exchange(false, std::memory_order_acq_rel);
}

bool ParameterManager::wasOverdubTriggered()
{
    return overdubTriggered.exchange(false, std::memory_order_acq_rel);
}

void ParameterManager::setFeedbackLevel(float level)
{
    const float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
    feedbackLevel.store(clampedLevel, std::memory_order_release);
}

void ParameterManager::setVolumeLevel(float level)
{
    const float clampedLevel = juce::jlimit(0.0f, 2.0f, level);
    volumeLevel.store(clampedLevel, std::memory_order_release);
}

} // namespace OpenLooper2