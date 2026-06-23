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
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;
    
    // Transport control buttons
    juce::TextButton recordButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton overdubButton;
    juce::TextButton exportButton;
    juce::TextButton oneLoopOverdubButton;
    
    // Sliders for continuous parameters
    juce::Slider feedbackSlider;
    juce::Slider volumeSlider;
    juce::Label feedbackLabel;
    juce::Label volumeLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    
    // Debug toggle
    juce::ToggleButton debugToggle{"DBG"};
    
    // Current state display
    juce::String currentStateText;
    float playbackPosition{0.0f};
    
    void updateButtonStates();
    juce::Colour getStateColour() const;
    void drawWaveformArea(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPianoRoll(juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
