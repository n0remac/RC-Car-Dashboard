String htmlEscape(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");
  return escaped;
}

String jsonEscape(const String &value) {
  String escaped = "";
  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else if ((uint8_t)c < 0x20) {
      escaped += ' ';
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String htmlPage() {
  String environmentSensor = bmeAvailable ? String("Online (") + bmeAddressLabel() + ")" : String("Offline");
  String tiltSensor = imuAvailable ? String("Online") : String("Offline");
  String tiltOrientation = tiltOrientationName();
  String pitchInvert = onOffLabel(invertPitchAxis);
  String rollInvert = onOffLabel(invertRollAxis);
  String axisLabels = onOffLabel(showTiltAxisLabels);
  String tolerance = String(tiltBubbleToleranceDeg, 1) + " deg";
  String axisLabelButton = showTiltAxisLabels ? "Hide Axis Labels" : "Show Axis Labels";
  float environmentTempF = (environmentTempC * 1.8f) + 32.0f;
  String temperatureValue = bmeAvailable ? String(environmentTempF, 1) + " F" : String("Unavailable");
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
  String steeringStatusValue = steeringInputStatusLabel();
  String steeringValidValue = steeringInputValid ? String("Yes") : String("No");
  String turnSignalValue = turnSignalLabel();
  String turnSignalOutputValue = turnSignalOutputLabel();
  String turnSignalInputStatusValue = turnSignalInputPulseStatusLabel();
  String turnSignalInputPulseValue = turnSignalInput.pulseWidthUs > 0 ?
    String(turnSignalInput.pulseWidthUs) :
    String("--");
  String headlightStateValue = headlightInputStatusLabel();
  String headlightPulseValue = headlightInput.pulseWidthUs > 0 ?
    String(headlightInput.pulseWidthUs) :
    String("--");
  String headlightPulseAgeValue = headlightInput.pulseWidthUs > 0 ?
    String(headlightInput.pulseAgeMs) :
    String("--");
  String soundSwitchStatusValue = soundSwitchInputStatusLabel();
  String soundSwitchPulseValue = soundSwitchInput.pulseWidthUs > 0 ?
    String(soundSwitchInput.pulseWidthUs) :
    String("--");
  String soundSwitchPulseAgeValue = soundSwitchInput.pulseWidthUs > 0 ?
    String(soundSwitchInput.pulseAgeMs) :
    String("--");
  String throttleStatusValue = throttleInputStatusLabel();
  String throttlePulseValue = throttleInput.pulseWidthUs > 0 ?
    String(throttleInput.pulseWidthUs) :
    String("--");
  String throttlePulseAgeValue = throttleInput.pulseWidthUs > 0 ?
    String(throttleInput.pulseAgeMs) :
    String("--");
  String steeringCenterValue = String(steeringCenterUs);
  String steeringLeftValue = String(steeringLeftUs);
  String steeringRightValue = String(steeringRightUs);
  String calibrationStatusValue = steeringCalibrationStatusLabel();
  String calibrationValidValue = steeringCalibrationValid() ? String("Yes") : String("No");
  String brightnessValue = String(screenBrightnessPercent);
  String wifiStaSsidValue = wifiStaSsid.length() > 0 ? htmlEscape(wifiStaSsid) : String("None");
  String wifiStaStatusValue = wifiStationStatusLabel();
  String wifiStaIpValue = wifiStationIpLabel();
  String wifiStaSavedValue = wifiStaCredentialsSaved ? String("Yes") : String("No");
  String audioOutputStatusValue = speakerI2sStatusLabel();
  String audioOutputPinsValue = speakerI2sPinsLabel();
  String audioStorageStatusValue = browserAudioStorageReady() ? String("Ready") : String("Unavailable");
  String audioStorageUsedValue = String((unsigned long)browserAudioStorageUsedBytes()) + " / " +
    String((unsigned long)browserAudioStorageTotalBytes()) + " B";
  String audioStorageFreeValue = String((unsigned long)browserAudioStorageFreeBytes()) + " B";
  String audioFileSavedValue = browserAudioFileSaved() ? String("Yes") : String("No");
  String audioFileSizeValue = String((unsigned long)browserAudioFileSize()) + " B";
  String audioFileInfoValue = browserAudioFileInfoLabel();
  String audioPlaybackValue = browserAudioIsPlaying() ? String("Playing") : String("Stopped");
  String audioStatusValue = browserAudioStatusLabel();

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
    .network-form {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 10px;
    }
    .network-actions {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(96px, 1fr));
      gap: 10px;
    }
    .network-list {
      display: grid;
      gap: 8px;
      min-height: 28px;
    }
    .network-row {
      display: grid;
      grid-template-columns: minmax(0, 1fr) 86px;
      gap: 10px;
      align-items: center;
      background: #151515;
      border: 1px solid #303030;
      border-radius: 10px;
      padding: 10px;
    }
    .network-name {
      overflow-wrap: anywhere;
      line-height: 1.3;
    }
    .network-meta {
      color: #9b9b9b;
      font-size: 12px;
      margin-top: 4px;
    }
    .network-row button,
    .network-actions button {
      padding: 11px;
      font-size: 14px;
    }
    .audio-actions {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(96px, 1fr));
      gap: 10px;
    }
    .audio-message {
      color: #d8d8d8;
      font-size: 14px;
      line-height: 1.4;
      min-height: 20px;
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
    button:disabled {
      cursor: not-allowed;
      opacity: 0.45;
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
      The display stays on a single combined dashboard panel. Tilt, temperature, GPS, fused speed, turn signals, and headlights are live; the remaining indicators are still fixed demo values.
    </div>

    <div class="status-grid">
      <div class="status">
        <span class="label">Display</span>
        Unified Dashboard<br>
        Brightness: <span id="brightnessStatusValue">BRIGHTNESS_VALUE</span>%
      </div>
      <div class="status">
        <span class="label">Audio Output</span>
        <span id="audioOutputStatusValue">AUDIO_OUTPUT_STATUS</span><br>
        <span id="audioOutputPinsValue">AUDIO_OUTPUT_PINS</span>
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

    <div class="subhead">Screen Brightness</div>
    <div class="control-panel">
      <div class="slider-row">
        <div class="value-row">
          <span>Backlight</span>
          <span><span id="brightnessValue">BRIGHTNESS_VALUE</span>%</span>
        </div>
        <input
          id="brightnessSlider"
          type="range"
          min="5"
          max="100"
          step="1"
          value="BRIGHTNESS_VALUE"
        >
      </div>
    </div>

    <div class="subhead">Local Sound</div>
    <div class="control-panel">
      <div class="diagnostic-grid">
        <div class="diagnostic">
          <span class="label">Storage</span>
          <span id="audioStorageStatusValue">AUDIO_STORAGE_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Used / Total</span>
          <span id="audioStorageUsedValue">AUDIO_STORAGE_USED</span>
        </div>
        <div class="diagnostic">
          <span class="label">Free</span>
          <span id="audioStorageFreeValue">AUDIO_STORAGE_FREE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Saved Sound</span>
          <span id="audioSavedValue">AUDIO_FILE_SAVED</span>
        </div>
        <div class="diagnostic">
          <span class="label">File Size</span>
          <span id="audioFileSizeValue">AUDIO_FILE_SIZE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Playback</span>
          <span id="audioPlaybackValue">AUDIO_PLAYBACK</span>
        </div>
        <div class="diagnostic">
          <span class="label">File Info</span>
          <span id="audioFileInfoValue">AUDIO_FILE_INFO</span>
        </div>
        <div class="diagnostic">
          <span class="label">Status</span>
          <span id="audioStatusValue">AUDIO_STATUS</span>
        </div>
      </div>
      <label class="manual-field">
        <span class="label">PCM WAV File</span>
        <input id="audioFileInput" type="file" accept=".wav,audio/wav">
      </label>
      <div class="audio-actions">
        <button class="secondary" id="audioUploadButton" type="button">Upload</button>
        <button class="tertiary" id="audioPlayButton" type="button">Play</button>
        <button class="warning" id="audioStopButton" type="button">Stop</button>
      </div>
      <div class="audio-message" id="audioMessageValue">AUDIO_STATUS</div>
    </div>

    <div class="subhead">Network</div>
    <div class="control-panel">
      <div class="diagnostic-grid">
        <div class="diagnostic">
          <span class="label">AP Network</span>
          CarRadio
        </div>
        <div class="diagnostic">
          <span class="label">AP Address</span>
          192.168.4.1
        </div>
        <div class="diagnostic">
          <span class="label">Station Status</span>
          <span id="wifiStatusValue">WIFI_STA_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Station Address</span>
          <span id="wifiIpValue">WIFI_STA_IP</span>
        </div>
        <div class="diagnostic">
          <span class="label">Station Network</span>
          <span id="wifiSsidValue">WIFI_STA_SSID</span>
        </div>
        <div class="diagnostic">
          <span class="label">Saved Credentials</span>
          <span id="wifiSavedValue">WIFI_STA_SAVED</span>
        </div>
      </div>
      <div class="network-form">
        <label class="manual-field">
          <span class="label">SSID</span>
          <input id="wifiSsidInput" type="text" value="">
        </label>
        <label class="manual-field">
          <span class="label">Password</span>
          <input id="wifiPasswordInput" type="password" value="">
        </label>
        <div class="network-actions">
          <button class="secondary" id="wifiScanButton" type="button">Scan</button>
          <button class="tertiary" id="wifiConnectButton" type="button">Connect</button>
          <button class="warning" id="wifiDisconnectButton" type="button">Disconnect</button>
        </div>
      </div>
      <div class="network-list" id="wifiNetworkList">No scan results yet.</div>
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
          <span class="label">GPIO32 Pulse Status</span>
          <span id="turnSignalInputStatusValue">TURN_SIGNAL_INPUT_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO32 Pulse</span>
          <span id="turnSignalInputPulseValue">TURN_SIGNAL_INPUT_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Headlights</span>
          <span id="headlightStateValue">HEADLIGHT_STATE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO12 Pulse</span>
          <span id="headlightPulseValue">HEADLIGHT_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Pulse Age</span>
          <span id="headlightPulseAgeValue">HEADLIGHT_PULSE_AGE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Sound Switch</span>
          <span id="soundSwitchStatusValue">SOUND_SWITCH_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO39 Pulse</span>
          <span id="soundSwitchPulseValue">SOUND_SWITCH_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO39 Pulse Age</span>
          <span id="soundSwitchPulseAgeValue">SOUND_SWITCH_PULSE_AGE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Throttle Status</span>
          <span id="throttleStatusValue">THROTTLE_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO33 Pulse</span>
          <span id="throttlePulseValue">THROTTLE_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO33 Pulse Age</span>
          <span id="throttlePulseAgeValue">THROTTLE_PULSE_AGE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Calibration</span>
          <span id="calibrationStatusValue">CALIBRATION_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Center</span>
          <span id="centerUsValue">CENTER_US</span> us
        </div>
        <div class="diagnostic">
          <span class="label">Left / Right</span>
          <span id="leftRightUsValue">LEFT_US / RIGHT_US</span> us
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
          <span class="label">Center us</span>
          <input id="centerUsInput" type="number" min="900" max="2100" value="CENTER_US">
        </label>
        <label class="manual-field">
          <span class="label">Left us</span>
          <input id="leftUsInput" type="number" min="900" max="2100" value="LEFT_US">
        </label>
        <label class="manual-field">
          <span class="label">Right us</span>
          <input id="rightUsInput" type="number" min="900" max="2100" value="RIGHT_US">
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
    const brightnessSlider = document.getElementById('brightnessSlider');
    const steeringWheel = document.getElementById('steeringWheel');
    const steeringValue = document.getElementById('steeringValue');
    const thresholdValue = document.getElementById('thresholdValue');
    const brightnessValue = document.getElementById('brightnessValue');
    const brightnessStatusValue = document.getElementById('brightnessStatusValue');
    const steeringStatusValue = document.getElementById('steeringStatusValue');
    const steeringValidValue = document.getElementById('steeringValidValue');
    const turnSignalValue = document.getElementById('turnSignalValue');
    const turnSignalOutputValue = document.getElementById('turnSignalOutputValue');
    const turnSignalInputStatusValue = document.getElementById('turnSignalInputStatusValue');
    const turnSignalInputPulseValue = document.getElementById('turnSignalInputPulseValue');
    const headlightStateValue = document.getElementById('headlightStateValue');
    const headlightPulseValue = document.getElementById('headlightPulseValue');
    const headlightPulseAgeValue = document.getElementById('headlightPulseAgeValue');
    const soundSwitchStatusValue = document.getElementById('soundSwitchStatusValue');
    const soundSwitchPulseValue = document.getElementById('soundSwitchPulseValue');
    const soundSwitchPulseAgeValue = document.getElementById('soundSwitchPulseAgeValue');
    const throttleStatusValue = document.getElementById('throttleStatusValue');
    const throttlePulseValue = document.getElementById('throttlePulseValue');
    const throttlePulseAgeValue = document.getElementById('throttlePulseAgeValue');
    const calibrationStatusValue = document.getElementById('calibrationStatusValue');
    const centerUsValue = document.getElementById('centerUsValue');
    const leftRightUsValue = document.getElementById('leftRightUsValue');
    const centerUsInput = document.getElementById('centerUsInput');
    const leftUsInput = document.getElementById('leftUsInput');
    const rightUsInput = document.getElementById('rightUsInput');
    const thresholdInput = document.getElementById('thresholdInput');
    const calibrationButtons = document.querySelectorAll('[data-calibrate]');
    const wifiStatusValue = document.getElementById('wifiStatusValue');
    const wifiIpValue = document.getElementById('wifiIpValue');
    const wifiSsidValue = document.getElementById('wifiSsidValue');
    const wifiSavedValue = document.getElementById('wifiSavedValue');
    const wifiSsidInput = document.getElementById('wifiSsidInput');
    const wifiPasswordInput = document.getElementById('wifiPasswordInput');
    const wifiScanButton = document.getElementById('wifiScanButton');
    const wifiConnectButton = document.getElementById('wifiConnectButton');
    const wifiDisconnectButton = document.getElementById('wifiDisconnectButton');
    const wifiNetworkList = document.getElementById('wifiNetworkList');
    const audioOutputStatusValue = document.getElementById('audioOutputStatusValue');
    const audioOutputPinsValue = document.getElementById('audioOutputPinsValue');
    const audioStorageStatusValue = document.getElementById('audioStorageStatusValue');
    const audioStorageUsedValue = document.getElementById('audioStorageUsedValue');
    const audioStorageFreeValue = document.getElementById('audioStorageFreeValue');
    const audioSavedValue = document.getElementById('audioSavedValue');
    const audioFileSizeValue = document.getElementById('audioFileSizeValue');
    const audioPlaybackValue = document.getElementById('audioPlaybackValue');
    const audioFileInfoValue = document.getElementById('audioFileInfoValue');
    const audioStatusValue = document.getElementById('audioStatusValue');
    const audioFileInput = document.getElementById('audioFileInput');
    const audioUploadButton = document.getElementById('audioUploadButton');
    const audioPlayButton = document.getElementById('audioPlayButton');
    const audioStopButton = document.getElementById('audioStopButton');
    const audioMessageValue = document.getElementById('audioMessageValue');
    let submitTimer = 0;

    function formatBytes(bytes) {
      const size = Number(bytes) || 0;
      if (size < 1024) {
        return size + ' B';
      }
      if (size < 1048576) {
        return (size / 1024).toFixed(1) + ' KB';
      }
      return (size / 1048576).toFixed(2) + ' MB';
    }

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

    function updateBrightnessPreview() {
      brightnessValue.textContent = brightnessSlider.value;
      brightnessStatusValue.textContent = brightnessSlider.value;
    }

    function sendBrightnessChange() {
      const params = new URLSearchParams();
      params.set('brightness', brightnessSlider.value);
      sendSetUpdate(params);
    }

    function sendManualCalibrationUpdate() {
      const params = new URLSearchParams();
      params.set('center_us', centerUsInput.value);
      params.set('left_us', leftUsInput.value);
      params.set('right_us', rightUsInput.value);
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

    function applyWifiState(state) {
      wifiStatusValue.textContent = state.wifi_sta_status;
      wifiIpValue.textContent = state.wifi_sta_ip;
      wifiSsidValue.textContent = state.wifi_sta_ssid || 'None';
      wifiSavedValue.textContent = state.wifi_sta_saved ? 'Yes' : 'No';
    }

    function applyAudioState(state) {
      audioOutputStatusValue.textContent = state.audio_output_status;
      audioOutputPinsValue.textContent = state.audio_output_pins;
      audioStorageStatusValue.textContent = state.audio_storage_ready ? 'Ready' : 'Unavailable';
      audioStorageUsedValue.textContent = formatBytes(state.audio_storage_used) + ' / ' + formatBytes(state.audio_storage_total);
      audioStorageFreeValue.textContent = formatBytes(state.audio_storage_free);
      audioSavedValue.textContent = state.audio_file_saved ? 'Yes' : 'No';
      audioFileSizeValue.textContent = formatBytes(state.audio_file_size);
      audioPlaybackValue.textContent = state.audio_playing ? 'Playing' : 'Stopped';
      audioFileInfoValue.textContent = state.audio_file_info;
      audioStatusValue.textContent = state.audio_status;
      audioMessageValue.textContent = state.audio_status;
      audioUploadButton.disabled = !state.audio_storage_ready;
      audioPlayButton.disabled = !state.audio_storage_ready || !state.audio_file_saved || state.audio_playing;
      audioStopButton.disabled = !state.audio_playing;
    }

    function applySteeringState(state) {
      applyWifiState(state);
      applyAudioState(state);
      steeringSlider.value = state.steering_angle;
      steeringWheel.style.transform = 'rotate(' + state.steering_angle + 'deg)';
      steeringValue.textContent = state.steering_angle;
      steeringStatusValue.textContent = state.steering_status;
      steeringValidValue.textContent = state.steering_valid ? 'Yes' : 'No';
      turnSignalValue.textContent = state.turn_signal;
      turnSignalOutputValue.textContent = state.turn_signal_output;
      turnSignalInputStatusValue.textContent = state.turn_signal_input_status;
      turnSignalInputPulseValue.textContent = state.turn_signal_input_pulse_us > 0 ? state.turn_signal_input_pulse_us + ' us' : '--';
      headlightStateValue.textContent = state.headlight_status;
      headlightPulseValue.textContent = state.headlight_pulse_us > 0 ? state.headlight_pulse_us + ' us' : '--';
      headlightPulseAgeValue.textContent = state.headlight_pulse_us > 0 ? state.headlight_pulse_age_ms + ' ms' : '--';
      soundSwitchStatusValue.textContent = state.sound_switch_status;
      soundSwitchPulseValue.textContent = state.sound_switch_pulse_us > 0 ? state.sound_switch_pulse_us + ' us' : '--';
      soundSwitchPulseAgeValue.textContent = state.sound_switch_pulse_us > 0 ? state.sound_switch_pulse_age_ms + ' ms' : '--';
      throttleStatusValue.textContent = state.throttle_status;
      throttlePulseValue.textContent = state.throttle_pulse_us > 0 ? state.throttle_pulse_us + ' us' : '--';
      throttlePulseAgeValue.textContent = state.throttle_pulse_us > 0 ? state.throttle_pulse_age_ms + ' ms' : '--';
      calibrationStatusValue.textContent = state.calibration_status;
      brightnessSlider.value = state.brightness;
      brightnessValue.textContent = state.brightness;
      brightnessStatusValue.textContent = state.brightness;
      centerUsValue.textContent = state.center_us;
      leftRightUsValue.textContent = state.left_us + ' / ' + state.right_us;
      thresholdSlider.value = state.turn_threshold;
      thresholdValue.textContent = state.turn_threshold;
      updateInputWhenIdle(centerUsInput, state.center_us);
      updateInputWhenIdle(leftUsInput, state.left_us);
      updateInputWhenIdle(rightUsInput, state.right_us);
      updateInputWhenIdle(thresholdInput, state.turn_threshold);
    }

    function pollSteeringState() {
      fetch('/state')
        .then(response => response.json())
        .then(applySteeringState)
        .catch(() => {});
    }

    function setWifiMessage(message) {
      wifiNetworkList.textContent = message;
    }

    function renderWifiNetworks(networks) {
      wifiNetworkList.innerHTML = '';
      if (!networks.length) {
        setWifiMessage('No networks found.');
        return;
      }

      networks.forEach(network => {
        const row = document.createElement('div');
        row.className = 'network-row';

        const details = document.createElement('div');
        const name = document.createElement('div');
        name.className = 'network-name';
        name.textContent = network.ssid || '(hidden network)';
        const meta = document.createElement('div');
        meta.className = 'network-meta';
        meta.textContent = network.rssi + ' dBm - channel ' + network.channel + (network.secure ? ' - secured' : ' - open');
        details.appendChild(name);
        details.appendChild(meta);

        const button = document.createElement('button');
        button.className = 'secondary';
        button.type = 'button';
        button.textContent = 'Use';
        button.addEventListener('click', () => {
          wifiSsidInput.value = network.ssid;
          wifiPasswordInput.focus();
        });

        row.appendChild(details);
        row.appendChild(button);
        wifiNetworkList.appendChild(row);
      });
    }

    function scanWifiNetworks() {
      setWifiMessage('Scanning...');
      fetch('/wifi/scan')
        .then(response => response.json())
        .then(data => {
          if (data.error) {
            throw new Error(data.message || data.error);
          }
          return data;
        })
        .then(data => renderWifiNetworks(data.networks || []))
        .catch(error => setWifiMessage(error.message || 'Scan failed.'));
    }

    function connectWifiNetwork() {
      const params = new URLSearchParams();
      params.set('ssid', wifiSsidInput.value);
      params.set('password', wifiPasswordInput.value);
      fetch('/wifi/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
      })
        .then(response => response.json())
        .then(data => {
          if (data.error) {
            throw new Error(data.message || data.error);
          }
          wifiPasswordInput.value = '';
          pollSteeringState();
        })
        .catch(error => setWifiMessage(error.message || 'Connect request failed.'));
    }

    function disconnectWifiNetwork() {
      fetch('/wifi/disconnect', { method: 'POST' })
        .then(() => pollSteeringState())
        .catch(() => setWifiMessage('Disconnect request failed.'));
    }

    function setAudioMessage(message) {
      audioMessageValue.textContent = message;
    }

    function handleAudioJsonResponse(response) {
      return response.json()
        .then(data => {
          if (data.message) {
            setAudioMessage(data.message);
          }
          if (typeof data.audio_status === 'string') {
            audioStatusValue.textContent = data.audio_status;
          }
          pollSteeringState();
          return data;
        })
        .catch(() => {
          setAudioMessage(response.ok ? 'Audio request complete.' : 'Audio request failed.');
          pollSteeringState();
        });
    }

    function uploadBrowserAudio() {
      const file = audioFileInput.files[0];
      if (!file) {
        setAudioMessage('Choose a WAV file first.');
        return;
      }

      const form = new FormData();
      form.append('sound', file, file.name);
      audioUploadButton.disabled = true;
      setAudioMessage('Uploading...');

      fetch('/audio/upload', {
        method: 'POST',
        body: form
      })
        .then(handleAudioJsonResponse)
        .catch(() => {
          setAudioMessage('Upload request failed.');
          pollSteeringState();
        });
    }

    function playBrowserAudio() {
      fetch('/audio/play', { method: 'POST' })
        .then(handleAudioJsonResponse)
        .catch(() => {
          setAudioMessage('Play request failed.');
          pollSteeringState();
        });
    }

    function stopBrowserAudio() {
      fetch('/audio/stop', { method: 'POST' })
        .then(handleAudioJsonResponse)
        .catch(() => {
          setAudioMessage('Stop request failed.');
          pollSteeringState();
        });
    }

    thresholdSlider.addEventListener('input', updateThresholdPreview);
    thresholdSlider.addEventListener('change', sendThresholdChange);
    brightnessSlider.addEventListener('input', updateBrightnessPreview);
    brightnessSlider.addEventListener('change', sendBrightnessChange);
    centerUsInput.addEventListener('change', sendManualCalibrationUpdate);
    leftUsInput.addEventListener('change', sendManualCalibrationUpdate);
    rightUsInput.addEventListener('change', sendManualCalibrationUpdate);
    thresholdInput.addEventListener('change', sendManualCalibrationUpdate);
    calibrationButtons.forEach(button => {
      button.addEventListener('click', () => sendCalibrationCapture(button.dataset.calibrate));
    });
    wifiScanButton.addEventListener('click', scanWifiNetworks);
    wifiConnectButton.addEventListener('click', connectWifiNetwork);
    wifiDisconnectButton.addEventListener('click', disconnectWifiNetwork);
    audioUploadButton.addEventListener('click', uploadBrowserAudio);
    audioPlayButton.addEventListener('click', playBrowserAudio);
    audioStopButton.addEventListener('click', stopBrowserAudio);
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
  html.replace("STEERING_STATUS", steeringStatusValue);
  html.replace("STEERING_VALID", steeringValidValue);
  html.replace("CENTER_US", steeringCenterValue);
  html.replace("LEFT_US", steeringLeftValue);
  html.replace("RIGHT_US", steeringRightValue);
  html.replace("CALIBRATION_STATUS", calibrationStatusValue);
  html.replace("CALIBRATION_VALID", calibrationValidValue);
  html.replace("BRIGHTNESS_VALUE", brightnessValue);
  html.replace("WIFI_STA_STATUS", wifiStaStatusValue);
  html.replace("WIFI_STA_IP", wifiStaIpValue);
  html.replace("WIFI_STA_SSID", wifiStaSsidValue);
  html.replace("WIFI_STA_SAVED", wifiStaSavedValue);
  html.replace("AUDIO_OUTPUT_STATUS", audioOutputStatusValue);
  html.replace("AUDIO_OUTPUT_PINS", audioOutputPinsValue);
  html.replace("AUDIO_STORAGE_STATUS", audioStorageStatusValue);
  html.replace("AUDIO_STORAGE_USED", audioStorageUsedValue);
  html.replace("AUDIO_STORAGE_FREE", audioStorageFreeValue);
  html.replace("AUDIO_FILE_SAVED", audioFileSavedValue);
  html.replace("AUDIO_FILE_SIZE", audioFileSizeValue);
  html.replace("AUDIO_FILE_INFO", audioFileInfoValue);
  html.replace("AUDIO_PLAYBACK", audioPlaybackValue);
  html.replace("AUDIO_STATUS", audioStatusValue);
  html.replace("TURN_THRESHOLD", turnThresholdValue);
  html.replace("TURN_SIGNAL_VALUE", turnSignalValue);
  html.replace("TURN_SIGNAL_OUTPUT", turnSignalOutputValue);
  html.replace("TURN_SIGNAL_INPUT_STATUS", turnSignalInputStatusValue);
  html.replace(
    "TURN_SIGNAL_INPUT_PULSE",
    turnSignalInputPulseValue == "--" ? turnSignalInputPulseValue : turnSignalInputPulseValue + " us"
  );
  html.replace("HEADLIGHT_STATE", headlightStateValue);
  html.replace("HEADLIGHT_PULSE_AGE", headlightPulseAgeValue == "--" ? headlightPulseAgeValue : headlightPulseAgeValue + " ms");
  html.replace("HEADLIGHT_PULSE", headlightPulseValue == "--" ? headlightPulseValue : headlightPulseValue + " us");
  html.replace("SOUND_SWITCH_STATUS", soundSwitchStatusValue);
  html.replace("SOUND_SWITCH_PULSE_AGE", soundSwitchPulseAgeValue == "--" ? soundSwitchPulseAgeValue : soundSwitchPulseAgeValue + " ms");
  html.replace("SOUND_SWITCH_PULSE", soundSwitchPulseValue == "--" ? soundSwitchPulseValue : soundSwitchPulseValue + " us");
  html.replace("THROTTLE_STATUS", throttleStatusValue);
  html.replace("THROTTLE_PULSE_AGE", throttlePulseAgeValue == "--" ? throttlePulseAgeValue : throttlePulseAgeValue + " ms");
  html.replace("THROTTLE_PULSE", throttlePulseValue == "--" ? throttlePulseValue : throttlePulseValue + " us");
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
  json += ",\"center_us\":";
  json += String(steeringCenterUs);
  json += ",\"left_us\":";
  json += String(steeringLeftUs);
  json += ",\"right_us\":";
  json += String(steeringRightUs);
  json += ",\"calibration_valid\":";
  if (steeringCalibrationValid()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"calibration_status\":\"";
  json += jsonEscape(steeringCalibrationStatusLabel());
  json += "\"";
  json += ",\"steering_valid\":";
  if (steeringInputValid) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"steering_status\":\"";
  json += jsonEscape(steeringInputStatusLabel());
  json += "\"";
  json += ",\"turn_signal\":\"";
  json += jsonEscape(turnSignalLabel());
  json += "\"";
  json += ",\"turn_signal_output\":\"";
  json += jsonEscape(turnSignalOutputLabel());
  json += "\"";
  json += ",\"turn_signal_input_status\":\"";
  json += jsonEscape(turnSignalInputPulseStatusLabel());
  json += "\"";
  json += ",\"turn_signal_input_pulse_fresh\":";
  if (turnSignalInput.pulseFresh) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"turn_signal_input_pulse_us\":";
  json += String(turnSignalInput.pulseWidthUs);
  json += ",\"headlights_on\":";
  if (dashboardHeadlightsOn) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"headlight_status\":\"";
  json += jsonEscape(headlightInputStatusLabel());
  json += "\"";
  json += ",\"headlight_pulse_fresh\":";
  if (headlightInput.pulseFresh) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"headlight_pulse_us\":";
  json += String(headlightInput.pulseWidthUs);
  json += ",\"headlight_pulse_age_ms\":";
  json += String(headlightInput.pulseAgeMs);
  json += ",\"sound_switch_status\":\"";
  json += jsonEscape(soundSwitchInputStatusLabel());
  json += "\"";
  json += ",\"sound_switch_on\":";
  if (soundSwitchOn) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"sound_switch_pulse_fresh\":";
  if (soundSwitchInput.pulseFresh) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"sound_switch_pulse_us\":";
  json += String(soundSwitchInput.pulseWidthUs);
  json += ",\"sound_switch_pulse_age_ms\":";
  json += String(soundSwitchInput.pulseAgeMs);
  json += ",\"throttle_status\":\"";
  json += jsonEscape(throttleInputStatusLabel());
  json += "\"";
  json += ",\"throttle_pulse_fresh\":";
  if (throttleInput.pulseFresh) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"throttle_pulse_us\":";
  json += String(throttleInput.pulseWidthUs);
  json += ",\"throttle_pulse_age_ms\":";
  json += String(throttleInput.pulseAgeMs);
  json += ",\"brightness\":";
  json += String(screenBrightnessPercent);
  json += ",\"wifi_sta_status\":\"";
  json += jsonEscape(wifiStationStatusLabel());
  json += "\"";
  json += ",\"wifi_sta_ip\":\"";
  json += jsonEscape(wifiStationIpLabel());
  json += "\"";
  json += ",\"wifi_sta_ssid\":\"";
  json += jsonEscape(wifiStaSsid);
  json += "\"";
  json += ",\"wifi_sta_saved\":";
  if (wifiStaCredentialsSaved) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_output_started\":";
  if (speakerI2sStarted()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_output_status\":\"";
  json += jsonEscape(speakerI2sStatusLabel());
  json += "\"";
  json += ",\"audio_output_pins\":\"";
  json += jsonEscape(speakerI2sPinsLabel());
  json += "\"";
  json += ",\"audio_storage_ready\":";
  if (browserAudioStorageReady()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_storage_total\":";
  json += String((unsigned long)browserAudioStorageTotalBytes());
  json += ",\"audio_storage_used\":";
  json += String((unsigned long)browserAudioStorageUsedBytes());
  json += ",\"audio_storage_free\":";
  json += String((unsigned long)browserAudioStorageFreeBytes());
  json += ",\"audio_file_saved\":";
  if (browserAudioFileSaved()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_file_size\":";
  json += String((unsigned long)browserAudioFileSize());
  json += ",\"audio_file_info\":\"";
  json += jsonEscape(browserAudioFileInfoLabel());
  json += "\"";
  json += ",\"audio_playing\":";
  if (browserAudioIsPlaying()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_status\":\"";
  json += jsonEscape(browserAudioStatusLabel());
  json += "\"";
  json += "}";
  return json;
}

void handleState() {
  server.send(200, "application/json", stateJson());
}

void handleWifiScan() {
  int networkCount = WiFi.scanNetworks(false, true);
  String json = "{\"networks\":[";

  for (int i = 0; i < networkCount; i++) {
    if (i > 0) {
      json += ",";
    }

    wifi_auth_mode_t encryptionType = WiFi.encryptionType(i);
    bool secure = encryptionType != WIFI_AUTH_OPEN;
    json += "{\"ssid\":\"";
    json += jsonEscape(WiFi.SSID(i));
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"channel\":";
    json += String(WiFi.channel(i));
    json += ",\"secure\":";
    json += secure ? "true" : "false";
    json += "}";
  }

  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleWifiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "application/json", "{\"error\":\"ssid required\"}");
    return;
  }

  String requestedSsid = server.arg("ssid");
  String requestedPassword = server.hasArg("password") ? server.arg("password") : String("");
  requestedSsid.trim();

  if (requestedSsid.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"ssid required\"}");
    return;
  }

  saveWifiStationCredentials(requestedSsid, requestedPassword);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  beginWifiStation();

  String json = "{\"status\":\"connecting\",\"ssid\":\"";
  json += jsonEscape(wifiStaSsid);
  json += "\"}";
  server.send(200, "application/json", json);
}

void handleWifiDisconnect() {
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  clearWifiStationCredentials();
  server.send(200, "application/json", "{\"status\":\"disconnected\"}");
}

void handleSet() {
  bool liveSteeringUpdate = (
    server.hasArg("turn_threshold") ||
    server.hasArg("center_us") ||
    server.hasArg("left_us") ||
    server.hasArg("right_us") ||
    server.hasArg("calibrate") ||
    server.hasArg("brightness")
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

  if (server.hasArg("brightness")) {
    setScreenBrightnessPercent(server.arg("brightness").toInt());
  }

  bool steeringSettingsChanged = false;

  if (server.hasArg("calibrate")) {
    String target = server.arg("calibrate");
    unsigned long sampledUs = readSteeringPulseAverageUs(STEERING_CALIBRATION_SAMPLE_COUNT);

    if (sampledUs > 0 && target == "center") {
      steeringCenterUs = sampledUs;
      steeringSettingsChanged = true;
    } else if (sampledUs > 0 && target == "left") {
      steeringLeftUs = sampledUs;
      steeringSettingsChanged = true;
    } else if (sampledUs > 0 && target == "right") {
      steeringRightUs = sampledUs;
      steeringSettingsChanged = true;
    }
  }

  if (server.hasArg("center_us")) {
    steeringCenterUs = clampInt(
      server.arg("center_us").toInt(),
      STEERING_PULSE_MIN_VALID_US,
      STEERING_PULSE_MAX_VALID_US
    );
    steeringSettingsChanged = true;
  }

  if (server.hasArg("left_us")) {
    steeringLeftUs = clampInt(
      server.arg("left_us").toInt(),
      STEERING_PULSE_MIN_VALID_US,
      STEERING_PULSE_MAX_VALID_US
    );
    steeringSettingsChanged = true;
  }

  if (server.hasArg("right_us")) {
    steeringRightUs = clampInt(
      server.arg("right_us").toInt(),
      STEERING_PULSE_MIN_VALID_US,
      STEERING_PULSE_MAX_VALID_US
    );
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
