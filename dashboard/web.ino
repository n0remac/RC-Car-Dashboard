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
  String turnSignalInputPulseValue = steeringReceiverInput.pulseWidthUs > 0 ?
    String(steeringReceiverInput.pulseWidthUs) :
    String("--");
  String turnSignalFilteredPulseValue = vehicleControl.steeringPulseUs > 0 ?
    String(vehicleControl.steeringPulseUs) :
    String("--");
  String turnSignalFilterValue = steeringReceiverInput.filterReady ? String("Ready") : String("Collecting");
  String headlightStateValue = headlightInputStatusLabel();
  String headlightPulseValue = headlightInput.pulseWidthUs > 0 ?
    String(headlightInput.pulseWidthUs) :
    String("--");
  String headlightFilteredPulseValue = headlightInput.filteredPulseWidthUs > 0 ?
    String(headlightInput.filteredPulseWidthUs) :
    String("--");
  String headlightFilterValue = headlightInput.filterReady ? String("Ready") : String("Collecting");
  String headlightPulseAgeValue = headlightInput.pulseWidthUs > 0 ?
    String(headlightInput.pulseAgeMs) :
    String("--");
  String soundSwitchStatusValue = soundSwitchInputStatusLabel();
  String soundSwitchPulseValue = soundSwitchInput.pulseWidthUs > 0 ?
    String(soundSwitchInput.pulseWidthUs) :
    String("--");
  String soundSwitchFilteredPulseValue = soundSwitchInput.filteredPulseWidthUs > 0 ?
    String(soundSwitchInput.filteredPulseWidthUs) :
    String("--");
  String soundSwitchFilterValue = soundSwitchInput.filterReady ? String("Ready") : String("Collecting");
  String soundSwitchPulseAgeValue = soundSwitchInput.pulseWidthUs > 0 ?
    String(soundSwitchInput.pulseAgeMs) :
    String("--");
  String throttleStatusValue = throttleInputStatusLabel();
  String throttlePulseValue = vehicleControl.throttlePulseUs > 0 ?
    String(vehicleControl.throttlePulseUs) :
    String("--");
  String throttleRawPulseValue = throttleReceiverInput.pulseWidthUs > 0 ?
    String(throttleReceiverInput.pulseWidthUs) :
    String("--");
  String throttleFilterValue = throttleReceiverInput.filterReady ? String("Ready") : String("Collecting");
  String throttlePulseAgeValue = vehicleControl.mode == VehicleControlMode::Receiver ?
    (throttleReceiverInput.pulseFresh ? String(throttleReceiverInput.pulseAgeMs) : String("--")) :
    (vehicleControl.armed ? String(vehicleControlHeartbeatAgeMs()) : String("--"));
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
  String hornModeValue = hornWebModeLabel();
  String hornRcRequestedValue = rcHornRequested ? String("Yes") : String("No");
  String hornRequestedValue = hornPlaybackRequested() ? String("Yes") : String("No");
  String hornSynthActiveValue = hornSynthIsActive() ? String("Yes") : String("No");
  String hornSynthStatusValue = hornSynthStatusLabel();

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
    .horn-mode {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 2px;
      background: #101010;
      border: 1px solid #3a3a3a;
      border-radius: 10px;
      padding: 2px;
    }
    .horn-mode button {
      border-radius: 8px;
      padding: 11px 8px;
      background: transparent;
      color: #b9b9b9;
      font-size: 14px;
    }
    .horn-mode button.active {
      background: #0f766e;
      color: #ffffff;
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
        <div class="diagnostic">
          <span class="label">Horn Override</span>
          <span id="hornModeValue">HORN_MODE</span>
        </div>
        <div class="diagnostic">
          <span class="label">RC Horn Request</span>
          <span id="hornRcRequestedValue">HORN_RC_REQUESTED</span>
        </div>
        <div class="diagnostic">
          <span class="label">Horn Requested</span>
          <span id="hornRequestedValue">HORN_REQUESTED</span>
        </div>
        <div class="diagnostic">
          <span class="label">Horn Synth Active</span>
          <span id="hornSynthActiveValue">HORN_SYNTH_ACTIVE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Horn Synth</span>
          <span id="hornSynthStatusValue">HORN_SYNTH_STATUS</span>
        </div>
      </div>
      <div class="horn-mode" role="group" aria-label="Horn override">
        <button data-horn-mode="off" type="button">Off</button>
        <button data-horn-mode="rc" type="button">RC</button>
        <button data-horn-mode="on" type="button">On</button>
      </div>
      <label class="manual-field">
        <span class="label">PCM WAV File</span>
        <input id="audioFileInput" type="file" accept=".wav,audio/wav">
      </label>
      <div class="audio-actions">
        <button class="secondary" id="audioUploadButton" type="button">Upload</button>
        <button class="tertiary" id="hornShortButton" type="button">Honk</button>
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

    <div class="subhead">Vehicle Control</div>
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
          <span class="label">Control Status</span>
          <span id="steeringStatusValue">STEERING_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">Calibration Valid</span>
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
          <span class="label">GPIO32 Signal Status</span>
          <span id="turnSignalInputStatusValue">TURN_SIGNAL_INPUT_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO32 Raw Pulse</span>
          <span id="turnSignalInputPulseValue">TURN_SIGNAL_INPUT_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO32 Filtered Pulse</span>
          <span id="turnSignalFilteredPulseValue">TURN_SIGNAL_FILTERED_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO32 Filter</span>
          <span id="turnSignalFilterValue">TURN_SIGNAL_FILTER</span>
        </div>
        <div class="diagnostic">
          <span class="label">Headlights</span>
          <span id="headlightStateValue">HEADLIGHT_STATE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO12 Raw Pulse</span>
          <span id="headlightPulseValue">HEADLIGHT_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO12 Filtered Pulse</span>
          <span id="headlightFilteredPulseValue">HEADLIGHT_FILTERED_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO12 Filter</span>
          <span id="headlightFilterValue">HEADLIGHT_FILTER</span>
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
          <span class="label">GPIO39 Raw Pulse</span>
          <span id="soundSwitchPulseValue">SOUND_SWITCH_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO39 Filtered Pulse</span>
          <span id="soundSwitchFilteredPulseValue">SOUND_SWITCH_FILTERED_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO39 Filter</span>
          <span id="soundSwitchFilterValue">SOUND_SWITCH_FILTER</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO39 Pulse Age</span>
          <span id="soundSwitchPulseAgeValue">SOUND_SWITCH_PULSE_AGE</span>
        </div>
        <div class="diagnostic">
          <span class="label">Throttle Control Status</span>
          <span id="throttleStatusValue">THROTTLE_STATUS</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO33 Raw Pulse</span>
          <span id="throttleRawPulseValue">THROTTLE_RAW_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO33 Filtered Pulse</span>
          <span id="throttlePulseValue">THROTTLE_PULSE</span>
        </div>
        <div class="diagnostic">
          <span class="label">GPIO33 Filter</span>
          <span id="throttleFilterValue">THROTTLE_FILTER</span>
        </div>
        <div class="diagnostic">
          <span class="label">Pulse / Command Age</span>
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
      <a href="/">192.168.4.1</a>. Vehicle driving controls are available on
      <a href="/controller">/controller</a>.
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
    const turnSignalFilteredPulseValue = document.getElementById('turnSignalFilteredPulseValue');
    const turnSignalFilterValue = document.getElementById('turnSignalFilterValue');
    const headlightStateValue = document.getElementById('headlightStateValue');
    const headlightPulseValue = document.getElementById('headlightPulseValue');
    const headlightFilteredPulseValue = document.getElementById('headlightFilteredPulseValue');
    const headlightFilterValue = document.getElementById('headlightFilterValue');
    const headlightPulseAgeValue = document.getElementById('headlightPulseAgeValue');
    const soundSwitchStatusValue = document.getElementById('soundSwitchStatusValue');
    const soundSwitchPulseValue = document.getElementById('soundSwitchPulseValue');
    const soundSwitchFilteredPulseValue = document.getElementById('soundSwitchFilteredPulseValue');
    const soundSwitchFilterValue = document.getElementById('soundSwitchFilterValue');
    const soundSwitchPulseAgeValue = document.getElementById('soundSwitchPulseAgeValue');
    const throttleStatusValue = document.getElementById('throttleStatusValue');
    const throttlePulseValue = document.getElementById('throttlePulseValue');
    const throttleRawPulseValue = document.getElementById('throttleRawPulseValue');
    const throttleFilterValue = document.getElementById('throttleFilterValue');
    const throttlePulseAgeValue = document.getElementById('throttlePulseAgeValue');
    const calibrationStatusValue = document.getElementById('calibrationStatusValue');
    const centerUsValue = document.getElementById('centerUsValue');
    const leftRightUsValue = document.getElementById('leftRightUsValue');
    const centerUsInput = document.getElementById('centerUsInput');
    const leftUsInput = document.getElementById('leftUsInput');
    const rightUsInput = document.getElementById('rightUsInput');
    const thresholdInput = document.getElementById('thresholdInput');
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
    const hornModeValue = document.getElementById('hornModeValue');
    const hornRcRequestedValue = document.getElementById('hornRcRequestedValue');
    const hornRequestedValue = document.getElementById('hornRequestedValue');
    const hornSynthActiveValue = document.getElementById('hornSynthActiveValue');
    const hornSynthStatusValue = document.getElementById('hornSynthStatusValue');
    const hornModeButtons = document.querySelectorAll('[data-horn-mode]');
    const audioFileInput = document.getElementById('audioFileInput');
    const audioUploadButton = document.getElementById('audioUploadButton');
    const hornShortButton = document.getElementById('hornShortButton');
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
      const hornModeLabels = { off: 'Off', rc: 'RC', on: 'On' };
      hornModeValue.textContent = hornModeLabels[state.horn_web_mode] || 'RC';
      hornRcRequestedValue.textContent = state.horn_rc_requested ? 'Yes' : 'No';
      hornRequestedValue.textContent = state.horn_requested ? 'Yes' : 'No';
      hornSynthActiveValue.textContent = state.horn_synth_active ? 'Yes' : 'No';
      hornSynthStatusValue.textContent = state.horn_synth_status;
      audioMessageValue.textContent = state.horn_synth_active ? state.horn_synth_status : state.audio_status;
      hornModeButtons.forEach(button => {
        const active = button.dataset.hornMode === state.horn_web_mode;
        button.classList.toggle('active', active);
        button.setAttribute('aria-pressed', active ? 'true' : 'false');
      });
      audioUploadButton.disabled = !state.audio_storage_ready || state.horn_synth_active;
      hornShortButton.disabled = state.horn_web_mode === 'off';
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
      turnSignalFilteredPulseValue.textContent = state.turn_signal_input_filtered_pulse_us > 0 ? state.turn_signal_input_filtered_pulse_us + ' us' : '--';
      turnSignalFilterValue.textContent = state.turn_signal_input_filter_ready ? 'Ready' : 'Collecting';
      headlightStateValue.textContent = state.headlight_status;
      headlightPulseValue.textContent = state.headlight_pulse_us > 0 ? state.headlight_pulse_us + ' us' : '--';
      headlightFilteredPulseValue.textContent = state.headlight_filtered_pulse_us > 0 ? state.headlight_filtered_pulse_us + ' us' : '--';
      headlightFilterValue.textContent = state.headlight_filter_ready ? 'Ready' : 'Collecting';
      headlightPulseAgeValue.textContent = state.headlight_pulse_us > 0 ? state.headlight_pulse_age_ms + ' ms' : '--';
      soundSwitchStatusValue.textContent = state.sound_switch_status;
      soundSwitchPulseValue.textContent = state.sound_switch_pulse_us > 0 ? state.sound_switch_pulse_us + ' us' : '--';
      soundSwitchFilteredPulseValue.textContent = state.sound_switch_filtered_pulse_us > 0 ? state.sound_switch_filtered_pulse_us + ' us' : '--';
      soundSwitchFilterValue.textContent = state.sound_switch_filter_ready ? 'Ready' : 'Collecting';
      soundSwitchPulseAgeValue.textContent = state.sound_switch_pulse_us > 0 ? state.sound_switch_pulse_age_ms + ' ms' : '--';
      throttleStatusValue.textContent = state.throttle_status;
      throttlePulseValue.textContent = state.throttle_pulse_us > 0 ? state.throttle_pulse_us + ' us' : '--';
      throttleRawPulseValue.textContent = state.throttle_raw_pulse_us > 0 ? state.throttle_raw_pulse_us + ' us' : '--';
      throttleFilterValue.textContent = state.throttle_filter_ready ? 'Ready' : 'Collecting';
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

    function setHornMode(mode) {
      const params = new URLSearchParams();
      params.set('mode', mode);
      fetch('/audio/horn', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
      })
        .then(handleAudioJsonResponse)
        .catch(() => {
          setAudioMessage('Horn mode request failed.');
          pollSteeringState();
        });
    }

    function triggerShortHorn() {
      fetch('/audio/horn/short', { method: 'POST' })
        .then(handleAudioJsonResponse)
        .catch(() => {
          setAudioMessage('Horn honk request failed.');
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
    wifiScanButton.addEventListener('click', scanWifiNetworks);
    wifiConnectButton.addEventListener('click', connectWifiNetwork);
    wifiDisconnectButton.addEventListener('click', disconnectWifiNetwork);
    audioUploadButton.addEventListener('click', uploadBrowserAudio);
    hornShortButton.addEventListener('click', triggerShortHorn);
    hornModeButtons.forEach(button => {
      button.addEventListener('click', () => setHornMode(button.dataset.hornMode));
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
  html.replace("HORN_MODE", hornModeValue);
  html.replace("HORN_RC_REQUESTED", hornRcRequestedValue);
  html.replace("HORN_REQUESTED", hornRequestedValue);
  html.replace("HORN_SYNTH_ACTIVE", hornSynthActiveValue);
  html.replace("HORN_SYNTH_STATUS", hornSynthStatusValue);
  html.replace("TURN_THRESHOLD", turnThresholdValue);
  html.replace("TURN_SIGNAL_VALUE", turnSignalValue);
  html.replace("TURN_SIGNAL_OUTPUT", turnSignalOutputValue);
  html.replace("TURN_SIGNAL_INPUT_STATUS", turnSignalInputStatusValue);
  html.replace(
    "TURN_SIGNAL_INPUT_PULSE",
    turnSignalInputPulseValue == "--" ? turnSignalInputPulseValue : turnSignalInputPulseValue + " us"
  );
  html.replace(
    "TURN_SIGNAL_FILTERED_PULSE",
    turnSignalFilteredPulseValue == "--" ? turnSignalFilteredPulseValue : turnSignalFilteredPulseValue + " us"
  );
  html.replace("TURN_SIGNAL_FILTER", turnSignalFilterValue);
  html.replace("HEADLIGHT_STATE", headlightStateValue);
  html.replace("HEADLIGHT_PULSE_AGE", headlightPulseAgeValue == "--" ? headlightPulseAgeValue : headlightPulseAgeValue + " ms");
  html.replace("HEADLIGHT_PULSE", headlightPulseValue == "--" ? headlightPulseValue : headlightPulseValue + " us");
  html.replace("HEADLIGHT_FILTERED_PULSE", headlightFilteredPulseValue == "--" ? headlightFilteredPulseValue : headlightFilteredPulseValue + " us");
  html.replace("HEADLIGHT_FILTER", headlightFilterValue);
  html.replace("SOUND_SWITCH_STATUS", soundSwitchStatusValue);
  html.replace("SOUND_SWITCH_PULSE_AGE", soundSwitchPulseAgeValue == "--" ? soundSwitchPulseAgeValue : soundSwitchPulseAgeValue + " ms");
  html.replace("SOUND_SWITCH_PULSE", soundSwitchPulseValue == "--" ? soundSwitchPulseValue : soundSwitchPulseValue + " us");
  html.replace("SOUND_SWITCH_FILTERED_PULSE", soundSwitchFilteredPulseValue == "--" ? soundSwitchFilteredPulseValue : soundSwitchFilteredPulseValue + " us");
  html.replace("SOUND_SWITCH_FILTER", soundSwitchFilterValue);
  html.replace("THROTTLE_STATUS", throttleStatusValue);
  html.replace("THROTTLE_PULSE_AGE", throttlePulseAgeValue == "--" ? throttlePulseAgeValue : throttlePulseAgeValue + " ms");
  html.replace("THROTTLE_PULSE", throttlePulseValue == "--" ? throttlePulseValue : throttlePulseValue + " us");
  html.replace("THROTTLE_RAW_PULSE", throttleRawPulseValue == "--" ? throttleRawPulseValue : throttleRawPulseValue + " us");
  html.replace("THROTTLE_FILTER", throttleFilterValue);
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

String controllerStateJson() {
  String json = "{";
  json += "\"armed\":";
  json += vehicleControl.armed ? "true" : "false";
  json += ",\"mode\":\"";
  json += vehicleControlModeValue();
  json += "\"";
  json += ",\"status\":\"";
  json += jsonEscape(vehicleControlStatusLabel());
  json += "\"";
  json += ",\"steering\":";
  json += String(vehicleControl.steeringPercent);
  json += ",\"throttle\":";
  json += String(vehicleControl.throttlePercent);
  json += ",\"steering_pulse_us\":";
  json += String(vehicleControl.steeringPulseUs);
  json += ",\"throttle_pulse_us\":";
  json += String(vehicleControl.throttlePulseUs);
  json += ",\"heartbeat_age_ms\":";
  json += String(vehicleControlHeartbeatAgeMs());
  json += ",\"watchdog_timeout_ms\":";
  json += String(VEHICLE_CONTROL_WATCHDOG_MS);
  json += ",\"calibration_valid\":";
  json += steeringCalibrationValid() ? "true" : "false";
  json += "}";
  return json;
}

void sendControllerError(int code, const String &message) {
  String json = "{\"error\":\"";
  json += jsonEscape(message);
  json += "\"}";
  server.send(code, "application/json", json);
}

bool parseControllerPercent(const String &rawValue, int &percent) {
  String value = rawValue;
  value.trim();
  if (value.length() == 0) {
    return false;
  }

  int index = 0;
  bool negative = false;
  if (value.charAt(0) == '-') {
    negative = true;
    index = 1;
  } else if (value.charAt(0) == '+') {
    index = 1;
  }

  if (index >= value.length()) {
    return false;
  }

  int parsed = 0;
  for (; index < value.length(); index++) {
    char character = value.charAt(index);
    if (character < '0' || character > '9') {
      return false;
    }
    parsed = (parsed * 10) + (character - '0');
    if (parsed > 100) {
      return false;
    }
  }

  percent = negative ? -parsed : parsed;
  return percent >= -100 && percent <= 100;
}

void handleControllerPage() {
  server.send(200, "text/html", controllerPage());
}

void handleControllerState() {
  server.send(200, "application/json", controllerStateJson());
}

void handleControllerArm() {
  if (!armVehicleControl()) {
    sendControllerError(409, "Web control could not be activated; check steering calibration and PWM setup");
    return;
  }

  server.send(200, "application/json", controllerStateJson());
}

void handleControllerCommand() {
  if (!vehicleControl.armed) {
    sendControllerError(409, "Controller is not armed");
    return;
  }
  if (!server.hasArg("steering") || !server.hasArg("throttle")) {
    sendControllerError(400, "steering and throttle are required");
    return;
  }

  int steeringPercent = 0;
  int throttlePercent = 0;
  if (!parseControllerPercent(server.arg("steering"), steeringPercent) ||
      !parseControllerPercent(server.arg("throttle"), throttlePercent)) {
    sendControllerError(400, "steering and throttle must be integers from -100 to 100");
    return;
  }

  setVehicleControlCommand(steeringPercent, throttlePercent);
  vehicleControl.lastHeartbeatMs = millis();
  vehicleControl.watchdogStopped = false;
  server.send(200, "application/json", controllerStateJson());
}

void handleControllerStop() {
  stopVehicleControl(false);
  server.send(200, "application/json", controllerStateJson());
}

String controllerPage() {
  return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <title>Car Controller</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: Arial, sans-serif;
      background: #080b10;
      color: #f8fafc;
      display: grid;
      place-items: center;
      padding: 18px;
    }
    main { width: min(100%, 520px); }
    h1 { margin: 0; font-size: 1.7rem; }
    .lede { margin: 8px 0 18px; color: #a9b4c3; line-height: 1.4; }
    .panel {
      background: #131923;
      border: 1px solid #283445;
      border-radius: 18px;
      padding: 16px;
      box-shadow: 0 18px 44px rgba(0, 0, 0, .35);
    }
    .status {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-bottom: 16px;
    }
    .metric {
      background: #0b1119;
      border: 1px solid #223043;
      border-radius: 12px;
      padding: 11px;
      min-height: 66px;
    }
    .label { color: #92a0b3; display: block; font-size: .72rem; letter-spacing: .08em; text-transform: uppercase; }
    .metric strong { display: block; margin-top: 5px; font-size: 1.05rem; }
    button {
      min-height: 52px;
      border: 0;
      border-radius: 12px;
      padding: 12px 16px;
      color: white;
      background: #2563eb;
      font-size: 1rem;
      font-weight: 700;
      touch-action: manipulation;
    }
    button:disabled { opacity: .45; }
    #stopButton { background: #dc2626; }
    .actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 18px; }
    .joystick-wrap { display: grid; justify-items: center; gap: 12px; }
    #joystick {
      width: min(78vw, 330px);
      aspect-ratio: 1;
      position: relative;
      border-radius: 50%;
      border: 2px solid #3c4d64;
      background: radial-gradient(circle at 50% 50%, #1c293a 0 8%, #111924 9% 100%);
      touch-action: none;
      user-select: none;
      opacity: .5;
    }
    #joystick::before, #joystick::after { content: ""; position: absolute; background: #314056; }
    #joystick::before { height: 1px; left: 10%; right: 10%; top: 50%; }
    #joystick::after { width: 1px; top: 10%; bottom: 10%; left: 50%; }
    #joystick.armed { opacity: 1; }
    #knob {
      width: 30%;
      aspect-ratio: 1;
      position: absolute;
      left: 50%;
      top: 50%;
      border-radius: 50%;
      transform: translate(-50%, -50%);
      background: #38bdf8;
      border: 4px solid #dbeafe;
      box-shadow: 0 5px 16px rgba(0, 0, 0, .4);
      pointer-events: none;
    }
    .directions { color: #9caabd; display: flex; justify-content: space-between; width: min(78vw, 330px); font-size: .82rem; }
    .notice { min-height: 20px; margin: 16px 0 0; color: #c8d4e3; text-align: center; line-height: 1.35; }
    .back { display: block; color: #7dd3fc; margin-top: 18px; text-align: center; }
  </style>
</head>
<body>
  <main>
    <h1>Car Controller</h1>
    <p class="lede">Receiver input is the default. Activating web control makes the ESP32 drive these wires, so do not arm while the receiver is actively driving them.</p>
    <section class="panel">
      <div class="status">
        <div class="metric"><span class="label">Controller</span><strong id="statusValue">Loading…</strong></div>
        <div class="metric"><span class="label">Heartbeat</span><strong id="heartbeatValue">—</strong></div>
        <div class="metric"><span class="label">Steering</span><strong id="steeringValue">0%</strong></div>
        <div class="metric"><span class="label">Throttle</span><strong id="throttleValue">0%</strong></div>
      </div>
      <div class="actions">
        <button id="armButton" type="button">Activate Web Control</button>
        <button id="stopButton" type="button" disabled>Receiver Active</button>
      </div>
      <div class="joystick-wrap">
        <div id="joystick" role="application" aria-label="Steering and throttle joystick"><div id="knob"></div></div>
        <div class="directions"><span>Left</span><span>Forward ↑ / Reverse ↓</span><span>Right</span></div>
      </div>
      <p class="notice" id="noticeValue">Receiver control is active.</p>
    </section>
    <a class="back" href="/">Back to dashboard</a>
  </main>
  <script>
    const armButton = document.getElementById('armButton');
    const stopButton = document.getElementById('stopButton');
    const joystick = document.getElementById('joystick');
    const knob = document.getElementById('knob');
    const statusValue = document.getElementById('statusValue');
    const heartbeatValue = document.getElementById('heartbeatValue');
    const steeringValue = document.getElementById('steeringValue');
    const throttleValue = document.getElementById('throttleValue');
    const noticeValue = document.getElementById('noticeValue');
    let armed = false;
    let steering = 0;
    let throttle = 0;
    let activePointer = null;
    let commandInFlight = false;
    let commandQueued = false;

    function clamp(value, min, max) { return Math.max(min, Math.min(max, value)); }
    function setNotice(message) { noticeValue.textContent = message; }

    function updateJoystick() {
      knob.style.left = (50 + steering * 0.38) + '%';
      knob.style.top = (50 - throttle * 0.38) + '%';
      steeringValue.textContent = steering + '%';
      throttleValue.textContent = throttle + '%';
    }

    function applyState(state) {
      armed = Boolean(state.armed);
      const webMode = state.mode === 'web';
      joystick.classList.toggle('armed', armed);
      armButton.disabled = armed || !state.calibration_valid;
      armButton.textContent = armed ? 'Web Control Active' : 'Activate Web Control';
      stopButton.disabled = !webMode;
      stopButton.textContent = webMode ? 'Return to Receiver' : 'Receiver Active';
      statusValue.textContent = state.status || (armed ? 'Armed' : 'Ready');
      heartbeatValue.textContent = armed ? (state.heartbeat_age_ms + ' ms') : '—';
      if (!armed) {
        steering = 0;
        throttle = 0;
        updateJoystick();
      }
    }

    async function request(path, options) {
      const response = await fetch(path, options);
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || 'Request failed');
      return data;
    }

    async function armControl() {
      try {
        const state = await request('/controller/arm', { method: 'POST' });
        applyState(state);
        setNotice('Web output is active. Hold and drag the joystick to drive.');
      } catch (error) {
        setNotice(error.message || 'Unable to arm controller.');
      }
    }

    function sendCommand() {
      if (!armed) return;
      if (commandInFlight) {
        commandQueued = true;
        return;
      }
      commandInFlight = true;
      const params = new URLSearchParams({ steering: String(steering), throttle: String(throttle) });
      request('/controller/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
      }).then(applyState).catch(error => {
        armed = false;
        joystick.classList.remove('armed');
        setNotice(error.message || 'Controller command failed.');
      }).finally(() => {
        commandInFlight = false;
        if (commandQueued) {
          commandQueued = false;
          sendCommand();
        }
      });
    }

    async function stopControl() {
      armed = false;
      activePointer = null;
      steering = 0;
      throttle = 0;
      updateJoystick();
      try {
        const state = await request('/controller/stop', { method: 'POST' });
        applyState(state);
        setNotice('Web output stopped. Receiver input is active.');
      } catch (error) {
        setNotice(error.message || 'Stop request failed.');
      }
    }

    function updateFromPointer(event) {
      const rect = joystick.getBoundingClientRect();
      const radius = rect.width / 2;
      const x = clamp((event.clientX - (rect.left + radius)) / (radius * 0.76), -1, 1);
      const y = clamp((event.clientY - (rect.top + radius)) / (radius * 0.76), -1, 1);
      steering = Math.round(x * 100);
      throttle = Math.round(-y * 100);
      updateJoystick();
      sendCommand();
    }

    function releaseJoystick(event) {
      if (activePointer !== null && event.pointerId !== activePointer) return;
      activePointer = null;
      steering = 0;
      throttle = 0;
      updateJoystick();
      sendCommand();
    }

    joystick.addEventListener('pointerdown', event => {
      if (!armed) {
        setNotice('Arm Control before driving.');
        return;
      }
      activePointer = event.pointerId;
      joystick.setPointerCapture(event.pointerId);
      updateFromPointer(event);
    });
    joystick.addEventListener('pointermove', event => {
      if (event.pointerId === activePointer) updateFromPointer(event);
    });
    joystick.addEventListener('pointerup', releaseJoystick);
    joystick.addEventListener('pointercancel', releaseJoystick);
    armButton.addEventListener('click', armControl);
    stopButton.addEventListener('click', stopControl);
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) stopControl();
    });
    window.addEventListener('pagehide', () => {
      const body = new Blob([''], { type: 'application/x-www-form-urlencoded' });
      navigator.sendBeacon('/controller/stop', body);
    });
    setInterval(sendCommand, 100);
    setInterval(() => {
      request('/controller/state').then(applyState).catch(() => {});
    }, 250);
    request('/controller/state').then(applyState).catch(() => setNotice('Controller state is unavailable.'));
    updateJoystick();
  </script>
</body>
</html>
)HTML";
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
  if (vehicleControl.mode == VehicleControlMode::Receiver ?
      steeringReceiverInput.pulseFresh :
      vehicleControl.armed) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"turn_signal_input_pulse_us\":";
  json += String(
    vehicleControl.mode == VehicleControlMode::Receiver ?
      steeringReceiverInput.pulseWidthUs :
      vehicleControl.steeringPulseUs
  );
  json += ",\"turn_signal_input_filtered_pulse_us\":";
  json += String(vehicleControl.steeringPulseUs);
  json += ",\"turn_signal_input_filter_ready\":";
  if (vehicleControl.mode == VehicleControlMode::Receiver ?
      steeringReceiverInput.filterReady :
      vehicleControl.armed) {
    json += "true";
  } else {
    json += "false";
  }
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
  json += ",\"headlight_filtered_pulse_us\":";
  json += String(headlightInput.filteredPulseWidthUs);
  json += ",\"headlight_filter_ready\":";
  if (headlightInput.filterReady) {
    json += "true";
  } else {
    json += "false";
  }
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
  json += ",\"sound_switch_filtered_pulse_us\":";
  json += String(soundSwitchInput.filteredPulseWidthUs);
  json += ",\"sound_switch_filter_ready\":";
  if (soundSwitchInput.filterReady) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"sound_switch_pulse_age_ms\":";
  json += String(soundSwitchInput.pulseAgeMs);
  json += ",\"horn_web_mode\":\"";
  json += hornWebModeValue();
  json += "\"";
  json += ",\"horn_rc_requested\":";
  if (rcHornRequested) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"horn_requested\":";
  if (hornPlaybackRequested()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"horn_synth_active\":";
  if (hornSynthIsActive()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"horn_synth_status\":\"";
  json += jsonEscape(hornSynthStatusLabel());
  json += "\"";
  json += ",\"throttle_status\":\"";
  json += jsonEscape(throttleInputStatusLabel());
  json += "\"";
  json += ",\"throttle_pulse_fresh\":";
  if (vehicleControl.mode == VehicleControlMode::Receiver ?
      throttleReceiverInput.pulseFresh :
      vehicleControl.armed) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"throttle_pulse_us\":";
  json += String(vehicleControl.throttlePulseUs);
  json += ",\"throttle_raw_pulse_us\":";
  json += String(
    vehicleControl.mode == VehicleControlMode::Receiver ?
      throttleReceiverInput.pulseWidthUs :
      0
  );
  json += ",\"throttle_filter_ready\":";
  if (vehicleControl.mode == VehicleControlMode::Receiver ?
      throttleReceiverInput.filterReady :
      vehicleControl.armed) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"throttle_pulse_age_ms\":";
  json += String(
    vehicleControl.mode == VehicleControlMode::Receiver ?
      throttleReceiverInput.pulseAgeMs :
      vehicleControlHeartbeatAgeMs()
  );
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
    if (!steeringCalibrationValid()) {
      stopVehicleControl(false);
    } else {
      setVehicleControlCommand(vehicleControl.steeringPercent, vehicleControl.throttlePercent);
    }
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
