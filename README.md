# HIDLab

HIDLab is a browser-controlled USB keyboard emulator for the Arduino Nano ESP32. Add text to a queue from a phone or computer, then tell the board when to type it into the application that has focus on the USB-connected computer.

The project is an embedded systems and cybersecurity lab for studying **USB Human Interface Device (HID) trust**. It demonstrates how a device that presents itself as a keyboard can translate a network request into physical-looking keyboard input. A Python simulator lets you try the queue and playback controls without an Arduino.

## How it works

```text
Browser → HTTP over Wi-Fi → Arduino Nano ESP32 → USB HID → Focused application
```

The Arduino hosts its own web interface and JSON API. The browser sends commands over Wi-Fi, and the board manages the passage queue and emits USB keyboard events. The browser running the controls can be on a different device from the computer receiving the keystrokes. The receiving computer needs a USB data connection to the board.

### Features

- **Passage queue.** Add, edit, reorder, and remove waiting passages. The queue holds up to two passages, including the active passage. Firmware accepts up to 12,000 characters per passage, using printable ASCII and line breaks.
- **Playback controls.** Start, pause, resume, or stop typing. Pause preserves the current position. Stop returns the full passage to the front of the queue so the next start begins at the first character.
- **Typing modes.** Standard mode uses a fixed interval of 150 ms between characters. Humanized mode varies the target speed within a configurable words-per-minute range and can insert and correct occasional typos.
- **USB state handling.** Playback pauses when the USB host disconnects or suspends. Once USB is ready again, use **Resume** to continue.
- **Browser controller.** The firmware grants control to one browser session at a time. The browser renews a 30-second lease while active and can release control from the web interface.
- **Wi-Fi profiles.** The board saves up to four network profiles and provides a password-protected `HIDLab-Setup` access point.

## Run the simulator without hardware

The simulator uses the Python 3 standard library. It serves a local control page and logs simulated keystrokes to your terminal.

1. Clone the repository and enter the project folder.

   ```bash
   git clone https://github.com/haydenbarefoot/HIDLab.git
   cd HIDLab
   ```

2. Start the simulator from the repository root.

   ```bash
   python simulator/main.py
   ```

   On Windows, `py simulator/main.py` also works if you use the Python launcher. On macOS or Linux, use `python3 simulator/main.py` if your Python command is `python3`.

3. Open `http://127.0.0.1:8000` in a browser on the same computer.
4. Add a short passage, select **Start**, and watch the terminal for `[SIMULATED KEY]` entries. Try **Pause**, **Resume**, and **Stop** with a longer passage.
5. Press `Ctrl+C` in the terminal to shut down the simulator.

The simulator covers queue and playback behavior. USB enumeration, Wi-Fi setup, controller leases, and humanized typing are firmware features.

## Run on the Arduino Nano ESP32

### Requirements

- Arduino Nano ESP32 and a USB-C cable that supports data
- Arduino IDE with the **Arduino ESP32 Boards** package installed through Boards Manager
- **ArduinoJson 7.x**, installed through Library Manager
- An isolated Wi-Fi network or a direct connection to the board’s setup access point

The firmware was compiled for the `arduino:esp32:nano_nora` board target using Arduino ESP32 Boards `2.0.18-arduino.5` and ArduinoJson `7.4.3`. See the [Arduino Nano ESP32 user manual](https://docs.arduino.cc/tutorials/nano-esp32/cheat-sheet/) for board setup and USB HID details.

### Configure and upload

1. Open `firmware/HIDLab/HIDLab.ino` in Arduino IDE.
2. Create `firmware/HIDLab/wifi_credentials.h` beside the sketch.

   ```cpp
   #pragma once

   #define WIFI_SSID "YOUR_LAB_WIFI"
   #define WIFI_PASSWORD "YOUR_LAB_WIFI_PASSWORD"
   #define HIDLAB_SETUP_PASSWORD "REPLACE_WITH_A_UNIQUE_SETUP_PASSWORD"
   ```

   Replace the placeholders with your own values. Use a setup password of at least eight characters. For setup-access-point-only use, set `WIFI_SSID` and `WIFI_PASSWORD` to empty strings. Git ignores `wifi_credentials.h`, so keep your credentials in that local file.

3. Connect the board and select **Arduino Nano ESP32** and its USB port. If the IDE shows a **USB Mode** menu, select **Normal mode (TinyUSB)**. [Arduino USB mode documentation](https://docs.arduino.cc/tutorials/nano-esp32/debugging/)
4. Select **Verify**, then **Upload**. Open Serial Monitor at **115200 baud** to see startup messages and the assigned IP address.

### Connect and try the first passage

1. On the same lab Wi-Fi network, open the device IP printed in Serial Monitor. You can also try `http://hidlab.local` if your network and browser support mDNS.
2. If the board is unreachable on lab Wi-Fi, join `HIDLab-Setup` using your setup password and open the setup IP printed in Serial Monitor. The current firmware enables startup diagnostics and attempts to start this access point at boot, including when lab Wi-Fi is available.
3. In the control page, check for **Controller connected** and **USB keyboard ready**. Add a plain-text marker such as `HIDLab keyboard check`.
4. On the USB-connected computer, focus an empty document in a text editor. Select **Start** from the control browser and check that the text appears in the editor. A separate phone or browser device makes it easy to keep the editor focused.
5. Queue a longer sample to verify **Pause**, **Resume**, and **Stop**. **Release control** frees the browser session and allows any active passage to continue.

### Troubleshooting

| Symptom | What to check |
| --- | --- |
| `wifi_credentials.h` is missing | Create the file in `firmware/HIDLab/` alongside the sketch and define all three macros. |
| `ArduinoJson.h` is missing | Install ArduinoJson 7.x through Arduino IDE’s Library Manager. |
| The board or serial port is missing | Check that the cable supports data and that the Nano ESP32 board package is installed. See the Arduino user manual for bootloader recovery. |
| `hidlab.local` does not resolve | Use the IP printed in Serial Monitor. If needed, join `HIDLab-Setup` directly. |
| The page shows **USB unavailable** | Check the data cable and verify that the host recognizes the board as a keyboard. After a disconnect or suspend, wait for USB to be ready before selecting **Resume**. |
| Another browser has control | Release control from that browser or wait for its lease to expire after it stops renewing. |
| Punctuation appears differently than expected | Check the receiving computer’s active keyboard layout. Try a short passage with letters, digits, and punctuation before using longer text. |

## Cybersecurity use cases

HIDLab demonstrates two related concepts from MITRE ATT&CK. Plugging a new peripheral into a system models the physical device introduction described in **Hardware Additions (T1200)**. Sending keyboard events from that peripheral demonstrates the mechanism described in **Input Injection (T1674)**. In a lab, a harmless text marker is enough to show whether the input reached the intended application. [MITRE ATT&CK T1200](https://attack.mitre.org/techniques/T1200/) · [MITRE ATT&CK T1674](https://attack.mitre.org/techniques/T1674/)

| Use case | Lab exercise |
| --- | --- |
| **USB device-control validation** | On an authorized test endpoint, compare how the host handles a newly connected HID keyboard before and after applying a device-control policy. Record whether the board enumerates and whether a harmless text marker reaches the editor. |
| **Endpoint detection practice** | Record the USB connection time and the playback start time. Compare available device-arrival events, endpoint alerts, and the observed text. This shows which parts of the sequence your tooling can see. [MITRE ATT&CK detection strategy for input injection](https://attack.mitre.org/detectionstrategies/DET0568/) |
| **Security awareness and incident response** | Demonstrate how a small peripheral can register as a keyboard and type into a focused application. Use the exercise to practice identifying, documenting, and responding to an unfamiliar USB device. |
| **Embedded web and IoT security review** | Trace a browser request through the controller lease, HTTP API, input validation, and USB output. Review who can reach the device, who can claim control, and which state is visible through diagnostics. |
| **Input and UI testing** | Replay a known text sample into an authorized test application. Use Standard mode for consistent pacing or Humanized mode to exercise varying intervals and corrections. |

### Lab boundaries

Use HIDLab only with equipment and networks you own or have explicit permission to test. Keep the board on an isolated lab network and use harmless text in a blank editor for demonstrations.

The firmware serves **plain HTTP**. The controller lease coordinates browser access, and any client that can reach the device can attempt to claim an available session. The `/diagnostic` endpoint exposes device status without a controller token. Keep the board off public and shared networks, avoid port forwarding, and use a unique setup password.

## Project layout

| Path | Purpose |
| --- | --- |
| `firmware/HIDLab/HIDLab.ino` | Device startup, USB lifecycle, HTTP routes, and controller sessions |
| `firmware/HIDLab/PassageQueue.h` | Passage validation, queue operations, and playback state |
| `firmware/HIDLab/HumanTypingEngine.h` | Keyboard output, typing cadence, and typo correction |
| `firmware/HIDLab/NetworkManager.h` | Saved Wi-Fi profiles, reconnection, setup access point, and mDNS |
| `firmware/HIDLab/WebPage.h` | Browser interface served directly by the Arduino |
| `simulator/main.py` | Local Python simulator and HTTP API |
| `web/index.html` | Browser interface for the Python simulator |
