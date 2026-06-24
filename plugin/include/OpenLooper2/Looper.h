#pragma once

#include "LoopBufferManager.h"
#include "TransportController.h"
#include "OverdubEngine.h"
#include "ParameterManager.h"
#include "MidiLooper.h"
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
     * @param newSampleRate The audio sample rate
     * @param newSamplesPerBlock Expected samples per audio block
     * @param newNumChannels Number of audio channels
     */
    void initialize(double newSampleRate, int newSamplesPerBlock, int newNumChannels);

    /**
     * Process a block of audio samples.
     * @param buffer The audio buffer to process
     * @param midiMessages The MIDI buffer (pass-through + playback in MIDI mode)
     * @param apvts The AudioProcessorValueTreeState for parameter access
     */
    void processBlock(juce::AudioBuffer<float>& buffer,
                     juce::MidiBuffer& midiMessages,
                     const juce::AudioProcessorValueTreeState& apvts);

    /**
     * Update host transport information for synchronization.
     * @param positionInfo The host's position information
     */
    void updateHostTransport(const juce::AudioPlayHead::PositionInfo& positionInfo);

    /**
     * Get access to individual components for UI updates.
     */
    const TransportController& getTransportController() const { return transportController; }
    TransportController& getTransportController() { return transportController; }
    const LoopBufferManager& getLoopBufferManager() const { return loopBufferManager; }
    LoopBufferManager& getLoopBufferManager() { return loopBufferManager; }
    const ParameterManager& getParameterManager() const { return parameterManager; }

    /**
     * Create the parameter layout for the AudioProcessorValueTreeState.
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /**
     * Check if the looper is initialized.
     */
    bool isInitialized() const { return initialized; }

    /**
     * Set MIDI mode (no audio processing, MIDI looping instead).
     * Called from prepareToPlay based on bus layout.
     */
    void setMidiMode(bool enabled) { midiMode = enabled; }
    bool isMidiMode() const { return midiMode; }

    /**
     * Get access to the MidiLooper for UI queries.
     */
    const MidiLooper& getMidiLooper() const { return midiLooper; }

    /**
     * Direct trigger methods for UI buttons (thread-safe, lock-free).
     * These set atomic flags consumed by the audio thread.
     */
    void triggerRecord() { pendingRecord.store(true, std::memory_order_release); }
    void triggerPlay()   { pendingPlay.store(true, std::memory_order_release); }
    void triggerStop()   { pendingStop.store(true, std::memory_order_release); }
    void triggerOverdub(){ pendingOverdub.store(true, std::memory_order_release); }
    void triggerOneLoopOverdub(bool waitForBegin, bool oneLoop) { 
        pendingWaitLoopBegin.store(waitForBegin, std::memory_order_release);
        pendingOneLoopRecord.store(oneLoop, std::memory_order_release);
        pendingOneLoopOverdub.store(true, std::memory_order_release); 
    }

    /**
     * Export the current loop as a WAV file.
     * Opens a file chooser dialog; call from UI thread only.
     */
    void exportLoop();

    /** True while waiting for loop start to begin 1-loop overdub. */
    bool isOneLoopOverdubArmed() const { return oneLoopOverdubArmed.load(std::memory_order_acquire); }

    /** Toggle debug logging on/off. */
    void setDebugEnabled(bool enabled) { debugEnabled.store(enabled, std::memory_order_release); }
    bool isDebugEnabled() const { return debugEnabled.load(std::memory_order_acquire); }

    /** Toggle monitor passthrough (input audio passes through to output). Off by default to prevent feedback. */
    void setMonitorPassthrough(bool enabled) { monitorPassthrough.store(enabled, std::memory_order_release); }
    bool isMonitorPassthrough() const { return monitorPassthrough.load(std::memory_order_acquire); }

    /**
     * Compute waveform peaks from the loop buffer for UI display.
     * Call from UI thread, not audio thread.
     */
    void updateWaveformDisplay();

    /**
     * Waveform display data. Written by audio thread, read by UI thread.
     * Uses a simple fixed array of peak values representing the loop content.
     */
    static constexpr int WAVEFORM_RESOLUTION = 200;
    float waveformPeaks[WAVEFORM_RESOLUTION] = {};
    std::atomic<int> waveformLength{0};  // how many entries are valid (0 = no loop)
    std::atomic<bool> waveformDirty{false}; // set true by audio thread when updated
    std::atomic<bool> waveformNeedsUpdate{false}; // signal for UI thread to compute peaks
    std::atomic<float> inputLevel{0.0f};   // current input peak level for UI metering

private:
    LoopBufferManager loopBufferManager;
    TransportController transportController;
    OverdubEngine overdubEngine;
    ParameterManager parameterManager;
    MidiLooper midiLooper;
    
    bool initialized{false};
    bool midiMode{false};
    double sampleRate{44100.0};
    int samplesPerBlock{512};
    int numChannels{2};
    
    // Atomic flags for UI→audio thread triggers
    std::atomic<bool> pendingRecord{false};
    std::atomic<bool> pendingPlay{false};
    std::atomic<bool> pendingStop{false};
    std::atomic<bool> pendingOverdub{false};
    std::atomic<bool> pendingOneLoopOverdub{false};
    std::atomic<bool> pendingWaitLoopBegin{false};
    std::atomic<bool> pendingOneLoopRecord{false};
    std::atomic<bool> oneLoopOverdubArmed{false};  // waiting for loop start
    std::atomic<bool> oneLoopOverdubActive{false}; // overdubbing for one loop
    std::atomic<bool> debugEnabled{false};
    std::atomic<bool> monitorPassthrough{false};
    
    // File chooser kept alive during async save
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Temporary buffers for processing
    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> loopBuffer;
    
    // Last host position info for MIDI looper
    juce::AudioPlayHead::PositionInfo lastPositionInfo;

    /**
     * Handle transport state changes based on parameter triggers.
     */
    void handleTransportControls();

    /**
     * Process audio based on current transport state.
     */
    void processAudioForCurrentState(juce::AudioBuffer<float>& buffer);

    /**
     * Check if buffer contains invalid data (NaN or Inf).
     */
    bool containsInvalidData(const juce::AudioBuffer<float>& buffer) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Looper)
};

} // namespace OpenLooper2