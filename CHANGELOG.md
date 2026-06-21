# Changelog

## 2026-06-21 — Fix audio monitoring feedback issues in Ableton Live

### Problem
When adding the plugin to an Ableton Live channel with the track armed:
- Monitor OFF: Plugin's input meter showed no signal (Ableton doesn't route audio through the plugin chain unless transport is running)
- Monitor IN/Auto: Input meter worked but caused audio feedback loops

### Root Cause
1. **Stopped state** passed audio through unchanged (`buffer` left untouched), so live input was output directly — creating a feedback loop when Ableton's monitoring was set to Auto/In.
2. **Overdubbing state** output the mixed result (loop + live input) via `buffer.makeCopyOf(loopBuffer)`. With Auto monitoring, this live input in the output fed back through the mic.

### Fix
- **Stopped state**: Clear the output buffer instead of passing through. A looper has nothing to output when stopped.
- **Overdubbing state**: Output only the pre-existing loop content (re-read from buffer manager), not the mixed result containing live input. The overdub still writes the mix back to the loop buffer, so new input is captured — it's just not echoed back immediately.

### Usage
Set Ableton's track monitoring to **Auto**. The plugin now works without feedback in all states:
- Stopped: output silent, input meter active
- Recording: captures input, output silent
- Playing: outputs loop content only
- Overdubbing: captures input mixed into loop, outputs only existing loop content

### Files Modified
- `plugin/source/Looper.cpp` — `processAudioForCurrentState()` Stopped and Overdubbing cases
