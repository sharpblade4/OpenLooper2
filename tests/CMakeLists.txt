cmake_minimum_required(VERSION 3.22)

# Test executable
add_executable(LoopBufferTest
    LoopBufferTest.cpp
    ../plugin/source/LoopBufferManager.cpp
    ../plugin/source/CircularAudioBuffer.cpp
    ../plugin/source/TransportController.cpp
)

target_include_directories(LoopBufferTest PRIVATE
    ../plugin/include
)

target_link_libraries(LoopBufferTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

# Looper integration test (includes overdub feedback test)
add_executable(LooperTest
    LooperTest.cpp
    ../plugin/source/Looper.cpp
    ../plugin/source/LoopBufferManager.cpp
    ../plugin/source/CircularAudioBuffer.cpp
    ../plugin/source/TransportController.cpp
    ../plugin/source/OverdubEngine.cpp
    ../plugin/source/ParameterManager.cpp
    ../plugin/source/ActiveNoteTracker.cpp
    ../plugin/source/MidiLoopBuffer.cpp
    ../plugin/source/MidiLooper.cpp
)

target_include_directories(LooperTest PRIVATE
    ../plugin/include
)

target_link_libraries(LooperTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_dsp
)

add_custom_command(
    TARGET LooperTest
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Running looper tests..."
    COMMAND $<TARGET_FILE:LooperTest>
    COMMAND ${CMAKE_COMMAND} -E echo "Looper tests completed"
    COMMENT "Running looper unit tests"
    VERBATIM
)

add_test(NAME LooperTest COMMAND LooperTest)

# Run test automatically after building
add_custom_command(
    TARGET LoopBufferTest
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Running tests..."
    COMMAND $<TARGET_FILE:LoopBufferTest>
    COMMAND ${CMAKE_COMMAND} -E echo "Tests completed"
    COMMENT "Running unit tests"
    VERBATIM
)

# Enable testing for CTest integration
enable_testing()
add_test(NAME LoopBufferTest COMMAND LoopBufferTest)
