# CarDashboard

CarDashboard is an ESP32/Arduino firmware project for a 240×135 TFT vehicle dashboard. The main sketch reads RC-style PWM inputs, GPS, an IMU, and a BME280; renders gauges on the display; hosts a browser dashboard; plays WAV audio and a synthesized horn; and can temporarily take over the steering and throttle signal wires from a dedicated web controller.

The production sketch is [`dashboard/dashboard.ino`](dashboard/dashboard.ino). Arduino concatenates the other `.ino` files in `dashboard/` into the same translation unit.

## Features

- 240×135 TFT dashboard with speed, RPM, gear, fuel, warning, environment, and tilt information
- GPS speed/location with short IMU-based speed bridging
- LSM6DSOX accelerometer/gyro tilt sensing and BME280 environment sensing
- Interrupt-based reading and three-sample filtering of RC PWM signals
- Steering-derived automatic turn signals and physical turn LED outputs
- Browser status/configuration UI at `/`
- Separate touch/pointer driving controller at `/controller`
- AP mode plus optional saved station Wi-Fi credentials
- LittleFS WAV upload/playback and a low-latency synthesized horn through I2S
- Preferences-backed brightness, calibration, orientation, and Wi-Fi settings

## Quick start

1. Open `dashboard/dashboard.ino` in the Arduino IDE (or build the `dashboard/` sketch with an equivalent ESP32 Arduino toolchain).
2. Select the ESP32 board configuration matching the 240×135 T-Display hardware and configure `TFT_eSPI` for that display.
3. Install the dependencies listed below and enable a LittleFS-capable partition layout if browser audio storage is required.
4. Review every GPIO assignment in [PROJECT.md](PROJECT.md) against the actual wiring.
5. Flash the firmware and open the serial monitor at 115200 baud.
6. Join Wi-Fi network `CarRadio` with password `carradio123`, then browse to `http://192.168.4.1/`.

The AP credentials are currently compile-time constants and should be changed before exposing the device beyond a controlled environment.

## Dependencies

The main sketch uses the ESP32 Arduino core and these libraries:

- `TFT_eSPI`
- Adafruit BME280 Library and Adafruit Unified Sensor
- `Arduino_LSM6DSOX`
- `TinyGPSPlus`
- `arduino-audio-tools` (`AudioTools.h`)
- ESP32 core facilities: WiFi, WebServer, Preferences, LittleFS, HardwareSerial, LEDC, and FreeRTOS

There is no checked-in board manifest or lockfile, so compatible library versions must currently be selected in the local Arduino environment.

## How data reaches the browser

The firmware uses the synchronous ESP32 `WebServer` on port 80. The main page polls `GET /state` once per second and applies the returned JSON to its controls and readouts. Sensor acquisition happens in the firmware loop, not in the browser:

- RC PWM edges are timestamped by GPIO interrupts, validated, and filtered.
- GPS NMEA bytes are decoded continuously from UART1.
- IMU samples feed tilt and speed fusion.
- BME280 values are refreshed once per second.
- `/state` serializes the latest in-memory values.

The main dashboard contains embedded HTML, CSS, and JavaScript in `dashboard/web.ino`; there is no separate frontend build.

## How web driving works

Web driving is ordinary HTTP, not WebSockets:

1. `POST /controller/arm` validates steering calibration, detaches the RC input interrupts, attaches 50 Hz/16-bit LEDC PWM to GPIO 32 and 33, writes neutral, and arms the watchdog.
2. Pointer position in the browser becomes integer `steering` and `throttle` values from -100 to 100.
3. `POST /controller/command` sends those values as `application/x-www-form-urlencoded`. Drag events send immediately, and a 100 ms timer supplies the heartbeat.
4. Firmware maps percentages to calibrated steering pulses and fixed throttle pulse endpoints, then writes the corresponding PWM duty cycles.
5. Releasing the pointer sends zero/zero. Hiding or leaving the page requests `/controller/stop`.
6. If no valid command arrives for 500 ms, firmware writes neutral, detaches PWM, restores GPIO input interrupts, and returns to receiver mode.

See [PROJECT.md](PROJECT.md) for message formats, pulse mappings, sensor details, routes, and file-by-file architecture.

## Repository layout

```text
dashboard/                         Main integrated firmware sketch
  dashboard.ino                   Global state, hardware, setup/loop, control logic
  web.ino                         Embedded web UIs, JSON, and HTTP handlers
  gps.ino                         GPS parsing and GPS/IMU speed fusion
  panels.ino                      TFT panel rendering
  browser_audio.ino               LittleFS WAV upload, validation, and playback
  speaker_audio.ino               I2S output configuration
  horn_synth.ino                  Real-time synthesized horn task
utils/                             Standalone experiments and hardware diagnostics
  BrowserVideoToTDisplay/         Browser-to-TFT JPEG/WebSocket experiment
  bluetoothSpeaker/               Bluetooth A2DP speaker experiment
  joystick/                       ADC joystick/audio experiment
  pulseReader/                    GPIO 32 signal oscilloscope
  scanner/                        BME280 display/wiring test
```

The utility sketches are independent programs; they are not compiled into the dashboard firmware.

## Current limitations

- HTTP control and Wi-Fi AP access have no per-user authentication or transport encryption.
- The synchronous web server handles control traffic in the main firmware loop.
- Pin assignments, pulse endpoints, AP credentials, and most timing constants are compiled into the firmware.
- Fuel level, odometer, and some gauge state are placeholders rather than physical sensor inputs.
- No automated build or test configuration is included.

