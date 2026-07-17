#ifndef CAR_REMOTE_ENABLED
#define CAR_REMOTE_ENABLED 1
#endif

#ifndef CAR_REMOTE_SYNC_URL
#define CAR_REMOTE_SYNC_URL "http://165.227.55.254:8081/api/car/device/sync"
#endif

#ifndef CAR_REMOTE_DEVICE_ID
#define CAR_REMOTE_DEVICE_ID "car-dashboard-01"
#endif

// Supply this at build time. An empty token deliberately disables outbound
// requests so a production device does not transmit unauthenticated control
// traffic by accident.
#ifndef CAR_REMOTE_DEVICE_TOKEN
#define CAR_REMOTE_DEVICE_TOKEN "34d32e3f-0c5a-4d8b-9f1e-2c7a6b1e2f3a"
#endif

static const unsigned long REMOTE_ARMED_SYNC_INTERVAL_MS = 100UL;
static const unsigned long REMOTE_IDLE_SYNC_INTERVAL_MS = 400UL;
static const unsigned long REMOTE_INITIAL_RETRY_MS = 500UL;
static const unsigned long REMOTE_MAX_RETRY_MS = 10000UL;
static const unsigned long REMOTE_ONLINE_TIMEOUT_MS = 2000UL;
static const uint16_t REMOTE_HTTP_TIMEOUT_MS = 300;
static const size_t REMOTE_MAX_RESPONSE_BYTES = 2048;

unsigned long remoteLastAttemptMs = 0;
unsigned long remoteLastSuccessMs = 0;
unsigned long remoteNextAttemptMs = 0;
unsigned long remoteRetryMs = REMOTE_INITIAL_RETRY_MS;
int remoteLastHttpStatus = 0;
bool remoteServerArmed = false;
bool remoteGenerationSeen = false;
uint64_t remoteGeneration = 0;
String remoteSessionId = "";
uint64_t remoteSequence = 0;
bool remoteStopLatched = false;
uint64_t remoteStoppedGeneration = 0;
String remoteLastError = "Not initialized";

bool remoteConfigured() {
  return CAR_REMOTE_ENABLED != 0 && strlen(CAR_REMOTE_DEVICE_TOKEN) > 0;
}

bool remoteOnline() {
  return remoteLastSuccessMs != 0 &&
    (millis() - remoteLastSuccessMs) <= REMOTE_ONLINE_TIMEOUT_MS;
}

unsigned long remoteLastSyncAgeMs() {
  return remoteLastSuccessMs == 0 ? 0 : millis() - remoteLastSuccessMs;
}

String remoteServerLabel() {
  return String(CAR_REMOTE_SYNC_URL);
}

String remoteStatusLabel() {
  if (!CAR_REMOTE_ENABLED) return "Disabled";
  if (!remoteConfigured()) return "Token not configured";
  if (WiFi.status() != WL_CONNECTED) return "Waiting for station WiFi";
  return remoteOnline() ? String("Connected") : String("Disconnected");
}

String remoteLastSyncLabel() {
  return remoteLastSuccessMs == 0 ? String("Never") :
    String(remoteLastSyncAgeMs()) + " ms ago";
}

String remoteErrorLabel() {
  return remoteLastError.length() == 0 ? String("None") : remoteLastError;
}

void initRemoteConnection() {
  remoteRetryMs = REMOTE_INITIAL_RETRY_MS;
  remoteNextAttemptMs = 0;
  remoteLastError = remoteConfigured() ? "Waiting for station WiFi" :
    "CAR_REMOTE_DEVICE_TOKEN is not configured";
}

void latchRemoteStopFromLocal() {
  if (remoteServerArmed && remoteGenerationSeen) {
    remoteStopLatched = true;
    remoteStoppedGeneration = remoteGeneration;
  }
}

String remoteTelemetryJson() {
  String json;
  json.reserve(700);
  const char *gear = "P";
  if (dashboardGearIndex == GEAR_REVERSE_INDEX) gear = "R";
  else if (dashboardGearIndex == GEAR_NEUTRAL_INDEX) gear = "N";
  else if (dashboardGearIndex == GEAR_DRIVE_INDEX) gear = "D";
  String signal = turnSignalLabel();
  signal.toLowerCase();
  const char *receiver = vehicleControl.mode == VehicleControlMode::Receiver ?
    "receiver" : vehicleControlOwnerValue();
  const float mphToKmh = 1.609344f;
  const float gravityMps2 = 9.80665f;

  json = "{\"protocol_version\":1";
  json += ",\"speed\":" + String(dashboardMph * mphToKmh, 2);
  json += ",\"rpm\":" + String((int)round(dashboardRpmK * 1000.0f));
  json += ",\"gear\":\"" + String(gear) + "\"";
  json += ",\"fuel_percent\":" + String(clamp01(dashboardFuelLevel) * 100.0f, 1);
  json += ",\"headlights\":";
  json += dashboardHeadlightsOn ? "true" : "false";
  json += ",\"turn_signal\":\"" + jsonEscape(signal) + "\"";
  json += ",\"temperature\":" + String(environmentTempC, 2);
  json += ",\"humidity\":" + String(environmentHumidity, 2);
  json += ",\"pressure\":" + String(environmentPressureHpa, 2);
  json += ",\"altitude\":" + String(environmentAltitudeM, 2);
  json += ",\"pitch\":" + String(orientedPitchDeg, 2);
  json += ",\"roll\":" + String(orientedRollDeg, 2);
  json += ",\"acceleration\":{\"x\":" + String(ax * gravityMps2, 3);
  json += ",\"y\":" + String(ay * gravityMps2, 3);
  json += ",\"z\":" + String(az * gravityMps2, 3) + "}";
  json += ",\"gyro\":{\"x\":" + String(gx, 3);
  json += ",\"y\":" + String(gy, 3);
  json += ",\"z\":" + String(gz, 3) + "}";
  json += ",\"gps\":{\"fix\":";
  json += gpsLocationValid ? "true" : "false";
  json += ",\"latitude\":" + String(gpsLatitude, 6);
  json += ",\"longitude\":" + String(gpsLongitude, 6);
  json += ",\"satellites\":" + String(gpsSatellites);
  json += ",\"speed\":" + String(gpsRawMph * mphToKmh, 2) + "}";
  json += ",\"receiver\":\"" + String(receiver) + "\"}";
  return json;
}

bool applyRemoteResponse(const String &response, String &errorMessage) {
  JsonDocument document;
  DeserializationError parseError = deserializeJson(document, response);
  if (parseError) {
    errorMessage = String("Malformed server response: ") + parseError.c_str();
    return false;
  }
  if (!document.is<JsonObject>() || !document["armed"].is<bool>()) {
    errorMessage = "Server response is missing armed";
    return false;
  }

  bool armed = document["armed"].as<bool>();
  remoteServerArmed = armed;
  if (!armed) {
    remoteGenerationSeen = false;
    remoteSessionId = "";
    remoteSequence = 0;
    remoteStopLatched = false;
    if (vehicleControlOwner == VehicleControlOwner::Remote) {
      stopVehicleControl(false);
    }
    return true;
  }

  if (!document["session_id"].is<const char *>() ||
      !document["sequence"].is<uint64_t>() ||
      !document["generation"].is<uint64_t>() ||
      !document["steering"].is<int>() ||
      !document["throttle"].is<int>()) {
    errorMessage = "Armed response is missing a command field";
    return false;
  }

  String sessionId = document["session_id"].as<String>();
  uint64_t sequence = document["sequence"].as<uint64_t>();
  uint64_t generation = document["generation"].as<uint64_t>();
  int steering = document["steering"].as<int>();
  int throttle = document["throttle"].as<int>();
  if (sessionId.length() == 0) {
    errorMessage = "Armed response has an empty session_id";
    return false;
  }
  if (steering < -100 || steering > 100 || throttle < -100 || throttle > 100) {
    errorMessage = "Remote command is outside -100 to 100";
    return false;
  }
  if (vehicleControlOwner == VehicleControlOwner::Local) {
    errorMessage = "Local controller owns vehicle control";
    return true;
  }
  if (remoteGenerationSeen &&
      (generation != remoteGeneration || sessionId != remoteSessionId ||
       sequence < remoteSequence)) {
    errorMessage = "Remote session, generation, or sequence changed unexpectedly";
    return false;
  }
  remoteGeneration = generation;
  remoteGenerationSeen = true;
  remoteSessionId = sessionId;
  remoteSequence = sequence;
  if (remoteStopLatched) {
    if (generation == remoteStoppedGeneration) {
      errorMessage = "Remote session was stopped locally";
      return true;
    }
    remoteStopLatched = false;
  }
  if (!vehicleControl.armed && !armVehicleControlForOwner(VehicleControlOwner::Remote)) {
    errorMessage = "Remote control could not be armed";
    return false;
  }
  if (vehicleControlOwner != VehicleControlOwner::Remote) {
    errorMessage = "Remote controller does not own vehicle control";
    return false;
  }

  setVehicleControlCommand(steering, throttle);
  vehicleControl.lastHeartbeatMs = millis();
  vehicleControl.watchdogStopped = false;
  return true;
}

void scheduleRemoteRetry(unsigned long now, const String &errorMessage) {
  remoteLastError = errorMessage;
  remoteNextAttemptMs = now + remoteRetryMs;
  remoteRetryMs = min(remoteRetryMs * 2UL, REMOTE_MAX_RETRY_MS);
}

void failRemoteSync(unsigned long now, const String &errorMessage) {
  if (vehicleControlOwner == VehicleControlOwner::Remote) {
    stopVehicleControl(true);
  }
  scheduleRemoteRetry(now, errorMessage);
}

void updateRemoteConnection() {
  if (!remoteConfigured()) {
    return;
  }

  unsigned long now = millis();
  if ((long)(now - remoteNextAttemptMs) < 0) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    failRemoteSync(now, "Station WiFi is disconnected");
    return;
  }

  remoteLastAttemptMs = now;
  HTTPClient http;
  http.setConnectTimeout(REMOTE_HTTP_TIMEOUT_MS);
  http.setTimeout(REMOTE_HTTP_TIMEOUT_MS);
  if (!http.begin(CAR_REMOTE_SYNC_URL)) {
    failRemoteSync(now, "Unable to initialize remote HTTP request");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + CAR_REMOTE_DEVICE_TOKEN);

  String payload = remoteTelemetryJson();
  int status = http.POST(payload);
  remoteLastHttpStatus = status;
  if (status < 200 || status >= 300) {
    String message = status > 0 ? String("Remote server returned HTTP ") + status :
      String("Remote request failed: ") + http.errorToString(status);
    http.end();
    failRemoteSync(now, message);
    return;
  }

  int responseSize = http.getSize();
  if (responseSize > (int)REMOTE_MAX_RESPONSE_BYTES) {
    http.end();
    failRemoteSync(now, "Remote response is too large");
    return;
  }
  String response = http.getString();
  http.end();
  if (response.length() > REMOTE_MAX_RESPONSE_BYTES) {
    failRemoteSync(now, "Remote response is too large");
    return;
  }

  String responseError;
  if (!applyRemoteResponse(response, responseError)) {
    failRemoteSync(now, responseError);
    return;
  }

  remoteLastSuccessMs = millis();
  remoteLastError = responseError;
  remoteRetryMs = REMOTE_INITIAL_RETRY_MS;
  unsigned long interval = vehicleControlOwner == VehicleControlOwner::Remote ?
    REMOTE_ARMED_SYNC_INTERVAL_MS : REMOTE_IDLE_SYNC_INTERVAL_MS;
  remoteNextAttemptMs = remoteLastSuccessMs + interval;
}

String apiRemoteJson() {
  String json = "{\"enabled\":";
  json += CAR_REMOTE_ENABLED ? "true" : "false";
  json += ",\"configured\":";
  json += remoteConfigured() ? "true" : "false";
  json += ",\"connected\":";
  json += remoteOnline() ? "true" : "false";
  json += ",\"server\":\"" + jsonEscape(String(CAR_REMOTE_SYNC_URL)) + "\"";
  json += ",\"device_id\":\"" + jsonEscape(String(CAR_REMOTE_DEVICE_ID)) + "\"";
  json += ",\"last_sync_age_ms\":";
  json += remoteLastSuccessMs == 0 ? "null" : String(remoteLastSyncAgeMs());
  json += ",\"last_http_status\":" + String(remoteLastHttpStatus);
  json += ",\"remote_armed\":";
  json += remoteServerArmed ? "true" : "false";
  json += ",\"generation\":";
  json += remoteGenerationSeen ? String(remoteGeneration) : "null";
  json += ",\"error\":\"" + jsonEscape(remoteLastError) + "\"}";
  return json;
}
