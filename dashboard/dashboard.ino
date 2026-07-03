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
Preferences wifiPreferences;

// ----------------------
// WiFi AP config
// ----------------------
const char *AP_SSID = "CarRadio";
const char *AP_PASSWORD = "carradio123";
const char *WIFI_PREF_NAMESPACE = "wifi_sta";

String wifiStaSsid = "";
String wifiStaPassword = "";
bool wifiStaCredentialsSaved = false;

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
bool screenSpriteAvailable = false;

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
bool dashboardHeadlightsOn = false;
int dashboardGearIndex = 3;
String dashboardOdometer = "000000";

// ----------------------
// Headlight input state
// ----------------------
static const int HEADLIGHT_INPUT_PIN = 12;
static const unsigned long HEADLIGHT_INPUT_STALE_US = 250000UL;
static const unsigned long HEADLIGHT_PULSE_MIN_VALID_US = 750UL;
static const unsigned long HEADLIGHT_PULSE_ON_THRESHOLD_US = 2000UL;
static const unsigned long HEADLIGHT_PULSE_MAX_VALID_US = 2500UL;

bool headlightInputRawHigh = false;
bool headlightInputPulseFresh = false;
unsigned long headlightInputPulseWidthUs = 0;
unsigned long headlightInputPulseAgeMs = 0;

volatile bool headlightIsrRawHigh = false;
volatile unsigned long headlightIsrPulseStartUs = 0;
volatile unsigned long headlightIsrLastPulseWidthUs = 0;
volatile unsigned long headlightIsrLastPulseAtUs = 0;

unsigned long lastBlinkToggle = 0;
bool warningOn = true;
const unsigned long BLINK_INTERVAL_MS = 500;

// ----------------------
// Steering / turn signal state
// ----------------------
static const int STEERING_MIN_DEG = -45;
static const int STEERING_MAX_DEG = 45;
static const int STEERING_INPUT_PIN = 32;
static const unsigned long STEERING_DEFAULT_RIGHT_US = 1025UL;
static const unsigned long STEERING_DEFAULT_CENTER_US = 1500UL;
static const unsigned long STEERING_DEFAULT_LEFT_US = 2000UL;
static const int STEERING_CALIBRATION_SAMPLE_COUNT = 5;
static const float STEERING_SMOOTHING_ALPHA = 0.25f;
static const unsigned long STEERING_PULSE_CAPTURE_WINDOW_US = 25000UL;
static const unsigned long STEERING_PULSE_MIN_VALID_US = 900UL;
static const unsigned long STEERING_PULSE_MAX_VALID_US = 2100UL;
static const int STEERING_PULSE_THRESHOLD_MV = 200;
static const float STEERING_PULSE_FILTER_ALPHA = 0.20f;
static const unsigned long STEERING_PULSE_DEADBAND_US = 8UL;
static const int TURN_SIGNAL_RELEASE_MARGIN_DEG = 3;
static const int TURN_THRESHOLD_MIN_DEG = 0;
static const int TURN_THRESHOLD_MAX_DEG = 45;
static const int LEFT_TURN_LED_PIN = 26;
static const int RIGHT_TURN_LED_PIN = 25;
static const bool TURN_LED_ACTIVE_LOW = true;

int steeringWheelAngleDeg = 0;
float smoothedSteeringAngleDeg = 0.0f;
bool steeringInputValid = false;
bool leftTurnSignalActive = false;
bool rightTurnSignalActive = false;
unsigned long steeringCenterUs = STEERING_DEFAULT_CENTER_US;
unsigned long steeringLeftUs = STEERING_DEFAULT_LEFT_US;
unsigned long steeringRightUs = STEERING_DEFAULT_RIGHT_US;
int turnSignalThresholdDeg = 15;
bool turnSignalInputPulseFresh = false;
unsigned long turnSignalInputPulseWidthUs = 0;
float smoothedTurnSignalPulseUs = 0.0f;
bool turnSignalPulseSmoothingReady = false;

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
String jsonEscape(const String &value);
void handleRoot();
void handleState();
void handleSet();
void handleWifiScan();
void handleWifiConnect();
void handleWifiDisconnect();

void renderGaugeScreen(TFT_eSprite &s);

void initGps();
void updateGps();
void updateSpeedFusion();
void resetSpeedFusion();

bool prepareSpeakerI2sForBrowserAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);
bool speakerI2sStarted();
String speakerI2sStatusLabel();
String speakerI2sPinsLabel();

bool browserAudioIsPlaying();
void initBrowserAudioStorage();
void updateBrowserAudioPlayback();
bool browserAudioStorageReady();
size_t browserAudioStorageTotalBytes();
size_t browserAudioStorageUsedBytes();
size_t browserAudioStorageFreeBytes();
bool browserAudioFileSaved();
size_t browserAudioFileSize();
String browserAudioFileInfoLabel();
String browserAudioStatusLabel();
void handleBrowserAudioUpload();
void handleBrowserAudioUploadComplete();
void handleBrowserAudioPlay();
void handleBrowserAudioStop();

bool initBME280();
void updateEnvironment();
void updateIMU();
void renderCurrentScreen();
void renderSpriteUnavailableScreen();
void loadScreenBrightness();
void saveScreenBrightness();
void applyScreenBrightness();
void setScreenBrightnessPercent(int brightnessPercent);
void initWifi();
void loadWifiStationCredentials();
void saveWifiStationCredentials(const String &ssid, const String &password);
void clearWifiStationCredentials();
void beginWifiStation();
String wifiStationStatusLabel();
String wifiStationIpLabel();
void applyTiltOrientation();
void resetTiltReference();
unsigned long readSteeringPulseAverageUs(int sampleCount);
void handleHeadlightInputChange();
void updateHeadlightInput();
String headlightInputStatusLabel();
String headlightInputRawLabel();
void updateTurnSignalPulseInput();
String turnSignalInputPulseStatusLabel();
void loadSteeringCalibration();
void saveSteeringCalibration();
bool steeringCalibrationValid();
bool captureSteeringPulseByAdc(unsigned long &pulseWidthUs, int &pulsePeakMv);
float mapSteeringPulseToAngle(unsigned long pulseWidthUs);
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

void loadWifiStationCredentials() {
  wifiPreferences.begin(WIFI_PREF_NAMESPACE, false);
  wifiStaSsid = wifiPreferences.getString("ssid", "");
  wifiStaPassword = wifiPreferences.getString("password", "");
  wifiStaCredentialsSaved = wifiStaSsid.length() > 0;
}

void saveWifiStationCredentials(const String &ssid, const String &password) {
  wifiStaSsid = ssid;
  wifiStaPassword = password;
  wifiStaCredentialsSaved = wifiStaSsid.length() > 0;
  wifiPreferences.putString("ssid", wifiStaSsid);
  wifiPreferences.putString("password", wifiStaPassword);
}

void clearWifiStationCredentials() {
  wifiStaSsid = "";
  wifiStaPassword = "";
  wifiStaCredentialsSaved = false;
  wifiPreferences.remove("ssid");
  wifiPreferences.remove("password");
}

void beginWifiStation() {
  if (wifiStaSsid.length() == 0) {
    return;
  }

  WiFi.begin(wifiStaSsid.c_str(), wifiStaPassword.c_str());
  Serial.print("Connecting station WiFi to ");
  Serial.println(wifiStaSsid);
}

void initWifi() {
  loadWifiStationCredentials();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIp = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIp);

  beginWifiStation();
}

String wifiStationStatusLabel() {
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return "Connected";
  }
  if (status == WL_CONNECT_FAILED) {
    return "Connect failed";
  }
  if (status == WL_CONNECTION_LOST) {
    return "Connection lost";
  }
  if (status == WL_DISCONNECTED) {
    return wifiStaCredentialsSaved ? String("Disconnected") : String("Not configured");
  }
  if (status == WL_IDLE_STATUS) {
    return "Connecting";
  }
  if (status == WL_NO_SSID_AVAIL) {
    return "Network unavailable";
  }

  return "Unknown";
}

String wifiStationIpLabel() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "Unavailable";
}

void IRAM_ATTR handleHeadlightInputChange() {
  bool rawHigh = digitalRead(HEADLIGHT_INPUT_PIN) == HIGH;
  unsigned long nowUs = micros();

  headlightIsrRawHigh = rawHigh;
  if (rawHigh) {
    headlightIsrPulseStartUs = nowUs;
  } else if (headlightIsrPulseStartUs != 0) {
    headlightIsrLastPulseWidthUs = nowUs - headlightIsrPulseStartUs;
    headlightIsrLastPulseAtUs = nowUs;
  }
}

void updateHeadlightInput() {
  noInterrupts();
  bool rawHigh = headlightIsrRawHigh;
  unsigned long pulseWidthUs = headlightIsrLastPulseWidthUs;
  unsigned long lastPulseAtUs = headlightIsrLastPulseAtUs;
  interrupts();

  bool pulseSeen = lastPulseAtUs != 0;
  unsigned long pulseAgeUs = pulseSeen ?
    (micros() - lastPulseAtUs) :
    (HEADLIGHT_INPUT_STALE_US + 1);

  headlightInputRawHigh = rawHigh;
  headlightInputPulseWidthUs = pulseWidthUs;
  headlightInputPulseAgeMs = pulseSeen ? (pulseAgeUs / 1000UL) : 0;
  headlightInputPulseFresh =
    pulseSeen &&
    pulseAgeUs <= HEADLIGHT_INPUT_STALE_US &&
    pulseWidthUs >= HEADLIGHT_PULSE_MIN_VALID_US &&
    pulseWidthUs <= HEADLIGHT_PULSE_MAX_VALID_US;

  if (headlightInputPulseFresh) {
    dashboardHeadlightsOn = pulseWidthUs > HEADLIGHT_PULSE_ON_THRESHOLD_US;
  } else {
    dashboardHeadlightsOn = rawHigh;
  }
}

void updateTurnSignalPulseInput() {
  unsigned long adcPulseWidthUs = 0;
  int adcPulsePeakMv = 0;
  bool adcPulseValid = captureSteeringPulseByAdc(adcPulseWidthUs, adcPulsePeakMv);
  if (adcPulseValid) {
    if (!turnSignalPulseSmoothingReady) {
      smoothedTurnSignalPulseUs = (float)adcPulseWidthUs;
      turnSignalPulseSmoothingReady = true;
    } else {
      float delta = (float)adcPulseWidthUs - smoothedTurnSignalPulseUs;
      float absDelta = delta < 0.0f ? -delta : delta;
      if (absDelta > (float)STEERING_PULSE_DEADBAND_US) {
        smoothedTurnSignalPulseUs += delta * STEERING_PULSE_FILTER_ALPHA;
      }
    }

    turnSignalInputPulseWidthUs = (unsigned long)round(smoothedTurnSignalPulseUs);
    turnSignalInputPulseFresh = true;
    return;
  }

  turnSignalInputPulseWidthUs = 0;
  turnSignalInputPulseFresh = false;
  turnSignalPulseSmoothingReady = false;
}

bool captureSteeringPulseByAdc(unsigned long &pulseWidthUs, int &pulsePeakMv) {
  unsigned long captureStartUs = micros();
  unsigned long pulseStartUs = 0;
  bool waitingForLow = true;
  bool waitingForRise = false;
  bool waitingForFall = false;
  pulsePeakMv = 0;

  while ((micros() - captureStartUs) <= STEERING_PULSE_CAPTURE_WINDOW_US) {
    int mv = analogReadMilliVolts(STEERING_INPUT_PIN);
    if (mv > pulsePeakMv) {
      pulsePeakMv = mv;
    }

    bool high = mv >= STEERING_PULSE_THRESHOLD_MV;

    if (waitingForLow) {
      if (!high) {
        waitingForLow = false;
        waitingForRise = true;
      }
      continue;
    }

    if (waitingForRise) {
      if (high) {
        pulseStartUs = micros();
        waitingForRise = false;
        waitingForFall = true;
      }
      continue;
    }

    if (waitingForFall && !high) {
      pulseWidthUs = micros() - pulseStartUs;
      return pulseWidthUs >= STEERING_PULSE_MIN_VALID_US &&
        pulseWidthUs <= STEERING_PULSE_MAX_VALID_US;
    }
  }

  return false;
}

unsigned long readSteeringPulseAverageUs(int sampleCount) {
  if (sampleCount <= 0) {
    sampleCount = 1;
  }

  unsigned long pulseTotalUs = 0;
  int validSamples = 0;
  for (int i = 0; i < sampleCount; i++) {
    unsigned long pulseWidthUs = 0;
    int pulsePeakMv = 0;
    if (captureSteeringPulseByAdc(pulseWidthUs, pulsePeakMv)) {
      pulseTotalUs += pulseWidthUs;
      validSamples++;
    }
  }

  if (validSamples == 0) {
    return 0;
  }
  return pulseTotalUs / validSamples;
}

void saveSteeringCalibration() {
  steeringPreferences.putUInt("center_us", steeringCenterUs);
  steeringPreferences.putUInt("left_us", steeringLeftUs);
  steeringPreferences.putUInt("right_us", steeringRightUs);
  steeringPreferences.putInt("threshold_deg", turnSignalThresholdDeg);
}

void loadSteeringCalibration() {
  steeringPreferences.begin("steering", false);

  steeringCenterUs = steeringPreferences.getUInt(
    "center_us",
    STEERING_DEFAULT_CENTER_US
  );
  steeringLeftUs = steeringPreferences.getUInt(
    "left_us",
    STEERING_DEFAULT_LEFT_US
  );
  steeringRightUs = steeringPreferences.getUInt(
    "right_us",
    STEERING_DEFAULT_RIGHT_US
  );
  turnSignalThresholdDeg = clampInt(
    steeringPreferences.getInt("threshold_deg", turnSignalThresholdDeg),
    TURN_THRESHOLD_MIN_DEG,
    TURN_THRESHOLD_MAX_DEG
  );

  saveSteeringCalibration();
}

bool steeringCalibrationValid() {
  return steeringRightUs < steeringCenterUs && steeringCenterUs < steeringLeftUs;
}

float mapSteeringPulseToAngle(unsigned long pulseWidthUs) {
  if (!steeringCalibrationValid()) {
    return 0.0f;
  }

  if (pulseWidthUs <= steeringRightUs) {
    return STEERING_MAX_DEG;
  }
  if (pulseWidthUs >= steeringLeftUs) {
    return STEERING_MIN_DEG;
  }
  if (pulseWidthUs < steeringCenterUs) {
    float progress = (float)(steeringCenterUs - pulseWidthUs) /
      (float)(steeringCenterUs - steeringRightUs);
    return progress * STEERING_MAX_DEG;
  }

  float progress = (float)(pulseWidthUs - steeringCenterUs) /
    (float)(steeringLeftUs - steeringCenterUs);
  return progress * STEERING_MIN_DEG;
}

void updateSteeringInput() {
  if (!turnSignalInputPulseFresh || !steeringCalibrationValid()) {
    steeringInputValid = false;
    steeringWheelAngleDeg = 0;
    smoothedSteeringAngleDeg = 0.0f;
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
    return;
  }

  steeringInputValid = true;
  float targetAngle = mapSteeringPulseToAngle(turnSignalInputPulseWidthUs);
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

String headlightInputStatusLabel() {
  if (headlightInputPulseFresh) {
    return dashboardHeadlightsOn ? "On pulse" : "Off pulse";
  }
  return dashboardHeadlightsOn ? "High" : "Low";
}

String headlightInputRawLabel() {
  return headlightInputRawHigh ? "HIGH" : "LOW";
}

String turnSignalInputPulseStatusLabel() {
  if (turnSignalInputPulseFresh) {
    return "ADC receiving";
  }
  return "No pulse";
}

String steeringInputStatusLabel() {
  if (!steeringCalibrationValid()) {
    return "Calibration invalid";
  }
  if (steeringInputValid) {
    return "Online";
  }
  return "No pulse";
}

String steeringCalibrationStatusLabel() {
  if (steeringCalibrationValid()) {
    return "Valid";
  }
  return "Invalid: right us must be below center us and left us above center us";
}

// ----------------------
// Rendering
// ----------------------
void renderSpriteUnavailableScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Dashboard display buffer failed", 8, 10, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Web controls may still be available.", 8, 34, 1);
  tft.drawString(String("Audio: ") + browserAudioStatusLabel(), 8, 50, 1);
  tft.drawString(String("AP: ") + AP_SSID, 8, 66, 1);
}

void renderCurrentScreen() {
  if (!screenSpriteAvailable) {
    renderSpriteUnavailableScreen();
    return;
  }

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
  updateTurnSignalPulseInput();
  pinMode(HEADLIGHT_INPUT_PIN, INPUT);
  headlightIsrRawHigh = digitalRead(HEADLIGHT_INPUT_PIN) == HIGH;
  attachInterrupt(digitalPinToInterrupt(HEADLIGHT_INPUT_PIN), handleHeadlightInputChange, CHANGE);
  updateHeadlightInput();
  pinMode(LEFT_TURN_LED_PIN, OUTPUT);
  pinMode(RIGHT_TURN_LED_PIN, OUTPUT);
  writeTurnSignalLed(LEFT_TURN_LED_PIN, false);
  writeTurnSignalLed(RIGHT_TURN_LED_PIN, false);
  updateSteeringInput();
  updateTurnSignalOutputs();

  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);

  initBrowserAudioStorage();

  spr.setColorDepth(8);
  screenSpriteAvailable = spr.createSprite(SCREEN_W, SCREEN_H) != nullptr;
  if (screenSpriteAvailable) {
    spr.fillSprite(TFT_BLACK);
  } else {
    Serial.println("Dashboard sprite allocation failed; using direct TFT fallback");
    renderSpriteUnavailableScreen();
  }

  initWifi();

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
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/wifi/disconnect", HTTP_POST, handleWifiDisconnect);
  server.on("/audio/upload", HTTP_POST, handleBrowserAudioUploadComplete, handleBrowserAudioUpload);
  server.on("/audio/play", HTTP_POST, handleBrowserAudioPlay);
  server.on("/audio/stop", HTTP_POST, handleBrowserAudioStop);
  server.begin();

  renderCurrentScreen();
}

void loop() {
  updateHeadlightInput();
  updateTurnSignalPulseInput();
  updateSteeringInput();
  updateTurnSignalOutputs();
  server.handleClient();
  updateBrowserAudioPlayback();
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

  if (!browserAudioIsPlaying()) {
    delay(20);
  }
}
