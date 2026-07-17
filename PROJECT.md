# Project architecture

## System overview

CarDashboard is a single ESP32 firmware image. Arduino combines every `.ino` tab in `dashboard/`; consequently, globals and functions declared in one file are directly visible to the others. `dashboard.ino` owns shared state and the lifecycle, while the other files separate UI rendering, GPS, and audio concerns.

```text
RC PWM inputs ──interrupt/filter──┐
GPS UART ───────TinyGPS++─────────┤
IMU + BME280 ───I2C───────────────┼── shared firmware state ── TFT renderer
                                  │                         └── `/api/v1` JSON + SSE
Browser controller ──HTTP POST────┴── mode switch ── LEDC PWM ── steering/throttle
OrcasMakers relay ⇄ outbound HTTP sync ┘
Browser WAV ──HTTP upload──LittleFS──I2S ── amplifier/speaker
RC/web horn request ──synth task─────I2S ── amplifier/speaker
```

There is no cloud service or external application server. The ESP32 is the HTTP server and serves all browser code from string literals.

## Hardware map

| GPIO/bus | Direction | Purpose |
|---|---:|---|
| 4 | Output | TFT backlight PWM |
| 12 | Input | Headlight RC pulse |
| 13 | Output | I2S LRC/word select |
| 15 | Output | I2S audio data |
| 17 | Output | I2S bit clock |
| 21/22 | I2C | SDA/SCL for LSM6DSOX and BME280 |
| 25 | Output | Right turn LED, active low |
| 26 | Output | Left turn LED, active low |
| 27 | Input | GPS UART1 RX at 9600 baud |
| 32 | Input or output | Steering RC input / web-control PWM output |
| 33 | Input or output | Throttle RC input / web-control PWM output |
| 39 | Input | Sound/horn RC pulse |

The TFT pins are supplied by the installed `TFT_eSPI` board configuration and are not declared in this repository. The audio conflict guard checks its pins against I2C, turn LEDs, backlight, headlights, steering, and GPS; notably, review all assignments whenever hardware changes are made.

## Firmware lifecycle

`setup()` performs the following initialization:

1. Loads screen brightness and steering calibration from ESP32 Preferences.
2. enters receiver control mode and attaches steering/throttle GPIO interrupts.
3. Attaches headlight and sound-switch pulse interrupts and configures turn LEDs.
4. Starts serial, TFT/sprite rendering, LittleFS audio storage, and the horn task.
5. Starts the `CarRadio` access point and optionally reconnects saved station Wi-Fi.
6. Starts I2C, probes the LSM6DSOX and BME280 (addresses `0x76`, then `0x77`), and starts GPS UART1.
7. Registers asynchronous HTTP/SSE routes, starts the server, and renders the screen.

The cooperative `loop()` updates pulse inputs, horn requests, the control watchdog, receiver-derived dashboard state, turn outputs, 4 Hz SSE publication, audio, GPS, IMU, speed fusion, one-second environment samples, 500 ms blinking, and 50 ms TFT rendering. The TFT renderer draws into a fixed 240×135 8-bit sprite; at display scales below 100%, it nearest-neighbor samples that completed frame into reusable scanlines and places the result at the configured X/Y offset. HTTP request processing runs asynchronously. A 2 ms delay is used when audio is idle.

## Sensor and input acquisition

### RC pulse framework

`RcPulseInput` is shared by steering, throttle, headlights, and the sound switch. A `CHANGE` ISR records the rising-edge timestamp and the high duration at the falling edge. Normal loop code copies the volatile values with interrupts briefly disabled.

A pulse is usable only when it is within that input's configured range and newer than its stale timeout. Valid samples feed a rolling three-sample median for steering/throttle. Headlight and sound inputs use three consecutive candidate samples as a debounce step. Losing the signal resets filter readiness rather than retaining an old command.

| Input | Valid range | Stale after | Interpretation |
|---|---:|---:|---|
| Steering GPIO 32 | 900–2100 µs | 250 ms | Median pulse maps through stored right/center/left calibration to -45°…45° |
| Throttle GPIO 33 | 900–2100 µs | 250 ms | Around 1534 µs is neutral; sustained lower pulses select drive and sustained higher pulses select reverse |
| Headlight GPIO 12 | 750–2500 µs | 250 ms | On above 2000 µs after debounce |
| Sound GPIO 39 | 750–2500 µs | 250 ms | Horn request above 2000 µs after debounce |

Steering angle supplies the dashboard wheel display and automatic turn intent. The configured threshold defaults to 15°; a 3° release margin provides hysteresis. GPIO 26/25 outputs blink at 500 ms intervals.

Throttle is also converted into display-only RPM (up to 8,000 RPM) and Park/Reverse/Neutral/Drive gear state. Receiver gear changes require three consecutive filtered pulse classifications. Drive is entered below 1484 µs and Reverse above 1609 µs; once selected, a direction is held until the pulse returns within 25 µs of the 1534 µs center. Centered throttle displays Neutral for 500 ms before settling into Park. A stale receiver signal displays Neutral and resets the pending gear and Park timers. Web control uses its explicit signed throttle command immediately but applies the same Neutral-to-Park delay at zero. This is inferred command position, not measured motor RPM or a physical transmission state.

### IMU and environment

The LSM6DSOX is read when acceleration and gyroscope samples are available. Firmware derives pitch/roll, supports orientation and axis inversion preferences, stores a zero reference, and exposes longitudinal acceleration to speed fusion. The TFT tilt panel refreshes at 20 Hz.

The BME280 is sampled every second for temperature, humidity, pressure, and altitude using a 1013.25 hPa sea-level reference. Missing sensors are tolerated and reported in the UI.

### GPS and speed fusion

`gps.ino` decodes NMEA data from UART1 RX GPIO 27 using TinyGPS++. A fresh speed requires at least four satellites and a sample within 1.5 seconds. GPS samples anchor a predicted speed; longitudinal IMU acceleration bridges gaps for up to two seconds and is constrained to ±8 mph around the last anchor. Outside that bridge, predicted speed bleeds down by 1.25 mph/s.

The fusion mode can favor GPS (70% GPS blend) or acceleration (30% GPS blend). Firmware learns accelerometer bias during steady motion and attempts to determine the mounted axis sign by correlating acceleration with GPS speed changes. The resulting `dashboardMph` is therefore fused/estimated speed, while raw GPS values remain available in status JSON.

Several displayed fields are not currently sensor-backed: fuel defaults to 16%, odometer to `000000`, and RPM/gear are inferred from throttle pulses.

## Receiver and web control modes

### Receiver mode (default)

GPIO 32 and 33 are inputs. `CHANGE` interrupts measure the external receiver's steering and throttle pulse widths. Firmware observes those wires for dashboard visualization but does not relay them: the wiring is assumed to let the receiver control the vehicle directly.

### Arming web mode

`POST /api/v1/control/arm` authenticates the client and calls `armVehicleControl()`:

- Steering calibration must satisfy `right < center < left`.
- Receiver interrupts are detached before output is enabled.
- ESP32 Arduino 3.x uses `ledcAttach(pin, 50, 16)`; older cores configure LEDC channels 0/1 and attach the pins.
- Mode becomes `web`, a neutral command is written, and the 500 ms heartbeat starts.

If PWM setup fails, the firmware immediately restores receiver mode.

### Browser-to-car command path

The controller page converts pointer coordinates independently on each axis, clamps them to the joystick circle's normalized range, and rounds to integer percentages:

```text
horizontal: left -100 <── 0 ──> +100 right
vertical: reverse -100 <── 0 ──> +100 forward
```

It sends:

```http
POST /api/v1/control/command
Content-Type: application/json

{"session_id":"a1b2c3d4e5f60708","sequence":1,"steering":-25,"throttle":40}
```

The session must match the latest arm response, sequence numbers must strictly increase, and both control values must be integers in `[-100, 100]`. Rejected session/sequence requests do not refresh the watchdog or touch PWM.

Steering uses the saved calibration:

- `-100` → `steeringLeftUs` (default 2000 µs)
- `0` → `steeringCenterUs` (default 1500 µs)
- `+100` → `steeringRightUs` (default 1025 µs)

Throttle uses fixed endpoints:

- `-100` → full reverse, 2045 µs
- `0` → off/neutral, 1534 µs
- `+100` → full forward, 979 µs

Pulses are emitted at 50 Hz with 16-bit duty resolution. The browser sends on pointer movement and every 100 ms while armed. Only one request is in flight; one queued update is retained so slow responses do not create an unbounded request backlog.

### Stops and failure behavior

- Pointer release sends zero steering and throttle but remains armed.
- The Stop button calls public `POST /api/v1/control/stop` and restores receiver inputs.
- `visibilitychange` calls stop when the page becomes hidden.
- `pagehide` uses `navigator.sendBeacon()` to request stop.
- If the firmware receives no command for 500 ms, its independent watchdog writes neutral and restores receiver mode.
- A failed command response disarms the browser UI, but the firmware watchdog is the authoritative fallback.

These are useful safeguards, not a substitute for a hardware emergency stop. Wi-Fi loss, browser scheduling, electrical contention, or firmware failure must be treated as possible.

### Controller state response

`GET /api/v1/control/state` and successful control POSTs return control state including `session_id` and `last_sequence`.

```json
{
  "armed": false,
  "mode": "receiver",
  "status": "Receiver waiting",
  "steering": 0,
  "throttle": 0,
  "steering_pulse_us": 0,
  "throttle_pulse_us": 0,
  "heartbeat_age_ms": 0,
  "watchdog_timeout_ms": 500,
  "calibration_valid": true
}
```

## HTTP interface

| Method | Route | Purpose |
|---|---|---|
| GET | `/` | Full dashboard, configuration, audio, and Wi-Fi UI |
| GET | `/api/v1/state` | Canonical aggregate state |
| GET | `/api/v1/sensors`, `/api/v1/inputs` | Focused telemetry resources |
| GET | `/api/v1/events` | One-client SSE stream at 4 Hz |
| POST | `/api/v1/settings` | Atomically update JSON settings |
| GET | `/controller` | Dedicated driving controller UI |
| GET | `/api/v1/control/state` | Compact driving state JSON |
| POST | `/api/v1/control/arm` | Authenticate, switch outputs, and create a session |
| POST | `/api/v1/control/command` | Apply ordered JSON steering/throttle command |
| POST | `/api/v1/control/stop` | Public neutralize/receiver safety route |
| GET | `/wifi/scan` | Scan nearby networks |
| POST | `/wifi/connect` | Save credentials and begin station connection |
| POST | `/wifi/disconnect` | Clear station credentials/disconnect |
| POST | `/audio/upload` | Multipart WAV upload to LittleFS |
| POST | `/audio/horn` | Select horn override (`off`, `rc`, or `on`) |
| POST | `/audio/horn/short` | Trigger a 220 ms horn |

The complete machine-readable contract is [docs/openapi.yaml](docs/openapi.yaml). Legacy routes remain as deprecated adapters.

## Audio subsystem

`browser_audio.ino` mounts LittleFS, accepts an uploaded WAV, validates RIFF chunks and PCM format, and streams samples in small loop iterations. Only 16-bit mono or stereo WAV is supported by the I2S setup. A volume stream controls playback gain.

`horn_synth.ino` produces a two-tone 342.5/387.5 Hz horn at 22.05 kHz mono, with attack/release envelopes and a low-pass filter. A pinned FreeRTOS task continually services short I2S buffers for low trigger latency. Horn synthesis has priority over browser-file playback and reconfigures the shared I2S output under a mutex.

Horn request behavior combines the debounced RC sound input with a browser override:

- `off`: web/RC horn playback disabled through the override policy
- `rc`: follow the physical RC sound switch
- `on`: hold the horn from the browser

The separate short-horn endpoint triggers a timed request when the selected mode allows it.

## Persistence and networking

ESP32 Preferences stores:

- steering center/left/right pulse calibration and turn threshold
- dashboard brightness, 50–100% TFT scale, X/Y pixel position, 24-bit color theme, and tilt-related settings
- station Wi-Fi SSID and password

At boot, the device always creates the WPA2 access point `CarRadio` / `carradio123` and also uses station mode when credentials exist (`WIFI_AP_STA`). The AP normally appears at `192.168.4.1`. Browser and API requests connect directly without a pairing token. Traffic is unauthenticated plaintext and must remain on the WPA2-protected access point or another trusted local network; it must not be exposed directly to the internet.

When `CAR_REMOTE_DEVICE_TOKEN` is defined, the station connection posts OrcasMakers protocol-version-1 telemetry to the device-sync endpoint. The strict flat payload converts dashboard and GPS speeds from mph to km/h, RPM from thousands to revolutions/minute, fuel to percent, and accelerometer readings from g to m/s². The response carries the current remote command. Idle synchronization runs every 400 ms and remote control runs every 100 ms; failures back off from 500 ms to 10 seconds. An armed response must have a stable non-empty session ID and generation, a non-decreasing sequence, and integer controls within -100 to 100. Any invalid or failed sync immediately neutralizes remote output; only a valid response refreshes the independent 500 ms watchdog. The default empty token disables all outbound relay requests.

Vehicle web mode has explicit `local` or `remote` ownership. Neither source may arm over the other, while the public Stop routes can always restore receiver mode. The default relay URL is plaintext HTTP for initial development and must be replaced with an HTTPS endpoint before operating connected drive hardware.

## File responsibilities

- `dashboard/dashboard.ino`: includes, constants, shared structs/state, generic RC pulse acquisition, sensor setup/update, control mode switching, PWM output, Preferences, Wi-Fi startup, routes, and `setup()`/`loop()`.
- `dashboard/api.ino`: authentication, canonical serializers, JSON handlers, legacy route registration, control sessions, and SSE publication.
- `dashboard/web.ino`: main and controller HTML/CSS/JavaScript, JSON serialization, setting/Wi-Fi handlers, and all controller HTTP handlers.
- `dashboard/gps.ino`: TinyGPS++ UART parsing, labels, and GPS/IMU speed fusion.
- `dashboard/panels.ino`: TFT gauge, environment, and warning panel drawing.
- `dashboard/browser_audio.ino`: LittleFS lifecycle, WAV parsing/upload, volume, and incremental playback.
- `dashboard/speaker_audio.ino`: shared AudioTools I2S configuration and pin-conflict checks.
- `dashboard/horn_synth.ino`: horn waveform, envelope/state machine, synchronization, and real-time audio task.
- `dashboard/remote.ino`: authenticated outbound OrcasMakers telemetry sync, remote command validation, retry state, and relay diagnostics.

`utils/` holds standalone prototypes. They are useful for validating individual hardware paths but may use different pins from the integrated sketch. In particular, the video utility is the only code here that uses a WebSocket; that must not be confused with production vehicle control.

## Development notes

- Build with a partition that provides more than 1.4 MB for the application while retaining a filesystem; the verified generic ESP32 configuration is `PartitionScheme=huge_app`.
- Keep ISR work minimal. Pulse interpretation, filtering, strings, HTTP, and rendering belong in loop/task context.
- When changing control pins, update the I2S conflict checks and the hardware table together.
- Preserve the sequence “detach input interrupt, then attach PWM” and the reverse sequence on stop to avoid measuring the firmware's own output or driving during input mode.
- Any change to controller cadence must remain comfortably below the 500 ms watchdog timeout, including realistic Wi-Fi latency.
- Treat pulse calibration changes as control changes: web mode immediately recomputes the active command after valid calibration updates.
- Large embedded HTML and manually concatenated JSON consume heap. Watch free memory if adding UI/state fields.
- Dashboard color settings are stored as 24-bit RGB values and exposed as `#RRGGBB`; the renderer converts them to RGB565 without changing the persisted/API values.
- Utility sketches should be compiled separately and should not be copied into `dashboard/` unless intended to become part of the production translation unit.

## Suggested validation checklist

1. Compile for both the intended ESP32 Arduino core and configured TFT target.
2. With actuators disconnected, verify pulse input widths and stale/filter transitions in `/api/v1/state`.
3. Confirm steering calibration ordering and endpoints before arming.
4. Scope GPIO 32/33 to confirm 50 Hz neutral and endpoint pulse widths.
5. Arm, then disable Wi-Fi and confirm receiver mode returns within approximately 500 ms.
6. Test pointer release, tab hiding, page navigation, and Stop independently.
7. Verify no external receiver drives GPIO 32/33 during web mode.
8. Exercise missing GPS/IMU/BME280 and unavailable LittleFS/I2S cases.
