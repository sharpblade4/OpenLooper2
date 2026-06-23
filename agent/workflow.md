---
inclusion: always
---

# OpenLooper2 Development Workflow

## Build & Test Cycle

### Always Follow This Sequence:
1. Make code changes
2. Run unit tests first: `cmake --build build --target LoopBufferTest && ./build/tests/LoopBufferTest`
3. Build the VST3 plugin: `cmake --build build --target OpenLooper2Plugin_VST3`
   - **IMPORTANT**: Use `OpenLooper2Plugin_VST3` target, NOT `OpenLooper2Plugin` (which only builds the static library, not the actual .vst3 bundle)
4. Deploy to Ableton: `rm -rf ~/Documents/VST_For_Ableton/OpenLooper2.vst3 && cp -r build/plugin/OpenLooper2Plugin_artefacts/Debug/VST3/OpenLooper2.vst3 ~/Documents/VST_For_Ableton/`

### Testing Requirements:
- **ALWAYS add unit tests when fixing bugs** - Add tests to `tests/LoopBufferTest.cpp` that reproduce the bug and verify the fix
- Run tests before building the plugin to catch issues early
- Tests should be fast and focused on the specific functionality being tested

### Short Development Cycles:
- Build and test frequently to catch issues early
- Don't wait for user to run commands - execute them yourself
- Verify tests pass before deploying to Ableton
- Use debug logging (`DBG()` macro) when investigating issues

### Deployment Path:
- Plugin binary location: `build/plugin/OpenLooper2Plugin_artefacts/Debug/VST3/OpenLooper2.vst3`
- Ableton plugin folder: `~/Documents/VST_For_Ableton/`
- Always remove old version before copying new one to avoid file conflicts
- User must restart Ableton to load the new plugin version

### Viewing Debug Logs:
- JUCE `DBG()` outputs go to system console, not Ableton
- View in Console.app (macOS): Search for "OpenLooper" or specific log messages
- Or use Terminal: `log stream --predicate 'processImagePath contains "OpenLooper"' --level debug`

### Code Quality:
- Fix compiler warnings when they appear
- Use type-safe interfaces (avoid implicit conversions)
- Keep function signatures consistent with their implementations
- Document parameter types clearly in headers

## Project Structure:
- `plugin/source/` - Implementation files
- `plugin/include/OpenLooper2/` - Header files
- `tests/` - Unit tests
- `build/` - Build artifacts (not in git)
- `agent/` - Documentation and plans
