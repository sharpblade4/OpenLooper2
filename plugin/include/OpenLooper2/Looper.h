#pragma once

#include "LoopBufferManager.h"
#include "TransportController.h"
#include "OverdubEngine.h"
#include "ParameterManager.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace OpenLooper2 {

/**
 * Main looper class that integrates all audio looper components.
 * Provides a unified interface for the audio processor.
 */
class Looper
{
public:
    Looper();
    ~Looper();

    /**
     * Initialize the looper with audio specifications.
     * @param sampleRate The audio sample rate
     * @param samplesPerBlock Expected samples per audio block
     * @param numChannels Number of audio channels
     */
    void initialize(double sampleRate, int samplesPerBlock, int numChannels);

    /**
     * Process a block of audio samples.
     * @param buffer The audio buffer to process
     * @param apvts The AudioProcessorValueTreeState for parameter access
     */
    void processBlock(juce::AudioBuffer<float>& buffer, 
                     const juce::AudioProcessorValueTreeState& apvts);

    /**
     * Get access to individual components for UI updates.
     */
    const TransportController& getTransportController() const { return transportController; }
    const LoopBufferManager& getLoopBufferManager() const { return loopBufferManager; }
    const ParameterManager& getParameterManager() const { return parameterManager; }

    /**
     * Create the parameter layout for the AudioProcessorValueTreeState.
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /**
     * Check if the looper is initialized.
     */
    bool isInitialized() const { return initialized; }

private:
    LoopBufferManager loopBufferManager;
    TransportController transportController;
    OverdubEngine overdubEngine;
    ParameterManager parameterManager;
    
    bool initialized{false};
    double sampleRate{44100.0};
    int samplesPerBlock{512};
    int numChannels{2};
    
    // Temporary buffers for processing
    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> loopBuffer;

    /**
     * Handle transport state changes based on parameter triggers.
     */
    void handleTransportControls();

    /**
     * Process audio based on current transport state.
     */
    void processAudioForCurrentState(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Looper)
};

} // namespace OpenLooper2