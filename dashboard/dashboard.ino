#include <FS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Arduino_LSM6DSOX.h>
#include <Preferences.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WebServer server(80);
Preferences steeringPreferences;
Preferences dashboardPreferences;

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
static const int SCREEN_BACKLIGHT_PIN = 4;
static const int SCREEN_BRIGHTNESS_MIN_PERCENT = 5;
static const int SCREEN_BRIGHTNESS_MAX_PERCENT = 100;
static const int SCREEN_BRIGHTNESS_DEFAULT_PERCENT = 5;

int screenBrightnessPercent = SCREEN_BRIGHTNESS_DEFAULT_PERCENT;

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
// Steering / turn signal state
// ----------------------
static const int STEERING_MIN_DEG = -45;
static const int STEERING_MAX_DEG = 45;
static const int STEERING_INPUT_PIN = 32;
static const int STEERING_DEFAULT_RIGHT_OFFSET_MV = 100;
static const int STEERING_DEFAULT_LEFT_OFFSET_MV = 70;
static const int STEERING_VALID_MIN_MV = 0;
static const int STEERING_VALID_MAX_MV = 1000;
static const int STEERING_SAMPLE_COUNT = 8;
static const int STEERING_CALIBRATION_SAMPLE_COUNT = 40;
static const int STEERING_CALIBRATION_SAMPLE_DELAY_MS = 3;
static const float STEERING_SMOOTHING_ALPHA = 0.25f;
static const int TURN_SIGNAL_RELEASE_MARGIN_DEG = 3;
static const int TURN_THRESHOLD_MIN_DEG = 0;
static const int TURN_THRESHOLD_MAX_DEG = 45;
static const int LEFT_TURN_LED_PIN = 25;
static const int RIGHT_TURN_LED_PIN = 26;
static const bool TURN_LED_ACTIVE_LOW = true;

int steeringWheelAngleDeg = 0;
float smoothedSteeringAngleDeg = 0.0f;
int steeringInputRaw = 0;
int steeringInputMv = 0;
int steeringInputMinMv = 9999;
int steeringInputMaxMv = 0;
bool steeringInputValid = false;
bool leftTurnSignalActive = false;
bool rightTurnSignalActive = false;
int steeringCenterMv = 0;
int steeringLeftMv = 0;
int steeringRightMv = 0;
int turnSignalThresholdDeg = 15;

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
void handleState();
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
void loadScreenBrightness();
void saveScreenBrightness();
void applyScreenBrightness();
void setScreenBrightnessPercent(int brightnessPercent);
void applyTiltOrientation();
void resetTiltReference();
int readSteeringAverageMv(int sampleCount, int sampleDelayMs);
void loadSteeringCalibration();
void saveSteeringCalibration();
bool steeringCalibrationValid();
void updateSteeringInput();
void updateTurnSignalIntent();
void updateTurnSignalOutputs();
bool leftTurnSignalRequested();
bool rightTurnSignalRequested();
bool leftTurnSignalFlashing();
bool rightTurnSignalFlashing();
String turnSignalLabel();
String turnSignalOutputLabel();
String steeringInputStatusLabel();
String steeringCalibrationStatusLabel();
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

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
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

void loadScreenBrightness() {
  dashboardPreferences.begin("dashboard", false);
  screenBrightnessPercent = clampInt(
    dashboardPreferences.getInt("brightness", SCREEN_BRIGHTNESS_DEFAULT_PERCENT),
    SCREEN_BRIGHTNESS_MIN_PERCENT,
    SCREEN_BRIGHTNESS_MAX_PERCENT
  );
}

void saveScreenBrightness() {
  dashboardPreferences.putInt("brightness", screenBrightnessPercent);
}

void applyScreenBrightness() {
  int duty = map(
    screenBrightnessPercent,
    0,
    100,
    0,
    255
  );
  analogWrite(SCREEN_BACKLIGHT_PIN, duty);
}

void setScreenBrightnessPercent(int brightnessPercent) {
  screenBrightnessPercent = clampInt(
    brightnessPercent,
    SCREEN_BRIGHTNESS_MIN_PERCENT,
    SCREEN_BRIGHTNESS_MAX_PERCENT
  );
  applyScreenBrightness();
  saveScreenBrightness();
}

int readSteeringAverageMv(int sampleCount, int sampleDelayMs) {
  if (sampleCount <= 0) {
    sampleCount = 1;
  }

  long mvTotal = 0;
  for (int i = 0; i < sampleCount; i++) {
    mvTotal += analogReadMilliVolts(STEERING_INPUT_PIN);
    if (sampleDelayMs > 0) {
      delay(sampleDelayMs);
    }
  }

  return mvTotal / sampleCount;
}

void saveSteeringCalibration() {
  steeringPreferences.putInt("center_mv", steeringCenterMv);
  steeringPreferences.putInt("left_mv", steeringLeftMv);
  steeringPreferences.putInt("right_mv", steeringRightMv);
  steeringPreferences.putInt("threshold_deg", turnSignalThresholdDeg);
}

void loadSteeringCalibration() {
  steeringPreferences.begin("steering", false);

  steeringCenterMv = readSteeringAverageMv(
    STEERING_CALIBRATION_SAMPLE_COUNT,
    STEERING_CALIBRATION_SAMPLE_DELAY_MS
  );
  steeringInputMv = steeringCenterMv;
  steeringInputMinMv = steeringCenterMv;
  steeringInputMaxMv = steeringCenterMv;

  steeringLeftMv = steeringPreferences.getInt(
    "left_mv",
    steeringCenterMv + STEERING_DEFAULT_LEFT_OFFSET_MV
  );
  steeringRightMv = steeringPreferences.getInt(
    "right_mv",
    clampInt(steeringCenterMv - STEERING_DEFAULT_RIGHT_OFFSET_MV, 0, 3300)
  );
  turnSignalThresholdDeg = clampInt(
    steeringPreferences.getInt("threshold_deg", turnSignalThresholdDeg),
    TURN_THRESHOLD_MIN_DEG,
    TURN_THRESHOLD_MAX_DEG
  );

  saveSteeringCalibration();
}

bool steeringCalibrationValid() {
  return steeringRightMv < steeringCenterMv && steeringCenterMv < steeringLeftMv;
}

float mapSteeringVoltageToAngle(int millivolts) {
  if (!steeringCalibrationValid()) {
    return 0.0f;
  }

  if (millivolts <= steeringRightMv) {
    return STEERING_MAX_DEG;
  }
  if (millivolts >= steeringLeftMv) {
    return STEERING_MIN_DEG;
  }
  if (millivolts < steeringCenterMv) {
    float progress = (float)(steeringCenterMv - millivolts) /
      (float)(steeringCenterMv - steeringRightMv);
    return progress * STEERING_MAX_DEG;
  }

  float progress = (float)(millivolts - steeringCenterMv) /
    (float)(steeringLeftMv - steeringCenterMv);
  return progress * STEERING_MIN_DEG;
}

void updateSteeringInput() {
  long rawTotal = 0;
  long mvTotal = 0;
  for (int i = 0; i < STEERING_SAMPLE_COUNT; i++) {
    rawTotal += analogRead(STEERING_INPUT_PIN);
    mvTotal += analogReadMilliVolts(STEERING_INPUT_PIN);
  }
  steeringInputRaw = rawTotal / STEERING_SAMPLE_COUNT;
  steeringInputMv = mvTotal / STEERING_SAMPLE_COUNT;
  if (steeringInputMv < steeringInputMinMv) {
    steeringInputMinMv = steeringInputMv;
  }
  if (steeringInputMv > steeringInputMaxMv) {
    steeringInputMaxMv = steeringInputMv;
  }

  if (!steeringCalibrationValid()) {
    steeringInputValid = false;
    steeringWheelAngleDeg = 0;
    smoothedSteeringAngleDeg = 0.0f;
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
    return;
  }

  if (steeringInputMv < STEERING_VALID_MIN_MV ||
      steeringInputMv > STEERING_VALID_MAX_MV) {
    steeringInputValid = false;
    steeringWheelAngleDeg = 0;
    smoothedSteeringAngleDeg = 0.0f;
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
    return;
  }

  steeringInputValid = true;
  float targetAngle = mapSteeringVoltageToAngle(steeringInputMv);
  smoothedSteeringAngleDeg +=
    (targetAngle - smoothedSteeringAngleDeg) * STEERING_SMOOTHING_ALPHA;
  steeringWheelAngleDeg = clampInt(
    (int)round(smoothedSteeringAngleDeg),
    STEERING_MIN_DEG,
    STEERING_MAX_DEG
  );
  updateTurnSignalIntent();
}

void updateTurnSignalIntent() {
  if (!steeringInputValid) {
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
    return;
  }

  int activateThreshold = turnSignalThresholdDeg;
  if (activateThreshold <= 0 && steeringWheelAngleDeg == 0) {
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
    return;
  }

  int releaseThreshold = turnSignalThresholdDeg - TURN_SIGNAL_RELEASE_MARGIN_DEG;
  if (releaseThreshold < 0) {
    releaseThreshold = 0;
  }

  if (steeringWheelAngleDeg <= -activateThreshold) {
    leftTurnSignalActive = true;
    rightTurnSignalActive = false;
    return;
  }

  if (steeringWheelAngleDeg >= activateThreshold) {
    rightTurnSignalActive = true;
    leftTurnSignalActive = false;
    return;
  }

  if (leftTurnSignalActive && steeringWheelAngleDeg <= -releaseThreshold) {
    return;
  }

  if (rightTurnSignalActive && steeringWheelAngleDeg >= releaseThreshold) {
    return;
  }

  leftTurnSignalActive = false;
  rightTurnSignalActive = false;
}

bool leftTurnSignalRequested() {
  return leftTurnSignalActive;
}

bool rightTurnSignalRequested() {
  return rightTurnSignalActive;
}

bool leftTurnSignalFlashing() {
  return leftTurnSignalRequested() && warningOn;
}

bool rightTurnSignalFlashing() {
  return rightTurnSignalRequested() && warningOn;
}

void writeTurnSignalLed(int pin, bool active) {
  if (TURN_LED_ACTIVE_LOW) {
    digitalWrite(pin, active ? LOW : HIGH);
  } else {
    digitalWrite(pin, active ? HIGH : LOW);
  }
}

void updateTurnSignalOutputs() {
  writeTurnSignalLed(LEFT_TURN_LED_PIN, leftTurnSignalFlashing());
  writeTurnSignalLed(RIGHT_TURN_LED_PIN, rightTurnSignalFlashing());
}

String turnSignalLabel() {
  if (leftTurnSignalRequested()) {
    return "Left";
  }
  if (rightTurnSignalRequested()) {
    return "Right";
  }
  return "Off";
}

String turnSignalOutputLabel() {
  if (leftTurnSignalFlashing()) {
    return "Left on";
  }
  if (rightTurnSignalFlashing()) {
    return "Right on";
  }
  return "Off";
}

String steeringInputStatusLabel() {
  if (!steeringCalibrationValid()) {
    return "Calibration invalid";
  }
  if (steeringInputValid) {
    return "Online";
  }
  if (steeringInputMv < STEERING_VALID_MIN_MV) {
    return "Low / disconnected";
  }
  if (steeringInputMv > STEERING_VALID_MAX_MV) {
    return "High / over range";
  }
  return "Invalid";
}

String steeringCalibrationStatusLabel() {
  if (steeringCalibrationValid()) {
    return "Valid";
  }
  return "Invalid: right must be below center and left above center";
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
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  loadScreenBrightness();
  applyScreenBrightness();
  pinMode(STEERING_INPUT_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(STEERING_INPUT_PIN, ADC_0db);
  loadSteeringCalibration();
  pinMode(LEFT_TURN_LED_PIN, OUTPUT);
  pinMode(RIGHT_TURN_LED_PIN, OUTPUT);
  writeTurnSignalLed(LEFT_TURN_LED_PIN, false);
  writeTurnSignalLed(RIGHT_TURN_LED_PIN, false);
  updateSteeringInput();
  updateTurnSignalOutputs();

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
  server.on("/state", handleState);
  server.on("/set", handleSet);
  server.begin();

  renderCurrentScreen();
}

void loop() {
  updateSteeringInput();
  updateTurnSignalOutputs();
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
    updateTurnSignalOutputs();
  }

  if (now - lastTiltRender >= TILT_RENDER_INTERVAL_MS) {
    lastTiltRender = now;
    renderCurrentScreen();
  }

  delay(20);
}
