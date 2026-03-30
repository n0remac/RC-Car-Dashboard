String htmlPage() {
  String environmentSensor = bmeAvailable ? String("Online (") + bmeAddressLabel() + ")" : String("Offline");
  String tiltOrientation = tiltOrientationName();
  String pitchInvert = onOffLabel(invertPitchAxis);
  String rollInvert = onOffLabel(invertRollAxis);
  String axisLabels = onOffLabel(showTiltAxisLabels);
  String tolerance = String(tiltBubbleToleranceDeg, 1) + " deg";
  String axisLabelButton = showTiltAxisLabels ? "Hide Axis Labels" : "Show Axis Labels";
  String temperatureValue = bmeAvailable ? String(environmentTempC, 1) + " C" : String("Unavailable");
  String pitchValue = String(pitchDeg, 1) + " deg";
  String rollValue = String(rollDeg, 1) + " deg";

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
      The display now stays on a single combined dashboard panel. Tilt and temperature are live; the other indicators remain fixed demo values for layout work.
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
        <span class="label">Tilt Orientation</span>
        TILT_ORIENTATION
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

    <div class="buttons">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_reset" value="1">
        <button class="warning" type="submit">Reset Tilt Zero</button>
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
</body>
</html>
)HTML";

  html.replace("ENVIRONMENT_SENSOR", environmentSensor);
  html.replace("TEMPERATURE_VALUE", temperatureValue);
  html.replace("TILT_ORIENTATION", tiltOrientation);
  html.replace("PITCH_VALUE", pitchValue);
  html.replace("ROLL_VALUE", rollValue);
  html.replace("PITCH_INVERT", pitchInvert);
  html.replace("ROLL_INVERT", rollInvert);
  html.replace("AXIS_LABELS", axisLabels);
  html.replace("TILT_TOLERANCE", tolerance);
  html.replace("AXIS_LABEL_BUTTON", axisLabelButton);
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSet() {
  if (server.hasArg("tilt_rotation")) {
    int requestedRotation = server.arg("tilt_rotation").toInt();
    if (requestedRotation == 0 || requestedRotation == 90 ||
        requestedRotation == 180 || requestedRotation == 270) {
      tiltOrientationDeg = requestedRotation;
      applyTiltOrientation();
    }
  }

  if (server.hasArg("tilt_reset")) {
    resetTiltReference();
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

  renderCurrentScreen();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "OK");
}
