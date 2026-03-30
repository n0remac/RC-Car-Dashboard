String htmlPage() {
  String modeName = screenModeName();
  String environmentSensor = bmeAvailable ? String("Online (") + bmeAddressLabel() + ")" : String("Offline");
  String tiltOrientation = tiltOrientationName();
  String pitchInvert = onOffLabel(invertPitchAxis);
  String rollInvert = onOffLabel(invertRollAxis);
  String axisLabels = onOffLabel(showTiltAxisLabels);
  String tolerance = String(tiltBubbleToleranceDeg, 1) + " deg";
  String axisLabelButton = showTiltAxisLabels ? "Hide Axis Labels" : "Show Axis Labels";

  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CarRadio Control</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #111;
      color: #eee;
      margin: 0;
      padding: 20px;
    }
    .card {
      max-width: 480px;
      margin: 0 auto;
      background: #1c1c1c;
      border-radius: 14px;
      padding: 20px;
      box-shadow: 0 0 20px rgba(0, 0, 0, 0.4);
    }
    h1 {
      margin-top: 0;
      color: #6ee7ff;
    }
    .status {
      margin: 16px 0;
      padding: 12px;
      border-radius: 10px;
      background: #2a2a2a;
    }
    .buttons {
      display: grid;
      gap: 12px;
      margin-top: 18px;
    }
    .subhead {
      margin: 20px 0 8px;
      color: #bbb;
      font-size: 14px;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    .button-row {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 10px;
    }
    button {
      width: 100%;
      border: none;
      border-radius: 10px;
      padding: 16px;
      font-size: 18px;
      cursor: pointer;
      background: #2f80ed;
      color: white;
    }
    button.secondary {
      background: #27ae60;
    }
    button.tertiary {
      background: #f39c12;
    }
    button.info {
      background: #0f766e;
    }
    button.quaternary {
      background: #8e44ad;
    }
    button.warning {
      background: #c0392b;
    }
    .small {
      color: #aaa;
      margin-top: 16px;
      font-size: 14px;
    }
    a {
      color: #6ee7ff;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>CarRadio</h1>
    <div class="status">
      <strong>Current Screen:</strong> MODE_NAME
    </div>

    <div class="status">
      <strong>Tilt Orientation:</strong> TILT_ORIENTATION
    </div>

    <div class="status">
      <strong>Environment Sensor:</strong> ENVIRONMENT_SENSOR
    </div>

    <div class="status">
      <strong>Pitch Invert:</strong> PITCH_INVERT<br>
      <strong>Roll Invert:</strong> ROLL_INVERT<br>
      <strong>Axis Labels:</strong> AXIS_LABELS<br>
      <strong>Bubble Tolerance:</strong> TILT_TOLERANCE
    </div>

    <div class="buttons">
      <form action="/set" method="get">
        <input type="hidden" name="screen" value="gauge">
        <button type="submit">Show Gauge</button>
      </form>

      <form action="/set" method="get">
        <input type="hidden" name="screen" value="status">
        <button class="secondary" type="submit">Show Status</button>
      </form>

      <form action="/set" method="get">
        <input type="hidden" name="screen" value="environment">
        <button class="info" type="submit">Show Environment</button>
      </form>

      <form action="/set" method="get">
        <input type="hidden" name="screen" value="tilt">
        <button class="tertiary" type="submit">Show Tilt</button>
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

    <div class="buttons">
      <form action="/set" method="get">
        <input type="hidden" name="tilt_reset" value="1">
        <button class="warning" type="submit">Reset Tilt Zero</button>
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
        <button class="quaternary" type="submit">AXIS_LABEL_BUTTON</button>
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
      <a href="/">192.168.4.1</a>
    </div>
  </div>
</body>
</html>
)HTML";

  html.replace("MODE_NAME", modeName);
  html.replace("ENVIRONMENT_SENSOR", environmentSensor);
  html.replace("TILT_ORIENTATION", tiltOrientation);
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
  if (server.hasArg("screen")) {
    String screen = server.arg("screen");

    if (screen == "gauge") {
      currentScreen = SCREEN_GAUGE;
    } else if (screen == "status") {
      currentScreen = SCREEN_STATUS;
    } else if (screen == "environment") {
      currentScreen = SCREEN_ENVIRONMENT;
    } else if (screen == "tilt") {
      currentScreen = SCREEN_TILT;
    }
  }

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
