#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// ----------------------
// WiFi AP config
// ----------------------
const char *AP_SSID = "CarRadio";
const char *AP_PASSWORD = "carradio123";

// ----------------------
// Screen config
// ----------------------
static const int SCREEN_W = 240;
static const int SCREEN_H = 135;

// ----------------------
// ADC input config
// ----------------------
static const int SIGNAL_PIN = 32;

// Use ADC_11db so spikes up toward 3.3V are visible.
// If you only care about the tiny 0-500mV steering signal,
// ADC_0db gives better low-voltage detail but may hide/clamp large spikes.
static const adc_attenuation_t ADC_ATTENUATION = ADC_11db;

// ----------------------
// Oscilloscope config
// ----------------------
static const int SAMPLE_COUNT = 240;          // one x-pixel per sample
static const int SAMPLE_INTERVAL_US = 500;    // 500us = 0.5ms per sample
                                             // 240 samples = 120ms window

static const int GRAPH_X = 0;
static const int GRAPH_Y = 28;
static const int GRAPH_W = 240;
static const int GRAPH_H = 92;

// Display modes
bool autoScale = true;
int manualScaleMaxMv = 3300;

// ----------------------
// Runtime state
// ----------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WebServer server(80);

int samplesMv[SAMPLE_COUNT];

int minMv = 9999;
int maxMv = 0;
int avgMv = 0;
int latestMv = 0;

unsigned long captureCount = 0;
unsigned long lastScreenMs = 0;
static const unsigned long SCREEN_INTERVAL_MS = 100;

// ----------------------
// Helpers
// ----------------------
int clampInt(int value, int low, int high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

String boolJson(bool value) {
  return value ? "true" : "false";
}

void resetStats() {
  minMv = 9999;
  maxMv = 0;
  avgMv = 0;
  latestMv = 0;
}

void captureSamples() {
  resetStats();

  long total = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int mv = analogReadMilliVolts(SIGNAL_PIN);

    samplesMv[i] = mv;
    latestMv = mv;

    if (mv < minMv) minMv = mv;
    if (mv > maxMv) maxMv = mv;

    total += mv;

    delayMicroseconds(SAMPLE_INTERVAL_US);
  }

  avgMv = total / SAMPLE_COUNT;
  captureCount++;
}

int mvToY(int mv, int scaleMinMv, int scaleMaxMv) {
  if (scaleMaxMv <= scaleMinMv) {
    scaleMaxMv = scaleMinMv + 1;
  }

  mv = clampInt(mv, scaleMinMv, scaleMaxMv);

  float normalized = (float)(mv - scaleMinMv) / (float)(scaleMaxMv - scaleMinMv);

  // Higher voltage should draw higher on screen, so invert Y.
  int y = GRAPH_Y + GRAPH_H - 1 - (int)(normalized * (GRAPH_H - 1));

  return clampInt(y, GRAPH_Y, GRAPH_Y + GRAPH_H - 1);
}

void drawGrid(int scaleMinMv, int scaleMaxMv) {
  spr.drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_DARKGREY);

  // Horizontal grid lines
  for (int i = 1; i < 4; i++) {
    int y = GRAPH_Y + (GRAPH_H * i) / 4;
    spr.drawFastHLine(GRAPH_X, y, GRAPH_W, TFT_DARKGREY);
  }

  // Vertical grid lines
  for (int i = 1; i < 6; i++) {
    int x = GRAPH_X + (GRAPH_W * i) / 6;
    spr.drawFastVLine(x, GRAPH_Y, GRAPH_H, TFT_DARKGREY);
  }

  // Scale labels
  spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  spr.setTextSize(1);
  spr.drawString(String(scaleMaxMv) + "mV", 2, GRAPH_Y + 2, 1);
  spr.drawString(String(scaleMinMv) + "mV", 2, GRAPH_Y + GRAPH_H - 10, 1);
}

void drawWaveform(int scaleMinMv, int scaleMaxMv) {
  for (int i = 1; i < SAMPLE_COUNT; i++) {
    int x0 = i - 1;
    int x1 = i;

    int y0 = mvToY(samplesMv[i - 1], scaleMinMv, scaleMaxMv);
    int y1 = mvToY(samplesMv[i], scaleMinMv, scaleMaxMv);

    spr.drawLine(x0, y0, x1, y1, TFT_GREEN);
  }
}

void renderScreen() {
  spr.fillSprite(TFT_BLACK);

  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString("ADC OSCILLOSCOPE GPIO32", 4, 2, 2);

  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString("Now " + String(latestMv) + "mV", 4, 16, 1);

  spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  spr.drawString("Min " + String(minMv) + " Max " + String(maxMv) + " Avg " + String(avgMv), 92, 16, 1);

  int scaleMinMv = 0;
  int scaleMaxMv = manualScaleMaxMv;

  if (autoScale) {
    int padding = 50;
    scaleMinMv = minMv - padding;
    scaleMaxMv = maxMv + padding;

    if (scaleMinMv < 0) scaleMinMv = 0;
    if (scaleMaxMv < 100) scaleMaxMv = 100;
    if (scaleMaxMv > 3300) scaleMaxMv = 3300;
  }

  drawGrid(scaleMinMv, scaleMaxMv);
  drawWaveform(scaleMinMv, scaleMaxMv);

  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString(autoScale ? "AUTO" : "0-3300mV", 4, 123, 1);
  spr.drawString("Window " + String((SAMPLE_COUNT * SAMPLE_INTERVAL_US) / 1000) + "ms", 60, 123, 1);
  spr.drawString("AP 192.168.4.1", 150, 123, 1);

  spr.pushSprite(0, 0);
}

// ----------------------
// Web UI
// ----------------------
String jsonState() {
  String json = "{";

  json += "\"latest_mv\":" + String(latestMv) + ",";
  json += "\"min_mv\":" + String(minMv) + ",";
  json += "\"max_mv\":" + String(maxMv) + ",";
  json += "\"avg_mv\":" + String(avgMv) + ",";
  json += "\"sample_count\":" + String(SAMPLE_COUNT) + ",";
  json += "\"sample_interval_us\":" + String(SAMPLE_INTERVAL_US) + ",";
  json += "\"window_ms\":" + String((SAMPLE_COUNT * SAMPLE_INTERVAL_US) / 1000) + ",";
  json += "\"auto_scale\":" + boolJson(autoScale) + ",";
  json += "\"capture_count\":" + String(captureCount) + ",";
  json += "\"samples\":[";

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    if (i > 0) json += ",";
    json += String(samplesMv[i]);
  }

  json += "]}";

  return json;
}

String htmlPage() {
  String html = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 ADC Oscilloscope</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: system-ui, sans-serif;
      background: #111;
      color: #eee;
      margin: 20px;
    }
    .card {
      background: #1b1b1b;
      border: 1px solid #333;
      border-radius: 12px;
      padding: 16px;
      max-width: 760px;
      margin-bottom: 16px;
    }
    canvas {
      width: 100%;
      max-width: 720px;
      height: 280px;
      background: #050505;
      border: 1px solid #444;
      border-radius: 8px;
    }
    .row {
      display: flex;
      gap: 18px;
      flex-wrap: wrap;
      font-size: 1.1rem;
    }
    .value {
      color: #7CFF7C;
      font-weight: 700;
    }
    button {
      padding: 8px 12px;
      font-size: 1rem;
      border-radius: 8px;
      border: 1px solid #555;
      background: #2b2b2b;
      color: #eee;
    }
  </style>
</head>
<body>
  <h1>ESP32 ADC Oscilloscope</h1>

  <div class="card">
    <canvas id="scope" width="720" height="280"></canvas>
  </div>

  <div class="card">
    <div class="row">
      <div>Latest: <span id="latest" class="value">-</span></div>
      <div>Min: <span id="min" class="value">-</span></div>
      <div>Max: <span id="max" class="value">-</span></div>
      <div>Avg: <span id="avg" class="value">-</span></div>
      <div>Window: <span id="window" class="value">-</span></div>
    </div>
    <p>
      <button onclick="toggleScale()">Toggle auto scale</button>
      <span id="scaleMode"></span>
    </p>
  </div>

<script>
let autoScale = true;

function yForMv(mv, minMv, maxMv, h) {
  if (maxMv <= minMv) maxMv = minMv + 1;
  mv = Math.max(minMv, Math.min(maxMv, mv));
  const n = (mv - minMv) / (maxMv - minMv);
  return h - (n * h);
}

function drawScope(samples, minMv, maxMv, autoScaleMode) {
  const canvas = document.getElementById('scope');
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  ctx.fillStyle = '#050505';
  ctx.fillRect(0, 0, w, h);

  let scaleMin = 0;
  let scaleMax = 3300;

  if (autoScaleMode) {
    scaleMin = Math.max(0, minMv - 50);
    scaleMax = Math.min(3300, maxMv + 50);
    if (scaleMax < 100) scaleMax = 100;
  }

  // Grid
  ctx.strokeStyle = '#333';
  ctx.lineWidth = 1;

  for (let i = 1; i < 4; i++) {
    const y = (h * i) / 4;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  for (let i = 1; i < 8; i++) {
    const x = (w * i) / 8;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }

  // Labels
  ctx.fillStyle = '#aaa';
  ctx.font = '12px system-ui';
  ctx.fillText(scaleMax + ' mV', 8, 16);
  ctx.fillText(scaleMin + ' mV', 8, h - 8);

  // Wave
  ctx.strokeStyle = '#65ff65';
  ctx.lineWidth = 2;
  ctx.beginPath();

  samples.forEach((mv, i) => {
    const x = (i / (samples.length - 1)) * w;
    const y = yForMv(mv, scaleMin, scaleMax, h);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

async function refresh() {
  const res = await fetch('/state');
  const s = await res.json();

  document.getElementById('latest').textContent = s.latest_mv + ' mV';
  document.getElementById('min').textContent = s.min_mv + ' mV';
  document.getElementById('max').textContent = s.max_mv + ' mV';
  document.getElementById('avg').textContent = s.avg_mv + ' mV';
  document.getElementById('window').textContent = s.window_ms + ' ms';
  document.getElementById('scaleMode').textContent = autoScale ? 'Auto scale' : 'Fixed 0-3300 mV';

  drawScope(s.samples, s.min_mv, s.max_mv, autoScale);
}

function toggleScale() {
  autoScale = !autoScale;
  refresh();
}

setInterval(refresh, 300);
refresh();
</script>
</body>
</html>
)rawliteral";

  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleState() {
  server.send(200, "application/json", jsonState());
}

void handleSet() {
  if (server.hasArg("autoscale")) {
    autoScale = server.arg("autoscale").toInt() != 0;
  }

  server.send(200, "application/json", jsonState());
}

// ----------------------
// Setup / loop
// ----------------------
void setup() {
  Serial.begin(115200);

  pinMode(SIGNAL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SIGNAL_PIN, ADC_ATTENUATION);

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samplesMv[i] = 0;
  }

  tft.init();
  tft.setRotation(1);

  spr.setColorDepth(16);
  spr.createSprite(SCREEN_W, SCREEN_H);
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/set", handleSet);
  server.begin();

  captureSamples();
  renderScreen();
}

void loop() {
  captureSamples();

  server.handleClient();

  unsigned long now = millis();
  if (now - lastScreenMs >= SCREEN_INTERVAL_MS) {
    lastScreenMs = now;
    renderScreen();
  }

  delay(5);
}