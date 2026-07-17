#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Arduino_LSM6DSOX.h>
#include <Preferences.h>
#include <functional>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
AsyncWebServer server(80);
AsyncEventSource apiEvents("/api/v1/events");
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
unsigned long wifiStaConnectStartedMs = 0;
static const unsigned long WIFI_STA_CONNECT_TIMEOUT_MS = 15000UL;

// ----------------------
// Screen config
// ----------------------
static const int SCREEN_W = 240;
static const int SCREEN_H = 135;
static const int SCREEN_BACKLIGHT_PIN = 4;
static const int SCREEN_BRIGHTNESS_MIN_PERCENT = 5;
static const int SCREEN_BRIGHTNESS_MAX_PERCENT = 100;
static const int SCREEN_BRIGHTNESS_DEFAULT_PERCENT = 5;
static const int DASHBOARD_SCALE_MIN_PERCENT = 50;
static const int DASHBOARD_SCALE_MAX_PERCENT = 100;
static const int DASHBOARD_SCALE_DEFAULT_PERCENT = 100;

struct DashboardColors {
  uint32_t background;
  uint32_t primary;
  uint32_t detail;
  uint32_t accent;
  uint32_t gearSelectedBackground;
  uint32_t gearSelectedText;
  uint32_t gearUnselectedText;
  uint32_t turnActive;
  uint32_t turnInactive;
  uint32_t headlightActive;
  uint32_t headlightInactive;
  uint32_t warningActive;
  uint32_t warningInactive;
};

static const DashboardColors DASHBOARD_COLOR_DEFAULTS = {
  0x000000,
  0xFFFFFF,
  0x404040,
  0xFF0000,
  0xFFFFFF,
  0x000000,
  0x6E6E6E,
  0x20D25A,
  0x1C3820,
  0x4090FF,
  0x203454,
  0xFFB134,
  0x3C341C
};

int screenBrightnessPercent = SCREEN_BRIGHTNESS_DEFAULT_PERCENT;
int dashboardScalePercent = DASHBOARD_SCALE_DEFAULT_PERCENT;
int dashboardOffsetX = 0;
int dashboardOffsetY = 0;
DashboardColors dashboardColors = DASHBOARD_COLOR_DEFAULTS;
bool screenSpriteAvailable = false;
bool dashboardTransformDirty = true;
uint8_t dashboardScaledRow[SCREEN_W];

// ----------------------
// IMU / I2C config
// ----------------------
static const int SDA_PIN = 21;
static const int SCL_PIN = 22;
static const uint8_t BME_ADDRESS_PRIMARY = 0x76;
static const uint8_t BME_ADDRESS_SECONDARY = 0x77;
static const float BME_SEA_LEVEL_HPA = 1013.25f;
static const bool SWAP_IMU_XY_AXES = true;

// ----------------------
// Gauge state
// ----------------------
float dashboardRpmK = 0.0f;
float dashboardMph = 0.0f;
float dashboardFuelLevel = 0.16f;
bool dashboardHeadlightsOn = false;
int dashboardGearIndex = 0;
String dashboardOdometer = "000000";

static const int GEAR_PARK_INDEX = 0;
static const int GEAR_REVERSE_INDEX = 1;
static const int GEAR_NEUTRAL_INDEX = 2;
static const int GEAR_DRIVE_INDEX = 3;
static const uint8_t RC_PULSE_FILTER_SAMPLE_COUNT = 3;

struct RcPulseInput {
  int pin;
  unsigned long staleUs;
  unsigned long minValidUs;
  unsigned long maxValidUs;
  bool rawHigh;
  bool pulseFresh;
  unsigned long pulseWidthUs;
  unsigned long pulseAgeMs;
  volatile bool isrRawHigh;
  volatile unsigned long isrPulseStartUs;
  volatile unsigned long isrLastPulseWidthUs;
  volatile unsigned long isrLastPulseAtUs;
  volatile uint32_t isrPulseSequence;
  uint32_t lastProcessedPulseSequence;
  bool newPulseAvailable;
  bool newValidPulse;
  unsigned long lastValidPulseAtUs;
  bool filterReady;
  bool filteredPulseFresh;
  unsigned long filteredPulseWidthUs;
  unsigned long filteredPulseAgeMs;
  unsigned long filterSamples[RC_PULSE_FILTER_SAMPLE_COUNT];
  uint8_t filterSampleCount;
  uint8_t filterNextSampleIndex;
  bool debouncedState;
  bool candidateState;
  uint8_t candidateStateCount;
};

// ----------------------
// Headlight input state
// ----------------------
static const int HEADLIGHT_INPUT_PIN = 12;
static const unsigned long HEADLIGHT_INPUT_STALE_US = 250000UL;
static const unsigned long HEADLIGHT_PULSE_MIN_VALID_US = 750UL;
static const unsigned long HEADLIGHT_PULSE_ON_THRESHOLD_US = 2000UL;
static const unsigned long HEADLIGHT_PULSE_MAX_VALID_US = 2500UL;

RcPulseInput headlightInput = {
  HEADLIGHT_INPUT_PIN,
  HEADLIGHT_INPUT_STALE_US,
  HEADLIGHT_PULSE_MIN_VALID_US,
  HEADLIGHT_PULSE_MAX_VALID_US,
  false,
  false,
  0,
  0,
  false,
  0,
  0,
  0
};

// ----------------------
// Sound trigger input state
// ----------------------
static const int SOUND_SWITCH_INPUT_PIN = 39;
static const unsigned long SOUND_SWITCH_INPUT_STALE_US = HEADLIGHT_INPUT_STALE_US;
static const unsigned long SOUND_SWITCH_PULSE_MIN_VALID_US = HEADLIGHT_PULSE_MIN_VALID_US;
static const unsigned long SOUND_SWITCH_PULSE_ON_THRESHOLD_US = 2000UL;
static const unsigned long SOUND_SWITCH_PULSE_MAX_VALID_US = HEADLIGHT_PULSE_MAX_VALID_US;

RcPulseInput soundSwitchInput = {
  SOUND_SWITCH_INPUT_PIN,
  SOUND_SWITCH_INPUT_STALE_US,
  SOUND_SWITCH_PULSE_MIN_VALID_US,
  SOUND_SWITCH_PULSE_MAX_VALID_US,
  false,
  false,
  0,
  0,
  false,
  0,
  0,
  0
};
bool soundSwitchOn = false;
bool rcHornRequested = false;
bool webHornRequested = false;
bool hornWasRequested = false;

enum class HornWebMode : uint8_t {
  Off,
  Rc,
  On
};

HornWebMode hornWebMode = HornWebMode::Rc;
static const uint16_t WEB_SHORT_HORN_DURATION_MS = 220;

unsigned long lastBlinkToggle = 0;
bool warningOn = true;
const unsigned long BLINK_INTERVAL_MS = 500;

// ----------------------
// Steering / turn signal state
// ----------------------
static const int STEERING_MIN_DEG = -45;
static const int STEERING_MAX_DEG = 45;
static const int STEERING_CONTROL_PIN = 32;
static const unsigned long STEERING_DEFAULT_RIGHT_US = 1025UL;
static const unsigned long STEERING_DEFAULT_CENTER_US = 1500UL;
static const unsigned long STEERING_DEFAULT_LEFT_US = 2000UL;
static const unsigned long STEERING_PULSE_MIN_VALID_US = 900UL;
static const unsigned long STEERING_PULSE_MAX_VALID_US = 2100UL;
static const unsigned long STEERING_PULSE_STALE_US = 250000UL;
static const int TURN_SIGNAL_RELEASE_MARGIN_DEG = 3;
static const int TURN_THRESHOLD_MIN_DEG = 0;
static const int TURN_THRESHOLD_MAX_DEG = 45;
static const int LEFT_TURN_LED_PIN = 26;
static const int RIGHT_TURN_LED_PIN = 25;
static const bool TURN_LED_ACTIVE_LOW = true;

int steeringWheelAngleDeg = 0;
bool steeringInputValid = false;
bool leftTurnSignalActive = false;
bool rightTurnSignalActive = false;
unsigned long steeringCenterUs = STEERING_DEFAULT_CENTER_US;
unsigned long steeringLeftUs = STEERING_DEFAULT_LEFT_US;
unsigned long steeringRightUs = STEERING_DEFAULT_RIGHT_US;
int turnSignalThresholdDeg = 15;

RcPulseInput steeringReceiverInput = {
  STEERING_CONTROL_PIN,
  STEERING_PULSE_STALE_US,
  STEERING_PULSE_MIN_VALID_US,
  STEERING_PULSE_MAX_VALID_US,
  false,
  false,
  0,
  0,
  false,
  0,
  0,
  0
};

// ----------------------
// Vehicle control state. Receiver mode releases these GPIOs as inputs; web mode
// drives PWM. Never enable web mode while an active receiver drives either wire.
// ----------------------
static const int THROTTLE_CONTROL_PIN = 33;
static const unsigned long THROTTLE_PULSE_STALE_US = 250000UL;
static const unsigned long THROTTLE_PULSE_MIN_VALID_US = 900UL;
static const unsigned long THROTTLE_PULSE_MAX_VALID_US = 2100UL;
static const unsigned long THROTTLE_OFF_US = 1534UL;
static const unsigned long THROTTLE_FULL_FORWARD_US = 979UL;
static const unsigned long THROTTLE_FULL_REVERSE_US = 2045UL;
static const unsigned long THROTTLE_DRIVE_ENTRY_TOLERANCE_US = 50UL;
static const unsigned long THROTTLE_REVERSE_ENTRY_TOLERANCE_US = 75UL;
static const unsigned long THROTTLE_DIRECTION_RELEASE_TOLERANCE_US = 25UL;
static const uint8_t THROTTLE_GEAR_DEBOUNCE_SAMPLE_COUNT = 3;
static const unsigned long THROTTLE_PARK_DELAY_MS = 10000UL;
static const float THROTTLE_RPM_MAX_K = 8.0f;
static const uint8_t STEERING_PWM_CHANNEL = 0;
static const uint8_t THROTTLE_PWM_CHANNEL = 1;
static const uint32_t VEHICLE_PWM_FREQUENCY_HZ = 50;
static const uint8_t VEHICLE_PWM_RESOLUTION_BITS = 16;
static const uint32_t VEHICLE_PWM_PERIOD_US = 1000000UL / VEHICLE_PWM_FREQUENCY_HZ;
static const unsigned long VEHICLE_CONTROL_WATCHDOG_MS = 500UL;

enum class ThrottleGearDirection : uint8_t {
  Neutral,
  Reverse,
  Drive
};

struct ThrottleGearState {
  ThrottleGearDirection committedDirection;
  ThrottleGearDirection candidateDirection;
  uint8_t candidateCount;
  bool initialized;
  bool centeredTimerActive;
  unsigned long centeredSinceMs;
};

ThrottleGearState throttleGearState = {
  ThrottleGearDirection::Neutral,
  ThrottleGearDirection::Neutral,
  0,
  false,
  false,
  0
};

RcPulseInput throttleReceiverInput = {
  THROTTLE_CONTROL_PIN,
  THROTTLE_PULSE_STALE_US,
  THROTTLE_PULSE_MIN_VALID_US,
  THROTTLE_PULSE_MAX_VALID_US,
  false,
  false,
  0,
  0,
  false,
  0,
  0,
  0
};

enum class VehicleControlMode : uint8_t {
  Receiver,
  Web
};

enum class VehicleControlOwner : uint8_t {
  None,
  Local,
  Remote
};

struct VehicleControlState {
  VehicleControlMode mode;
  bool pwmReady;
  bool armed;
  bool watchdogStopped;
  int steeringPercent;
  int throttlePercent;
  unsigned long steeringPulseUs;
  unsigned long throttlePulseUs;
  unsigned long lastHeartbeatMs;
};

VehicleControlState vehicleControl = {
  VehicleControlMode::Receiver,
  false,
  false,
  false,
  0,
  0,
  STEERING_DEFAULT_CENTER_US,
  THROTTLE_OFF_US,
  0
};

VehicleControlOwner vehicleControlOwner = VehicleControlOwner::None;

String apiControlSessionId = "";
uint32_t apiControlLastSequence = 0;
bool apiControlSequenceSeen = false;

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
bool tiltReferenceCalibrated = false;

unsigned long lastTiltRender = 0;
const unsigned long TILT_RENDER_INTERVAL_MS = 50;

// ----------------------
// Cross-tab declarations
// ----------------------
String htmlPage();
String jsonEscape(const String &value);
void handleRoot(AsyncWebServerRequest *request);
void handleState(AsyncWebServerRequest *request);
void handleSet(AsyncWebServerRequest *request);
void handleWifiScan(AsyncWebServerRequest *request);
void handleWifiConnect(AsyncWebServerRequest *request);
void handleWifiDisconnect(AsyncWebServerRequest *request);

void renderGaugeScreen(TFT_eSprite &s);

void initGps();
void updateGps();
void updateSpeedFusion();
void resetSpeedFusion();

enum SpeedSourceMode {
  SPEED_SOURCE_IDLE,
  SPEED_SOURCE_GPS,
  SPEED_SOURCE_IMU_BRIDGE,
  SPEED_SOURCE_HOLD
};

enum SpeedFusionLeadMode {
  SPEED_LEAD_GPS,
  SPEED_LEAD_ACCEL
};

extern bool gpsDataSeen;
extern bool gpsLocationValid;
extern bool gpsSpeedValid;
extern uint32_t gpsSatellites;
extern float gpsRawMph;
extern double gpsLatitude;
extern double gpsLongitude;
extern unsigned long gpsFixAgeMs;
extern unsigned long lastGpsSpeedSampleMs;
extern SpeedFusionLeadMode speedFusionLeadMode;

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
void handleBrowserAudioUpload(
  AsyncWebServerRequest *request,
  String filename,
  size_t index,
  uint8_t *data,
  size_t length,
  bool final
);
void handleBrowserAudioUploadComplete(AsyncWebServerRequest *request);
bool initHornSynth();
bool startHornSynth(String &message);
void stopHornSynth();
void triggerShortHornSynth(uint16_t durationMs);
void cancelHornSynth();
bool hornSynthIsActive();
String hornSynthStatusLabel();
void handleHornMode(AsyncWebServerRequest *request);
void handleShortHorn(AsyncWebServerRequest *request);
String hornResponseJson(bool ok, const String &message);
void sendHornResponse(int code, bool ok, const String &message);

bool initBME280();
void updateEnvironment();
void updateIMU();
void renderCurrentScreen();
void renderSpriteUnavailableScreen();
void loadScreenBrightness();
void saveScreenBrightness();
void saveDashboardSettings();
void applyScreenBrightness();
void setScreenBrightnessPercent(int brightnessPercent);
int dashboardScaledWidth(int scalePercent);
int dashboardScaledHeight(int scalePercent);
int dashboardMaxOffsetX(int scalePercent);
int dashboardMaxOffsetY(int scalePercent);
void applyDashboardDisplayTransform(int scalePercent, int offsetX, int offsetY);
void initWifi();
void loadWifiStationCredentials();
void saveWifiStationCredentials(const String &ssid, const String &password);
void clearWifiStationCredentials();
void beginWifiStation();
String wifiStationStatusLabel();
String wifiStationIpLabel();
String wifiStationNetworkLabel();
String wifiStationErrorLabel();
void applyTiltOrientation();
void resetTiltReference();
void initRcPulseInput(RcPulseInput &input);
void IRAM_ATTR handleRcPulseInputChange(RcPulseInput &input);
void updateRcPulseInput(RcPulseInput &input);
void updateRcPulseMedianInput(RcPulseInput &input);
void updateRcPulseDigitalInput(RcPulseInput &input, unsigned long onThresholdUs);
void resetRcPulseFilter(RcPulseInput &input);
void refreshRcPulseFilterFreshness(RcPulseInput &input, unsigned long nowUs);
void recordRcPulseSample(RcPulseInput &input);
unsigned long medianRcPulseSample(const RcPulseInput &input);
void handleHeadlightInputChange();
void updateHeadlightInput();
String headlightInputStatusLabel();
String headlightInputRawLabel();
void handleSoundSwitchInputChange();
void updateSoundSwitchInput();
void updateHornPlayback();
bool hornPlaybackRequested();
const char *hornWebModeValue();
String hornWebModeLabel();
String soundSwitchInputStatusLabel();
void initVehicleControl();
void enableReceiverControl();
bool enableWebControl();
void releaseVehiclePwmOutputs();
void handleSteeringReceiverInputChange();
void handleThrottleReceiverInputChange();
bool armVehicleControl();
bool armVehicleControlForOwner(VehicleControlOwner owner);
void stopVehicleControl(bool watchdogStop = false);
void setVehicleControlCommand(int steeringPercent, int throttlePercent);
void updateVehicleControlWatchdog();
unsigned long vehicleControlHeartbeatAgeMs();
const char *vehicleControlModeValue();
const char *vehicleControlOwnerValue();
String vehicleControlStatusLabel();
unsigned long steeringPulseForPercent(int steeringPercent);
unsigned long throttlePulseForPercent(int throttlePercent);
void resetThrottleGearState();
ThrottleGearDirection classifyReceiverThrottleDirection(unsigned long pulseWidthUs);
void updateReceiverThrottleGear(unsigned long pulseWidthUs);
void updateWebThrottleGear();
void applyThrottleDashboardState(unsigned long pulseWidthUs);
void updateVehicleControlDashboard();
String throttleInputStatusLabel();
String turnSignalInputPulseStatusLabel();
void loadSteeringCalibration();
void saveSteeringCalibration();
bool steeringCalibrationValid();
float mapSteeringPulseToAngle(unsigned long pulseWidthUs);
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
String controllerPage();
String controllerStateJson();
void handleControllerPage(AsyncWebServerRequest *request);
void handleControllerState(AsyncWebServerRequest *request);
void handleControllerArm(AsyncWebServerRequest *request);
void handleControllerCommand(AsyncWebServerRequest *request);
void handleControllerStop(AsyncWebServerRequest *request);
void registerApiRoutes();
void updateApiEvents();
void initRemoteConnection();
void updateRemoteConnection();
void latchRemoteStopFromLocal();
String apiRemoteJson();
String remoteServerLabel();
String remoteStatusLabel();
String remoteLastSyncLabel();
String remoteErrorLabel();
typedef std::function<void(AsyncWebServerRequest *, JsonVariant &)> ApiJsonHandler;
void registerApiJsonPost(const char *path, ApiJsonHandler handler);
void sendApiError(AsyncWebServerRequest *request, int code, const char *error, const String &message);
bool requestHasArg(AsyncWebServerRequest *request, const char *name);
String requestArg(AsyncWebServerRequest *request, const char *name);
String bmeAddressLabel();
String tiltOrientationName();
String onOffLabel(bool enabled);
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t dashboardColor565(uint32_t color);
String dashboardColorHex(uint32_t color);

// ----------------------
// Helpers
// ----------------------
float degToRad(float deg) {
  return deg * 0.0174532925f;
}

float angleDeltaDeg(float valueDeg, float referenceDeg) {
  float deltaDeg = valueDeg - referenceDeg;
  while (deltaDeg > 180.0f) {
    deltaDeg -= 360.0f;
  }
  while (deltaDeg < -180.0f) {
    deltaDeg += 360.0f;
  }
  return deltaDeg;
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

uint16_t dashboardColor565(uint32_t color) {
  return rgb565(
    (uint8_t)((color >> 16) & 0xFF),
    (uint8_t)((color >> 8) & 0xFF),
    (uint8_t)(color & 0xFF)
  );
}

String dashboardColorHex(uint32_t color) {
  char value[8];
  snprintf(value, sizeof(value), "#%06lX", (unsigned long)(color & 0xFFFFFFUL));
  return String(value);
}

void swapImuHorizontalAxes(float &xValue, float &yValue) {
  if (!SWAP_IMU_XY_AXES) {
    return;
  }

  float swapped = xValue;
  xValue = yValue;
  yValue = swapped;
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
    swapImuHorizontalAxes(ax, ay);
    accelerationUpdated = true;
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
    swapImuHorizontalAxes(gx, gy);
  }

  rawRollDeg = atan2(ay, az) * 180.0f / PI;
  rawPitchDeg = atan2(-ax, sqrt((ay * ay) + (az * az))) * 180.0f / PI;
  applyTiltOrientation();

  if (accelerationUpdated) {
    if (!tiltReferenceCalibrated) {
      resetTiltReference();
      tiltReferenceCalibrated = true;
    }

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

  pitchDeg = angleDeltaDeg(orientedPitchDeg, pitchZeroDeg);
  rollDeg = angleDeltaDeg(orientedRollDeg, rollZeroDeg);

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
  dashboardScalePercent = clampInt(
    dashboardPreferences.getInt("scale_pct", DASHBOARD_SCALE_DEFAULT_PERCENT),
    DASHBOARD_SCALE_MIN_PERCENT,
    DASHBOARD_SCALE_MAX_PERCENT
  );
  dashboardOffsetX = clampInt(
    dashboardPreferences.getInt("offset_x", 0),
    0,
    dashboardMaxOffsetX(dashboardScalePercent)
  );
  dashboardOffsetY = clampInt(
    dashboardPreferences.getInt("offset_y", 0),
    0,
    dashboardMaxOffsetY(dashboardScalePercent)
  );
  dashboardColors.background = dashboardPreferences.getUInt("clr_bg", DASHBOARD_COLOR_DEFAULTS.background) & 0xFFFFFFUL;
  dashboardColors.primary = dashboardPreferences.getUInt("clr_primary", DASHBOARD_COLOR_DEFAULTS.primary) & 0xFFFFFFUL;
  dashboardColors.detail = dashboardPreferences.getUInt("clr_detail", DASHBOARD_COLOR_DEFAULTS.detail) & 0xFFFFFFUL;
  dashboardColors.accent = dashboardPreferences.getUInt("clr_accent", DASHBOARD_COLOR_DEFAULTS.accent) & 0xFFFFFFUL;
  dashboardColors.gearSelectedBackground = dashboardPreferences.getUInt("clr_gsel_bg", DASHBOARD_COLOR_DEFAULTS.gearSelectedBackground) & 0xFFFFFFUL;
  dashboardColors.gearSelectedText = dashboardPreferences.getUInt("clr_gsel_txt", DASHBOARD_COLOR_DEFAULTS.gearSelectedText) & 0xFFFFFFUL;
  dashboardColors.gearUnselectedText = dashboardPreferences.getUInt("clr_gmute", DASHBOARD_COLOR_DEFAULTS.gearUnselectedText) & 0xFFFFFFUL;
  dashboardColors.turnActive = dashboardPreferences.getUInt("clr_turn_on", DASHBOARD_COLOR_DEFAULTS.turnActive) & 0xFFFFFFUL;
  dashboardColors.turnInactive = dashboardPreferences.getUInt("clr_turn_off", DASHBOARD_COLOR_DEFAULTS.turnInactive) & 0xFFFFFFUL;
  dashboardColors.headlightActive = dashboardPreferences.getUInt("clr_light_on", DASHBOARD_COLOR_DEFAULTS.headlightActive) & 0xFFFFFFUL;
  dashboardColors.headlightInactive = dashboardPreferences.getUInt("clr_light_off", DASHBOARD_COLOR_DEFAULTS.headlightInactive) & 0xFFFFFFUL;
  dashboardColors.warningActive = dashboardPreferences.getUInt("clr_warn_on", DASHBOARD_COLOR_DEFAULTS.warningActive) & 0xFFFFFFUL;
  dashboardColors.warningInactive = dashboardPreferences.getUInt("clr_warn_off", DASHBOARD_COLOR_DEFAULTS.warningInactive) & 0xFFFFFFUL;
  dashboardTransformDirty = true;
  tiltOrientationDeg = dashboardPreferences.getInt("tilt_rotation", tiltOrientationDeg);
  if (tiltOrientationDeg != 0 && tiltOrientationDeg != 90 &&
      tiltOrientationDeg != 180 && tiltOrientationDeg != 270) {
    tiltOrientationDeg = 0;
  }
  invertPitchAxis = dashboardPreferences.getBool("invert_pitch", invertPitchAxis);
  invertRollAxis = dashboardPreferences.getBool("invert_roll", invertRollAxis);
  showTiltAxisLabels = dashboardPreferences.getBool("tilt_labels", showTiltAxisLabels);
  tiltBubbleToleranceDeg = dashboardPreferences.getFloat("tilt_tolerance", tiltBubbleToleranceDeg);
  if (tiltBubbleToleranceDeg < 0.0f || tiltBubbleToleranceDeg > 10.0f) {
    tiltBubbleToleranceDeg = 1.0f;
  }
  int savedSpeedMode = dashboardPreferences.getInt("speed_mode", SPEED_LEAD_GPS);
  speedFusionLeadMode = savedSpeedMode == SPEED_LEAD_ACCEL ? SPEED_LEAD_ACCEL : SPEED_LEAD_GPS;
}

void saveScreenBrightness() {
  dashboardPreferences.putInt("brightness", screenBrightnessPercent);
}

void saveDashboardColorIfChanged(const char *key, uint32_t color) {
  uint32_t normalized = color & 0xFFFFFFUL;
  if (dashboardPreferences.getUInt(key, 0xFFFFFFFFUL) != normalized) {
    dashboardPreferences.putUInt(key, normalized);
  }
}

void saveDashboardSettings() {
  dashboardPreferences.putInt("brightness", screenBrightnessPercent);
  dashboardPreferences.putInt("scale_pct", dashboardScalePercent);
  dashboardPreferences.putInt("offset_x", dashboardOffsetX);
  dashboardPreferences.putInt("offset_y", dashboardOffsetY);
  saveDashboardColorIfChanged("clr_bg", dashboardColors.background);
  saveDashboardColorIfChanged("clr_primary", dashboardColors.primary);
  saveDashboardColorIfChanged("clr_detail", dashboardColors.detail);
  saveDashboardColorIfChanged("clr_accent", dashboardColors.accent);
  saveDashboardColorIfChanged("clr_gsel_bg", dashboardColors.gearSelectedBackground);
  saveDashboardColorIfChanged("clr_gsel_txt", dashboardColors.gearSelectedText);
  saveDashboardColorIfChanged("clr_gmute", dashboardColors.gearUnselectedText);
  saveDashboardColorIfChanged("clr_turn_on", dashboardColors.turnActive);
  saveDashboardColorIfChanged("clr_turn_off", dashboardColors.turnInactive);
  saveDashboardColorIfChanged("clr_light_on", dashboardColors.headlightActive);
  saveDashboardColorIfChanged("clr_light_off", dashboardColors.headlightInactive);
  saveDashboardColorIfChanged("clr_warn_on", dashboardColors.warningActive);
  saveDashboardColorIfChanged("clr_warn_off", dashboardColors.warningInactive);
  dashboardPreferences.putInt("tilt_rotation", tiltOrientationDeg);
  dashboardPreferences.putBool("invert_pitch", invertPitchAxis);
  dashboardPreferences.putBool("invert_roll", invertRollAxis);
  dashboardPreferences.putBool("tilt_labels", showTiltAxisLabels);
  dashboardPreferences.putFloat("tilt_tolerance", tiltBubbleToleranceDeg);
  dashboardPreferences.putInt("speed_mode", (int)speedFusionLeadMode);
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

int dashboardScaledWidth(int scalePercent) {
  return ((SCREEN_W * scalePercent) + 50) / 100;
}

int dashboardScaledHeight(int scalePercent) {
  return ((SCREEN_H * scalePercent) + 50) / 100;
}

int dashboardMaxOffsetX(int scalePercent) {
  return SCREEN_W - dashboardScaledWidth(scalePercent);
}

int dashboardMaxOffsetY(int scalePercent) {
  return SCREEN_H - dashboardScaledHeight(scalePercent);
}

void applyDashboardDisplayTransform(int scalePercent, int offsetX, int offsetY) {
  int nextScale = clampInt(
    scalePercent,
    DASHBOARD_SCALE_MIN_PERCENT,
    DASHBOARD_SCALE_MAX_PERCENT
  );
  int nextOffsetX = clampInt(offsetX, 0, dashboardMaxOffsetX(nextScale));
  int nextOffsetY = clampInt(offsetY, 0, dashboardMaxOffsetY(nextScale));

  if (nextScale == dashboardScalePercent &&
      nextOffsetX == dashboardOffsetX &&
      nextOffsetY == dashboardOffsetY) {
    return;
  }

  dashboardScalePercent = nextScale;
  dashboardOffsetX = nextOffsetX;
  dashboardOffsetY = nextOffsetY;
  dashboardTransformDirty = true;
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
  wifiStaConnectStartedMs = 0;
  wifiPreferences.remove("ssid");
  wifiPreferences.remove("password");
}

void beginWifiStation() {
  if (wifiStaSsid.length() == 0) {
    return;
  }

  WiFi.begin(wifiStaSsid.c_str(), wifiStaPassword.c_str());
  wifiStaConnectStartedMs = millis();
  if (wifiStaConnectStartedMs == 0) {
    wifiStaConnectStartedMs = 1;
  }
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
    wifiStaConnectStartedMs = 0;
    return "Connected";
  }
  if (status == WL_CONNECT_FAILED) {
    return "Connect failed";
  }
  if (status == WL_CONNECTION_LOST) {
    return "Connection lost";
  }
  if (status == WL_DISCONNECTED) {
    if (wifiStaCredentialsSaved && wifiStaConnectStartedMs != 0 &&
        (millis() - wifiStaConnectStartedMs) < WIFI_STA_CONNECT_TIMEOUT_MS) {
      return "Connecting";
    }
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

String wifiStationNetworkLabel() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
  return "None";
}

String wifiStationErrorLabel() {
  wl_status_t status = WiFi.status();
  if (!wifiStaCredentialsSaved || status == WL_CONNECTED || status == WL_IDLE_STATUS) {
    return "None";
  }
  if (status == WL_NO_SSID_AVAIL) {
    return "Saved network was not found.";
  }
  if (status == WL_CONNECT_FAILED) {
    return "Connection rejected. Check the WiFi password.";
  }
  if (status == WL_CONNECTION_LOST) {
    return "Connection to the WiFi network was lost.";
  }
  if (status == WL_DISCONNECTED) {
    if (wifiStaConnectStartedMs != 0 &&
        (millis() - wifiStaConnectStartedMs) < WIFI_STA_CONNECT_TIMEOUT_MS) {
      return "None";
    }
    return "Unable to connect. Check the password and signal strength.";
  }
  return "Unknown WiFi connection error.";
}

void initRcPulseInput(RcPulseInput &input) {
  pinMode(input.pin, INPUT);
  input.isrRawHigh = digitalRead(input.pin) == HIGH;
  input.isrPulseStartUs = 0;
  input.isrLastPulseWidthUs = 0;
  input.isrLastPulseAtUs = 0;
  input.isrPulseSequence = 0;
  input.lastProcessedPulseSequence = 0;
  input.pulseFresh = false;
  input.pulseWidthUs = 0;
  input.pulseAgeMs = 0;
  input.newPulseAvailable = false;
  input.newValidPulse = false;
  input.lastValidPulseAtUs = 0;
  resetRcPulseFilter(input);
}

void IRAM_ATTR handleRcPulseInputChange(RcPulseInput &input) {
  bool rawHigh = digitalRead(input.pin) == HIGH;
  unsigned long nowUs = micros();

  input.isrRawHigh = rawHigh;
  if (rawHigh) {
    input.isrPulseStartUs = nowUs;
  } else if (input.isrPulseStartUs != 0) {
    input.isrLastPulseWidthUs = nowUs - input.isrPulseStartUs;
    input.isrLastPulseAtUs = nowUs;
    input.isrPulseStartUs = 0;
    input.isrPulseSequence++;
  }
}

void updateRcPulseInput(RcPulseInput &input) {
  noInterrupts();
  bool rawHigh = input.isrRawHigh;
  unsigned long pulseWidthUs = input.isrLastPulseWidthUs;
  unsigned long lastPulseAtUs = input.isrLastPulseAtUs;
  uint32_t pulseSequence = input.isrPulseSequence;
  interrupts();

  unsigned long nowUs = micros();
  bool pulseSeen = lastPulseAtUs != 0;
  unsigned long pulseAgeUs = pulseSeen ?
    (nowUs - lastPulseAtUs) :
    (input.staleUs + 1);
  bool validPulse =
    pulseSeen &&
    pulseWidthUs >= input.minValidUs &&
    pulseWidthUs <= input.maxValidUs;
  bool sourceWasFresh =
    input.lastValidPulseAtUs != 0 &&
    (nowUs - input.lastValidPulseAtUs) <= input.staleUs;

  input.rawHigh = rawHigh;
  input.pulseWidthUs = pulseWidthUs;
  input.pulseAgeMs = pulseSeen ? (pulseAgeUs / 1000UL) : 0;
  input.pulseFresh = validPulse && pulseAgeUs <= input.staleUs;
  input.newPulseAvailable = pulseSequence != input.lastProcessedPulseSequence;
  input.newValidPulse = input.newPulseAvailable && validPulse;

  if (input.newPulseAvailable) {
    input.lastProcessedPulseSequence = pulseSequence;
  }

  if (input.newValidPulse) {
    if (!sourceWasFresh) {
      resetRcPulseFilter(input);
    }
    input.lastValidPulseAtUs = lastPulseAtUs;
  }

  refreshRcPulseFilterFreshness(input, nowUs);
}

void resetRcPulseFilter(RcPulseInput &input) {
  input.filterReady = false;
  input.filteredPulseFresh = false;
  input.filteredPulseWidthUs = 0;
  input.filteredPulseAgeMs = 0;
  input.filterSampleCount = 0;
  input.filterNextSampleIndex = 0;
  for (uint8_t i = 0; i < RC_PULSE_FILTER_SAMPLE_COUNT; ++i) {
    input.filterSamples[i] = 0;
  }
  input.debouncedState = false;
  input.candidateState = false;
  input.candidateStateCount = 0;
}

void refreshRcPulseFilterFreshness(RcPulseInput &input, unsigned long nowUs) {
  bool validSourceFresh =
    input.lastValidPulseAtUs != 0 &&
    (nowUs - input.lastValidPulseAtUs) <= input.staleUs;

  if (!validSourceFresh) {
    input.lastValidPulseAtUs = 0;
    resetRcPulseFilter(input);
    return;
  }

  input.filteredPulseAgeMs = (nowUs - input.lastValidPulseAtUs) / 1000UL;
  input.filteredPulseFresh = input.filterReady;
}

void recordRcPulseSample(RcPulseInput &input) {
  if (!input.newValidPulse) {
    return;
  }

  input.filterSamples[input.filterNextSampleIndex] = input.pulseWidthUs;
  input.filterNextSampleIndex =
    (input.filterNextSampleIndex + 1) % RC_PULSE_FILTER_SAMPLE_COUNT;
  if (input.filterSampleCount < RC_PULSE_FILTER_SAMPLE_COUNT) {
    input.filterSampleCount++;
  }

  if (input.filterSampleCount == RC_PULSE_FILTER_SAMPLE_COUNT) {
    input.filteredPulseWidthUs = medianRcPulseSample(input);
    input.filterReady = true;
  }
}

unsigned long medianRcPulseSample(const RcPulseInput &input) {
  unsigned long first = input.filterSamples[0];
  unsigned long second = input.filterSamples[1];
  unsigned long third = input.filterSamples[2];

  if (first > second) {
    unsigned long swap = first;
    first = second;
    second = swap;
  }
  if (second > third) {
    unsigned long swap = second;
    second = third;
    third = swap;
  }
  if (first > second) {
    unsigned long swap = first;
    first = second;
    second = swap;
  }

  return second;
}

void updateRcPulseMedianInput(RcPulseInput &input) {
  updateRcPulseInput(input);
  recordRcPulseSample(input);
  refreshRcPulseFilterFreshness(input, micros());
}

void updateRcPulseDigitalInput(RcPulseInput &input, unsigned long onThresholdUs) {
  updateRcPulseInput(input);
  recordRcPulseSample(input);

  if (input.newValidPulse) {
    bool candidateState = input.pulseWidthUs > onThresholdUs;
    if (input.candidateStateCount == 0 ||
        candidateState != input.candidateState) {
      input.candidateState = candidateState;
      input.candidateStateCount = 1;
    } else if (input.candidateStateCount < RC_PULSE_FILTER_SAMPLE_COUNT) {
      input.candidateStateCount++;
    }

    if (input.candidateStateCount >= RC_PULSE_FILTER_SAMPLE_COUNT) {
      input.debouncedState = input.candidateState;
    }
  }

  refreshRcPulseFilterFreshness(input, micros());
}

void IRAM_ATTR handleHeadlightInputChange() {
  handleRcPulseInputChange(headlightInput);
}

void updateHeadlightInput() {
  updateRcPulseDigitalInput(headlightInput, HEADLIGHT_PULSE_ON_THRESHOLD_US);
  dashboardHeadlightsOn =
    headlightInput.filteredPulseFresh && headlightInput.debouncedState;
}

void IRAM_ATTR handleSoundSwitchInputChange() {
  handleRcPulseInputChange(soundSwitchInput);
}

void updateSoundSwitchInput() {
  updateRcPulseDigitalInput(soundSwitchInput, SOUND_SWITCH_PULSE_ON_THRESHOLD_US);
  soundSwitchOn =
    soundSwitchInput.filteredPulseFresh && soundSwitchInput.debouncedState;
  rcHornRequested = soundSwitchOn;
}

bool hornPlaybackRequested() {
  if (hornWebMode == HornWebMode::Off) {
    return false;
  }
  return rcHornRequested || webHornRequested;
}

const char *hornWebModeValue() {
  switch (hornWebMode) {
    case HornWebMode::Off:
      return "off";
    case HornWebMode::On:
      return "on";
    case HornWebMode::Rc:
    default:
      return "rc";
  }
}

String hornWebModeLabel() {
  if (hornWebMode == HornWebMode::Off) {
    return "Off";
  }
  if (hornWebMode == HornWebMode::On) {
    return "On";
  }
  return "RC";
}

bool setHornWebMode(const String &value) {
  String mode = value;
  mode.trim();
  mode.toLowerCase();

  if (mode == "off") {
    hornWebMode = HornWebMode::Off;
  } else if (mode == "rc") {
    hornWebMode = HornWebMode::Rc;
  } else if (mode == "on") {
    hornWebMode = HornWebMode::On;
  } else {
    return false;
  }

  webHornRequested = hornWebMode == HornWebMode::On;
  if (hornWebMode == HornWebMode::Off) {
    cancelHornSynth();
  }
  return true;
}

void updateHornPlayback() {
  webHornRequested = hornWebMode == HornWebMode::On;
  bool hornRequested = hornPlaybackRequested();

  if (hornRequested == hornWasRequested) {
    return;
  }

  if (hornRequested) {
    String playbackMessage;
    startHornSynth(playbackMessage);
  } else {
    stopHornSynth();
  }

  hornWasRequested = hornRequested;
}

void handleHornMode(AsyncWebServerRequest *request) {
  if (!requestHasArg(request, "mode") || !setHornWebMode(requestArg(request, "mode"))) {
    request->send(400, "application/json", hornResponseJson(false, "Horn mode must be off, rc, or on"));
    return;
  }

  request->send(200, "application/json", hornResponseJson(true, String("Horn mode: ") + hornWebModeLabel()));
}

void handleShortHorn(AsyncWebServerRequest *request) {
  if (hornWebMode == HornWebMode::Off) {
    request->send(409, "application/json", hornResponseJson(false, "Set horn override to RC or On first"));
    return;
  }

  triggerShortHornSynth(WEB_SHORT_HORN_DURATION_MS);
  request->send(200, "application/json", hornResponseJson(true, "Horn honk"));
}

String hornResponseJson(bool ok, const String &message) {
  String json = "{";
  json += "\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"message\":\"";
  json += jsonEscape(message);
  json += "\",\"horn_web_mode\":\"";
  json += hornWebModeValue();
  json += "\",\"horn_synth_active\":";
  json += hornSynthIsActive() ? "true" : "false";
  json += ",\"horn_synth_status\":\"";
  json += jsonEscape(hornSynthStatusLabel());
  json += "\"}";
  return json;
}

uint32_t vehiclePwmDutyForPulse(unsigned long pulseUs) {
  const uint32_t maxDuty = (1UL << VEHICLE_PWM_RESOLUTION_BITS) - 1UL;
  return (uint32_t)(((uint64_t)pulseUs * maxDuty + (VEHICLE_PWM_PERIOD_US / 2UL)) /
    VEHICLE_PWM_PERIOD_US);
}

void writeVehiclePwmPulse(uint8_t channel, uint8_t pin, unsigned long pulseUs) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, vehiclePwmDutyForPulse(pulseUs));
#else
  ledcWrite(channel, vehiclePwmDutyForPulse(pulseUs));
#endif
}

unsigned long steeringPulseForPercent(int steeringPercent) {
  int clampedPercent = clampInt(steeringPercent, -100, 100);
  if (!steeringCalibrationValid()) {
    return STEERING_DEFAULT_CENTER_US;
  }

  if (clampedPercent < 0) {
    unsigned long range = steeringLeftUs - steeringCenterUs;
    return steeringCenterUs + ((range * (unsigned long)(-clampedPercent)) / 100UL);
  }

  unsigned long range = steeringCenterUs - steeringRightUs;
  return steeringCenterUs - ((range * (unsigned long)clampedPercent) / 100UL);
}

unsigned long throttlePulseForPercent(int throttlePercent) {
  int clampedPercent = clampInt(throttlePercent, -100, 100);

  if (clampedPercent > 0) {
    unsigned long range = THROTTLE_OFF_US - THROTTLE_FULL_FORWARD_US;
    return THROTTLE_OFF_US - ((range * (unsigned long)clampedPercent) / 100UL);
  }

  unsigned long range = THROTTLE_FULL_REVERSE_US - THROTTLE_OFF_US;
  return THROTTLE_OFF_US + ((range * (unsigned long)(-clampedPercent)) / 100UL);
}

void IRAM_ATTR handleSteeringReceiverInputChange() {
  handleRcPulseInputChange(steeringReceiverInput);
}

void IRAM_ATTR handleThrottleReceiverInputChange() {
  handleRcPulseInputChange(throttleReceiverInput);
}

void resetThrottleGearState() {
  throttleGearState.committedDirection = ThrottleGearDirection::Neutral;
  throttleGearState.candidateDirection = ThrottleGearDirection::Neutral;
  throttleGearState.candidateCount = 0;
  throttleGearState.initialized = false;
  throttleGearState.centeredTimerActive = false;
  throttleGearState.centeredSinceMs = 0;
}

ThrottleGearDirection classifyReceiverThrottleDirection(unsigned long pulseWidthUs) {
  if (throttleGearState.initialized) {
    if (throttleGearState.committedDirection == ThrottleGearDirection::Drive &&
        pulseWidthUs < THROTTLE_OFF_US - THROTTLE_DIRECTION_RELEASE_TOLERANCE_US) {
      return ThrottleGearDirection::Drive;
    }
    if (throttleGearState.committedDirection == ThrottleGearDirection::Reverse &&
        pulseWidthUs > THROTTLE_OFF_US + THROTTLE_DIRECTION_RELEASE_TOLERANCE_US) {
      return ThrottleGearDirection::Reverse;
    }
  }

  if (pulseWidthUs < THROTTLE_OFF_US - THROTTLE_DRIVE_ENTRY_TOLERANCE_US) {
    return ThrottleGearDirection::Drive;
  }
  if (pulseWidthUs > THROTTLE_OFF_US + THROTTLE_REVERSE_ENTRY_TOLERANCE_US) {
    return ThrottleGearDirection::Reverse;
  }
  return ThrottleGearDirection::Neutral;
}

void updateReceiverThrottleGear(unsigned long pulseWidthUs) {
  if (!throttleReceiverInput.newValidPulse || !throttleReceiverInput.filterReady) {
    return;
  }

  ThrottleGearDirection classifiedDirection =
    classifyReceiverThrottleDirection(pulseWidthUs);

  if (throttleGearState.initialized &&
      throttleGearState.committedDirection == ThrottleGearDirection::Neutral &&
      classifiedDirection != ThrottleGearDirection::Neutral) {
    throttleGearState.centeredTimerActive = false;
    throttleGearState.centeredSinceMs = 0;
  }

  if (throttleGearState.initialized &&
      classifiedDirection == throttleGearState.committedDirection) {
    throttleGearState.candidateDirection = classifiedDirection;
    throttleGearState.candidateCount = 0;
    if (classifiedDirection == ThrottleGearDirection::Neutral &&
        !throttleGearState.centeredTimerActive) {
      throttleGearState.centeredTimerActive = true;
      throttleGearState.centeredSinceMs = millis();
    }
    return;
  }

  if (throttleGearState.candidateCount == 0 ||
      classifiedDirection != throttleGearState.candidateDirection) {
    throttleGearState.candidateDirection = classifiedDirection;
    throttleGearState.candidateCount = 1;
  } else if (throttleGearState.candidateCount < THROTTLE_GEAR_DEBOUNCE_SAMPLE_COUNT) {
    throttleGearState.candidateCount++;
  }

  if (throttleGearState.candidateCount < THROTTLE_GEAR_DEBOUNCE_SAMPLE_COUNT) {
    return;
  }

  throttleGearState.committedDirection = classifiedDirection;
  throttleGearState.candidateCount = 0;
  throttleGearState.initialized = true;
  if (classifiedDirection == ThrottleGearDirection::Neutral) {
    throttleGearState.centeredTimerActive = true;
    throttleGearState.centeredSinceMs = millis();
  } else {
    throttleGearState.centeredTimerActive = false;
    throttleGearState.centeredSinceMs = 0;
  }
}

void updateWebThrottleGear() {
  ThrottleGearDirection commandedDirection = ThrottleGearDirection::Neutral;
  if (vehicleControl.throttlePercent > 0) {
    commandedDirection = ThrottleGearDirection::Drive;
  } else if (vehicleControl.throttlePercent < 0) {
    commandedDirection = ThrottleGearDirection::Reverse;
  }

  if (!throttleGearState.initialized ||
      commandedDirection != throttleGearState.committedDirection) {
    throttleGearState.committedDirection = commandedDirection;
    throttleGearState.candidateDirection = commandedDirection;
    throttleGearState.candidateCount = 0;
    throttleGearState.initialized = true;
    if (commandedDirection == ThrottleGearDirection::Neutral) {
      throttleGearState.centeredTimerActive = true;
      throttleGearState.centeredSinceMs = millis();
    } else {
      throttleGearState.centeredTimerActive = false;
      throttleGearState.centeredSinceMs = 0;
    }
  }
}

void applyThrottleDashboardState(unsigned long pulseWidthUs) {
  dashboardRpmK = 0.0f;

  if (!throttleGearState.initialized ||
      throttleGearState.committedDirection == ThrottleGearDirection::Neutral) {
    bool parkDelayElapsed =
      throttleGearState.initialized &&
      throttleGearState.centeredTimerActive &&
      (millis() - throttleGearState.centeredSinceMs) >= THROTTLE_PARK_DELAY_MS;
    dashboardGearIndex = parkDelayElapsed ? GEAR_PARK_INDEX : GEAR_NEUTRAL_INDEX;
    return;
  }

  if (throttleGearState.committedDirection == ThrottleGearDirection::Drive) {
    float forwardProgress = pulseWidthUs < THROTTLE_OFF_US ?
      (float)(THROTTLE_OFF_US - pulseWidthUs) /
        (float)(THROTTLE_OFF_US - THROTTLE_FULL_FORWARD_US) :
      0.0f;
    dashboardRpmK = clamp01(forwardProgress) * THROTTLE_RPM_MAX_K;
    dashboardGearIndex = GEAR_DRIVE_INDEX;
    return;
  }

  float reverseProgress = pulseWidthUs > THROTTLE_OFF_US ?
    (float)(pulseWidthUs - THROTTLE_OFF_US) /
      (float)(THROTTLE_FULL_REVERSE_US - THROTTLE_OFF_US) :
    0.0f;
  dashboardRpmK = clamp01(reverseProgress) * THROTTLE_RPM_MAX_K;
  dashboardGearIndex = GEAR_REVERSE_INDEX;
}

void updateVehicleControlDashboard() {
  bool receiverMode = vehicleControl.mode == VehicleControlMode::Receiver;
  if (receiverMode) {
    updateRcPulseMedianInput(steeringReceiverInput);
    updateRcPulseMedianInput(throttleReceiverInput);
    vehicleControl.steeringPulseUs = steeringReceiverInput.filteredPulseFresh ?
      steeringReceiverInput.filteredPulseWidthUs : 0;
    vehicleControl.throttlePulseUs = throttleReceiverInput.filteredPulseFresh ?
      throttleReceiverInput.filteredPulseWidthUs : 0;
    vehicleControl.steeringPercent = 0;
    vehicleControl.throttlePercent = 0;
  }

  bool steeringSignalAvailable =
    !receiverMode || steeringReceiverInput.filteredPulseFresh;
  if (!steeringCalibrationValid() || !steeringSignalAvailable) {
    steeringInputValid = false;
    steeringWheelAngleDeg = 0;
    leftTurnSignalActive = false;
    rightTurnSignalActive = false;
  } else {
    steeringInputValid = true;
    steeringWheelAngleDeg = clampInt(
      (int)round(mapSteeringPulseToAngle(vehicleControl.steeringPulseUs)),
      STEERING_MIN_DEG,
      STEERING_MAX_DEG
    );
    updateTurnSignalIntent();
  }

  if (receiverMode && !throttleReceiverInput.filteredPulseFresh) {
    resetThrottleGearState();
    dashboardRpmK = 0.0f;
    dashboardGearIndex = GEAR_NEUTRAL_INDEX;
    return;
  }

  unsigned long pulseWidthUs = vehicleControl.throttlePulseUs;
  if (receiverMode) {
    updateReceiverThrottleGear(pulseWidthUs);
  } else {
    updateWebThrottleGear();
  }
  applyThrottleDashboardState(pulseWidthUs);
}

void releaseVehiclePwmOutputs() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(STEERING_CONTROL_PIN);
  ledcDetach(THROTTLE_CONTROL_PIN);
#else
  ledcDetachPin(STEERING_CONTROL_PIN);
  ledcDetachPin(THROTTLE_CONTROL_PIN);
#endif
  vehicleControl.pwmReady = false;
}

void enableReceiverControl() {
  releaseVehiclePwmOutputs();
  resetThrottleGearState();
  vehicleControl.mode = VehicleControlMode::Receiver;
  vehicleControl.armed = false;
  vehicleControlOwner = VehicleControlOwner::None;
  vehicleControl.lastHeartbeatMs = 0;
  vehicleControl.steeringPercent = 0;
  vehicleControl.throttlePercent = 0;

  initRcPulseInput(steeringReceiverInput);
  attachInterrupt(
    digitalPinToInterrupt(STEERING_CONTROL_PIN),
    handleSteeringReceiverInputChange,
    CHANGE
  );
  initRcPulseInput(throttleReceiverInput);
  attachInterrupt(
    digitalPinToInterrupt(THROTTLE_CONTROL_PIN),
    handleThrottleReceiverInputChange,
    CHANGE
  );
  updateVehicleControlDashboard();
}

bool enableWebControl() {
  detachInterrupt(digitalPinToInterrupt(STEERING_CONTROL_PIN));
  detachInterrupt(digitalPinToInterrupt(THROTTLE_CONTROL_PIN));

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  bool steeringAttached = ledcAttach(
    STEERING_CONTROL_PIN,
    VEHICLE_PWM_FREQUENCY_HZ,
    VEHICLE_PWM_RESOLUTION_BITS
  );
  bool throttleAttached = ledcAttach(
    THROTTLE_CONTROL_PIN,
    VEHICLE_PWM_FREQUENCY_HZ,
    VEHICLE_PWM_RESOLUTION_BITS
  );
  if (!steeringAttached || !throttleAttached) {
#else
  uint32_t steeringFrequency = ledcSetup(
    STEERING_PWM_CHANNEL,
    VEHICLE_PWM_FREQUENCY_HZ,
    VEHICLE_PWM_RESOLUTION_BITS
  );
  uint32_t throttleFrequency = ledcSetup(
    THROTTLE_PWM_CHANNEL,
    VEHICLE_PWM_FREQUENCY_HZ,
    VEHICLE_PWM_RESOLUTION_BITS
  );
  if (steeringFrequency == 0 || throttleFrequency == 0) {
#endif
    enableReceiverControl();
    return false;
  }

#if ESP_ARDUINO_VERSION_MAJOR < 3
  ledcAttachPin(STEERING_CONTROL_PIN, STEERING_PWM_CHANNEL);
  ledcAttachPin(THROTTLE_CONTROL_PIN, THROTTLE_PWM_CHANNEL);
#endif
  resetThrottleGearState();
  vehicleControl.mode = VehicleControlMode::Web;
  vehicleControl.pwmReady = true;
  return true;
}

void setVehicleControlCommand(int steeringPercent, int throttlePercent) {
  if (vehicleControl.mode != VehicleControlMode::Web) {
    updateVehicleControlDashboard();
    return;
  }

  vehicleControl.steeringPercent = clampInt(steeringPercent, -100, 100);
  vehicleControl.throttlePercent = clampInt(throttlePercent, -100, 100);
  vehicleControl.steeringPulseUs = steeringPulseForPercent(vehicleControl.steeringPercent);
  vehicleControl.throttlePulseUs = throttlePulseForPercent(vehicleControl.throttlePercent);

  if (vehicleControl.pwmReady) {
    writeVehiclePwmPulse(
      STEERING_PWM_CHANNEL,
      STEERING_CONTROL_PIN,
      vehicleControl.steeringPulseUs
    );
    writeVehiclePwmPulse(
      THROTTLE_PWM_CHANNEL,
      THROTTLE_CONTROL_PIN,
      vehicleControl.throttlePulseUs
    );
  }

  updateVehicleControlDashboard();
}

void initVehicleControl() {
  enableReceiverControl();
}

bool armVehicleControl() {
  return armVehicleControlForOwner(VehicleControlOwner::Local);
}

bool armVehicleControlForOwner(VehicleControlOwner owner) {
  if (owner == VehicleControlOwner::None) {
    return false;
  }
  if (vehicleControl.armed && vehicleControlOwner != owner) {
    return false;
  }
  if (!steeringCalibrationValid()) {
    return false;
  }
  if (vehicleControl.mode == VehicleControlMode::Receiver && !enableWebControl()) {
    return false;
  }
  if (!vehicleControl.pwmReady) {
    return false;
  }

  vehicleControl.armed = true;
  vehicleControlOwner = owner;
  vehicleControl.watchdogStopped = false;
  setVehicleControlCommand(0, 0);
  vehicleControl.lastHeartbeatMs = millis();
  return true;
}

void stopVehicleControl(bool watchdogStop) {
  if (vehicleControl.mode == VehicleControlMode::Web && vehicleControl.pwmReady) {
    setVehicleControlCommand(0, 0);
  }

  enableReceiverControl();
  apiControlSessionId = "";
  apiControlLastSequence = 0;
  apiControlSequenceSeen = false;
  vehicleControl.watchdogStopped = watchdogStop;
}

void updateVehicleControlWatchdog() {
  if (!vehicleControl.armed || vehicleControl.lastHeartbeatMs == 0) {
    return;
  }

  if ((millis() - vehicleControl.lastHeartbeatMs) > VEHICLE_CONTROL_WATCHDOG_MS) {
    stopVehicleControl(true);
  }
}

unsigned long vehicleControlHeartbeatAgeMs() {
  if (!vehicleControl.armed || vehicleControl.lastHeartbeatMs == 0) {
    return 0;
  }
  return millis() - vehicleControl.lastHeartbeatMs;
}

const char *vehicleControlModeValue() {
  return vehicleControl.mode == VehicleControlMode::Web ? "web" : "receiver";
}

const char *vehicleControlOwnerValue() {
  switch (vehicleControlOwner) {
    case VehicleControlOwner::Local:
      return "local";
    case VehicleControlOwner::Remote:
      return "remote";
    case VehicleControlOwner::None:
    default:
      return "none";
  }
}

String vehicleControlStatusLabel() {
  if (vehicleControl.mode == VehicleControlMode::Receiver) {
    return steeringReceiverInput.filteredPulseFresh ||
      throttleReceiverInput.filteredPulseFresh ?
      "Receiver active" :
      "Receiver waiting";
  }
  if (!vehicleControl.pwmReady) {
    return "PWM unavailable";
  }
  if (!steeringCalibrationValid()) {
    return "Calibration invalid";
  }
  if (vehicleControl.armed) {
    return vehicleControlOwner == VehicleControlOwner::Remote ?
      "Remote control armed" : "Local web control armed";
  }
  return "Web control ready";
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
  if (headlightInput.filteredPulseFresh) {
    return dashboardHeadlightsOn ? "On filtered" : "Off filtered";
  }
  if (headlightInput.pulseFresh) {
    return "Filtering";
  }
  return "No valid pulse";
}

String headlightInputRawLabel() {
  return headlightInput.rawHigh ? "HIGH" : "LOW";
}

String soundSwitchInputStatusLabel() {
  if (soundSwitchInput.filteredPulseFresh) {
    return soundSwitchOn ? "On filtered" : "Off filtered";
  }
  if (soundSwitchInput.pulseFresh) {
    return "Filtering";
  }
  return "No pulse";
}

String turnSignalInputPulseStatusLabel() {
  if (vehicleControl.mode == VehicleControlMode::Receiver) {
    if (steeringReceiverInput.filteredPulseFresh) {
      return "Filtered";
    }
    return steeringReceiverInput.pulseFresh ? "Filtering" : "No receiver pulse";
  }
  return vehicleControl.armed ? "Web PWM armed" : "Web PWM idle";
}

String throttleInputStatusLabel() {
  if (vehicleControl.mode == VehicleControlMode::Receiver) {
    if (throttleReceiverInput.filteredPulseFresh) {
      return "Filtered";
    }
    return throttleReceiverInput.pulseFresh ? "Filtering" : "No receiver pulse";
  }
  return vehicleControlStatusLabel();
}

String steeringInputStatusLabel() {
  return vehicleControlStatusLabel();
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
  if (dashboardTransformDirty) {
    tft.fillScreen(TFT_BLACK);
    dashboardTransformDirty = false;
  }

  if (dashboardScalePercent == DASHBOARD_SCALE_MAX_PERCENT) {
    spr.pushSprite(0, 0);
    return;
  }

  int scaledWidth = dashboardScaledWidth(dashboardScalePercent);
  int scaledHeight = dashboardScaledHeight(dashboardScalePercent);
  uint8_t *source = static_cast<uint8_t *>(spr.getPointer());

  tft.startWrite();
  for (int destinationY = 0; destinationY < scaledHeight; ++destinationY) {
    int sourceY = (destinationY * SCREEN_H) / scaledHeight;
    int sourceRow = sourceY * SCREEN_W;
    for (int destinationX = 0; destinationX < scaledWidth; ++destinationX) {
      int sourceX = (destinationX * SCREEN_W) / scaledWidth;
      dashboardScaledRow[destinationX] = source[sourceRow + sourceX];
    }
    tft.pushImage(
      dashboardOffsetX,
      dashboardOffsetY + destinationY,
      scaledWidth,
      1,
      dashboardScaledRow,
      true
    );
  }
  tft.endWrite();
}

// ----------------------
// Setup / loop
// ----------------------
void setup() {
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  loadScreenBrightness();
  applyScreenBrightness();
  loadSteeringCalibration();
  initVehicleControl();
  initRcPulseInput(headlightInput);
  attachInterrupt(digitalPinToInterrupt(HEADLIGHT_INPUT_PIN), handleHeadlightInputChange, CHANGE);
  updateHeadlightInput();
  initRcPulseInput(soundSwitchInput);
  attachInterrupt(digitalPinToInterrupt(SOUND_SWITCH_INPUT_PIN), handleSoundSwitchInputChange, CHANGE);
  updateSoundSwitchInput();
  pinMode(LEFT_TURN_LED_PIN, OUTPUT);
  pinMode(RIGHT_TURN_LED_PIN, OUTPUT);
  writeTurnSignalLed(LEFT_TURN_LED_PIN, false);
  writeTurnSignalLed(RIGHT_TURN_LED_PIN, false);
  updateVehicleControlDashboard();
  updateTurnSignalOutputs();

  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);

  initBrowserAudioStorage();
  initHornSynth();

  spr.setColorDepth(8);
  screenSpriteAvailable = spr.createSprite(SCREEN_W, SCREEN_H) != nullptr;
  if (screenSpriteAvailable) {
    spr.fillSprite(TFT_BLACK);
  } else {
    Serial.println("Dashboard sprite allocation failed; using direct TFT fallback");
    renderSpriteUnavailableScreen();
  }

  initWifi();
  initRemoteConnection();

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

  registerApiRoutes();
  server.begin();

  renderCurrentScreen();
}

void loop() {
  updateHeadlightInput();
  updateSoundSwitchInput();
  updateHornPlayback();
  updateVehicleControlWatchdog();
  updateRemoteConnection();
  updateVehicleControlDashboard();
  updateTurnSignalOutputs();
  updateApiEvents();
  if (!hornSynthIsActive()) {
    updateBrowserAudioPlayback();
  }
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

  if (!browserAudioIsPlaying() && !hornSynthIsActive()) {
    delay(2);
  }
}
