# Lag Mitigation Testing Guide

## Overview
This document describes how to test the lag-induced multiple key input mitigation feature added to the keyboard filter driver.

## Test Scenarios

### 1. Normal Typing
**Objective**: Verify that normal typing is not affected by the lag mitigation feature.
**Steps**:
1. Install and load the modified driver
2. Type normally at various speeds
3. Verify all expected keystrokes are registered
4. Check that no legitimate keystrokes are filtered out

**Expected Result**: All legitimate keystrokes should pass through normally.

### 2. Rapid Key Presses (Below Threshold)
**Objective**: Test that rapid legitimate key presses are preserved.
**Steps**:
1. Press the same key rapidly (faster than 300ms intervals)
2. Verify all key presses are registered when intended
3. Test with different keys (letters, numbers, special keys)

**Expected Result**: All intentional rapid key presses should be preserved.

### 3. Lag-Induced Duplicates (Above Threshold)
**Objective**: Verify that lag-induced duplicate key presses are filtered.
**Steps**:
1. Simulate system lag conditions
2. Press a key once during lag
3. Verify that duplicate key events within 300ms are filtered
4. Ensure only the first key press is registered

**Expected Result**: Duplicate key presses within 300ms should be filtered out.

### 4. Mixed Key Sequences
**Objective**: Test filtering with sequences of different keys.
**Steps**:
1. Type sequences like "aabbcc" where each letter is pressed twice quickly
2. Verify that only one instance of each letter is registered
3. Test with special keys and combinations

**Expected Result**: Only the first occurrence of each key within the threshold should be registered.

### 5. Key Up/Down Events
**Objective**: Verify that key release events are not filtered.
**Steps**:
1. Test that key-up events always pass through
2. Verify that only key-down events are subject to filtering
3. Test key combinations and modifiers

**Expected Result**: All key-up events should pass through unfiltered.

### 6. Memory Allocation Failure
**Objective**: Test graceful degradation when memory allocation fails.
**Steps**:
1. Simulate low memory conditions
2. Verify that the driver falls back to unfiltered mode
3. Ensure no system instability occurs

**Expected Result**: Driver should gracefully fallback to passing all inputs unfiltered.

## Configuration

### Adjustable Parameters
- **LAG_MITIGATION_THRESHOLD_MS**: Currently set to 300ms
  - Can be adjusted based on testing results
  - Lower values = more aggressive filtering
  - Higher values = less filtering but allows more duplicates

- **MAX_RECENT_KEYS**: Currently set to 16
  - Determines how many recent keys are tracked
  - Higher values use more memory but track more keys

## Debug Output
The driver produces debug output when:
- Duplicate keys are filtered: `"Filtered duplicate key 0x%x (time diff: %dms)"`
- Memory allocation fails: `"Memory allocation failed, passing through unfiltered"`
- Multiple keys are filtered: `"Filtered %d duplicate keys out of %d total"`

## Performance Considerations
- The filtering adds minimal overhead to each keystroke
- Memory allocation only occurs when processing multiple simultaneous key events
- Spinlock usage ensures thread safety with minimal blocking

## Known Limitations
- Only filters key-down events (make codes)
- Does not differentiate between different keyboard devices
- Fixed 300ms threshold (could be made configurable via registry)

## Future Enhancements
1. Make the threshold configurable via registry settings
2. Add per-keyboard device filtering for multi-keyboard systems
3. Implement adaptive thresholds based on typing patterns
4. Add statistics/telemetry for tuning