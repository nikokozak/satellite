# Installation Setup Guide: Plotter System

## Overview
This guide covers how to prepare, compile, and deploy the plotter installation for gallery exhibition.

---

## Table of Contents
1. [Compiling the Application](#compiling-the-application)
2. [Uploading Arduino Firmware](#uploading-arduino-firmware)
3. [Mac Mini Auto-Launch Setup](#mac-mini-auto-launch-setup)
4. [Physical Setup](#physical-setup)
5. [Testing the System](#testing-the-system)
6. [Troubleshooting](#troubleshooting)
7. [Gallery Operations](#gallery-operations)

---

## Compiling the Application

### Method 1: Using Make (Command Line)

From the project directory:
```bash
cd /Users/niko/Documents/Projects/SCRATCH.nosync/of_v0.12.0_osx_release/apps/myApps/plotter
make
```

This creates: `bin/plotter.app`

For a clean rebuild:
```bash
make clean
make
```

### Method 2: Using Xcode

1. Open `plotter.xcodeproj` in Xcode
2. Select the target (plotter)
3. Choose Product → Build (⌘B)
4. Or Product → Run (⌘R) to build and launch

The compiled app will be in `bin/plotter.app`

### Method 3: Using VS Code

If you have VS Code configured with OpenFrameworks:

1. Open the project folder in VS Code
2. Open Terminal in VS Code (Terminal → New Terminal)
3. Run: `make` or `make Release`
4. The app will be built to `bin/plotter.app`

**Note:** VS Code doesn't build directly - it uses the Makefile or tasks. Make sure you have:
- The C/C++ extension installed
- A `tasks.json` configured to run make commands

---

## Uploading Arduino Firmware

### Prerequisites
- Arduino IDE installed
- Arduino Uno connected via USB

### Steps

1. **Open the firmware:**
   ```
   /Users/niko/Documents/ITP/SeeingMachines/Plotter/plotter/plotter.ino
   ```

2. **In Arduino IDE:**
   - Tools → Board → Arduino Uno
   - Tools → Port → Select the USB port (usually `/dev/cu.usbmodem...`)
   - Click Upload (→) button

3. **Wait for upload to complete** (~20 seconds)

4. **Verify calibration runs:**
   - Open Serial Monitor (Tools → Serial Monitor)
   - Set baud rate to **115200**
   - You should see calibration messages as the plotter homes itself

**Important:** The Y-axis direction fix requires this new firmware to be uploaded.

---

## Mac Mini Auto-Launch Setup

### Option 1: Login Items (Recommended for Gallery)

1. **Set up auto-login:**
   - System Preferences → Users & Groups → Login Options
   - Set "Automatic login" to your user account
   - (May need to disable FileVault if enabled)

2. **Add app to Login Items:**
   - System Preferences → Users & Groups → [Your User] → Login Items
   - Click the `+` button
   - Navigate to:
     ```
     /Users/niko/Documents/Projects/SCRATCH.nosync/of_v0.12.0_osx_release/apps/myApps/plotter/bin/plotter.app
     ```
   - Add it to the list
   - Make sure "Hide" is unchecked (so you can see if it's running)

3. **Test auto-launch:**
   - Restart the Mac Mini
   - The app should launch automatically after login
   - It will start in autonomous mode (PATH_TRACK) with serial enabled

### Option 2: Launch Agent (Advanced)

If you need the app to restart automatically on crashes:

1. Create a launch agent file:
   ```bash
   nano ~/Library/LaunchAgents/com.plotter.installation.plist
   ```

2. Add this content:
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
   <plist version="1.0">
   <dict>
       <key>Label</key>
       <string>com.plotter.installation</string>
       <key>ProgramArguments</key>
       <array>
           <string>/Users/niko/Documents/Projects/SCRATCH.nosync/of_v0.12.0_osx_release/apps/myApps/plotter/bin/plotter.app/Contents/MacOS/plotter</string>
       </array>
       <key>RunAtLoad</key>
       <true/>
       <key>KeepAlive</key>
       <true/>
       <key>StandardErrorPath</key>
       <string>/tmp/plotter-error.log</string>
       <key>StandardOutPath</key>
       <string>/tmp/plotter-output.log</string>
   </dict>
   </plist>
   ```

3. Load the launch agent:
   ```bash
   launchctl load ~/Library/LaunchAgents/com.plotter.installation.plist
   ```

---

## Physical Setup

### Hardware Connections

1. **Mac Mini:**
   - HDMI to main monitor (for control/debugging)
   - HDMI/DisplayPort to second screen (mounted on plotter)
   - USB cable to Arduino Uno
   - Power cable

2. **Arduino Uno:**
   - USB cable to Mac Mini (provides power + serial communication)
   - Connected to stepper driver boards

3. **Stepper Drivers:**
   - External 12V/24V power supply (separate from Mac)
   - Connected to X and Y stepper motors
   - Limit switches connected to analog pins (A0-A3)

4. **Second Screen:**
   - HDMI from Mac Mini
   - Separate power adapter
   - Mounted on plotter carriage

### Power Setup

**For gallery operations, two power sources:**

1. **Main Power Strip** (for gallery staff):
   - Mac Mini
   - Main monitor
   - Arduino (via USB from Mac)
   - Second screen

2. **Stepper Power Supply** (can be same or separate):
   - 12V/24V PSU for stepper drivers
   - Can be on same power strip or separate

**Gallery staff only needs to control the main power strip.**

### Screen Positioning

The second window is currently hardcoded to position `-1920, 0` (left of main screen).

**To verify/adjust before installation:**

1. Run the app on the Mac Mini with both screens connected
2. Check if the cropped view appears on the plotter-mounted screen
3. If not, you may need to adjust line 134 in `ofApp.cpp`:
   ```cpp
   settings.setPosition({-1920.f, 0.f}); // Adjust X position based on actual screen layout
   ```
4. Common positions:
   - `{-1920.f, 0.f}` - Screen to the left
   - `{1920.f, 0.f}` - Screen to the right
   - Adjust first number to match your main screen width

---

## Testing the System

### Pre-Installation Test Checklist

**Before bringing to gallery:**

- [ ] Arduino firmware uploaded with Y-axis fix
- [ ] OF app compiled with latest changes
- [ ] Test on Mac Mini with both screens connected
- [ ] Verify second screen shows cropped view
- [ ] Test auto-launch on Mac Mini restart
- [ ] Verify serial reconnection (unplug/replug Arduino USB)
- [ ] Test Y-axis direction (should move correctly now)
- [ ] Load test images into `bin/data/imgs/` folder
- [ ] Test full autonomous operation for 30+ minutes

**At gallery:**

- [ ] Set up all physical connections
- [ ] Power on system and verify auto-start
- [ ] Watch first calibration cycle (Arduino homes itself)
- [ ] Observe first path execution
- [ ] Test power cycle (simulate end-of-day shutdown)

### Manual Controls (if needed)

While running, you can use these keyboard shortcuts:

- **SPACE** - Toggle serial communication on/off
- **M** - Switch between PLOTTER_TRACK (mouse) and PATH_TRACK (autonomous)
- **N** - Force load next image
- **P** - Re-process current image for new paths
- **D** - Toggle debug visualization (shows contours and paths)

---

## Troubleshooting

### Arduino Not Found

**Symptoms:** Console shows "Could not find Arduino port"

**Solution:**
- The app will automatically retry connection every 5 seconds
- Check USB cable is connected
- Verify Arduino is powered (LED should be on)
- If not reconnecting after 30 seconds, check serial port in Arduino IDE

### Second Screen in Wrong Position

**Solution:**
- Note the actual screen arrangement on Mac Mini
- Adjust line 134 in `ofApp.cpp` with correct position
- Recompile and test

### Y-Axis Moving Wrong Direction

**Solution:**
- Ensure new Arduino firmware is uploaded
- Check that `UP=0` and `DOWN=1` in `plotter/config.h`
- Re-upload firmware if needed

### Plotter Drifting or Hitting Limits

**Solution:**
- Power cycle both systems
- Arduino will auto-calibrate using limit switches
- If persistent, check:
  - Stepper power supply voltage
  - Mechanical binding or friction
  - Limit switches working (test with multimeter)

### Images Not Loading

**Solution:**
- Check images exist in `bin/data/imgs/` folder
- Images must be .jpg format
- Ensure `pic.jpg` exists as fallback
- Check console for file path errors

### App Crashes or Freezes

**Solution:**
- If using Launch Agent setup, it will auto-restart
- Check logs: `/tmp/plotter-output.log` and `/tmp/plotter-error.log`
- Restart Mac Mini if needed

---

## Gallery Operations

### Daily Startup (Gallery Staff)

1. **Morning:**
   - Plug in main power strip (or press Mac Mini power button)
   - Wait 2-3 minutes for full boot and calibration
   - System will run autonomously

2. **During Day:**
   - No intervention needed
   - System cycles through images automatically
   - If something seems wrong, power cycle and restart

3. **End of Day:**
   - Unplug main power strip (or press Mac Mini power button to shut down)
   - Stepper power can stay on or be turned off

### Emergency Stop

If plotter is behaving erratically:

1. **Immediate:** Unplug stepper power supply (stops motors)
2. **Then:** Quit the app (⌘Q) or power off Mac Mini
3. **Recovery:** Power cycle both systems (Arduino will re-calibrate)

### Configuration Summary

**Current Settings:**
- Serial: Enabled by default
- Mode: PATH_TRACK (autonomous) by default
- Reconnection: Every 5 seconds if connection lost
- Path timeout: 3 seconds per point
- Pause at waypoint: 3 seconds
- Distance threshold: 40 pixels
- Arduino auto-calibration: On every power-up
- Boundary margin: 50 steps from limits

**These should not need adjustment for normal operation.**

---

## Files Modified from Original

**OpenFrameworks (OF) Application:**
- `src/ofApp.h` - Added reconnection timer
- `src/ofApp.cpp` - Auto-start defaults, reconnection logic

**Arduino Firmware:**
- `plotter/config.h` - Y-axis direction fix (UP/DOWN swapped)

**Git Commits:**
1. Fix Y-axis inversion: swap UP/DOWN motor directions
2. Add automatic serial reconnection logic
3. Enable autonomous mode by default for installation

---

## Contact / Notes

**System designed for:**
- Continuous operation: 8-12 hours per day
- Power cycling: Safe to turn off/on daily
- Unattended operation: Fully autonomous once started
- Recovery: Self-correcting via position feedback and limit switches

**Known Limitations:**
- Second screen position is hardcoded (test at venue)
- No persistent state between power cycles (starts fresh each time)
- Stepper position tracking via software counters (minor drift possible over many hours, but limit switches prevent damage)

Good luck with the installation!
