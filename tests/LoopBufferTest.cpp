#include "../plugin/include/OpenLooper2/CircularAudioBuffer.h"
#include "../plugin/include/OpenLooper2/LoopBufferManager.h"
#include "../plugin/include/OpenLooper2/TransportController.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <cassert>

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

// ========== CircularAudioBuffer Tests ==========

void testCircularAudioBuffer()
{
    std::cout << "=== CircularAudioBuffer ===" << std::endl;

    TEST("initializes with power-of-2 size")
        CircularAudioBuffer buf;
        buf.initialize(2, 1000);
        EXPECT(buf.getBufferSize() == 1024); // next power of 2
        EXPECT(buf.isInitialized());
        EXPECT(buf.getWritePosition() == 0);
    END_TEST

    TEST("write advances write head")
        CircularAudioBuffer buf;
        buf.initialize(1, 512);
        juce::AudioBuffer<float> input(1, 100);
        input.clear();
        buf.write(input, 0, 100);
        EXPECT(buf.getWritePosition() == 100);
        buf.write(input, 0, 100);
        EXPECT(buf.getWritePosition() == 200);
    END_TEST

    TEST("write and readAtPosition round-trip")
        CircularAudioBuffer buf;
        buf.initialize(2, 1024);

        juce::AudioBuffer<float> input(2, 256);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                input.setSample(ch, i, static_cast<float>(i + ch * 1000));

        buf.write(input, 0, 256);

        // Read back from position 0 (where we wrote)
        juce::AudioBuffer<float> output(2, 256);
        buf.readAtPosition(output, 0, 256, 0);

        bool match = true;
        for (int ch = 0; ch < 2 && match; ++ch)
            for (int i = 0; i < 256 && match; ++i)
                if (output.getSample(ch, i) != input.getSample(ch, i))
                    match = false;
        EXPECT(match);
    END_TEST

    TEST("writeAtPosition does not advance write head")
        CircularAudioBuffer buf;
        buf.initialize(1, 512);

        juce::AudioBuffer<float> data(1, 64);
        for (int i = 0; i < 64; ++i)
            data.setSample(0, i, 0.5f);

        buf.writeAtPosition(data, 0, 64, 100);
        EXPECT(buf.getWritePosition() == 0); // unchanged

        juce::AudioBuffer<float> output(1, 64);
        buf.readAtPosition(output, 0, 64, 100);
        EXPECT_NEAR(output.getSample(0, 0), 0.5f, 0.0001f);
        EXPECT_NEAR(output.getSample(0, 63), 0.5f, 0.0001f);
    END_TEST

    TEST("wraps around buffer boundary")
        CircularAudioBuffer buf;
        buf.initialize(1, 128); // actual size = 128 (already power of 2)

        // Write 100 samples to get write head near end
        juce::AudioBuffer<float> filler(1, 100);
        filler.clear();
        buf.write(filler, 0, 100);
        EXPECT(buf.getWritePosition() == 100);

        // Write 50 more — should wrap (100+50 = 150 > 128)
        juce::AudioBuffer<float> data(1, 50);
        for (int i = 0; i < 50; ++i)
            data.setSample(0, i, static_cast<float>(i + 1));
        buf.write(data, 0, 50);
        EXPECT(buf.getWritePosition() == (150 & 127)); // 22

        // Read back from position 100
        juce::AudioBuffer<float> output(1, 50);
        buf.readAtPosition(output, 0, 50, 100);
        EXPECT_NEAR(output.getSample(0, 0), 1.0f, 0.0001f);
        EXPECT_NEAR(output.getSample(0, 49), 50.0f, 0.0001f);
    END_TEST

    TEST("clear resets buffer and write head")
        CircularAudioBuffer buf;
        buf.initialize(1, 256);
        juce::AudioBuffer<float> data(1, 64);
        for (int i = 0; i < 64; ++i) data.setSample(0, i, 1.0f);
        buf.write(data, 0, 64);
        buf.clear();
        EXPECT(buf.getWritePosition() == 0);
        juce::AudioBuffer<float> output(1, 64);
        buf.readAtPosition(output, 0, 64, 0);
        EXPECT_NEAR(output.getSample(0, 0), 0.0f, 0.0001f);
    END_TEST
}

// ========== LoopBufferManager Tests ==========

void testLoopBufferManager()
{
    std::cout << std::endl << "=== LoopBufferManager ===" << std::endl;

    TEST("record and playback - constant signal")
        LoopBufferManager mgr;
        mgr.initialize(44100.0, 2, 60.0f);

        // Record 1024 samples of value 0.75
        juce::AudioBuffer<float> input(2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                input.setSample(ch, i, 0.75f);

        mgr.writeAudio(input, 0, 512);
        mgr.writeAudio(input, 0, 512);
        mgr.setLoopLength(1024);

        // Read back at position 0
        juce::AudioBuffer<float> output(2, 512);
        mgr.readAudio(output, 0, 512, 0);
        EXPECT_NEAR(output.getSample(0, 0), 0.75f, 0.0001f);
        EXPECT_NEAR(output.getSample(1, 511), 0.75f, 0.0001f);

        // Read at position 512
        mgr.readAudio(output, 0, 512, 512);
        EXPECT_NEAR(output.getSample(0, 0), 0.75f, 0.0001f);
    END_TEST

    TEST("record and playback - sine wave accuracy")
        LoopBufferManager mgr;
        mgr.initialize(44100.0, 1, 60.0f);

        const int loopLen = 4410; // 100ms at 44.1kHz
        const int blockSize = 512;
        const float freq = 440.0f;

        // Record sine wave
        int samplesWritten = 0;
        while (samplesWritten < loopLen)
        {
            int toWrite = juce::jmin(blockSize, loopLen - samplesWritten);
            juce::AudioBuffer<float> block(1, toWrite);
            for (int i = 0; i < toWrite; ++i)
            {
                float phase = 2.0f * juce::MathConstants<float>::pi * freq * (samplesWritten + i) / 44100.0f;
                block.setSample(0, i, 0.5f * std::sin(phase));
            }
            mgr.writeAudio(block, 0, toWrite);
            samplesWritten += toWrite;
        }
        mgr.setLoopLength(loopLen);

        // Verify playback matches
        int errors = 0;
        int samplesRead = 0;
        while (samplesRead < loopLen)
        {
            int toRead = juce::jmin(blockSize, loopLen - samplesRead);
            juce::AudioBuffer<float> block(1, toRead);
            mgr.readAudio(block, 0, toRead, samplesRead);
            for (int i = 0; i < toRead; ++i)
            {
                float phase = 2.0f * juce::MathConstants<float>::pi * freq * (samplesRead + i) / 44100.0f;
                float expected = 0.5f * std::sin(phase);
                if (std::abs(block.getSample(0, i) - expected) > 0.0001f)
                    errors++;
            }
            samplesRead += toRead;
        }
        EXPECT(errors == 0);
    END_TEST

    TEST("writeBackAudio modifies loop content")
        LoopBufferManager mgr;
        mgr.initialize(44100.0, 1, 60.0f);

        // Record silence
        juce::AudioBuffer<float> silence(1, 512);
        silence.clear();
        mgr.writeAudio(silence, 0, 512);
        mgr.setLoopLength(512);

        // Write back some data at position 100
        juce::AudioBuffer<float> overdub(1, 64);
        for (int i = 0; i < 64; ++i)
            overdub.setSample(0, i, 0.9f);
        mgr.writeBackAudio(overdub, 0, 64, 100);

        // Verify it's there
        juce::AudioBuffer<float> output(1, 64);
        mgr.readAudio(output, 0, 64, 100);
        EXPECT_NEAR(output.getSample(0, 0), 0.9f, 0.0001f);
        EXPECT_NEAR(output.getSample(0, 63), 0.9f, 0.0001f);

        // Verify other positions are still silent
        mgr.readAudio(output, 0, 64, 0);
        EXPECT_NEAR(output.getSample(0, 0), 0.0f, 0.0001f);
    END_TEST

    TEST("uninitialized read returns silence")
        LoopBufferManager mgr;
        juce::AudioBuffer<float> output(1, 64);
        for (int i = 0; i < 64; ++i) output.setSample(0, i, 1.0f);
        mgr.readAudio(output, 0, 64, 0);
        EXPECT_NEAR(output.getSample(0, 0), 0.0f, 0.0001f);
    END_TEST

    TEST("zero loop length returns silence")
        LoopBufferManager mgr;
        mgr.initialize(44100.0, 1, 60.0f);
        juce::AudioBuffer<float> output(1, 64);
        for (int i = 0; i < 64; ++i) output.setSample(0, i, 1.0f);
        mgr.readAudio(output, 0, 64, 0);
        EXPECT_NEAR(output.getSample(0, 0), 0.0f, 0.0001f);
    END_TEST
}

// ========== TransportController Tests ==========

void testTransportController()
{
    std::cout << std::endl << "=== TransportController ===" << std::endl;

    TEST("starts in Stopped state")
        TransportController tc;
        tc.initialize(44100.0, 512);
        EXPECT(tc.getCurrentState() == TransportController::State::Stopped);
        EXPECT(tc.getPlaybackPositionSamples() == 0);
    END_TEST

    TEST("Stopped -> Recording -> Playing")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        EXPECT(tc.getCurrentState() == TransportController::State::Recording);

        // Simulate recording 2 blocks
        tc.processBlock(512);
        tc.processBlock(512);
        EXPECT(tc.getPlaybackPositionSamples() == 1024);

        tc.stopRecording();
        EXPECT(tc.getCurrentState() == TransportController::State::Playing);
        EXPECT(tc.getLoopLength() == 1024);
        EXPECT(tc.getPlaybackPositionSamples() == 0); // reset after stop recording
    END_TEST

    TEST("Playing wraps position at loop length")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        tc.processBlock(1024);
        tc.stopRecording(); // loop length = 1024, state = Playing

        // Advance to just before wrap
        tc.processBlock(512);
        EXPECT(tc.getPlaybackPositionSamples() == 512);

        // Advance past wrap
        tc.processBlock(600);
        EXPECT(tc.getPlaybackPositionSamples() == (1112 % 1024)); // 88
    END_TEST

    TEST("Playing -> Overdubbing -> Playing")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        tc.processBlock(2048);
        tc.stopRecording();

        tc.startOverdub();
        EXPECT(tc.getCurrentState() == TransportController::State::Overdubbing);

        tc.processBlock(512);
        tc.stopOverdub();
        EXPECT(tc.getCurrentState() == TransportController::State::Playing);
    END_TEST

    TEST("stopPlayback resets to Stopped and position 0")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        tc.processBlock(1024);
        tc.stopRecording();
        tc.processBlock(512);

        tc.stopPlayback();
        EXPECT(tc.getCurrentState() == TransportController::State::Stopped);
        EXPECT(tc.getPlaybackPositionSamples() == 0);
    END_TEST

    TEST("Record→Stop→Play: stop during recording preserves loop length")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        tc.processBlock(512);
        tc.processBlock(512);
        // Simulate what Looper now does on Stop during Recording:
        tc.stopRecording(); // finalizes loop length, transitions to Playing
        EXPECT(tc.getLoopLength() == 1024);
        tc.stopPlayback();  // then stops
        EXPECT(tc.getCurrentState() == TransportController::State::Stopped);

        // Now Play should work because loop length is set
        tc.startPlayback();
        EXPECT(tc.getCurrentState() == TransportController::State::Playing);
    END_TEST

    TEST("startPlayback with no loop length does nothing")
        TransportController tc;
        tc.initialize(44100.0, 512);
        tc.startPlayback();
        EXPECT(tc.getCurrentState() == TransportController::State::Stopped);
    END_TEST

    TEST("startOverdub from Stopped does nothing")
        TransportController tc;
        tc.initialize(44100.0, 512);
        tc.startOverdub();
        EXPECT(tc.getCurrentState() == TransportController::State::Stopped);
    END_TEST

    TEST("normalized position is correct")
        TransportController tc;
        tc.initialize(44100.0, 512);

        tc.startRecording();
        tc.processBlock(1000);
        tc.stopRecording(); // loop length = 1000

        tc.processBlock(500);
        EXPECT_NEAR(tc.getPlaybackPosition(), 0.5f, 0.001f);
    END_TEST
}

// ========== Integration Test ==========

void testIntegration()
{
    std::cout << std::endl << "=== Integration: Record → Playback → Overdub ===" << std::endl;

    TEST("full cycle: record, play back, overdub adds content")
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        const int numChannels = 2;

        LoopBufferManager bufMgr;
        TransportController transport;

        bufMgr.initialize(sampleRate, numChannels, 60.0f);
        transport.initialize(sampleRate, blockSize);

        // --- Record phase: 4 blocks of 0.3 ---
        transport.startRecording();
        for (int b = 0; b < 4; ++b)
        {
            juce::AudioBuffer<float> block(numChannels, blockSize);
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    block.setSample(ch, i, 0.3f);
            bufMgr.writeAudio(block, 0, blockSize);
            transport.processBlock(blockSize);
        }
        transport.stopRecording();
        bufMgr.setLoopLength(transport.getLoopLength());

        const int loopLength = transport.getLoopLength();
        EXPECT(loopLength == 2048);

        // --- Playback phase: verify content ---
        transport.startPlayback();
        {
            juce::AudioBuffer<float> out(numChannels, blockSize);
            int pos = transport.getPlaybackPositionSamples();
            bufMgr.readAudio(out, 0, blockSize, pos % loopLength);
            EXPECT_NEAR(out.getSample(0, 0), 0.3f, 0.001f);
            EXPECT_NEAR(out.getSample(1, 255), 0.3f, 0.001f);
            transport.processBlock(blockSize);
        }

        // --- Overdub phase: add 0.2 to first block ---
        transport.startOverdub();
        {
            int pos = transport.getPlaybackPositionSamples();
            int loopPos = pos % loopLength;

            // Read existing
            juce::AudioBuffer<float> existing(numChannels, blockSize);
            bufMgr.readAudio(existing, 0, blockSize, loopPos);

            // New input
            juce::AudioBuffer<float> newInput(numChannels, blockSize);
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    newInput.setSample(ch, i, 0.2f);

            // Simple mix (feedback=1.0): existing + new
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    existing.setSample(ch, i, existing.getSample(ch, i) + newInput.getSample(ch, i));

            // Write back
            bufMgr.writeBackAudio(existing, 0, blockSize, loopPos);
            transport.processBlock(blockSize);
        }
        transport.stopOverdub();

        // --- Verify: re-read that section should now be ~0.5 ---
        {
            juce::AudioBuffer<float> verify(numChannels, blockSize);
            // The overdub happened at position 512 (after 1 playback block advanced)
            bufMgr.readAudio(verify, 0, blockSize, 512);
            EXPECT_NEAR(verify.getSample(0, 0), 0.5f, 0.001f);
            EXPECT_NEAR(verify.getSample(1, 100), 0.5f, 0.001f);
        }

        // --- Verify: un-overdubbed section still 0.3 ---
        {
            juce::AudioBuffer<float> verify(numChannels, blockSize);
            bufMgr.readAudio(verify, 0, blockSize, 1536); // last block
            EXPECT_NEAR(verify.getSample(0, 0), 0.3f, 0.001f);
        }
    END_TEST
}

// ========== Main ==========

int main()
{
    std::cout << "OpenLooper2 Unit Tests" << std::endl;
    std::cout << "=====================" << std::endl << std::endl;

    testCircularAudioBuffer();
    testLoopBufferManager();
    testTransportController();
    testIntegration();

    std::cout << std::endl << "=====================" << std::endl;
    std::cout << "Results: " << testsPassed << "/" << testsRun << " passed" << std::endl;

    return (testsPassed == testsRun) ? 0 : 1;
}
