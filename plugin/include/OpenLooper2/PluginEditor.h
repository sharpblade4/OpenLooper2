#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>


//==============================================================================
class AudioPluginAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::Timer,
                                         private juce::Button::Listener
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void buttonClicked (juce::Button* button) override;

private:
    AudioPluginAudioProcessor& processorRef;
    
    // Main control: context-sensitive Record/Overdub
    juce::TextButton recordOverdubButton;
    
    // Overdub modifier checkboxes
    juce::ToggleButton waitLoopBeginToggle{"Wait Loop Begin"};
    juce::ToggleButton oneLoopRecordToggle{"One Loop Record"};
    
    // Secondary transport buttons
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton exportButton;
    
    // Sliders for continuous parameters
    juce::Slider feedbackSlider;
    juce::Slider volumeSlider;
    juce::Label feedbackLabel;
    juce::Label volumeLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    
    // Debug toggle
    juce::ToggleButton debugToggle{"DBG"};
    
    // Monitor passthrough toggle
    juce::ToggleButton monitorToggle{"Monitor"};
    
    // Current state display
    juce::String currentStateText;
    float playbackPosition{0.0f};
    
    void updateButtonStates();
    juce::Colour getStateColour() const;
    void drawWaveformArea(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPianoRoll(juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
