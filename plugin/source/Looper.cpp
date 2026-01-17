#include "OpenLooper2/Looper.h"

namespace OpenLooper2 {

Looper::Looper()
{
}

Looper::~Looper()
{
}

void Looper::initialize(double sampleRate, int samplesPerBlock, int numChannels)
{
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    this->numChannels = numChannels;
    
    // Initialize all components
    const float maxLoopLengthSeconds = 60.0f; // 60 seconds maximum loop length
    
    loopBufferManager.initialize(sampleRate, numChannels, maxLoopLengthSeconds);
    transportController.initialize(sampleRate, samplesPerBlock);
    overdubEngine.initialize(sampleRate, samplesPerBlock);
    
    // Prepare temporary buffers
    tempBuffer.setSize(numChannels, samplesPerBlock);
    loopBuffer.setSize(numChannels, samplesPerBlock);
    
    initialized = true;
}

void Looper::processBlock(juce::AudioBuffer<float>& buffer, 
                         const juce::AudioProcessorValueTreeState& apvts)
{
    if (!initialized)
        return;
    
    const int numSamples = buffer.getNumSamples();
    
    // Update parameters from APVTS
    parameterManager.updateFromParameters(apvts);
    
    // Handle transport control triggers
    handleTransportControls();
    
    // Update transport timing
    transportController.processBlock(numSamples);
    
    // Process audio based on current state
    processAudioForCurrentState(buffer);
}

juce::AudioProcessorValueTreeState::ParameterLayout Looper::createParameterLayout()
{
    return ParameterManager::createParameterLayout();
}

void Looper::handleTransportControls()
{
    // Check for transport button triggers
    if (parameterManager.wasRecordTriggered())
    {
        const auto currentState = transportController.getCurrentState();
        if (currentState == TransportController::State::Stopped)
        {
            transportController.startRecording();
        }
        else if (currentState == TransportController::State::Recording)
        {
            transportController.stopRecording();
            // Set the loop length in the buffer manager
            const int loopLength = transportController.getLoopLength();
            loopBufferManager.setLoopLength(loopLength);
        }
    }
    
    if (parameterManager.wasPlayTriggered())
    {
        transportController.startPlayback();
    }
    
    if (parameterManager.wasStopTriggered())
    {
        transportController.stopPlayback();
    }
    
    if (parameterManager.wasOverdubTriggered())
    {
        const auto currentState = transportController.getCurrentState();
        if (currentState == TransportController::State::Playing)
        {
            transportController.startOverdub();
        }
        else if (currentState == TransportController::State::Overdubbing)
        {
            transportController.stopOverdub();
        }
    }
}

void Looper::processAudioForCurrentState(juce::AudioBuffer<float>& buffer)
{
    const auto currentState = transportController.getCurrentState();
    const int numSamples = buffer.getNumSamples();
    
    switch (currentState)
    {
        case TransportController::State::Recording:
        {
            // Write input audio to the loop buffer
            loopBufferManager.writeAudio(buffer, 0, numSamples);
            // Pass through the input audio
            break;
        }
        
        case TransportController::State::Playing:
        {
            // Read from loop buffer and replace the input
            const float position = transportController.getPlaybackPosition();
            loopBufferManager.readAudio(buffer, 0, numSamples, position);
            
            // Apply volume control
            const float volume = parameterManager.getVolumeLevel();
            buffer.applyGain(volume);
            break;
        }
        
        case TransportController::State::Overdubbing:
        {
            // Read existing loop content
            const float position = transportController.getPlaybackPosition();
            loopBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            loopBufferManager.readAudio(loopBuffer, 0, numSamples, position);
            
            // Mix input with existing content using overdub engine
            const float feedbackLevel = parameterManager.getFeedbackLevel();
            overdubEngine.processOverdub(loopBuffer, buffer, feedbackLevel);
            
            // Write the mixed result back to the loop buffer
            loopBufferManager.writeAudio(loopBuffer, 0, numSamples);
            
            // Output the mixed result
            buffer.makeCopyOf(loopBuffer);
            
            // Apply volume control
            const float volume = parameterManager.getVolumeLevel();
            buffer.applyGain(volume);
            break;
        }
        
        case TransportController::State::Stopped:
        default:
        {
            // Pass through input audio or silence
            break;
        }
    }
}

} // namespace OpenLooper2