#include "OpenLooper2/TransportController.h"

namespace OpenLooper2 {

TransportController::TransportController()
{
}

TransportController::~TransportController()
{
}

void TransportController::initialize(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    
    currentState.store(State::Stopped, std::memory_order_release);
    playbackPosition.store(0.0f, std::memory_order_release);
    playbackPositionSamples.store(0, std::memory_order_release);
    loopLengthSamples.store(0, std::memory_order_release);
    initialized.store(true, std::memory_order_release);
}

void TransportController::processBlock(int numSamples)
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const State state = currentState.load(std::memory_order_acquire);
    
    // Update position for playing and overdubbing states
    if (state == State::Playing || state == State::Overdubbing)
    {
        updatePosition(numSamples);
    }
    else if (state == State::Recording)
    {
        // During recording, we track the position to determine loop length
        updatePosition(numSamples);
    }
}

void TransportController::startRecording()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    resetPosition();
    currentState.store(State::Recording, std::memory_order_release);
}

void TransportController::stopRecording()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const State state = currentState.load(std::memory_order_acquire);
    if (state == State::Recording)
    {
        // Set the loop length based on the recorded duration
        const int recordedLength = playbackPositionSamples.load(std::memory_order_acquire);
        if (recordedLength > 0)
        {
            loopLengthSamples.store(recordedLength, std::memory_order_release);
            resetPosition();
            currentState.store(State::Playing, std::memory_order_release);
        }
        else
        {
            currentState.store(State::Stopped, std::memory_order_release);
        }
    }
}

void TransportController::startPlayback()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const int loopLength = loopLengthSamples.load(std::memory_order_acquire);
    if (loopLength > 0)
    {
        currentState.store(State::Playing, std::memory_order_release);
    }
}

void TransportController::stopPlayback()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    resetPosition();
    currentState.store(State::Stopped, std::memory_order_release);
}

void TransportController::startOverdub()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const int loopLength = loopLengthSamples.load(std::memory_order_acquire);
    if (loopLength > 0)
    {
        currentState.store(State::Overdubbing, std::memory_order_release);
    }
}

void TransportController::stopOverdub()
{
    if (!initialized.load(std::memory_order_acquire))
        return;
    
    const State state = currentState.load(std::memory_order_acquire);
    if (state == State::Overdubbing)
    {
        currentState.store(State::Playing, std::memory_order_release);
    }
}

void TransportController::setLoopLength(int lengthInSamples)
{
    if (lengthInSamples >= 0)
    {
        loopLengthSamples.store(lengthInSamples, std::memory_order_release);
    }
}

void TransportController::resetPosition()
{
    playbackPosition.store(0.0f, std::memory_order_release);
    playbackPositionSamples.store(0, std::memory_order_release);
}

void TransportController::updatePosition(int numSamples)
{
    const int currentPositionSamples = playbackPositionSamples.load(std::memory_order_acquire);
    const int loopLength = loopLengthSamples.load(std::memory_order_acquire);
    
    int newPositionSamples = currentPositionSamples + numSamples;
    
    // Handle loop wrapping for playing and overdubbing states
    const State state = currentState.load(std::memory_order_acquire);
    if ((state == State::Playing || state == State::Overdubbing) && loopLength > 0)
    {
        newPositionSamples = newPositionSamples % loopLength;
    }
    
    playbackPositionSamples.store(newPositionSamples, std::memory_order_release);
    
    // Calculate normalized position
    float normalizedPosition = 0.0f;
    if (loopLength > 0)
    {
        normalizedPosition = static_cast<float>(newPositionSamples) / static_cast<float>(loopLength);
    }
    
    playbackPosition.store(normalizedPosition, std::memory_order_release);
}

} // namespace OpenLooper2