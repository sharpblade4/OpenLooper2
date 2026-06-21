#include "OpenLooper2/OverdubEngine.h"

namespace OpenLooper2 {

OverdubEngine::OverdubEngine()
{
}

OverdubEngine::~OverdubEngine()
{
}

void OverdubEngine::initialize(double sr, int spb)
{
    this->sampleRate = sr;
    this->samplesPerBlock = spb;
    
    // Initialize DSP components
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(spb);
    spec.numChannels = 2; // Stereo
    
    feedbackGain.prepare(spec);
    overdubGain.prepare(spec);
    
    // Set initial gain values
    feedbackGain.setGainLinear(currentFeedbackLevel.load(std::memory_order_acquire));
    overdubGain.setGainLinear(currentOverdubGain.load(std::memory_order_acquire));
    
    // Initialize smoothing for parameter changes (20ms ramp time)
    const float rampTimeSeconds = 0.02f;
    feedbackSmoothing.reset(sr, rampTimeSeconds);
    gainSmoothing.reset(sr, rampTimeSeconds);
    
    feedbackSmoothing.setCurrentAndTargetValue(currentFeedbackLevel.load(std::memory_order_acquire));
    gainSmoothing.setCurrentAndTargetValue(currentOverdubGain.load(std::memory_order_acquire));
    
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
    
    // Apply smoothed feedback and mix in new input — advance smoothers once per sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float smoothedFeedback = feedbackSmoothing.getNextValue();
        const float smoothedGain = gainSmoothing.getNextValue();
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* bufferData = buffer.getWritePointer(channel);
            const float* inputData = input.getReadPointer(channel);
            
            bufferData[sample] = bufferData[sample] * smoothedFeedback + inputData[sample] * smoothedGain;
        }
    }
    
    // Apply gain staging to prevent clipping
    const float peakAfter = calculatePeakLevel(buffer);
    
    if (peakAfter > CLIPPING_THRESHOLD)
    {
        const float reductionFactor = CLIPPING_THRESHOLD / peakAfter;
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* bufferData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                bufferData[sample] = applySoftClipping(bufferData[sample] * reductionFactor);
            }
        }
    }
}

void OverdubEngine::setFeedbackLevel(float level)
{
    // Clamp to valid range
    const float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
    currentFeedbackLevel.store(clampedLevel, std::memory_order_release);
    
    // Update smoothed value target
    if (initialized.load(std::memory_order_acquire))
        feedbackSmoothing.setTargetValue(clampedLevel);
}

void OverdubEngine::setOverdubGain(float gain)
{
    // Clamp to valid range
    const float clampedGain = juce::jlimit(0.0f, 2.0f, gain);
    currentOverdubGain.store(clampedGain, std::memory_order_release);
    
    // Update smoothed value target
    if (initialized.load(std::memory_order_acquire))
        gainSmoothing.setTargetValue(clampedGain);
}

void OverdubEngine::updateGainParameters()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const float feedbackLevel = currentFeedbackLevel.load(std::memory_order_acquire);
    feedbackGain.setGainLinear(feedbackLevel);
}

float OverdubEngine::applySoftClipping(float value) const
{
    // Soft clipping using tanh-based algorithm
    // This provides smooth limiting without harsh artifacts
    
    if (std::abs(value) < SOFT_CLIP_KNEE)
    {
        // Below knee, pass through unchanged
        return value;
    }
    else
    {
        // Above knee, apply soft clipping
        const float sign = (value >= 0.0f) ? 1.0f : -1.0f;
        const float absValue = std::abs(value);
        
        // Smooth transition using tanh
        const float clipped = SOFT_CLIP_KNEE + (1.0f - SOFT_CLIP_KNEE) * std::tanh((absValue - SOFT_CLIP_KNEE) / (1.0f - SOFT_CLIP_KNEE));
        
        return sign * juce::jmin(clipped, 1.0f);
    }
}

float OverdubEngine::calculatePeakLevel(const juce::AudioBuffer<float>& buffer) const
{
    float peak = 0.0f;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float absValue = std::abs(data[sample]);
            if (absValue > peak)
                peak = absValue;
        }
    }
    
    return peak;
}

} // namespace OpenLooper2