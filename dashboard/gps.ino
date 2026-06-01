#include <HardwareSerial.h>
#include <TinyGPS++.h>

static const int GPS_RX_PIN = 27;
static const int GPS_TX_PIN = -1;
static const unsigned long GPS_BAUD = 9600;
static const unsigned long GPS_INVALID_AGE_MS = 0xFFFFFFFFUL;
static const unsigned long GPS_SAMPLE_DEBOUNCE_MS = 250;
static const unsigned long GPS_FRESH_WINDOW_MS = 1500;
static const unsigned long GPS_BRIDGE_WINDOW_MS = 2000;
static const uint32_t GPS_MIN_SATELLITES = 4;
static const float GPS_LEAD_BLEND_WEIGHT = 0.70f;
static const float ACCEL_LEAD_BLEND_WEIGHT = 0.30f;
static const float MPH_TO_MPS = 0.44704f;
static const float MPS_TO_MPH = 2.2369363f;
static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float MAX_BRIDGE_DRIFT_MPH = 8.0f;
static const float SPEED_BLEED_MPH_PER_S = 1.25f;
static const float ACCEL_NOISE_FLOOR_MPS2 = 0.08f;
static const float GPS_STEADY_DELTA_MPS = 0.18f;
static const float SIGN_LOCK_MIN_GPS_DELTA_MPS = 0.35f;
static const float SIGN_LOCK_MIN_IMU_DELTA_MPS = 0.12f;
static const float SIGN_LOCK_SCORE_THRESHOLD = 0.20f;

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

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

bool gpsDataSeen = false;
bool gpsLocationValid = false;
bool gpsSpeedValid = false;
uint32_t gpsSatellites = 0;
float gpsRawMph = 0.0f;
double gpsLatitude = 0.0;
double gpsLongitude = 0.0;
unsigned long gpsFixAgeMs = GPS_INVALID_AGE_MS;
unsigned long lastGpsByteMs = 0;
unsigned long lastGpsSpeedSampleMs = 0;
bool gpsSpeedSampleUpdated = false;

float fusedSpeedMph = 0.0f;
float predictedSpeedMph = 0.0f;
float lastGpsAnchoredMph = 0.0f;
float longitudinalAccelBiasG = 0.0f;
bool longitudinalAccelBiasReady = false;
float longitudinalAccelSign = 1.0f;
bool longitudinalAccelSignLocked = false;
float axisSignCorrelationScore = 0.0f;
float gpsIntervalUnsignedDeltaMps = 0.0f;
unsigned long lastProcessedImuSampleMs = 0;
unsigned long lastGpsAnchorMs = 0;
bool hasPreviousGpsSpeedSample = false;
float previousGpsSpeedMph = 0.0f;
SpeedSourceMode speedSourceMode = SPEED_SOURCE_IDLE;
SpeedFusionLeadMode speedFusionLeadMode = SPEED_LEAD_GPS;

float clampRange(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

bool gpsHasFreshSpeed(unsigned long now) {
  if (!gpsSpeedValid || gpsSatellites < GPS_MIN_SATELLITES || lastGpsSpeedSampleMs == 0) {
    return false;
  }

  return (now - lastGpsSpeedSampleMs) <= GPS_FRESH_WINDOW_MS;
}

float currentGpsBlendWeight() {
  if (speedFusionLeadMode == SPEED_LEAD_ACCEL &&
      imuAvailable && lastLongitudinalAccelSampleMs != 0) {
    return ACCEL_LEAD_BLEND_WEIGHT;
  }

  return GPS_LEAD_BLEND_WEIGHT;
}

float applyGpsBlend(float gpsMph, float currentPredictionMph) {
  float gpsBlendWeight = currentGpsBlendWeight();
  float blended = (gpsMph * gpsBlendWeight) + (currentPredictionMph * (1.0f - gpsBlendWeight));
  if (blended < 0.0f) {
    return 0.0f;
  }
  return blended;
}

void updateSpeedSourceMode(unsigned long now) {
  if (gpsHasFreshSpeed(now)) {
    speedSourceMode = SPEED_SOURCE_GPS;
    return;
  }

  if (imuAvailable && lastLongitudinalAccelSampleMs != 0 &&
      lastGpsAnchorMs != 0 && (now - lastGpsAnchorMs) <= GPS_BRIDGE_WINDOW_MS) {
    speedSourceMode = SPEED_SOURCE_IMU_BRIDGE;
    return;
  }

  if (lastGpsAnchorMs != 0 && fusedSpeedMph > 0.0f) {
    speedSourceMode = SPEED_SOURCE_HOLD;
    return;
  }

  speedSourceMode = SPEED_SOURCE_IDLE;
}

void maybeLockLongitudinalSign(float gpsDeltaMps) {
  if (longitudinalAccelSignLocked) {
    return;
  }

  if (fabs(gpsDeltaMps) < SIGN_LOCK_MIN_GPS_DELTA_MPS ||
      fabs(gpsIntervalUnsignedDeltaMps) < SIGN_LOCK_MIN_IMU_DELTA_MPS) {
    return;
  }

  axisSignCorrelationScore += gpsIntervalUnsignedDeltaMps * gpsDeltaMps;
  if (fabs(axisSignCorrelationScore) >= SIGN_LOCK_SCORE_THRESHOLD) {
    longitudinalAccelSign = axisSignCorrelationScore >= 0.0f ? 1.0f : -1.0f;
    longitudinalAccelSignLocked = true;
  }
}

void resetSpeedFusion() {
  predictedSpeedMph = 0.0f;
  fusedSpeedMph = 0.0f;
  lastGpsAnchoredMph = 0.0f;
  lastProcessedImuSampleMs = 0;
  lastGpsAnchorMs = 0;
  gpsIntervalUnsignedDeltaMps = 0.0f;
  hasPreviousGpsSpeedSample = false;
  previousGpsSpeedMph = 0.0f;
  longitudinalAccelSign = 1.0f;
  longitudinalAccelSignLocked = false;
  axisSignCorrelationScore = 0.0f;
  speedSourceMode = SPEED_SOURCE_IDLE;
  dashboardMph = 0.0f;

  if (lastLongitudinalAccelSampleMs != 0) {
    longitudinalAxisAccelG = readLongitudinalAxisAccelG();
    longitudinalAccelBiasG = longitudinalAxisAccelG;
    longitudinalAccelBiasReady = true;
  } else {
    longitudinalAccelBiasG = 0.0f;
    longitudinalAccelBiasReady = false;
  }
}

void initGps() {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS serial started on RX 27");
  resetSpeedFusion();
}

void updateGps() {
  unsigned long now = millis();

  gpsSpeedSampleUpdated = false;

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    lastGpsByteMs = now;
  }

  gpsDataSeen = gps.charsProcessed() > 0;
  gpsLocationValid = gps.location.isValid();
  gpsSpeedValid = gps.speed.isValid();
  gpsSatellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
  gpsRawMph = gpsSpeedValid ? gps.speed.mph() : 0.0f;
  gpsLatitude = gpsLocationValid ? gps.location.lat() : 0.0;
  gpsLongitude = gpsLocationValid ? gps.location.lng() : 0.0;

  bool speedSampleArrived = gpsSpeedValid &&
    gps.speed.age() < GPS_SAMPLE_DEBOUNCE_MS &&
    (lastGpsSpeedSampleMs == 0 || (now - lastGpsSpeedSampleMs) > GPS_SAMPLE_DEBOUNCE_MS);

  if (speedSampleArrived) {
    lastGpsSpeedSampleMs = now;
    gpsSpeedSampleUpdated = true;
  }

  if (lastGpsSpeedSampleMs != 0) {
    gpsFixAgeMs = now - lastGpsSpeedSampleMs;
  } else {
    gpsFixAgeMs = GPS_INVALID_AGE_MS;
  }
}

void updateSpeedFusion() {
  unsigned long now = millis();
  bool gpsFresh = gpsHasFreshSpeed(now);

  if (!longitudinalAccelBiasReady && lastLongitudinalAccelSampleMs != 0) {
    longitudinalAccelBiasG = longitudinalAxisAccelG;
    longitudinalAccelBiasReady = true;
  }

  if (lastLongitudinalAccelSampleMs != 0 &&
      lastLongitudinalAccelSampleMs != lastProcessedImuSampleMs) {
    unsigned long sampleMs = lastLongitudinalAccelSampleMs;
    float dtS = 0.0f;

    if (lastProcessedImuSampleMs != 0 && sampleMs > lastProcessedImuSampleMs) {
      dtS = (sampleMs - lastProcessedImuSampleMs) * 0.001f;
    }

    lastProcessedImuSampleMs = sampleMs;

    if (longitudinalAccelBiasReady && dtS > 0.0f && dtS <= 0.2f) {
      float unsignedLinearAccelMps2 = (longitudinalAxisAccelG - longitudinalAccelBiasG) * STANDARD_GRAVITY_MPS2;
      if (fabs(unsignedLinearAccelMps2) < ACCEL_NOISE_FLOOR_MPS2) {
        unsignedLinearAccelMps2 = 0.0f;
      }

      gpsIntervalUnsignedDeltaMps += unsignedLinearAccelMps2 * dtS;

      bool bridgeActive = lastGpsAnchorMs != 0 &&
        (sampleMs - lastGpsAnchorMs) <= GPS_BRIDGE_WINDOW_MS;
      float signedLinearAccelMps2 = unsignedLinearAccelMps2 *
        (longitudinalAccelSignLocked ? longitudinalAccelSign : 1.0f);

      if (gpsFresh || bridgeActive) {
        predictedSpeedMph += signedLinearAccelMps2 * dtS * MPS_TO_MPH;
        if (predictedSpeedMph < 0.0f) {
          predictedSpeedMph = 0.0f;
        }

        if (!gpsFresh && bridgeActive) {
          float minAllowedMph = lastGpsAnchoredMph - MAX_BRIDGE_DRIFT_MPH;
          if (minAllowedMph < 0.0f) {
            minAllowedMph = 0.0f;
          }
          float maxAllowedMph = lastGpsAnchoredMph + MAX_BRIDGE_DRIFT_MPH;
          predictedSpeedMph = clampRange(predictedSpeedMph, minAllowedMph, maxAllowedMph);
        }
      } else if (predictedSpeedMph > 0.0f) {
        predictedSpeedMph -= SPEED_BLEED_MPH_PER_S * dtS;
        if (predictedSpeedMph < 0.0f) {
          predictedSpeedMph = 0.0f;
        }
      }
    }
  }

  if (gpsSpeedSampleUpdated) {
    if (gpsFresh) {
      if (hasPreviousGpsSpeedSample) {
        float gpsDeltaMps = (gpsRawMph - previousGpsSpeedMph) * MPH_TO_MPS;
        maybeLockLongitudinalSign(gpsDeltaMps);

        if (fabs(gpsDeltaMps) <= GPS_STEADY_DELTA_MPS && longitudinalAccelBiasReady) {
          float biasBlend = gpsRawMph < 1.0f ? 0.10f : 0.02f;
          longitudinalAccelBiasG += (longitudinalAxisAccelG - longitudinalAccelBiasG) * biasBlend;
        }
      } else if (gpsRawMph < 1.0f && longitudinalAccelBiasReady) {
        longitudinalAccelBiasG += (longitudinalAxisAccelG - longitudinalAccelBiasG) * 0.10f;
      }

      previousGpsSpeedMph = gpsRawMph;
      hasPreviousGpsSpeedSample = true;

      predictedSpeedMph = applyGpsBlend(gpsRawMph, predictedSpeedMph);
      lastGpsAnchoredMph = predictedSpeedMph;
      lastGpsAnchorMs = now;
    }

    gpsIntervalUnsignedDeltaMps = 0.0f;
    gpsSpeedSampleUpdated = false;
  }

  fusedSpeedMph = predictedSpeedMph;
  dashboardMph = fusedSpeedMph;
  updateSpeedSourceMode(now);
}

String gpsLockLabel() {
  if (!gpsDataSeen) {
    return "No data";
  }
  if (gpsLocationValid && gpsSatellites >= GPS_MIN_SATELLITES) {
    return "Locked";
  }
  if (gpsLocationValid) {
    return "Weak";
  }
  return "Searching";
}

String gpsFixAgeLabel() {
  if (gpsFixAgeMs == GPS_INVALID_AGE_MS) {
    return "No fix";
  }
  if (gpsFixAgeMs < 1000) {
    return String(gpsFixAgeMs) + " ms";
  }
  return String(gpsFixAgeMs / 1000.0f, 1) + " s";
}

String gpsLatitudeLabel() {
  if (!gpsLocationValid) {
    return "Unavailable";
  }

  return String(gpsLatitude, 6);
}

String gpsLongitudeLabel() {
  if (!gpsLocationValid) {
    return "Unavailable";
  }

  return String(gpsLongitude, 6);
}

String speedFusionLeadModeLabel() {
  if (speedFusionLeadMode == SPEED_LEAD_ACCEL &&
      imuAvailable && lastLongitudinalAccelSampleMs != 0) {
    return "Accel Led";
  }

  return "GPS Led";
}

String speedSourceLabel() {
  if (speedSourceMode == SPEED_SOURCE_GPS) {
    return "GPS";
  }
  if (speedSourceMode == SPEED_SOURCE_IMU_BRIDGE) {
    return "IMU Bridge";
  }
  if (speedSourceMode == SPEED_SOURCE_HOLD) {
    return "Hold";
  }
  return "Idle";
}

void setSpeedFusionLeadMode(int requestedMode) {
  if (requestedMode != SPEED_LEAD_GPS && requestedMode != SPEED_LEAD_ACCEL) {
    return;
  }

  if ((int)speedFusionLeadMode == requestedMode) {
    return;
  }

  speedFusionLeadMode = (SpeedFusionLeadMode)requestedMode;
  resetSpeedFusion();
}
