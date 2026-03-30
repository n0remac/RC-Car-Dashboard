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
static const uint8_t BME_ADDRESS_PRIMARY = 0x76;
static const uint8_t BME_ADDRESS_SECONDARY = 0x77;
static const float BME_SEA_LEVEL_HPA = 1013.25f;

// ----------------------
// Gauge state
// ----------------------
float dashboardRpmK = 2.6f;
float dashboardMph = 42.0f;
float dashboardFuelLevel = 0.16f;
bool dashboardHeadlightsOn = true;
int dashboardGearIndex = 3;
String dashboardOdometer = "000000";

unsigned long lastBlinkToggle = 0;
bool warningOn = true;
const unsigned long BLINK_INTERVAL_MS = 500;

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

unsigned long lastTiltRender = 0;
const unsigned long TILT_RENDER_INTERVAL_MS = 50;

// ----------------------
// Cross-tab declarations
// ----------------------
String htmlPage();
void handleRoot();
void handleSet();

void renderGaugeScreen(TFT_eSprite &s);

bool initBME280();
void updateEnvironment();
void updateIMU();
void renderCurrentScreen();
void applyTiltOrientation();
void resetTiltReference();
String bmeAddressLabel();
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

void updateIMU() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
  }

  rawRollDeg = atan2(ay, az) * 180.0f / PI;
  rawPitchDeg = atan2(-ax, sqrt((ay * ay) + (az * az))) * 180.0f / PI;
  applyTiltOrientation();
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
  renderGaugeScreen(spr);
  spr.pushSprite(0, 0);
}

// ----------------------
// Setup / loop
// ----------------------
void setup() {
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

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

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  renderCurrentScreen();
}

void loop() {
  server.handleClient();
  updateIMU();

  unsigned long now = millis();

  if ((now - lastEnvironmentUpdate) >= ENVIRONMENT_INTERVAL_MS) {
    lastEnvironmentUpdate = now;
    updateEnvironment();
  }

  if (now - lastBlinkToggle >= BLINK_INTERVAL_MS) {
    lastBlinkToggle = now;
    warningOn = !warningOn;
  }

  if (now - lastTiltRender >= TILT_RENDER_INTERVAL_MS) {
    lastTiltRender = now;
    renderCurrentScreen();
  }

  delay(20);
}
