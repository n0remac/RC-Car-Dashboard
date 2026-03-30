#include <FS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Arduino_LSM6DSOX.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WebServer server(80);

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
// IMU / I2C config
// ----------------------
static const int SDA_PIN = 21;
static const int SCL_PIN = 22;
static const int SCREEN_BUTTON_PIN = 35;
static const uint8_t BME_ADDRESS_PRIMARY = 0x76;
static const uint8_t BME_ADDRESS_SECONDARY = 0x77;
static const float BME_SEA_LEVEL_HPA = 1013.25f;

// ----------------------
// App state
// ----------------------
enum ScreenMode {
  SCREEN_GAUGE,
  SCREEN_STATUS,
  SCREEN_ENVIRONMENT,
  SCREEN_TILT
};

ScreenMode currentScreen = SCREEN_GAUGE;

// ----------------------
// Gauge state
// ----------------------
static const int CX = 120;
static const int CY = 115;
static const int R_OUTER = 60;
static const int R_INNER = 52;
static const int R_NEEDLE = 45;
static const float START_DEG = 200.0f;
static const float END_DEG = 340.0f;

float needleValue = 0.0f;
float needleStep = 0.01f;

unsigned long lastBlinkToggle = 0;
bool warningOn = true;
const unsigned long BLINK_INTERVAL_MS = 500;
bool lastScreenButtonState = HIGH;
unsigned long lastButtonChangeMs = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 150;

// ----------------------
// Status screen data
// ----------------------
int batteryPercent = 87;
float distanceMiles = 12.4f;
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_INTERVAL_MS = 300;

// ----------------------
// Environment sensor data
// ----------------------
Adafruit_BME280 bme;
bool bmeAvailable = false;
uint8_t bmeAddress = BME_ADDRESS_PRIMARY;
float environmentTempC = 0.0f;
float environmentHumidity = 0.0f;
float environmentPressureHpa = 0.0f;
float environmentAltitudeM = 0.0f;
unsigned long lastEnvironmentUpdate = 0;
const unsigned long ENVIRONMENT_INTERVAL_MS = 1000;

// ----------------------
// Tilt / IMU state
// ----------------------
float ax = 0.0f;
float ay = 0.0f;
float az = 0.0f;
float gx = 0.0f;
float gy = 0.0f;
float gz = 0.0f;

float rawPitchDeg = 0.0f;
float rawRollDeg = 0.0f;
float pitchDeg = 0.0f;
float rollDeg = 0.0f;
int tiltOrientationDeg = 0;
float pitchZeroDeg = 0.0f;
float rollZeroDeg = 0.0f;
bool invertPitchAxis = false;
bool invertRollAxis = true;
bool showTiltAxisLabels = false;
float tiltBubbleToleranceDeg = 1.0f;

float speedMps = 0.0f;
float speedMph = 0.0f;

float accelBiasX = 0.0f;
float accelBiasY = 0.0f;
float accelBiasZ = 0.0f;

unsigned long lastIMUUpdateMs = 0;
unsigned long lastTiltRender = 0;
const unsigned long TILT_RENDER_INTERVAL_MS = 50;

// ----------------------
// Cross-tab declarations
// ----------------------
String htmlPage();
void handleRoot();
void handleSet();

void renderGaugeScreen(TFT_eSprite &s);
void renderStatusScreen(TFT_eSprite &s);
void renderEnvironmentScreen(TFT_eSprite &s);
void renderTiltScreen(TFT_eSprite &s);

bool initBME280();
void calibrateIMU();
void updateEnvironment();
void updateIMU();
void renderCurrentScreen();
void cycleScreen();
void handleScreenButton();
void applyTiltOrientation();
void resetTiltReference();
String bmeAddressLabel();
String screenModeName();
String tiltOrientationName();
String onOffLabel(bool enabled);
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

// ----------------------
// Helpers
// ----------------------
float degToRad(float deg) {
  return deg * 0.0174532925f;
}

int polarX(int cx, int radius, float deg) {
  return cx + (int)(cos(degToRad(deg)) * radius);
}

int polarY(int cy, int radius, float deg) {
  return cy + (int)(sin(degToRad(deg)) * radius);
}

float clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

String screenModeName() {
  if (currentScreen == SCREEN_STATUS) {
    return "Status";
  }
  if (currentScreen == SCREEN_ENVIRONMENT) {
    return "Environment";
  }
  if (currentScreen == SCREEN_TILT) {
    return "Tilt";
  }
  return "Gauge";
}

String bmeAddressLabel() {
  String label = String(bmeAddress, HEX);
  label.toUpperCase();
  if (label.length() < 2) {
    label = "0" + label;
  }
  return "0x" + label;
}

bool initBME280() {
  if (bme.begin(BME_ADDRESS_PRIMARY, &Wire)) {
    bmeAddress = BME_ADDRESS_PRIMARY;
    return true;
  }

  if (bme.begin(BME_ADDRESS_SECONDARY, &Wire)) {
    bmeAddress = BME_ADDRESS_SECONDARY;
    return true;
  }

  return false;
}

void updateEnvironment() {
  if (!bmeAvailable) {
    return;
  }

  float tempC = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressureHpa = bme.readPressure() / 100.0f;
  float altitudeM = bme.readAltitude(BME_SEA_LEVEL_HPA);

  if (isnan(tempC) || isnan(humidity) || isnan(pressureHpa) || isnan(altitudeM)) {
    return;
  }

  environmentTempC = tempC;
  environmentHumidity = humidity;
  environmentPressureHpa = pressureHpa;
  environmentAltitudeM = altitudeM;
}

void calibrateIMU() {
  const int samples = 100;
  float sumX = 0.0f;
  float sumY = 0.0f;
  float sumZ = 0.0f;

  for (int i = 0; i < samples; i++) {
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;

    while (!IMU.accelerationAvailable()) {
      delay(5);
    }

    IMU.readAcceleration(tx, ty, tz);
    sumX += tx;
    sumY += ty;
    sumZ += tz;
    delay(10);
  }

  accelBiasX = sumX / samples;
  accelBiasY = sumY / samples;
  accelBiasZ = sumZ / samples;
}

void updateIMU() {
  unsigned long now = millis();
  float dt = (now - lastIMUUpdateMs) / 1000.0f;
  if (dt <= 0.0f) {
    dt = 0.01f;
  }
  lastIMUUpdateMs = now;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
  }

  rawRollDeg = atan2(ay, az) * 180.0f / PI;
  rawPitchDeg = atan2(-ax, sqrt((ay * ay) + (az * az))) * 180.0f / PI;
  applyTiltOrientation();

  float forwardAccel = ax - accelBiasX;
  if (fabs(forwardAccel) < 0.03f) {
    forwardAccel = 0.0f;
  }

  speedMps += forwardAccel * 9.80665f * dt;

  if (fabs(speedMps) < 0.05f) {
    speedMps = 0.0f;
  }

  if (speedMps < 0.0f) {
    speedMps = 0.0f;
  }

  speedMph = speedMps * 2.23694f;
}

void applyTiltOrientation() {
  float orientedPitch = rawPitchDeg;
  float orientedRoll = rawRollDeg;

  if (tiltOrientationDeg == 90) {
    orientedPitch = rawRollDeg;
    orientedRoll = -rawPitchDeg;
  } else if (tiltOrientationDeg == 180) {
    orientedPitch = -rawPitchDeg;
    orientedRoll = -rawRollDeg;
  } else if (tiltOrientationDeg == 270) {
    orientedPitch = -rawRollDeg;
    orientedRoll = rawPitchDeg;
  }

  pitchDeg = orientedPitch - pitchZeroDeg;
  rollDeg = orientedRoll - rollZeroDeg;

  if (invertPitchAxis) {
    pitchDeg = -pitchDeg;
  }
  if (invertRollAxis) {
    rollDeg = -rollDeg;
  }

  if (fabs(pitchDeg) <= tiltBubbleToleranceDeg) {
    pitchDeg = 0.0f;
  }
  if (fabs(rollDeg) <= tiltBubbleToleranceDeg) {
    rollDeg = 0.0f;
  }
}

void resetTiltReference() {
  float orientedPitch = rawPitchDeg;
  float orientedRoll = rawRollDeg;

  if (tiltOrientationDeg == 90) {
    orientedPitch = rawRollDeg;
    orientedRoll = -rawPitchDeg;
  } else if (tiltOrientationDeg == 180) {
    orientedPitch = -rawPitchDeg;
    orientedRoll = -rawRollDeg;
  } else if (tiltOrientationDeg == 270) {
    orientedPitch = -rawRollDeg;
    orientedRoll = rawPitchDeg;
  }

  pitchZeroDeg = orientedPitch;
  rollZeroDeg = orientedRoll;
  applyTiltOrientation();
}

String tiltOrientationName() {
  return String(tiltOrientationDeg) + " deg";
}

String onOffLabel(bool enabled) {
  if (enabled) {
    return "On";
  }
  return "Off";
}

// ----------------------
// Rendering
// ----------------------
void renderCurrentScreen() {
  if (currentScreen == SCREEN_GAUGE) {
    renderGaugeScreen(spr);
  } else if (currentScreen == SCREEN_STATUS) {
    renderStatusScreen(spr);
  } else if (currentScreen == SCREEN_ENVIRONMENT) {
    renderEnvironmentScreen(spr);
  } else {
    renderTiltScreen(spr);
  }

  spr.pushSprite(0, 0);
}

void cycleScreen() {
  if (currentScreen == SCREEN_GAUGE) {
    currentScreen = SCREEN_STATUS;
  } else if (currentScreen == SCREEN_STATUS) {
    currentScreen = SCREEN_ENVIRONMENT;
  } else if (currentScreen == SCREEN_ENVIRONMENT) {
    currentScreen = SCREEN_TILT;
  } else {
    currentScreen = SCREEN_GAUGE;
  }

  renderCurrentScreen();
}

void handleScreenButton() {
  bool pressed = digitalRead(SCREEN_BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (pressed != lastScreenButtonState && (now - lastButtonChangeMs) >= BUTTON_DEBOUNCE_MS) {
    lastButtonChangeMs = now;
    lastScreenButtonState = pressed;

    if (pressed) {
      cycleScreen();
    }
  }
}

// ----------------------
// Setup / loop
// ----------------------
void setup() {
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(SCREEN_BUTTON_PIN, INPUT);
  lastScreenButtonState = digitalRead(SCREEN_BUTTON_PIN) == LOW;

  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);

  spr.setColorDepth(16);
  spr.createSprite(SCREEN_W, SCREEN_H);
  spr.fillSprite(TFT_BLACK);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!IMU.begin()) {
    spr.fillSprite(TFT_BLACK);
    spr.setTextColor(TFT_RED, TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("IMU init failed", 10, 20, 2);
    spr.drawString("Check wiring", 10, 45, 2);
    spr.pushSprite(0, 0);

    while (true) {
      delay(1000);
    }
  }

  bmeAvailable = initBME280();
  if (bmeAvailable) {
    updateEnvironment();
    lastEnvironmentUpdate = millis();
    Serial.print("BME280 online at ");
    Serial.println(bmeAddressLabel());
  } else {
    Serial.println("BME280 not detected on 0x76 or 0x77");
  }

  calibrateIMU();
  lastIMUUpdateMs = millis();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  renderCurrentScreen();
}

void loop() {
  server.handleClient();
  handleScreenButton();
  updateIMU();

  unsigned long now = millis();

  if ((now - lastEnvironmentUpdate) >= ENVIRONMENT_INTERVAL_MS) {
    lastEnvironmentUpdate = now;
    updateEnvironment();

    if (currentScreen == SCREEN_ENVIRONMENT) {
      renderCurrentScreen();
    }
  }

  if (currentScreen == SCREEN_GAUGE) {
    if (now - lastBlinkToggle >= BLINK_INTERVAL_MS) {
      lastBlinkToggle = now;
      warningOn = !warningOn;
    }

    needleValue += needleStep;
    if (needleValue >= 1.0f) {
      needleValue = 1.0f;
      needleStep = -needleStep;
    } else if (needleValue <= 0.0f) {
      needleValue = 0.0f;
      needleStep = -needleStep;
    }

    renderCurrentScreen();
    delay(30);
    return;
  }

  if (currentScreen == SCREEN_STATUS) {
    if (now - lastStatusUpdate >= STATUS_INTERVAL_MS) {
      lastStatusUpdate = now;

      batteryPercent--;
      if (batteryPercent < 12) {
        batteryPercent = 87;
      }

      distanceMiles += 0.1f;
      if (distanceMiles > 99.9f) {
        distanceMiles = 0.0f;
      }

      renderCurrentScreen();
    }

    delay(20);
    return;
  }

  if (currentScreen == SCREEN_ENVIRONMENT) {
    delay(20);
    return;
  }

  if (now - lastTiltRender >= TILT_RENDER_INTERVAL_MS) {
    lastTiltRender = now;
    renderCurrentScreen();
  }

  delay(20);
}
