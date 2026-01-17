#include "OpenLooper2/OverdubEngine.h"

namespace OpenLooper2 {

OverdubEngine::OverdubEngine()
{
}

OverdubEngine::~OverdubEngine()
{
}

void OverdubEngine::initialize(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    
    // Initialize DSP components
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2; // Stereo
    
    feedbackGain.prepare(spec);
    overdubGain.prepare(spec);
    
    // Set initial gain values
    feedbackGain.setGainLinear(currentFeedbackLevel.load(std::memory_order_acquire));
    overdubGain.setGainLinear(currentOverdubGain.load(std::memory_order_acquire));
    
    initialized.store(true, std::memory_order_release);
}

void OverdubEngine::processOverdub(juce::AudioBuffer<float>& buffer, 
                                  const juce::AudioBuffer<float>& input,
                                  float feedbackLevel)
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), input.getNumChannels());
    
    if (numSamples <= 0 || numChannels <= 0)
        return;
    
    // Update feedback level if changed
    setFeedbackLevel(feedbackLevel);
    updateGainParameters();
    
    // Create audio blocks for DSP processing
    juce::dsp::AudioBlock<float> bufferBlock(buffer);
    juce::dsp::AudioBlock<const float> inputBlock(input);
    
    // Apply feedback to existing buffer content
    feedbackGain.process(juce::dsp::ProcessContextReplacing<float>(bufferBlock));
    
    // Mix in the new input with overdub gain
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* inputData = input.getReadPointer(channel);
        float* bufferData = buffer.getWritePointer(channel);
        const float overdubGainValue = currentOverdubGain.load(std::memory_order_acquire);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            bufferData[sample] += inputData[sample] * overdubGainValue;
        }
    }
}

void OverdubEngine::setFeedbackLevel(float level)
{
    // Clamp to valid range
    const float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
    currentFeedbackLevel.store(clampedLevel, std::memory_order_release);
}

void OverdubEngine::setOverdubGain(float gain)
{
    // Clamp to valid range
    const float clampedGain = juce::jlimit(0.0f, 2.0f, gain);
    currentOverdubGain.store(clampedGain, std::memory_order_release);
}

void OverdubEngine::updateGainParameters()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const float feedbackLevel = currentFeedbackLevel.load(std::memory_order_acquire);
    feedbackGain.setGainLinear(feedbackLevel);
}

} // namespace OpenLooper2