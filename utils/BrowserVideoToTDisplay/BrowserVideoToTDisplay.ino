#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ----------------------
// WiFi AP config
// ----------------------
const char *AP_SSID = "TDisplayVideo";
const char *AP_PASSWORD = "videoplay123";

// ----------------------
// Screen config
// ----------------------
static const int SCREEN_W = 240;
static const int SCREEN_H = 135;
static const int SCREEN_BACKLIGHT_PIN = 4;
static const int SCREEN_BRIGHTNESS_PERCENT = 80;

// ----------------------
// Frame buffer config
// ----------------------
// 240x135 JPEG frames are usually much smaller than this.
// Increase if your browser quality setting is high.
static const size_t MAX_JPEG_SIZE = 70 * 1024;

TFT_eSPI tft = TFT_eSPI();
WebServer server(80);

uint8_t *jpegBuffer = nullptr;
size_t jpegLength = 0;
bool frameReady = false;
bool receivingFrame = false;
bool videoMode = false;

unsigned long lastFrameMs = 0;
unsigned long receivedFrameCount = 0;
unsigned long droppedFrameCount = 0;

// ----------------------
// JPEG draw callback
// ----------------------
bool tftJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft.height()) {
    return false;
  }

  if (x >= tft.width()) {
    return false;
  }

  if ((x + w) > tft.width()) {
    w = tft.width() - x;
  }

  if ((y + h) > tft.height()) {
    h = tft.height() - y;
  }

  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ----------------------
// HTML page
// ----------------------
String htmlPage() {
  return R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>T-Display Browser Video Player</title>
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
      max-width: 620px;
      margin: 0 auto;
      background: #151515;
      border: 1px solid #303030;
      border-radius: 16px;
      padding: 20px;
      box-shadow: 0 18px 40px rgba(0, 0, 0, 0.35);
    }

    h1 {
      margin: 0 0 8px;
      color: #ffffff;
    }

    .lede {
      color: #b9b9b9;
      margin-bottom: 18px;
      line-height: 1.4;
    }

    .panel {
      background: #1d1d1d;
      border: 1px solid #303030;
      border-radius: 12px;
      padding: 14px;
      display: grid;
      gap: 12px;
      margin-top: 14px;
    }

    .status-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }

    .status {
      background: #121212;
      border: 1px solid #2f2f2f;
      border-radius: 10px;
      padding: 10px;
      line-height: 1.35;
    }

    .label {
      display: block;
      color: #9d9d9d;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-bottom: 4px;
    }

    video, canvas {
      width: 100%;
      max-width: 480px;
      background: #000;
      border-radius: 12px;
      border: 1px solid #333;
    }

    canvas {
      image-rendering: pixelated;
    }

    button {
      border: none;
      border-radius: 10px;
      padding: 14px;
      font-size: 16px;
      cursor: pointer;
      background: #2f80ed;
      color: white;
    }

    button.secondary {
      background: #27ae60;
    }

    button.warning {
      background: #c2410c;
    }

    button:disabled {
      opacity: 0.45;
      cursor: not-allowed;
    }

    input[type="file"] {
      padding: 12px;
      background: #101010;
      border: 1px solid #333;
      border-radius: 10px;
      color: #f2f2f2;
    }

    input[type="range"] {
      width: 100%;
    }

    .row {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }

    .value-row {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      color: #d8d8d8;
      font-size: 14px;
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
    <h1>T-Display Browser Video Player</h1>
    <div class="lede">
      Choose a video file from this device. The browser decodes it, converts each frame to a small JPEG, and streams those frames to the ESP32 display.
    </div>

    <div class="panel">
      <label>
        <span class="label">Video File</span>
        <input id="videoFile" type="file" accept="video/*">
      </label>

      <video id="sourceVideo" controls playsinline muted></video>

      <div class="row">
        <button id="startButton" class="secondary" disabled>Start Streaming</button>
        <button id="stopButton" class="warning" disabled>Stop</button>
      </div>
    </div>

    <div class="panel">
      <div class="value-row">
        <span>Target FPS</span>
        <span><span id="fpsValue">8</span> fps</span>
      </div>
      <input id="fpsSlider" type="range" min="1" max="15" step="1" value="8">

      <div class="value-row">
        <span>JPEG Quality</span>
        <span><span id="qualityValue">0.55</span></span>
      </div>
      <input id="qualitySlider" type="range" min="0.20" max="0.90" step="0.05" value="0.55">
    </div>

    <div class="panel">
      <span class="label">240 x 135 Preview Sent to ESP32</span>
      <canvas id="frameCanvas" width="240" height="135"></canvas>
    </div>

    <div class="panel">
      <div class="status-grid">
        <div class="status">
          <span class="label">Streaming</span>
          <span id="streamingValue">No</span>
        </div>
        <div class="status">
          <span class="label">Client Frames Sent</span>
          <span id="sentValue">0</span>
        </div>
        <div class="status">
          <span class="label">Client Errors</span>
          <span id="errorValue">0</span>
        </div>
        <div class="status">
          <span class="label">ESP32 Frames</span>
          <span id="espFrameValue">0</span>
        </div>
        <div class="status">
          <span class="label">ESP32 Drops</span>
          <span id="espDropValue">0</span>
        </div>
        <div class="status">
          <span class="label">Last JPEG Size</span>
          <span id="jpegSizeValue">0 bytes</span>
        </div>
      </div>
    </div>

    <div class="small">
      Connect to Wi-Fi network <strong>TDisplayVideo</strong> and open
      <a href="/">192.168.4.1</a>. Keep this page open while streaming.
    </div>
  </div>

  <script>
    const videoFile = document.getElementById('videoFile');
    const sourceVideo = document.getElementById('sourceVideo');
    const frameCanvas = document.getElementById('frameCanvas');
    const ctx = frameCanvas.getContext('2d', { alpha: false });

    const startButton = document.getElementById('startButton');
    const stopButton = document.getElementById('stopButton');
    const fpsSlider = document.getElementById('fpsSlider');
    const qualitySlider = document.getElementById('qualitySlider');

    const fpsValue = document.getElementById('fpsValue');
    const qualityValue = document.getElementById('qualityValue');
    const streamingValue = document.getElementById('streamingValue');
    const sentValue = document.getElementById('sentValue');
    const errorValue = document.getElementById('errorValue');
    const espFrameValue = document.getElementById('espFrameValue');
    const espDropValue = document.getElementById('espDropValue');
    const jpegSizeValue = document.getElementById('jpegSizeValue');

    let streaming = false;
    let sending = false;
    let sentFrames = 0;
    let errorCount = 0;
    let lastSendMs = 0;
    let objectUrl = null;

    function updateControlLabels() {
      fpsValue.textContent = fpsSlider.value;
      qualityValue.textContent = Number(qualitySlider.value).toFixed(2);
    }

    function updateButtons() {
      const hasVideo = !!sourceVideo.src;
      startButton.disabled = !hasVideo || streaming;
      stopButton.disabled = !streaming;
      streamingValue.textContent = streaming ? 'Yes' : 'No';
    }

    videoFile.addEventListener('change', () => {
      const file = videoFile.files[0];
      if (!file) return;

      if (objectUrl) {
        URL.revokeObjectURL(objectUrl);
      }

      objectUrl = URL.createObjectURL(file);
      sourceVideo.src = objectUrl;
      sourceVideo.load();
      startButton.disabled = false;
      sentFrames = 0;
      errorCount = 0;
      sentValue.textContent = '0';
      errorValue.textContent = '0';
    });

    fpsSlider.addEventListener('input', updateControlLabels);
    qualitySlider.addEventListener('input', updateControlLabels);

    startButton.addEventListener('click', async () => {
      if (!sourceVideo.src) return;

      sentFrames = 0;
      errorCount = 0;
      sentValue.textContent = '0';
      errorValue.textContent = '0';

      streaming = true;
      updateButtons();

      try {
        await fetch('/mode?video=1');
      } catch (err) {
        // Continue anyway. Frame uploads will fail if ESP32 is unavailable.
      }

      try {
        await sourceVideo.play();
      } catch (err) {
        // Some browsers require user gesture; this button click should count.
      }

      requestAnimationFrame(streamLoop);
    });

    stopButton.addEventListener('click', async () => {
      streaming = false;
      updateButtons();

      try {
        await fetch('/mode?video=0');
      } catch (err) {}
    });

    sourceVideo.addEventListener('ended', () => {
      streaming = false;
      updateButtons();
    });

    function drawVideoContain(video, canvas, context) {
      const cw = canvas.width;
      const ch = canvas.height;
      const vw = video.videoWidth || cw;
      const vh = video.videoHeight || ch;

      context.fillStyle = '#000';
      context.fillRect(0, 0, cw, ch);

      const scale = Math.min(cw / vw, ch / vh);
      const dw = Math.round(vw * scale);
      const dh = Math.round(vh * scale);
      const dx = Math.floor((cw - dw) / 2);
      const dy = Math.floor((ch - dh) / 2);

      context.drawImage(video, dx, dy, dw, dh);
    }

    function canvasToJpegBlob(canvas, quality) {
      return new Promise((resolve) => {
        canvas.toBlob(
          blob => resolve(blob),
          'image/jpeg',
          quality
        );
      });
    }

    async function sendCurrentFrame() {
      if (!streaming || sending) return;
      if (sourceVideo.paused || sourceVideo.ended) return;
      if (!sourceVideo.videoWidth || !sourceVideo.videoHeight) return;

      sending = true;

      try {
        drawVideoContain(sourceVideo, frameCanvas, ctx);

        const quality = Number(qualitySlider.value);
        const blob = await canvasToJpegBlob(frameCanvas, quality);

        if (!blob) {
          throw new Error('JPEG encode failed');
        }

        jpegSizeValue.textContent = blob.size + ' bytes';

        const form = new FormData();
        form.append('frame', blob, 'frame.jpg');

        const response = await fetch('/frame', {
          method: 'POST',
          body: form
        });

        if (!response.ok) {
          throw new Error('ESP32 rejected frame');
        }

        sentFrames++;
        sentValue.textContent = String(sentFrames);
      } catch (err) {
        errorCount++;
        errorValue.textContent = String(errorCount);
      } finally {
        sending = false;
      }
    }

    function streamLoop(now) {
      if (!streaming) return;

      const fps = Number(fpsSlider.value);
      const intervalMs = 1000 / fps;

      if ((now - lastSendMs) >= intervalMs) {
        lastSendMs = now;
        sendCurrentFrame();
      }

      requestAnimationFrame(streamLoop);
    }

    async function pollStatus() {
      try {
        const response = await fetch('/status');
        const state = await response.json();
        espFrameValue.textContent = state.frames;
        espDropValue.textContent = state.drops;
      } catch (err) {}
    }

    setInterval(pollStatus, 1000);
    updateControlLabels();
    updateButtons();
    pollStatus();
  </script>
</body>
</html>
)HTML";
}

// ----------------------
// Routes
// ----------------------
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleStatus() {
  String json = "{";
  json += "\"frames\":";
  json += String(receivedFrameCount);
  json += ",\"drops\":";
  json += String(droppedFrameCount);
  json += ",\"video\":";
  json += videoMode ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMode() {
  if (server.hasArg("video")) {
    videoMode = server.arg("video").toInt() == 1;

    if (videoMode) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Waiting for video...", SCREEN_W / 2, SCREEN_H / 2, 2);
      tft.setTextDatum(TL_DATUM);
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Video stopped", SCREEN_W / 2, SCREEN_H / 2, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  server.send(204, "text/plain", "");
}

void handleFrameComplete() {
  server.send(204, "text/plain", "");
}

void handleFrameUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    receivingFrame = true;
    jpegLength = 0;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (!jpegBuffer) {
      droppedFrameCount++;
      return;
    }

    if ((jpegLength + upload.currentSize) > MAX_JPEG_SIZE) {
      jpegLength = 0;
      receivingFrame = false;
      droppedFrameCount++;
      return;
    }

    memcpy(jpegBuffer + jpegLength, upload.buf, upload.currentSize);
    jpegLength += upload.currentSize;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (receivingFrame && jpegLength > 0) {
      frameReady = true;
      receivedFrameCount++;
      lastFrameMs = millis();
    } else {
      droppedFrameCount++;
    }

    receivingFrame = false;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    jpegLength = 0;
    receivingFrame = false;
    droppedFrameCount++;
  }
}

// ----------------------
// Render latest JPEG frame
// ----------------------
void renderLatestFrame() {
  if (!frameReady || !videoMode) {
    return;
  }

  frameReady = false;

  if (!jpegBuffer || jpegLength == 0) {
    return;
  }

  // JPEG was already scaled to 240x135 by the browser.
  // Draw it directly at the top-left corner.
  TJpgDec.drawJpg(0, 0, jpegBuffer, jpegLength);
}

// ----------------------
// Setup / loop
// ----------------------
void setup() {
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  analogWrite(SCREEN_BACKLIGHT_PIN, map(SCREEN_BRIGHTNESS_PERCENT, 0, 100, 0, 255));

  Serial.begin(115200);
  delay(200);

  jpegBuffer = (uint8_t *)malloc(MAX_JPEG_SIZE);
  if (!jpegBuffer) {
    Serial.println("Failed to allocate JPEG buffer");
  }

  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Starting AP...", SCREEN_W / 2, SCREEN_H / 2, 2);
  tft.setTextDatum(TL_DATUM);

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tftJpegOutput);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/mode", HTTP_GET, handleMode);

  server.on(
    "/frame",
    HTTP_POST,
    handleFrameComplete,
    handleFrameUpload);

  server.begin();

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connect WiFi:", SCREEN_W / 2, 35, 2);
  tft.drawString(AP_SSID, SCREEN_W / 2, 58, 2);
  tft.drawString("Open 192.168.4.1", SCREEN_W / 2, 85, 2);
  tft.setTextDatum(TL_DATUM);
}

void loop() {
  server.handleClient();
  renderLatestFrame();
}