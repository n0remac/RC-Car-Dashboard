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
float dashboardMph = 0.0f;
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
bool imuAvailable = false;
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
float orientedPitchDeg = 0.0f;
float orientedRollDeg = 0.0f;
int tiltOrientationDeg = 0;
float pitchZeroDeg = 0.0f;
float rollZeroDeg = 0.0f;
bool invertPitchAxis = false;
bool invertRollAxis = true;
bool showTiltAxisLabels = false;
float tiltBubbleToleranceDeg = 1.0f;
float longitudinalAxisAccelG = 0.0f;
unsigned long lastLongitudinalAccelSampleMs = 0;

unsigned long lastTiltRender = 0;
const unsigned long TILT_RENDER_INTERVAL_MS = 50;

// ----------------------
// Cross-tab declarations
// ----------------------
String htmlPage();
void handleRoot();
void handleSet();

void renderGaugeScreen(TFT_eSprite &s);

void initGps();
void updateGps();
void updateSpeedFusion();
void resetSpeedFusion();

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

float readLongitudinalAxisAccelG() {
  if (tiltOrientationDeg == 90) {
    return ay;
  }
  if (tiltOrientationDeg == 180) {
    return ax;
  }
  if (tiltOrientationDeg == 270) {
    return -ay;
  }

  return -ax;
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
  if (!imuAvailable) {
    return;
  }

  bool accelerationUpdated = false;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    accelerationUpdated = true;
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
  }

  rawRollDeg = atan2(ay, az) * 180.0f / PI;
  rawPitchDeg = atan2(-ax, sqrt((ay * ay) + (az * az))) * 180.0f / PI;
  applyTiltOrientation();

  if (accelerationUpdated) {
    longitudinalAxisAccelG = readLongitudinalAxisAccelG();
    lastLongitudinalAccelSampleMs = millis();
  }
}

void applyTiltOrientation() {
  orientedPitchDeg = rawPitchDeg;
  orientedRollDeg = rawRollDeg;

  if (tiltOrientationDeg == 90) {
    orientedPitchDeg = rawRollDeg;
    orientedRollDeg = -rawPitchDeg;
  } else if (tiltOrientationDeg == 180) {
    orientedPitchDeg = -rawPitchDeg;
    orientedRollDeg = -rawRollDeg;
  } else if (tiltOrientationDeg == 270) {
    orientedPitchDeg = -rawRollDeg;
    orientedRollDeg = rawPitchDeg;
  }

  pitchDeg = orientedPitchDeg - pitchZeroDeg;
  rollDeg = orientedRollDeg - rollZeroDeg;

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
  pitchZeroDeg = orientedPitchDeg;
  rollZeroDeg = orientedRollDeg;
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

  imuAvailable = IMU.begin();
  if (imuAvailable) {
    Serial.println("IMU online");
    updateIMU();
  } else {
    Serial.println("IMU not detected; continuing without tilt/accel data");
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

  initGps();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  renderCurrentScreen();
}

void loop() {
  server.handleClient();
  updateGps();
  updateIMU();
  updateSpeedFusion();

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
