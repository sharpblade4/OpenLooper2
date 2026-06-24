#include "OpenLooper2/Looper.h"
#include <juce_audio_formats/juce_audio_formats.h>

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
    midiLooper.initialize(newSampleRate, newSamplesPerBlock);
    
    // Prepare temporary buffers
    tempBuffer.setSize(newNumChannels, newSamplesPerBlock);
    loopBuffer.setSize(newNumChannels, newSamplesPerBlock);
    
    initialized = true;
}

void Looper::processBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         const juce::AudioProcessorValueTreeState& apvts)
{
    if (!initialized)
        return;
    
    const int numSamples = buffer.getNumSamples();
    
    // Validate buffer size
    if (numSamples <= 0 || numSamples > samplesPerBlock * 2)
    {
        buffer.clear();
        return;
    }

    // MIDI mode: pass-through + MIDI looping, no audio
    if (midiMode)
    {
        buffer.clear();
        
        // Debug: log incoming MIDI
        if (debugEnabled.load(std::memory_order_relaxed) && !midiMessages.isEmpty())
        {
            DBG("MIDI mode: " << midiMessages.getNumEvents() << " events in, state=" 
                << static_cast<int>(transportController.getCurrentState()));
        }
        
        // Update parameters
        parameterManager.updateFromParameters(apvts);
        
        // Handle transport controls (same state machine for both modes)
        handleTransportControls();
        
        // Check if stop was just triggered — need to send note-offs
        const auto state = transportController.getCurrentState();
        if (state == TransportController::State::Stopped && midiLooper.getNoteTracker().hasActiveNotes())
            midiLooper.stop(midiMessages, 0);
        
        // Process MIDI looper (incoming pass-through + playback merged)
        const float velocityScale = parameterManager.getVolumeLevel();
        const float feedback = parameterManager.getFeedbackLevel();
        midiLooper.processBlock(midiMessages, transportController, lastPositionInfo, velocityScale, feedback);
        
        // Update transport timing
        transportController.processBlock(numSamples);
        return;
    }

    // Audio mode: existing behavior
    midiMessages.clear();
    
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
    
    // Store for MidiLooper use
    lastPositionInfo = positionInfo;
    
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
    const bool oneLoopOverdubPressed = pendingOneLoopOverdub.exchange(false, std::memory_order_acq_rel);
    
    const auto state = transportController.getCurrentState();
    
    // --- 1-LOOP OVERDUB button (with decomposed flags) ---
    if (oneLoopOverdubPressed)
    {
        const int loopLength = transportController.getLoopLength();
        if (loopLength > 0 && (state == TransportController::State::Playing || state == TransportController::State::Stopped))
        {
            const bool waitForBegin = pendingWaitLoopBegin.exchange(false, std::memory_order_acq_rel);
            const bool oneLoop = pendingOneLoopRecord.exchange(false, std::memory_order_acq_rel);
            
            if (waitForBegin)
            {
                // Arm: will start overdubbing at next loop boundary
                oneLoopOverdubArmed.store(true, std::memory_order_release);
                oneLoopOverdubActive.store(false, std::memory_order_release);
            }
            else
            {
                // Start overdub immediately
                oneLoopOverdubArmed.store(false, std::memory_order_release);
                oneLoopOverdubActive.store(oneLoop, std::memory_order_release);
                if (state == TransportController::State::Stopped)
                    transportController.startPlayback();
                transportController.startOverdub();
                LOOPER_DBG("Overdub started immediately, oneLoop=" << (int)oneLoop);
            }
            
            // Store oneLoop flag for use when armed overdub starts
            if (waitForBegin)
            {
                // Stash the oneLoop preference for when armed overdub triggers
                pendingOneLoopRecord.store(oneLoop, std::memory_order_release);
                if (state == TransportController::State::Stopped)
                    transportController.startPlayback();
                LOOPER_DBG("Overdub armed (waitLoopBegin), oneLoop=" << (int)oneLoop);
            }
        }
    }
    
    // --- 1-LOOP OVERDUB state machine ---
    if (oneLoopOverdubArmed.load(std::memory_order_acquire))
    {
        const int pos = transportController.getPlaybackPositionSamples();
        if (pos < samplesPerBlock)  // near loop start
        {
            oneLoopOverdubArmed.store(false, std::memory_order_release);
            const bool oneLoop = pendingOneLoopRecord.exchange(false, std::memory_order_acq_rel);
            oneLoopOverdubActive.store(oneLoop, std::memory_order_release);
            transportController.startOverdub();
            LOOPER_DBG("Armed overdub started at loop boundary, oneLoop=" << (int)oneLoop);
        }
    }
    else if (oneLoopOverdubActive.load(std::memory_order_acquire))
    {
        const int pos = transportController.getPlaybackPositionSamples();
        if (pos < samplesPerBlock)  // wrapped back to start
        {
            oneLoopOverdubActive.store(false, std::memory_order_release);
            transportController.stopOverdub();
            waveformNeedsUpdate.store(true, std::memory_order_release);
            LOOPER_DBG("One-loop overdub finished");
        }
    }
    
    // --- RECORD button ---
    if (recordPressed)
    {
        if (state == TransportController::State::Recording)
        {
            // Recording → Playing (finalize loop)
            if (midiMode)
            {
                // Calculate loop length snapped to bar boundary
                const double ppq = lastPositionInfo.getPpqPosition().orFallback(0.0);
                auto ts = lastPositionInfo.getTimeSignature();
                int num = ts ? ts->numerator : 4;
                int den = ts ? ts->denominator : 4;
                double beatsPerBar = static_cast<double>(num) * (4.0 / den);
                double rawLength = ppq - midiLooper.getLoopStartPpq();
                double loopBars = std::ceil(rawLength / beatsPerBar);
                if (loopBars < 1.0) loopBars = 1.0;
                double loopLengthBeats = loopBars * beatsPerBar;
                midiLooper.getLoopBufferMut().setLoopLengthBeats(loopLengthBeats);
                LOOPER_DBG("MIDI Record→Playing, loop=" << loopLengthBeats << " beats");
            }
            transportController.stopRecording();
            loopBufferManager.setLoopLength(transportController.getLoopLength());
            waveformNeedsUpdate.store(true, std::memory_order_release);
            LOOPER_DBG("Record pressed: Recording→Playing, loop=" << transportController.getLoopLength());
        }
        else
        {
            if (midiMode && !midiLooper.isHostTransportRunning())
            {
                // Arm — wait for host transport to start
                midiLooper.arm();
                LOOPER_DBG("Record pressed: armed, waiting for host transport");
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
            waveformNeedsUpdate.store(true, std::memory_order_release);
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
    const bool passthrough = monitorPassthrough.load(std::memory_order_acquire);
    
    switch (currentState)
    {
        case TransportController::State::Recording:
        {
            // Write input audio to the loop buffer
            loopBufferManager.writeAudio(buffer, 0, numSamples);
            // If passthrough enabled, keep input in buffer; otherwise mute
            if (!passthrough)
                buffer.clear();
            break;
        }
        
        case TransportController::State::Playing:
        {
            const int positionSamples = transportController.getPlaybackPositionSamples();
            const int loopLength = transportController.getLoopLength();
            
            if (loopLength > 0)
            {
                const int loopPosition = positionSamples % loopLength;
                const float volume = parameterManager.getVolumeLevel();
                
                loopBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
                loopBufferManager.readAudio(loopBuffer, 0, numSamples, loopPosition);
                
                if (passthrough)
                {
                    // Mix loop on top of input
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.addFrom(ch, 0, loopBuffer, ch, 0, numSamples, volume);
                }
                else
                {
                    // Replace buffer with loop content only
                    buffer.makeCopyOf(loopBuffer);
                    buffer.applyGain(volume);
                }
            }
            else if (!passthrough)
            {
                buffer.clear();
            }
            break;
        }
        
        case TransportController::State::Overdubbing:
        {
            const int positionSamples = transportController.getPlaybackPositionSamples();
            const int loopLength = transportController.getLoopLength();
            const int loopPosition = positionSamples % loopLength;
            
            // Read existing loop content
            loopBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            loopBufferManager.readAudio(loopBuffer, 0, numSamples, loopPosition);
            
            // Save loop content for output
            tempBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            tempBuffer.makeCopyOf(loopBuffer);
            
            // Mix input with existing content using overdub engine
            const float feedbackLevel = parameterManager.getFeedbackLevel();
            overdubEngine.processOverdub(loopBuffer, buffer, feedbackLevel);
            
            // Write the mixed result back to the loop buffer
            loopBufferManager.writeBackAudio(loopBuffer, 0, numSamples, loopPosition);
            
            // Output: existing loop content (+ input passthrough if enabled)
            const float volume = parameterManager.getVolumeLevel();
            if (passthrough)
            {
                // Input already in buffer, add loop on top
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addFrom(ch, 0, tempBuffer, ch, 0, numSamples, volume);
            }
            else
            {
                // Output only loop content, no live input echo
                buffer.makeCopyOf(tempBuffer);
                buffer.applyGain(volume);
            }
            break;
        }
        
        case TransportController::State::Stopped:
        default:
        {
            // If passthrough disabled, silence output
            if (!passthrough)
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

void Looper::exportLoop()
{
    const int loopLength = loopBufferManager.getLoopLength();
    if (loopLength <= 0 || !initialized)
        return;
    
    // Read the entire loop into a buffer
    juce::AudioBuffer<float> exportBuffer(numChannels, loopLength);
    loopBufferManager.readAudio(exportBuffer, 0, loopLength, 0);
    
    // Show save dialog (async, stays alive via member unique_ptr)
    fileChooser = std::make_unique<juce::FileChooser>(
        "Export Loop as WAV",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("loop.wav"),
        "*.wav");
    
    auto* chooserPtr = fileChooser.get();
    chooserPtr->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, buf = std::move(exportBuffer)](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{})
                return;
            
            // Ensure .wav extension
            if (!file.hasFileExtension("wav"))
                file = file.withFileExtension("wav");
            
            // Write WAV file
            file.deleteFile();
            std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());
            if (outputStream == nullptr)
                return;
            
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(),
                    sampleRate,
                    static_cast<unsigned int>(buf.getNumChannels()),
                    24, {}, 0));
            
            if (writer != nullptr)
            {
                outputStream.release(); // writer takes ownership
                writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
            }
        });
}

} // namespace OpenLooper2