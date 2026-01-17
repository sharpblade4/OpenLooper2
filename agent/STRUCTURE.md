# OpenLooper2 Project Architecture

## Project Overview
OpenLooper2 is a JUCE-based audio plugin designed as a looper effect for Ableton Live, specifically targeting features missing in the Lite version. The project is structured as a VST3 plugin with plans for future MIDI looping capabilities.

## Build System Architecture

### CMake Structure
- **Root CMakeLists.txt**: Main project configuration using C++23 standard
- **CPM Package Manager**: Manages JUCE dependency (v7.0.7) via GitHub
- **Plugin CMakeLists.txt**: Defines VST3 plugin configuration and linking

### Dependencies
- **JUCE Framework**: Core audio plugin framework
- **CPM**: C++ Package Manager for dependency management
- **Target Format**: VST3 only (configured for Ableton Live compatibility)

## JUCE Framework Design Patterns

### Core Components
JUCE follows a component-based architecture with clear separation of concerns:

#### 1. AudioProcessor (Backend)
- **Purpose**: Handles all audio processing logic
- **Key Methods**:
  - `processBlock()`: Real-time audio processing
  - `prepareToPlay()`: Initialize audio parameters
  - `getStateInformation()`/`setStateInformation()`: Plugin state persistence
- **Current State**: Basic template implementation, ready for looper logic

#### 2. AudioProcessorEditor (Frontend)
- **Purpose**: Manages user interface and visual components
- **Key Methods**:
  - `paint()`: Custom drawing and graphics
  - `resized()`: Layout management for UI components
- **Current State**: Simple placeholder UI with basic text display

### JUCE Audio Processing Pipeline
```
Host (Ableton) → AudioProcessor::processBlock() → Audio Buffer Manipulation → Output
                      ↑
              Parameter Changes & State Management
```

## Current Project Structure

```
open_looper2/
├── cmake/                  # Build system utilities
├── libs/                   # External dependencies
│   ├── cpm/               # CPM package manager
│   └── juce/              # JUCE framework (managed by CPM)
├── plugin/                # Main plugin implementation
│   ├── include/OpenLooper2/
│   │   ├── PluginProcessor.h    # Audio processing interface
│   │   └── PluginEditor.h       # UI interface
│   ├── source/
│   │   ├── PluginProcessor.cpp  # Audio processing implementation
│   │   └── PluginEditor.cpp     # UI implementation
│   └── CMakeLists.txt     # Plugin-specific build configuration
└── agent/                 # Documentation and analysis
```

## Plugin Configuration
- **Company**: RonU
- **Plugin Code**: OPLP
- **Manufacturer Code**: RONU
- **Format**: VST3
- **Audio**: Stereo input/output, no MIDI (yet)
- **Build Target**: `/build/plugin/OpenLooper2Plugin_artefacts/VST3`

## Development Status
The project is currently in **template stage** with:
- ✅ Basic JUCE plugin structure
- ✅ CMake build system
- ✅ VST3 configuration
- ⏳ Audio looping logic (not implemented)
- ⏳ UI controls for looper functionality
- ⏳ State management for loop recording/playback
- 🔮 Future: MIDI looping capabilities

## Next Development Steps
1. Implement audio buffer recording in `processBlock()`
2. Add playback logic with loop controls
3. Create UI controls (record, play, stop, overdub)
4. Implement parameter management for loop settings
5. Add state persistence for recorded loops