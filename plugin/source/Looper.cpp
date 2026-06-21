#include "OpenLooper2/Looper.h"

#define LOOPER_DBG(msg) do { if (debugEnabled.load(std::memory_order_relaxed)) { DBG(msg); } } while(0)

namespace OpenLooper2 {

Looper::Looper()
{
}

Looper::~Looper()
{
}

void Looper::initialize(double newSampleRate, int newSamplesPerBlock, int newNumChannels)
{
    this->sampleRate = newSampleRate;
    this->samplesPerBlock = newSamplesPerBlock;
    this->numChannels = newNumChannels;
    
    // Initialize all components
    const float maxLoopLengthSeconds = 60.0f; // 60 seconds maximum loop length
    
    loopBufferManager.initialize(newSampleRate, newNumChannels, maxLoopLengthSeconds);
    transportController.initialize(newSampleRate, newSamplesPerBlock);
    overdubEngine.initialize(newSampleRate, newSamplesPerBlock);
    
    // Prepare temporary buffers
    tempBuffer.setSize(newNumChannels, newSamplesPerBlock);
    loopBuffer.setSize(newNumChannels, newSamplesPerBlock);
    
    initialized = true;
}

void Looper::processBlock(juce::AudioBuffer<float>& buffer, 
                         const juce::AudioProcessorValueTreeState& apvts)
{
    if (!initialized)
        return;
    
    const int numSamples = buffer.getNumSamples();
    
    // Validate buffer size
    if (numSamples <= 0 || numSamples > samplesPerBlock * 2)
    {
        // Invalid buffer size - clear and return
        buffer.clear();
        return;
    }
    
    // Check for invalid audio data (NaN or Inf)
    if (containsInvalidData(buffer))
    {
        // Clear invalid data and continue
        buffer.clear();
    }
    
    try
    {
        // Measure input level for UI metering
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float chPeak = buffer.getMagnitude(ch, 0, numSamples);
            if (chPeak > peak) peak = chPeak;
        }
        inputLevel.store(peak, std::memory_order_release);
        
        // Update parameters from APVTS
        parameterManager.updateFromParameters(apvts);
        
        // Update host sync setting
        const bool hostSyncEnabled = parameterManager.getHostSyncEnabled();
        transportController.setHostSyncEnabled(hostSyncEnabled);
        
        // Handle transport control triggers
        handleTransportControls();
        
        // Process audio based on current state (BEFORE advancing position)
        processAudioForCurrentState(buffer);
        
        // Update transport timing (AFTER processing audio)
        transportController.processBlock(numSamples);
    }
    catch (const std::exception& e)
    {
        // Log error and clear buffer to prevent audio artifacts
        DBG("Looper error: " << e.what());
        buffer.clear();
    }
    catch (...)
    {
        // Catch any other exceptions
        DBG("Looper: Unknown error occurred");
        buffer.clear();
    }
}

void Looper::updateHostTransport(const juce::AudioPlayHead::PositionInfo& positionInfo)
{
    if (!initialized)
        return;
    
    // Extract host transport information
    const bool isPlaying = positionInfo.getIsPlaying();
    const auto ppqPosition = positionInfo.getPpqPosition();
    const auto bpm = positionInfo.getBpm();
    
    // Update transport controller with host information
    if (ppqPosition.hasValue() && bpm.hasValue())
    {
        transportController.updateHostTransport(isPlaying, *ppqPosition, *bpm);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Looper::createParameterLayout()
{
    return ParameterManager::createParameterLayout();
}

void Looper::handleTransportControls()
{
    // Consume triggers from both the parameter system (automation) and direct UI flags
    const bool recordPressed = parameterManager.wasRecordTriggered() 
                            || pendingRecord.exchange(false, std::memory_order_acq_rel);
    const bool playPressed = parameterManager.wasPlayTriggered() 
                          || pendingPlay.exchange(false, std::memory_order_acq_rel);
    const bool stopPressed = parameterManager.wasStopTriggered() 
                          || pendingStop.exchange(false, std::memory_order_acq_rel);
    const bool overdubPressed = parameterManager.wasOverdubTriggered() 
                             || pendingOverdub.exchange(false, std::memory_order_acq_rel);
    
    const auto state = transportController.getCurrentState();
    
    // --- RECORD button ---
    if (recordPressed)
    {
        if (state == TransportController::State::Recording)
        {
            // Recording → Playing (finalize loop)
            transportController.stopRecording();
            loopBufferManager.setLoopLength(transportController.getLoopLength());
            waveformNeedsUpdate.store(true, std::memory_order_release);
            LOOPER_DBG("Record pressed: Recording→Playing, loop=" << transportController.getLoopLength());
        }
        else
        {
            // Any other state → Recording (clear and start fresh)
            loopBufferManager.clear();
            waveformLength.store(0, std::memory_order_release);
            transportController.startRecording();
            LOOPER_DBG("Record pressed: →Recording");
        }
    }
    
    // --- PLAY button ---
    if (playPressed)
    {
        if (state == TransportController::State::Recording)
        {
            // Recording → Playing (finalize loop)
            transportController.stopRecording();
            loopBufferManager.setLoopLength(transportController.getLoopLength());
            waveformNeedsUpdate.store(true, std::memory_order_release);
            LOOPER_DBG("Play pressed: Recording→Playing, loop=" << transportController.getLoopLength());
        }
        else if (state == TransportController::State::Stopped)
        {
            // Stopped → Playing (if we have a loop)
            transportController.startPlayback();
            LOOPER_DBG("Play pressed: Stopped→Playing");
        }
    }
    
    // --- STOP button ---
    if (stopPressed)
    {
        if (state == TransportController::State::Recording)
        {
            // Recording → Stopped (finalize loop but don't play)
            transportController.stopRecording();
            loopBufferManager.setLoopLength(transportController.getLoopLength());
            waveformNeedsUpdate.store(true, std::memory_order_release);
            transportController.stopPlayback();
            LOOPER_DBG("Stop pressed: Recording→Stopped, loop=" << transportController.getLoopLength());
        }
        else
        {
            // Any other state → Stopped
            transportController.stopPlayback();
            LOOPER_DBG("Stop pressed: →Stopped");
        }
    }
    
    // --- OVERDUB button ---
    if (overdubPressed)
    {
        if (state == TransportController::State::Playing)
        {
            transportController.startOverdub();
            LOOPER_DBG("Overdub pressed: Playing→Overdubbing");
        }
        else if (state == TransportController::State::Overdubbing)
        {
            transportController.stopOverdub();
            LOOPER_DBG("Overdub pressed: Overdubbing→Playing");
        }
        else if (state == TransportController::State::Stopped && transportController.getLoopLength() > 0)
        {
            // Stopped with loop → start playing then overdub
            transportController.startPlayback();
            transportController.startOverdub();
            LOOPER_DBG("Overdub pressed: Stopped→Overdubbing");
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
            // During recording, mute the output to avoid feedback
            // The user should monitor through Ableton's monitoring
            buffer.clear();
            break;
        }
        
        case TransportController::State::Playing:
        {
            // Read from loop buffer using sample-accurate position
            const int positionSamples = transportController.getPlaybackPositionSamples();
            const int loopLength = transportController.getLoopLength();
            
            LOOPER_DBG("Playing: positionSamples=" << positionSamples << ", loopLength=" << loopLength);
            
            if (loopLength > 0)
            {
                // Calculate position within the loop (handle wrapping)
                const int loopPosition = positionSamples % loopLength;
                
                LOOPER_DBG("  loopPosition=" << loopPosition);
                
                loopBufferManager.readAudio(buffer, 0, numSamples, loopPosition);
                
                // Apply volume control
                const float volume = parameterManager.getVolumeLevel();
                buffer.applyGain(volume);
            }
            else
            {
                LOOPER_DBG("  Loop length is 0, clearing buffer");
                buffer.clear();
            }
            break;
        }
        
        case TransportController::State::Overdubbing:
        {
            // Read existing loop content from current position
            const int positionSamples = transportController.getPlaybackPositionSamples();
            const int loopLength = transportController.getLoopLength();
            const int loopPosition = positionSamples % loopLength;
            
            loopBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            loopBufferManager.readAudio(loopBuffer, 0, numSamples, loopPosition);
            
            // Save original loop content for output (before mixing in live input)
            tempBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            tempBuffer.makeCopyOf(loopBuffer);
            
            // Mix input with existing content using overdub engine
            const float feedbackLevel = parameterManager.getFeedbackLevel();
            overdubEngine.processOverdub(loopBuffer, buffer, feedbackLevel);
            
            // Write the mixed result back to the loop buffer
            loopBufferManager.writeBackAudio(loopBuffer, 0, numSamples, loopPosition);
            
            // Output only the original loop content (before live input was mixed in)
            buffer.makeCopyOf(tempBuffer);
            
            // Apply volume control
            const float volume = parameterManager.getVolumeLevel();
            buffer.applyGain(volume);
            break;
        }
        
        case TransportController::State::Stopped:
        default:
        {
            // Clear output — looper has nothing to play. Input is only used
            // during Recording/Overdubbing. This prevents feedback loops when
            // the host routes input through the plugin (e.g. Ableton "Auto" monitoring).
            buffer.clear();
            break;
        }
    }
}

bool Looper::containsInvalidData(const juce::AudioBuffer<float>& buffer) const
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        
        for (int sample = 0; sample < samples; ++sample)
        {
            const float value = data[sample];
            if (std::isnan(value) || std::isinf(value))
            {
                return true;
            }
        }
    }
    
    return false;
}

void Looper::updateWaveformDisplay()
{
    const int loopLength = loopBufferManager.getLoopLength();
    if (loopLength <= 0)
    {
        waveformLength.store(0, std::memory_order_release);
        return;
    }
    
    // Read peaks from the loop buffer into the waveform array
    const int samplesPerBin = loopLength / WAVEFORM_RESOLUTION;
    if (samplesPerBin <= 0)
    {
        waveformLength.store(0, std::memory_order_release);
        return;
    }
    
    juce::AudioBuffer<float> readBuf(numChannels, samplesPerBin);
    
    for (int bin = 0; bin < WAVEFORM_RESOLUTION; ++bin)
    {
        const int pos = bin * samplesPerBin;
        loopBufferManager.readAudio(readBuf, 0, samplesPerBin, pos);
        
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = readBuf.getReadPointer(ch);
            for (int s = 0; s < samplesPerBin; ++s)
            {
                const float absVal = std::abs(data[s]);
                if (absVal > peak) peak = absVal;
            }
        }
        waveformPeaks[bin] = peak;
    }
    
    waveformLength.store(WAVEFORM_RESOLUTION, std::memory_order_release);
    waveformDirty.store(true, std::memory_order_release);
}

} // namespace OpenLooper2