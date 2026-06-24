#include "../plugin/include/OpenLooper2/Looper.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>

using namespace OpenLooper2;

static int testsRun = 0;
static int testsPassed = 0;

#define TEST(name) \
    { \
        testsRun++; \
        std::cout << "  " << name << "... "; \
        bool _passed = true; \
        try {

#define END_TEST \
        } catch (const std::exception& e) { \
            std::cout << "EXCEPTION: " << e.what(); \
            _passed = false; \
        } \
        if (_passed) { testsPassed++; std::cout << "✓" << std::endl; } \
    }

#define EXPECT(cond) \
    if (!(cond)) { \
        std::cout << "✗ FAILED: " #cond << " (line " << __LINE__ << ")" << std::endl; \
        _passed = false; \
    }

#define EXPECT_NEAR(a, b, tol) \
    if (std::abs((a) - (b)) > (tol)) { \
        std::cout << "✗ FAILED: " #a " ≈ " #b " (got " << (a) << " vs " << (b) << ", line " << __LINE__ << ")" << std::endl; \
        _passed = false; \
    }

// Helper: create a minimal APVTS for testing
static std::unique_ptr<juce::AudioProcessor> createDummyProcessor()
{
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor() : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };
    return std::make_unique<DummyProcessor>();
}

void testOverdubNoFeedback()
{
    std::cout << "=== Overdub No-Feedback Tests ===" << std::endl;

    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int numChannels = 2;

    TEST("overdub output does not contain live input signal")
        // Set up looper
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);

        // Create APVTS
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "Parameters",
            Looper::createParameterLayout());
        juce::MidiBuffer midi;

        // Step 1: Record a loop (4 blocks of silence to create a known loop)
        looper.triggerRecord();
        for (int i = 0; i < 4; ++i)
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear(); // record silence
            looper.processBlock(buf, midi, apvts);
        }

        // Step 2: Stop recording → transitions to Playing
        looper.triggerRecord(); // second press stops recording
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Step 3: Start overdubbing
        looper.triggerOverdub();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts); // consume the trigger
        }

        // Step 4: Feed a known signal as live input during overdub
        const float inputSignalValue = 0.75f;
        juce::AudioBuffer<float> inputBuf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                inputBuf.setSample(ch, s, inputSignalValue);

        looper.processBlock(inputBuf, midi, apvts);

        // Verify: output should NOT contain the live input signal.
        // The loop was recorded as silence, so output should be ~0.
        float outputPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                outputPeak = std::max(outputPeak, std::abs(inputBuf.getSample(ch, s)));

        EXPECT(outputPeak < 0.01f); // output should be near-silent (original loop was silence)
    END_TEST

    TEST("overdub writes input into loop buffer for next cycle")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "Parameters",
            Looper::createParameterLayout());
        juce::MidiBuffer midi;

        // Record 2 blocks of silence
        looper.triggerRecord();
        for (int i = 0; i < 2; ++i)
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Stop recording
        looper.triggerRecord();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Start overdub
        looper.triggerOverdub();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Overdub with a known signal for one block
        const float inputVal = 0.5f;
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            for (int ch = 0; ch < numChannels; ++ch)
                for (int s = 0; s < blockSize; ++s)
                    buf.setSample(ch, s, inputVal);
            looper.processBlock(buf, midi, apvts);
        }

        // Stop overdub → back to Playing
        looper.triggerOverdub();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Now in Playing state, loop through until we hit the block we overdubbed.
        // The loop is 2 blocks long. We overdubbed into the 2nd block (position depends
        // on transport advancement). Let's play through the full loop and check we get
        // non-zero audio somewhere.
        float maxPlayback = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
            for (int ch = 0; ch < numChannels; ++ch)
                for (int s = 0; s < blockSize; ++s)
                    maxPlayback = std::max(maxPlayback, std::abs(buf.getSample(ch, s)));
        }

        EXPECT(maxPlayback > 0.1f); // overdubbed content should be audible on playback
    END_TEST

    TEST("stopped state outputs silence")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "Parameters",
            Looper::createParameterLayout());
        juce::MidiBuffer midi;

        // Feed audio into stopped looper
        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.9f);

        looper.processBlock(buf, midi, apvts);

        // Output must be silent (no pass-through)
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(buf.getSample(ch, s)));

        EXPECT(peak < 0.001f);
    END_TEST
}

void testStateMachine()
{
    std::cout << "=== State Machine Tests ===" << std::endl;

    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int numChannels = 2;
    juce::MidiBuffer midi;

    // Helper lambda to process one block
    auto tick = [&](Looper& looper, juce::AudioProcessorValueTreeState& apvts, float fillVal = 0.0f) {
        juce::AudioBuffer<float> buf(numChannels, blockSize);
        if (fillVal != 0.0f)
            for (int ch = 0; ch < numChannels; ++ch)
                for (int s = 0; s < blockSize; ++s)
                    buf.setSample(ch, s, fillVal);
        else
            buf.clear();
        juce::MidiBuffer midi;
        looper.processBlock(buf, midi, apvts);
        return buf;
    };

    TEST("initial state is Stopped")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Stopped);
    END_TEST

    TEST("Record trigger: Stopped → Recording")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Recording);
    END_TEST

    TEST("Record trigger twice: Recording → Playing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        tick(looper, apvts); // record some audio
        looper.triggerRecord();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Playing);
        EXPECT(looper.getTransportController().getLoopLength() > 0);
    END_TEST

    TEST("Stop during Recording: Recording → Stopped with loop preserved")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        tick(looper, apvts);
        looper.triggerStop();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Stopped);
        EXPECT(looper.getTransportController().getLoopLength() > 0);
    END_TEST

    TEST("Play after Stop-during-Recording: Stopped → Playing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts, 0.5f);
        tick(looper, apvts, 0.5f);
        looper.triggerStop();
        tick(looper, apvts);
        looper.triggerPlay();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Playing);
    END_TEST

    TEST("Play during Recording: Recording → Playing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        tick(looper, apvts);
        looper.triggerPlay();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Playing);
        EXPECT(looper.getTransportController().getLoopLength() > 0);
    END_TEST

    TEST("Record from Playing: clears and starts new recording")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        // Record a loop
        looper.triggerRecord();
        tick(looper, apvts);
        looper.triggerRecord();
        tick(looper, apvts); // now Playing
        // Record again from Playing
        looper.triggerRecord();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Recording);
        EXPECT(looper.getLoopBufferManager().getLoopLength() == 0); // cleared
    END_TEST

    TEST("Overdub from Playing: Playing → Overdubbing → Playing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        looper.triggerRecord();
        tick(looper, apvts); // Playing
        looper.triggerOverdub();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Overdubbing);
        looper.triggerOverdub();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Playing);
    END_TEST

    TEST("Overdub from Stopped with loop: Stopped → Overdubbing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerRecord();
        tick(looper, apvts);
        looper.triggerStop();
        tick(looper, apvts); // Stopped with loop
        looper.triggerOverdub();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Overdubbing);
    END_TEST

    TEST("Play with no loop does nothing")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        looper.triggerPlay();
        tick(looper, apvts);
        EXPECT(looper.getTransportController().getCurrentState() == TransportController::State::Stopped);
    END_TEST

    TEST("Record→Stop→Play outputs recorded audio")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());
        // Record 2 blocks of 0.6
        looper.triggerRecord();
        tick(looper, apvts, 0.6f);
        tick(looper, apvts, 0.6f);
        looper.triggerStop();
        tick(looper, apvts);
        // Now play
        looper.triggerPlay();
        tick(looper, apvts); // consume trigger
        juce::AudioBuffer<float> out(numChannels, blockSize);
        out.clear();
        looper.processBlock(out, midi, apvts);
        // Should hear the recorded signal
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(out.getSample(ch, s)));
        EXPECT(peak > 0.3f);
    END_TEST
}

void testMonitorPassthrough()
{
    std::cout << "=== Monitor Passthrough Tests ===" << std::endl;

    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int numChannels = 2;
    juce::MidiBuffer midi;

    TEST("monitor OFF: stopped state outputs silence")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        looper.setMonitorPassthrough(false);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());

        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.8f);

        looper.processBlock(buf, midi, apvts);

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(buf.getSample(ch, s)));
        EXPECT(peak < 0.001f);
    END_TEST

    TEST("monitor ON: stopped state passes through input")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        looper.setMonitorPassthrough(true);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());

        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.8f);

        looper.processBlock(buf, midi, apvts);

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(buf.getSample(ch, s)));
        EXPECT_NEAR(peak, 0.8f, 0.01f);
    END_TEST

    TEST("monitor ON: playing state mixes loop with input")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        looper.setMonitorPassthrough(true);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());

        // Record a loop with signal 0.4
        looper.triggerRecord();
        for (int i = 0; i < 2; ++i)
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            for (int ch = 0; ch < numChannels; ++ch)
                for (int s = 0; s < blockSize; ++s)
                    buf.setSample(ch, s, 0.4f);
            looper.processBlock(buf, midi, apvts);
        }
        looper.triggerRecord();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Now playing — feed input of 0.3, expect output ≈ 0.3 + 0.4 = 0.7
        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.3f);
        looper.processBlock(buf, midi, apvts);

        // Check output is greater than either input or loop alone
        float minVal = 1.0f;
        for (int s = 0; s < blockSize; ++s)
            minVal = std::min(minVal, buf.getSample(0, s));
        EXPECT(minVal > 0.5f); // must be more than just the input (0.3) or loop (0.4) alone
    END_TEST

    TEST("monitor ON: recording state passes through input")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        looper.setMonitorPassthrough(true);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());

        looper.triggerRecord();
        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.6f);
        looper.processBlock(buf, midi, apvts);

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(buf.getSample(ch, s)));
        EXPECT_NEAR(peak, 0.6f, 0.01f);
    END_TEST

    TEST("monitor OFF: overdub output does not contain live input")
        Looper looper;
        looper.initialize(sampleRate, blockSize, numChannels);
        looper.setMonitorPassthrough(false);

        auto proc = createDummyProcessor();
        juce::AudioProcessorValueTreeState apvts(*proc, nullptr, "P", Looper::createParameterLayout());

        // Record silence
        looper.triggerRecord();
        for (int i = 0; i < 4; ++i)
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }
        looper.triggerRecord();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        // Overdub with loud signal
        looper.triggerOverdub();
        {
            juce::AudioBuffer<float> buf(numChannels, blockSize);
            buf.clear();
            looper.processBlock(buf, midi, apvts);
        }

        juce::AudioBuffer<float> buf(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buf.setSample(ch, s, 0.75f);
        looper.processBlock(buf, midi, apvts);

        // Output should be near-silent (loop was silence, no passthrough)
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < blockSize; ++s)
                peak = std::max(peak, std::abs(buf.getSample(ch, s)));
        EXPECT(peak < 0.01f);
    END_TEST
}

int main()
{
    std::cout << "\n=== OpenLooper2 Looper Tests ===\n" << std::endl;

    testStateMachine();
    testOverdubNoFeedback();
    testMonitorPassthrough();

    std::cout << "\n--- Results: " << testsPassed << "/" << testsRun << " passed ---" << std::endl;
    return (testsPassed == testsRun) ? 0 : 1;
}
