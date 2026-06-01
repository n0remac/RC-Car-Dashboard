String htmlPage() {
  String environmentSensor = bmeAvailable ? String("Online (") + bmeAddressLabel() + ")" : String("Offline");
  String tiltSensor = imuAvailable ? String("Online") : String("Offline");
  String tiltOrientation = tiltOrientationName();
  String pitchInvert = onOffLabel(invertPitchAxis);
  String rollInvert = onOffLabel(invertRollAxis);
  String axisLabels = onOffLabel(showTiltAxisLabels);
  String tolerance = String(tiltBubbleToleranceDeg, 1) + " deg";
  String axisLabelButton = showTiltAxisLabels ? "Hide Axis Labels" : "Show Axis Labels";
  String temperatureValue = bmeAvailable ? String(environmentTempC, 1) + " C" : String("Unavailable");
  String pitchValue = imuAvailable ? String(pitchDeg, 1) + " deg" : String("Unavailable");
  String rollValue = imuAvailable ? String(rollDeg, 1) + " deg" : String("Unavailable");
  String gpsLock = gpsLockLabel();
  String gpsSatellitesValue = gpsDataSeen ? String(gpsSatellites) : String("0");
  String gpsLatitudeValue = gpsLatitudeLabel();
  String gpsLongitudeValue = gpsLongitudeLabel();
  String gpsRawSpeedValue = gpsSpeedValid ? String(gpsRawMph, 1) + " mph" : String("Unavailable");
  String fusedSpeedValue = String(dashboardMph, 1) + " mph";
  String speedFusionModeValue = speedFusionLeadModeLabel();
  String speedSourceValue = speedSourceLabel();
  String gpsFixAgeValue = gpsFixAgeLabel();
  String gpsLedButtonLabel = speedFusionLeadMode == SPEED_LEAD_GPS ? "GPS Led Active" : "Use GPS Led";
  String accelLedButtonLabel = imuAvailable ?
    (speedFusionLeadMode == SPEED_LEAD_ACCEL ? "Accel Led Active" : "Use Accel Led") :
    String("Accel Unavailable");
  String steeringAngleValue = String(steeringWheelAngleDeg);
  String turnThresholdValue = String(turnSignalThresholdDeg);
  String steeringMvValue = String(steeringInputMv);
  String steeringRawValue = String(steeringInputRaw);
  String steeringMinMvValue = steeringInputMinMv == 9999 ? String("--") : String(steeringInputMinMv);
  String steeringMaxMvValue = String(steeringInputMaxMv);
  String steeringStatusValue = steeringInputStatusLabel();
  String steeringValidValue = steeringInputValid ? String("Yes") : String("No");
  String turnSignalValue = turnSignalLabel();
  String turnSignalOutputValue = turnSignalOutputLabel();
  String steeringCenterValue = String(steeringCenterMv);
  String steeringLeftValue = String(steeringLeftMv);
  String steeringRightValue = String(steeringRightMv);
  String calibrationStatusValue = steeringCalibrationStatusLabel();
  String calibrationValidValue = steeringCalibrationValid() ? String("Yes") : String("No");

  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CarDashboard Control</title>
  <style>
    :root {
      color-scheme: dark;
    }
    body {
      font-family: Arial, sans-serif;
      background: #090909;
      color: #f2f2f2;
      margin: 0;
      padding: 20px;
    }
    .card {
      max-width: 520px;
      margin: 0 auto;
      background: #151515;
      border: 1px solid #303030;
      border-radius: 16px;
      padding: 20px;
      box-shadow: 0 18px 40px rgba(0, 0, 0, 0.35);
    }
    h1 {
      margin: 0 0 6px;
      color: #ffffff;
      letter-spacing: 0.04em;
    }
    .lede {
      color: #b9b9b9;
      margin-bottom: 18px;
      line-height: 1.4;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-bottom: 18px;
    }
    .status {
      background: #1d1d1d;
      border: 1px solid #2f2f2f;
      border-radius: 12px;
      padding: 12px;
      line-height: 1.45;
    }
    .label {
      display: block;
      color: #9d9d9d;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-bottom: 4px;
    }
    .subhead {
      margin: 20px 0 10px;
      color: #bcbcbc;
      font-size: 13px;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    .button-row {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 10px;
    }
    .buttons {
      display: grid;
      gap: 10px;
    }
    .control-panel {
      background: #1d1d1d;
      border: 1px solid #2f2f2f;
      border-radius: 12px;
      padding: 14px;
      display: grid;
      gap: 14px;
    }
    .steering-layout {
      display: grid;
      grid-template-columns: 120px minmax(0, 1fr);
      gap: 16px;
      align-items: center;
    }
    .wheel {
      width: 92px;
      height: 92px;
      border: 7px solid #d7d7d7;
      border-radius: 50%;
      position: relative;
      margin: 0 auto;
      transform: rotate(STEERING_ANGLEdeg);
      transition: transform 120ms ease-out;
    }
    .wheel::before,
    .wheel::after {
      content: "";
      position: absolute;
      background: #d7d7d7;
      left: 50%;
      top: 50%;
      transform-origin: left center;
      width: 34px;
      height: 5px;
      border-radius: 999px;
    }
    .wheel::before {
      transform: translate(-2px, -2px) rotate(25deg);
    }
    .wheel::after {
      transform: translate(-32px, -2px) rotate(155deg);
    }
    .wheel-spoke {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 5px;
      height: 38px;
      border-radius: 999px;
      background: #d7d7d7;
      transform: translate(-2px, -2px);
      transform-origin: top center;
    }
    .slider-row {
      display: grid;
      gap: 7px;
    }
    .diagnostic-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .diagnostic {
      background: #151515;
      border: 1px solid #303030;
      border-radius: 10px;
      padding: 10px;
      line-height: 1.35;
    }
    .value-row {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      color: #d8d8d8;
      font-size: 14px;
    }
    .calibration-actions {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }
    .manual-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .manual-field {
      display: grid;
      gap: 6px;
    }
    .manual-field input {
      width: 100%;
      box-sizing: border-box;
      border: 1px solid #3a3a3a;
      border-radius: 8px;
      padding: 10px;
      color: #f2f2f2;
      background: #101010;
      font-size: 15px;
    }
    input[type="range"] {
      width: 100%;
    }
    button {
      width: 100%;
      border: none;
      border-radius: 10px;
      padding: 15px;
      font-size: 16px;
      cursor: pointer;
      background: #2f80ed;
      color: white;
    }
    button.secondary {
      background: #27ae60;
    }
    button.tertiary {
      background: #0f766e;
    }
    button.quaternary {
      background: #7c3aed;
    }
    button.warning {
      background: #c2410c;
    }
    .small {
      color: #9b9b9b;
      margin-top: 16px;
      font-size: 13px;
      line-height: 1.4;
    }
    a {
      color: #7dd3fc;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>CarDashboard</h1>
    <div class="lede">
      The display stays on a single combined dashboard panel. Tilt, temperature, GPS, and fused speed are live; the remaining indicators are still fixed demo values.
    </div>

    <div class="status-grid">
      <div class="status">
        <span class="label">Display</span>
        Unified Dashboard
      </div>
      <div class="status">
        <span class="label">Environment Sensor</span>
        ENVIRONMENT_SENSOR
      </div>
      <div class="status">
        <span class="label">Ambient Temp</span>
        TEMPERATURE_VALUE
      </div>
      <div class="status">
        <span class="label">GPS Lock</span>
        GPS_LOCK
      </div>
      <div class="status">
        <span class="label">Satellites</span>
        GPS_SATELLITES
      </div>
      <div class="status">
        <span class="label">Latitude</span>
        GPS_LATITUDE
      </div>
      <div class="status">
        <span class="label">Longitude</span>
        GPS_LONGITUDE
      </div>
      <div class="status">
        <span class="label">GPS Speed</span>
        GPS_SPEED
      </div>
      <div class="status">
        <span class="label">Fused Speed</span>
        FUSED_SPEED
      </div>
      <div class="status">
        <span class="label">Fusion Mode</span>
        SPEED_FUSION_MODE
      </div>
      <div class="status">
        <span class="label">Speed Source</span>
        SPEED_SOURCE
      </div>
      <div class="status">
        <span class="label">Fix Age</span>
        GPS_FIX_AGE
      </div>
      <div class="status">
        <span class="label">Tilt Orientation</span>
        TILT_ORIENTATION
      </div>
      <div class="status">
        <span class="label">Tilt Sensor</span>
        TILT_SENSOR
      </div>
      <div class="status">
        <span class="label">Pitch / Roll</span>
        PITCH_VALUE<br>ROLL_VALUE
      </div>
      <div class="status">
        <span class="label">Controls</span>
        Pitch invert: PITCH_INVERT<br>
        Roll invert: ROLL_INVERT<br>
        Axis labels: AXIS_LABELS<br>
        Bubble tolerance: TILT_TOLERANCE
      </div>
    </div>

    <div class="subhead">Steering Input</div>
    <div class="control-panel">
      <div class="steering-layout">
        <div class="wheel" id="steeringWheel">
          <div class="wheel-spoke"></div>
        </div>
        <div class="slider-row">
          <div class="value-row">
            <span>Angle</span>
            <span><span id="steeringValue">STEERING_ANGLE</span> deg</span>
          </div>
          <input
            id="steeringSlider"
            type="range"
            min="-45"
            max="45"
            step="1"
            value="STEERING_ANGLE"
            disabled
          >
        </div>
      </div>
      <div class="diagnostic-grid">
        <div class="diagnostic">
          <span class="label">GPIO32 Voltage</span>
          <span id="steeringVoltageValue">STEERING_MV</span> mV
        </div>
        <div class="diagnostic">
          <span class="label">Raw ADC</span>
          <span id="steeringRawValue">STEERING_RAW</span>
        </div>
        <div class="diagnostic">
          <span class="label">Input Status</span>
          <span id="steeringStatusValue">STEERING_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Valid Input</span>
          <span id="steeringValidValue">STEERING_VALID</span>
        </div>
        <div class="diagnostic">
          <span class="label">Turn Signal</span>
          <span id="turnSignalValue">TURN_SIGNAL_VALUE</span>
        </div>
        <div class="diagnostic">
          <span class="label">LED Output</span>
          <span id="turnSignalOutputValue">TURN_SIGNAL_OUTPUT</span>
        </div>
        <div class="diagnostic">
          <span class="label">Observed Min / Max</span>
          <span id="steeringRangeValue">STEERING_MIN_MV / STEERING_MAX_MV</span> mV
        </div>
        <div class="diagnostic">
          <span class="label">Calibration</span>
          <span id="calibrationStatusValue">CALIBRATION_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Center</span>
          <span id="centerMvValue">CENTER_MV</span> mV
        </div>
        <div class="diagnostic">
          <span class="label">Left / Right</span>
          <span id="leftRightMvValue">LEFT_MV / RIGHT_MV</span> mV
        </div>
      </div>
      <div class="slider-row">
        <div class="value-row">
          <span>Turn Signal Threshold</span>
          <span><span id="thresholdValue">TURN_THRESHOLD</span> deg</span>
        </div>
        <input
          id="thresholdSlider"
          type="range"
          min="0"
          max="45"
          step="1"
          value="TURN_THRESHOLD"
        >
      </div>
      <div class="calibration-actions">
        <button class="secondary" type="button" data-calibrate="center">Set Center</button>
        <button class="secondary" type="button" data-calibrate="left">Set Left</button>
        <button class="secondary" type="button" data-calibrate="right">Set Right</button>
      </div>
      <div class="manual-grid">
        <label class="manual-field">
          <span class="label">Center mV</span>
          <input id="centerMvInput" type="number" min="0" max="3300" value="CENTER_MV">
        </label>
        <label class="manual-field">
          <span class="label">Left mV</span>
          <input id="leftMvInput" type="number" min="0" max="3300" value="LEFT_MV">
        </label>
        <label class="manual-field">
          <span class="label">Right mV</span>
          <input id="rightMvInput" type="number" min="0" max="3300" value="RIGHT_MV">
        </label>
        <label class="manual-field">
          <span class="label">Threshold deg</span>
          <input id="thresholdInput" type="number" min="0" max="45" value="TURN_THRESHOLD">
        </label>
      </div>
    </div>

    <div class="button-row">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_reset" value="1">
        <button class="warning" type="submit">Reset Tilt Zero</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="speed_reset" value="1">
        <button class="warning" type="submit">Reset Speed Fusion</button>
      </form>
    </div>

    <div class="subhead">Speed Fusion Mode</div>
    <div class="button-row">
      <form action="/set" method="get">
        <input type="hidden" name="speed_mode" value="gps">
        <button class="secondary" type="submit">GPS_LED_BUTTON</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="speed_mode" value="accel">
        <button class="tertiary" type="submit">ACCEL_LED_BUTTON</button>
      </form>
    </div>

    <div class="subhead">Tilt Orientation</div>
    <div class="button-row">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_rotation" value="0">
        <button class="quaternary" type="submit">0 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_rotation" value="90">
        <button class="quaternary" type="submit">90 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_rotation" value="180">
        <button class="quaternary" type="submit">180 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_rotation" value="270">
        <button class="quaternary" type="submit">270 deg</button>
      </form>
    </div>

    <div class="subhead">Axis Controls</div>
    <div class="button-row">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_invert_pitch" value="1">
        <button class="secondary" type="submit">Toggle Pitch</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_invert_roll" value="1">
        <button class="secondary" type="submit">Toggle Roll</button>
      </form>
    </div>

    <div class="buttons">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_labels" value="1">
        <button class="tertiary" type="submit">AXIS_LABEL_BUTTON</button>
      </form>
    </div>

    <div class="subhead">Bubble Reset Tolerance</div>
    <div class="button-row">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_tolerance" value="0.0">
        <button class="warning" type="submit">0.0 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_tolerance" value="1.0">
        <button class="warning" type="submit">1.0 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_tolerance" value="2.5">
        <button class="warning" type="submit">2.5 deg</button>
      </form>
      <form action="/set" method="get">
        <input type="hidden" name="tilt_tolerance" value="5.0">
        <button class="warning" type="submit">5.0 deg</button>
      </form>
    </div>

    <div class="small">
      Connect to Wi-Fi network <strong>CarRadio</strong> and open
      <a href="/">192.168.4.1</a>.
    </div>
  </div>
  <script>
    const steeringSlider = document.getElementById('steeringSlider');
    const thresholdSlider = document.getElementById('thresholdSlider');
    const steeringWheel = document.getElementById('steeringWheel');
    const steeringValue = document.getElementById('steeringValue');
    const thresholdValue = document.getElementById('thresholdValue');
    const steeringVoltageValue = document.getElementById('steeringVoltageValue');
    const steeringRawValue = document.getElementById('steeringRawValue');
    const steeringStatusValue = document.getElementById('steeringStatusValue');
    const steeringValidValue = document.getElementById('steeringValidValue');
    const steeringRangeValue = document.getElementById('steeringRangeValue');
    const turnSignalValue = document.getElementById('turnSignalValue');
    const turnSignalOutputValue = document.getElementById('turnSignalOutputValue');
    const calibrationStatusValue = document.getElementById('calibrationStatusValue');
    const centerMvValue = document.getElementById('centerMvValue');
    const leftRightMvValue = document.getElementById('leftRightMvValue');
    const centerMvInput = document.getElementById('centerMvInput');
    const leftMvInput = document.getElementById('leftMvInput');
    const rightMvInput = document.getElementById('rightMvInput');
    const thresholdInput = document.getElementById('thresholdInput');
    const calibrationButtons = document.querySelectorAll('[data-calibrate]');
    let submitTimer = 0;

    function sendSetUpdate(params) {
      clearTimeout(submitTimer);
      submitTimer = setTimeout(() => {
        fetch('/set?' + params.toString());
      }, 120);
    }

    function updateThresholdPreview() {
      thresholdValue.textContent = thresholdSlider.value;
      thresholdInput.value = thresholdSlider.value;
    }

    function sendThresholdChange() {
      const params = new URLSearchParams();
      params.set('turn_threshold', thresholdSlider.value);
      sendSetUpdate(params);
    }

    function sendManualCalibrationUpdate() {
      const params = new URLSearchParams();
      params.set('center_mv', centerMvInput.value);
      params.set('left_mv', leftMvInput.value);
      params.set('right_mv', rightMvInput.value);
      params.set('turn_threshold', thresholdInput.value);
      sendSetUpdate(params);
    }

    function sendCalibrationCapture(target) {
      clearTimeout(submitTimer);
      fetch('/set?calibrate=' + encodeURIComponent(target))
        .then(() => pollSteeringState())
        .catch(() => {});
    }

    function updateInputWhenIdle(input, value) {
      if (document.activeElement !== input) {
        input.value = value;
      }
    }

    function applySteeringState(state) {
      steeringSlider.value = state.steering_angle;
      steeringWheel.style.transform = 'rotate(' + state.steering_angle + 'deg)';
      steeringValue.textContent = state.steering_angle;
      steeringVoltageValue.textContent = state.steering_mv;
      steeringRawValue.textContent = state.steering_raw;
      steeringStatusValue.textContent = state.steering_status;
      steeringValidValue.textContent = state.steering_valid ? 'Yes' : 'No';
      steeringRangeValue.textContent = state.steering_min_mv + ' / ' + state.steering_max_mv;
      turnSignalValue.textContent = state.turn_signal;
      turnSignalOutputValue.textContent = state.turn_signal_output;
      calibrationStatusValue.textContent = state.calibration_status;
      centerMvValue.textContent = state.center_mv;
      leftRightMvValue.textContent = state.left_mv + ' / ' + state.right_mv;
      thresholdSlider.value = state.turn_threshold;
      thresholdValue.textContent = state.turn_threshold;
      updateInputWhenIdle(centerMvInput, state.center_mv);
      updateInputWhenIdle(leftMvInput, state.left_mv);
      updateInputWhenIdle(rightMvInput, state.right_mv);
      updateInputWhenIdle(thresholdInput, state.turn_threshold);
    }

    function pollSteeringState() {
      fetch('/state')
        .then(response => response.json())
        .then(applySteeringState)
        .catch(() => {});
    }

    thresholdSlider.addEventListener('input', updateThresholdPreview);
    thresholdSlider.addEventListener('change', sendThresholdChange);
    centerMvInput.addEventListener('change', sendManualCalibrationUpdate);
    leftMvInput.addEventListener('change', sendManualCalibrationUpdate);
    rightMvInput.addEventListener('change', sendManualCalibrationUpdate);
    thresholdInput.addEventListener('change', sendManualCalibrationUpdate);
    calibrationButtons.forEach(button => {
      button.addEventListener('click', () => sendCalibrationCapture(button.dataset.calibrate));
    });
    pollSteeringState();
    setInterval(pollSteeringState, 250);
  </script>
</body>
</html>
)HTML";

  html.replace("ENVIRONMENT_SENSOR", environmentSensor);
  html.replace("TEMPERATURE_VALUE", temperatureValue);
  html.replace("GPS_LOCK", gpsLock);
  html.replace("GPS_SATELLITES", gpsSatellitesValue);
  html.replace("GPS_LATITUDE", gpsLatitudeValue);
  html.replace("GPS_LONGITUDE", gpsLongitudeValue);
  html.replace("GPS_SPEED", gpsRawSpeedValue);
  html.replace("FUSED_SPEED", fusedSpeedValue);
  html.replace("SPEED_FUSION_MODE", speedFusionModeValue);
  html.replace("SPEED_SOURCE", speedSourceValue);
  html.replace("GPS_FIX_AGE", gpsFixAgeValue);
  html.replace("GPS_LED_BUTTON", gpsLedButtonLabel);
  html.replace("ACCEL_LED_BUTTON", accelLedButtonLabel);
  html.replace("TILT_ORIENTATION", tiltOrientation);
  html.replace("TILT_SENSOR", tiltSensor);
  html.replace("PITCH_VALUE", pitchValue);
  html.replace("ROLL_VALUE", rollValue);
  html.replace("PITCH_INVERT", pitchInvert);
  html.replace("ROLL_INVERT", rollInvert);
  html.replace("AXIS_LABELS", axisLabels);
  html.replace("TILT_TOLERANCE", tolerance);
  html.replace("AXIS_LABEL_BUTTON", axisLabelButton);
  html.replace("STEERING_ANGLE", steeringAngleValue);
  html.replace("STEERING_MV", steeringMvValue);
  html.replace("STEERING_RAW", steeringRawValue);
  html.replace("STEERING_MIN_MV", steeringMinMvValue);
  html.replace("STEERING_MAX_MV", steeringMaxMvValue);
  html.replace("STEERING_STATUS", steeringStatusValue);
  html.replace("STEERING_VALID", steeringValidValue);
  html.replace("CENTER_MV", steeringCenterValue);
  html.replace("LEFT_MV", steeringLeftValue);
  html.replace("RIGHT_MV", steeringRightValue);
  html.replace("CALIBRATION_STATUS", calibrationStatusValue);
  html.replace("CALIBRATION_VALID", calibrationValidValue);
  html.replace("TURN_THRESHOLD", turnThresholdValue);
  html.replace("TURN_SIGNAL_VALUE", turnSignalValue);
  html.replace("TURN_SIGNAL_OUTPUT", turnSignalOutputValue);
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

String stateJson() {
  String json = "{";
  json += "\"steering_angle\":";
  json += String(steeringWheelAngleDeg);
  json += ",\"turn_threshold\":";
  json += String(turnSignalThresholdDeg);
  json += ",\"steering_raw\":";
  json += String(steeringInputRaw);
  json += ",\"steering_mv\":";
  json += String(steeringInputMv);
  json += ",\"steering_min_mv\":";
  if (steeringInputMinMv == 9999) {
    json += "\"--\"";
  } else {
    json += String(steeringInputMinMv);
  }
  json += ",\"steering_max_mv\":";
  json += String(steeringInputMaxMv);
  json += ",\"center_mv\":";
  json += String(steeringCenterMv);
  json += ",\"left_mv\":";
  json += String(steeringLeftMv);
  json += ",\"right_mv\":";
  json += String(steeringRightMv);
  json += ",\"calibration_valid\":";
  if (steeringCalibrationValid()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"calibration_status\":\"";
  json += steeringCalibrationStatusLabel();
  json += "\"";
  json += ",\"steering_valid\":";
  if (steeringInputValid) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"steering_status\":\"";
  json += steeringInputStatusLabel();
  json += "\"";
  json += ",\"turn_signal\":\"";
  json += turnSignalLabel();
  json += "\"";
  json += ",\"turn_signal_output\":\"";
  json += turnSignalOutputLabel();
  json += "\"";
  json += "}";
  return json;
}

void handleState() {
  server.send(200, "application/json", stateJson());
}

void handleSet() {
  bool liveSteeringUpdate = (
    server.hasArg("turn_threshold") ||
    server.hasArg("center_mv") ||
    server.hasArg("left_mv") ||
    server.hasArg("right_mv") ||
    server.hasArg("calibrate")
  ) &&
    !server.hasArg("tilt_rotation") &&
    !server.hasArg("tilt_reset") &&
    !server.hasArg("speed_reset") &&
    !server.hasArg("speed_mode") &&
    !server.hasArg("tilt_invert_pitch") &&
    !server.hasArg("tilt_invert_roll") &&
    !server.hasArg("tilt_labels") &&
    !server.hasArg("tilt_tolerance");

  if (server.hasArg("tilt_rotation")) {
    int requestedRotation = server.arg("tilt_rotation").toInt();
    if (requestedRotation == 0 || requestedRotation == 90 ||
        requestedRotation == 180 || requestedRotation == 270) {
      tiltOrientationDeg = requestedRotation;
      applyTiltOrientation();
      resetSpeedFusion();
    }
  }

  if (server.hasArg("tilt_reset")) {
    resetTiltReference();
  }

  if (server.hasArg("speed_reset")) {
    resetSpeedFusion();
  }

  if (server.hasArg("speed_mode")) {
    String requestedMode = server.arg("speed_mode");
    if (requestedMode == "gps") {
      setSpeedFusionLeadMode(SPEED_LEAD_GPS);
    } else if (requestedMode == "accel") {
      setSpeedFusionLeadMode(SPEED_LEAD_ACCEL);
    }
  }

  if (server.hasArg("tilt_invert_pitch")) {
    invertPitchAxis = !invertPitchAxis;
    applyTiltOrientation();
  }

  if (server.hasArg("tilt_invert_roll")) {
    invertRollAxis = !invertRollAxis;
    applyTiltOrientation();
  }

  if (server.hasArg("tilt_labels")) {
    showTiltAxisLabels = !showTiltAxisLabels;
  }

  if (server.hasArg("tilt_tolerance")) {
    float requestedTolerance = server.arg("tilt_tolerance").toFloat();
    if (requestedTolerance >= 0.0f && requestedTolerance <= 10.0f) {
      tiltBubbleToleranceDeg = requestedTolerance;
      applyTiltOrientation();
    }
  }

  bool steeringSettingsChanged = false;

  if (server.hasArg("calibrate")) {
    String target = server.arg("calibrate");
    int sampledMv = readSteeringAverageMv(
      STEERING_CALIBRATION_SAMPLE_COUNT,
      STEERING_CALIBRATION_SAMPLE_DELAY_MS
    );

    if (target == "center") {
      steeringCenterMv = sampledMv;
      steeringSettingsChanged = true;
    } else if (target == "left") {
      steeringLeftMv = sampledMv;
      steeringSettingsChanged = true;
    } else if (target == "right") {
      steeringRightMv = sampledMv;
      steeringSettingsChanged = true;
    }
  }

  if (server.hasArg("center_mv")) {
    steeringCenterMv = clampInt(server.arg("center_mv").toInt(), 0, 3300);
    steeringSettingsChanged = true;
  }

  if (server.hasArg("left_mv")) {
    steeringLeftMv = clampInt(server.arg("left_mv").toInt(), 0, 3300);
    steeringSettingsChanged = true;
  }

  if (server.hasArg("right_mv")) {
    steeringRightMv = clampInt(server.arg("right_mv").toInt(), 0, 3300);
    steeringSettingsChanged = true;
  }

  if (server.hasArg("turn_threshold")) {
    turnSignalThresholdDeg = clampInt(
      server.arg("turn_threshold").toInt(),
      TURN_THRESHOLD_MIN_DEG,
      TURN_THRESHOLD_MAX_DEG
    );
    steeringSettingsChanged = true;
  }

  if (steeringSettingsChanged) {
    saveSteeringCalibration();
    updateSteeringInput();
    updateTurnSignalIntent();
  }

  updateTurnSignalOutputs();

  renderCurrentScreen();

  if (liveSteeringUpdate) {
    server.send(204, "text/plain", "");
    return;
  }

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "OK");
}
