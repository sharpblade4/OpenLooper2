#include "OpenLooper2/PluginProcessor.h"
#include "OpenLooper2/PluginEditor.h"
#include "OpenLooper2/Looper.h"
#include "OpenLooper2/TransportController.h"
#include "OpenLooper2/MidiLooper.h"
#include "OpenLooper2/MidiLoopBuffer.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(p), 
    processorRef(p)
{
    // Set up Record button
    recordButton.setButtonText("Record");
    recordButton.addListener(this);
    addAndMakeVisible(recordButton);
    
    // Set up Play button
    playButton.setButtonText("Play");
    playButton.addListener(this);
    addAndMakeVisible(playButton);
    
    // Set up Stop button
    stopButton.setButtonText("Stop");
    stopButton.addListener(this);
    addAndMakeVisible(stopButton);
    
    // Set up Overdub button
    overdubButton.setButtonText("Overdub");
    overdubButton.addListener(this);
    addAndMakeVisible(overdubButton);
    
    // Set up Export button
    exportButton.setButtonText("Export");
    exportButton.addListener(this);
    addAndMakeVisible(exportButton);
    
    // Set up 1-Loop Overdub button
    oneLoopOverdubButton.setButtonText("1-Loop OD");
    oneLoopOverdubButton.addListener(this);
    addAndMakeVisible(oneLoopOverdubButton);
    
    // Set up Feedback slider
    feedbackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(feedbackSlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "feedback", feedbackSlider);
    
    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    feedbackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(feedbackLabel);
    
    // Set up Volume slider
    volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(volumeSlider);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "volume", volumeSlider);
    
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(volumeLabel);
    
    // Start timer for UI updates (30 Hz)
    startTimerHz(30);
    
    // Debug toggle
    debugToggle.onClick = [this]() {
        if (processorRef.looper)
            processorRef.looper->setDebugEnabled(debugToggle.getToggleState());
    };
    addAndMakeVisible(debugToggle);
    
    setSize(600, 440);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
    recordButton.removeListener(this);
    playButton.removeListener(this);
    stopButton.removeListener(this);
    overdubButton.removeListener(this);
    exportButton.removeListener(this);
    oneLoopOverdubButton.removeListener(this);
}

void AudioPluginAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (!processorRef.looper || !processorRef.looper->isInitialized())
        return;
    
    if (button == &recordButton)
        processorRef.looper->triggerRecord();
    else if (button == &playButton)
        processorRef.looper->triggerPlay();
    else if (button == &stopButton)
        processorRef.looper->triggerStop();
    else if (button == &overdubButton)
        processorRef.looper->triggerOverdub();
    else if (button == &exportButton)
        processorRef.looper->exportLoop();
    else if (button == &oneLoopOverdubButton)
        processorRef.looper->triggerOneLoopOverdub();
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff2a2a2a));
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("OpenLooper2", 0, 10, getWidth(), 40, juce::Justification::centred);
    
    // MIDI mode indicator
    if (processorRef.looper && processorRef.looper->isMidiMode())
    {
        g.setColour(juce::Colours::cyan);
        g.setFont(12.0f);
        g.drawText("MIDI", 20, 15, 50, 20, juce::Justification::centredLeft);
    }
    
    // State indicator
    const auto stateColour = getStateColour();
    g.setColour(stateColour);
    g.fillRoundedRectangle(getWidth() / 2.0f - 100, 60, 200, 40, 5.0f);
    
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText(currentStateText, getWidth() / 2 - 100, 60, 200, 40, juce::Justification::centred);
    
    // Input level meter (shows if plugin is receiving audio)
    if (processorRef.looper && processorRef.looper->isInitialized())
    {
        float level = processorRef.looper->inputLevel.load(std::memory_order_acquire);
        // Draw a small horizontal bar
        auto meterBounds = juce::Rectangle<float>(20.0f, 105.0f, 200.0f, 12.0f);
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(meterBounds);
        // Fill based on level
        float fillWidth = juce::jmin(level, 1.0f) * meterBounds.getWidth();
        g.setColour(level > 0.01f ? juce::Colours::limegreen : juce::Colours::darkgrey);
        g.fillRect(meterBounds.getX(), meterBounds.getY(), fillWidth, meterBounds.getHeight());
        // Label
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(10.0f);
        g.drawText("IN: " + juce::String(level, 3), meterBounds.translated(205.0f, 0.0f).withWidth(80.0f), 
                   juce::Justification::centredLeft);
    }
    
    // Waveform visualization area - now at the bottom with more space
    auto waveformBounds = juce::Rectangle<int>(20, 300, getWidth() - 40, 120);
    drawWaveformArea(g, waveformBounds);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Debug toggle in top-right
    debugToggle.setBounds(bounds.getWidth() - 70, 10, 60, 24);
    
    bounds.removeFromTop(120); // Space for title and state indicator
    
    // Transport buttons area
    auto buttonArea = bounds.removeFromTop(80);
    buttonArea.reduce(20, 10);
    
    const int buttonWidth = (buttonArea.getWidth() - 50) / 6;
    recordButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(10);
    playButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(10);
    stopButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(10);
    overdubButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(10);
    oneLoopOverdubButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(10);
    exportButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    
    // Sliders area
    bounds.removeFromTop(10);
    auto sliderArea = bounds.removeFromTop(100);
    sliderArea.reduce(40, 0);
    
    const int sliderWidth = sliderArea.getWidth() / 2 - 20;
    
    auto feedbackArea = sliderArea.removeFromLeft(sliderWidth);
    feedbackLabel.setBounds(feedbackArea.removeFromTop(20));
    feedbackSlider.setBounds(feedbackArea);
    
    sliderArea.removeFromLeft(40);
    
    auto volumeArea = sliderArea.removeFromLeft(sliderWidth);
    volumeLabel.setBounds(volumeArea.removeFromTop(20));
    volumeSlider.setBounds(volumeArea);
    
    // Waveform area is now at the bottom with more space
    // (will be drawn in paint method)
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    // Check if waveform needs recomputing (deferred from audio thread)
    if (processorRef.looper && processorRef.looper->waveformNeedsUpdate.exchange(false, std::memory_order_acq_rel))
    {
        processorRef.looper->updateWaveformDisplay();
    }
    
    updateButtonStates();
    repaint();
}

void AudioPluginAudioProcessorEditor::updateButtonStates()
{
    // Get current looper state
    if (processorRef.looper && processorRef.looper->isInitialized())
    {
        const auto& transport = processorRef.looper->getTransportController();
        const auto state = transport.getCurrentState();
        
        // Update playback position
        playbackPosition = transport.getPlaybackPosition();
        
        // Update button toggle states to show which is active
        recordButton.setToggleState(state == OpenLooper2::TransportController::State::Recording, 
                                    juce::dontSendNotification);
        playButton.setToggleState(state == OpenLooper2::TransportController::State::Playing, 
                                  juce::dontSendNotification);
        overdubButton.setToggleState(state == OpenLooper2::TransportController::State::Overdubbing, 
                                     juce::dontSendNotification);
        stopButton.setToggleState(state == OpenLooper2::TransportController::State::Stopped, 
                                  juce::dontSendNotification);
        oneLoopOverdubButton.setToggleState(processorRef.looper->isOneLoopOverdubArmed(),
                                            juce::dontSendNotification);
        
        switch (state)
        {
            case OpenLooper2::TransportController::State::Stopped:
                if (processorRef.looper->isMidiMode() && processorRef.looper->getMidiLooper().isArmed())
                    currentStateText = "Armed";
                else
                    currentStateText = "Stopped";
                break;
            case OpenLooper2::TransportController::State::Recording:
                currentStateText = "Recording";
                break;
            case OpenLooper2::TransportController::State::Playing:
                currentStateText = "Playing";
                break;
            case OpenLooper2::TransportController::State::Overdubbing:
                currentStateText = "Overdubbing";
                break;
        }
    }
    else
    {
        currentStateText = "Initializing...";
        playbackPosition = 0.0f;
    }
}

juce::Colour AudioPluginAudioProcessorEditor::getStateColour() const
{
    if (currentStateText == "Recording")
        return juce::Colours::red;
    else if (currentStateText == "Playing")
        return juce::Colours::green;
    else if (currentStateText == "Overdubbing")
        return juce::Colours::orange;
    else if (currentStateText == "Armed")
        return juce::Colours::yellow;
    else if (currentStateText == "Stopped")
        return juce::Colours::grey;
    else
        return juce::Colours::darkgrey;
}

void AudioPluginAudioProcessorEditor::drawWaveformArea(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // In MIDI mode, draw piano roll instead
    if (processorRef.looper && processorRef.looper->isMidiMode())
    {
        drawPianoRoll(g, bounds);
        return;
    }
    // Draw background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    
    // Draw border
    g.setColour(juce::Colour(0xff404040));
    g.drawRoundedRectangle(bounds.toFloat(), 5.0f, 2.0f);
    
    // Draw center line
    g.setColour(juce::Colour(0xff303030));
    const float centerY = static_cast<float>(bounds.getCentreY());
    g.drawLine(static_cast<float>(bounds.getX()), centerY, 
               static_cast<float>(bounds.getRight()), centerY, 1.0f);
    
    // Draw actual recorded waveform
    if (processorRef.looper && processorRef.looper->isInitialized())
    {
        const int numBins = processorRef.looper->waveformLength.load(std::memory_order_acquire);
        
        if (numBins > 0)
        {
            g.setColour(getStateColour().withAlpha(0.7f));
            
            const float width = static_cast<float>(bounds.getWidth());
            const float halfHeight = static_cast<float>(bounds.getHeight()) * 0.45f;
            
            for (int i = 0; i < numBins; ++i)
            {
                const float x = static_cast<float>(bounds.getX()) + (static_cast<float>(i) / static_cast<float>(numBins)) * width;
                const float barWidth = width / static_cast<float>(numBins);
                const float peak = processorRef.looper->waveformPeaks[i];
                const float barHeight = peak * halfHeight;
                
                if (barHeight > 0.5f)
                {
                    g.fillRect(x, centerY - barHeight, barWidth * 0.8f, barHeight * 2.0f);
                }
            }
        }
        else
        {
            // No loop recorded — show hint
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.setFont(14.0f);
            g.drawText("No loop recorded", bounds, juce::Justification::centred);
        }
    }
    
    // Draw playback position indicator
    if (processorRef.looper && processorRef.looper->isInitialized())
    {
        const auto& transport = processorRef.looper->getTransportController();
        const auto state = transport.getCurrentState();
        
        if (state != OpenLooper2::TransportController::State::Stopped && playbackPosition > 0.0f)
        {
            const float posX = static_cast<float>(bounds.getX()) + playbackPosition * static_cast<float>(bounds.getWidth());
            
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawLine(posX, static_cast<float>(bounds.getY()), 
                       posX, static_cast<float>(bounds.getBottom()), 2.0f);
        }
    }
    
    // State label
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(11.0f);
    if (processorRef.looper && processorRef.looper->isInitialized())
    {
        const int loopLen = processorRef.looper->getTransportController().getLoopLength();
        if (loopLen > 0)
        {
            const float seconds = static_cast<float>(loopLen) / 44100.0f;
            g.drawText(juce::String(seconds, 2) + "s", bounds.reduced(5), juce::Justification::topRight);
        }
    }
}

void AudioPluginAudioProcessorEditor::drawPianoRoll(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff404040));
    g.drawRoundedRectangle(bounds.toFloat(), 5.0f, 2.0f);

    if (!processorRef.looper || !processorRef.looper->isInitialized())
        return;

    const auto& midiLooper = processorRef.looper->getMidiLooper();
    const auto& loopBuffer = midiLooper.getLoopBuffer();
    const auto& events = loopBuffer.getEvents();
    const double loopLen = loopBuffer.getLoopLengthBeats();

    // Show armed status
    if (midiLooper.isArmed())
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.7f));
        g.setFont(14.0f);
        g.drawText("Armed - start transport", bounds, juce::Justification::centred);
        return;
    }

    if (events.empty() || loopLen <= 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(14.0f);
        g.drawText("MIDI - no loop recorded", bounds, juce::Justification::centred);
        return;
    }

    // Find note range for vertical scaling
    int minNote = 127, maxNote = 0;
    for (const auto& event : events)
    {
        if (event.message.isNoteOn())
        {
            minNote = juce::jmin(minNote, event.message.getNoteNumber());
            maxNote = juce::jmax(maxNote, event.message.getNoteNumber());
        }
    }

    if (minNote > maxNote) return;

    // Add padding to note range
    minNote = juce::jmax(0, minNote - 2);
    maxNote = juce::jmin(127, maxNote + 2);
    const int noteRange = maxNote - minNote + 1;

    const float w = static_cast<float>(bounds.getWidth());
    const float h = static_cast<float>(bounds.getHeight());
    const float noteHeight = h / static_cast<float>(noteRange);

    // Draw horizontal grid lines (semitones)
    g.setColour(juce::Colour(0xff252525));
    for (int n = minNote; n <= maxNote; ++n)
    {
        float y = bounds.getY() + h - (static_cast<float>(n - minNote + 1) * noteHeight);
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    }

    // Draw note blocks
    g.setColour(juce::Colours::cyan.withAlpha(0.8f));
    const float minNoteWidth = 4.0f;

    for (const auto& event : events)
    {
        if (!event.message.isNoteOn()) continue;

        float x = bounds.getX() + static_cast<float>(event.ppqPosition / loopLen) * w;
        float y = bounds.getY() + h - (static_cast<float>(event.message.getNoteNumber() - minNote + 1) * noteHeight);

        // Find matching note-off to determine width
        float noteW = minNoteWidth;
        for (const auto& offEvent : events)
        {
            if (offEvent.message.isNoteOff() &&
                offEvent.message.getNoteNumber() == event.message.getNoteNumber() &&
                offEvent.ppqPosition > event.ppqPosition)
            {
                noteW = juce::jmax(minNoteWidth,
                    static_cast<float>((offEvent.ppqPosition - event.ppqPosition) / loopLen) * w);
                break;
            }
        }

        // Velocity as brightness
        float alpha = 0.4f + 0.6f * (event.message.getVelocity() / 127.0f);
        g.setColour(juce::Colours::cyan.withAlpha(alpha));
        g.fillRect(x, y, noteW, juce::jmax(2.0f, noteHeight - 1.0f));
    }

    // Playback cursor
    const auto& transport = processorRef.looper->getTransportController();
    if (transport.getCurrentState() != OpenLooper2::TransportController::State::Stopped)
    {
        float posX = bounds.getX() + playbackPosition * w;
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawLine(posX, static_cast<float>(bounds.getY()),
                   posX, static_cast<float>(bounds.getBottom()), 2.0f);
    }

    // Loop length label
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(11.0f);
    g.drawText(juce::String(loopLen, 1) + " beats", bounds.reduced(5), juce::Justification::topRight);
}
