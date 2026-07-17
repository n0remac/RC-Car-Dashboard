# OrcasMakers implementation plan

## Goal

Add a `/car` page that displays live CarDashboard telemetry and lets an authenticated user control the vehicle through a server-side relay.

## 1. Create the car feature

Create `car.go` following the organizational style of `robot.go`.

Register it from `main.go`:

```go
Car(mux, authApp)
```

The feature owns:

- Device synchronization.
- Cached telemetry.
- Browser control sessions.
- Safety timeouts.
- The `/car` page and JavaScript.
- Optional telemetry streaming through SSE.

## 2. Build the relay state

Create a concurrency-safe `carRelay` containing:

- Mutex.
- Latest telemetry.
- Telemetry receipt timestamp.
- Device online status.
- Active browser session ID.
- Latest browser command.
- Command sequence.
- Command generation.
- Browser heartbeat timestamp.
- Armed state.
- Controller ownership information.

Do not persist active control state. A server restart must produce an unarmed state.

## 3. Add the authenticated device endpoint

Add:

```http
POST /api/car/device/sync
Authorization: Bearer <CAR_DEVICE_TOKEN>
Content-Type: application/json
```

The handler should:

1. Authenticate the ESP32.
2. Limit the request body size.
3. Decode and validate telemetry.
4. Save the latest state and receipt timestamp.
5. Determine whether the browser command is still fresh.
6. Return the current command or an unarmed response.

Read the token from:

- `CAR_DEVICE_TOKEN`, or
- A configured token file.

Use constant-time token comparison.

## 4. Add browser telemetry endpoints

Add:

- `GET /api/car/state`
- `GET /api/car/events`

`/api/car/state` returns:

```json
{
  "online": true,
  "telemetry_age_ms": 95,
  "controller_active": false,
  "telemetry": {}
}
```

Consider the device offline after approximately two seconds without a successful sync.

The SSE endpoint should publish updates when telemetry changes and periodic keepalives. Browser polling is an acceptable initial implementation if SSE adds too much complexity.

## 5. Add browser control endpoints

Add:

- `POST /api/car/control/arm`
- `POST /api/car/control/command`
- `POST /api/car/control/stop`
- `GET /api/car/control/state`

Arm response:

```json
{
  "armed": true,
  "session_id": "a1b2c3d4e5f60708",
  "sequence": 0
}
```

Command request:

```json
{
  "session_id": "a1b2c3d4e5f60708",
  "sequence": 1,
  "steering": -25,
  "throttle": 40
}
```

Validate:

- Active session matches.
- Sequence strictly increases.
- Values are integers from `-100` to `100`.
- Device is online before arming.
- Only one browser session is active.
- The controller heartbeat is fresh.

The Stop endpoint should be idempotent and available even if the session is already stale.

## 6. Implement server-side safety rules

The server must return `armed: false` when:

- No controller owns the session.
- No command or heartbeat arrived within 400–500 ms.
- The user pressed Stop.
- The controller session was replaced.
- The device is offline.
- The server recently restarted.

Additional safeguards:

- Start every new session at zero steering and throttle.
- Do not reuse old commands after reconnecting.
- Increment a generation counter whenever arm/stop ownership changes.
- Never persist armed state.
- Rate-limit arm and command requests.
- Log arm, stop, expiration, and device connection events.

## 7. Protect the controller

Use OrcasMakers’ existing authentication system for:

- `/car`
- All `/api/car/control/*` routes

The device sync endpoint uses the separate device token.

Telemetry visibility may be broader if desired, but control routes should not be publicly writable.

If the existing authentication system supports roles, require an operator or administrator role for arming.

## 8. Build the `/car` page

Display:

- Device online/offline status.
- Telemetry age.
- Controller state.
- Speed, RPM, gear, and fuel.
- Headlights and turn signals.
- Temperature, humidity, pressure, and altitude.
- Pitch and roll.
- Acceleration and gyro readings.
- GPS fix, coordinates, satellites, and speed.
- Steering and throttle input state.
- Current receiver/local/remote ownership.

Controls:

- Arm button with safety confirmation.
- Large Stop button.
- Touch and mouse joystick.
- Steering and throttle readouts.
- Keyboard support if useful.
- Clear armed/disarmed styling.
- Disable Arm while the device is offline.
- Automatically release to neutral when pointer interaction ends.

## 9. Implement browser heartbeat behavior

While armed:

- Send commands immediately after input changes.
- Send a heartbeat or repeat the latest command every 100 ms.
- Keep only one request in flight.
- Retain at most one queued command.
- Stop on `visibilitychange`, `pagehide`, connection failure, or session rejection.
- Use `sendBeacon()` for best-effort Stop during navigation.
- Treat the server response as authoritative.

The UI must visibly disarm when requests fail.

## 10. Add configuration and deployment documentation

Document these environment values:

```text
CAR_DEVICE_TOKEN=<long-random-secret>
ENVIRONMENT=production
```

Update the OrcasMakers README with:

- `/car` page location.
- Device sync URL.
- Token configuration.
- Authentication requirements.
- Online and controller timeout behavior.
- Deployment restart behavior.

Add `CAR_DEVICE_TOKEN` to the deployment secret store rather than committing it.

## 11. Add HTTPS before real driving

Plain HTTP to `165.227.55.254:8081` exposes the device token and vehicle commands.

Production deployment should:

- Put OrcasMakers behind HTTPS.
- Use a hostname with a valid certificate.
- Change CarDashboard to that HTTPS URL.
- Redirect browser HTTP traffic to HTTPS.
- Apply request rate limits.
- Avoid logging authorization headers or complete request bodies.

Initial HTTP testing should be performed with the drive hardware disconnected.

## 12. OrcasMakers tests

Add `car_test.go` covering:

- Correct page and JavaScript routes.
- Missing or incorrect device token.
- Valid device synchronization.
- Telemetry caching.
- Device online/offline transitions.
- Refusing to arm while offline.
- Successful arm and command flow.
- Invalid session rejection.
- Stale and duplicate sequence rejection.
- Steering/throttle range validation.
- Single-controller ownership.
- Browser heartbeat timeout.
- Stop response returned to the ESP32.
- Server startup always disarmed.
- Concurrent device and browser requests with `go test -race`.