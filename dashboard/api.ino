#include <esp_system.h>

static const unsigned long API_EVENT_INTERVAL_MS = 250UL;
static const unsigned long API_EVENT_KEEPALIVE_MS = 15000UL;

unsigned long lastApiEventMs = 0;
unsigned long lastApiKeepaliveMs = 0;

String randomHex(size_t byteCount) {
  static const char hex[] = "0123456789abcdef";
  String value;
  value.reserve(byteCount * 2);
  uint32_t randomValue = 0;
  for (size_t i = 0; i < byteCount; ++i) {
    if ((i % 4) == 0) {
      randomValue = esp_random();
    }
    uint8_t byteValue = (randomValue >> ((i % 4) * 8)) & 0xff;
    value += hex[byteValue >> 4];
    value += hex[byteValue & 0x0f];
  }
  return value;
}

bool constantTimeEquals(const String &left, const String &right) {
  size_t leftLength = left.length();
  size_t rightLength = right.length();
  size_t length = leftLength > rightLength ? leftLength : rightLength;
  uint8_t difference = (uint8_t)(leftLength ^ rightLength);
  for (size_t i = 0; i < length; ++i) {
    char leftChar = i < leftLength ? left.charAt(i) : 0;
    char rightChar = i < rightLength ? right.charAt(i) : 0;
    difference |= (uint8_t)(leftChar ^ rightChar);
  }
  return difference == 0;
}

bool requestHasArg(AsyncWebServerRequest *request, const char *name) {
  return request->hasParam(name, true) || request->hasParam(name);
}

String requestArg(AsyncWebServerRequest *request, const char *name) {
  if (request->hasParam(name, true)) {
    return request->getParam(name, true)->value();
  }
  if (request->hasParam(name)) {
    return request->getParam(name)->value();
  }
  return "";
}

void sendApiError(
  AsyncWebServerRequest *request,
  int code,
  const char *error,
  const String &message
) {
  String json = "{\"ok\":false,\"error\":\"";
  json += jsonEscape(error);
  json += "\",\"message\":\"";
  json += jsonEscape(message);
  json += "\"}";
  request->send(code, "application/json", json);
}

String apiControlJson() {
  String json = "{";
  json += "\"armed\":";
  json += vehicleControl.armed ? "true" : "false";
  json += ",\"mode\":\"";
  json += vehicleControlModeValue();
  json += "\",\"status\":\"";
  json += jsonEscape(vehicleControlStatusLabel());
  json += "\",\"steering\":" + String(vehicleControl.steeringPercent);
  json += ",\"throttle\":" + String(vehicleControl.throttlePercent);
  json += ",\"steering_pulse_us\":" + String(vehicleControl.steeringPulseUs);
  json += ",\"throttle_pulse_us\":" + String(vehicleControl.throttlePulseUs);
  json += ",\"heartbeat_age_ms\":" + String(vehicleControlHeartbeatAgeMs());
  json += ",\"watchdog_timeout_ms\":" + String(VEHICLE_CONTROL_WATCHDOG_MS);
  json += ",\"watchdog_stopped\":";
  json += vehicleControl.watchdogStopped ? "true" : "false";
  json += ",\"calibration_valid\":";
  json += steeringCalibrationValid() ? "true" : "false";
  json += ",\"session_id\":\"";
  json += vehicleControl.armed ? jsonEscape(apiControlSessionId) : String("");
  json += "\",\"last_sequence\":" + String(apiControlLastSequence);
  json += "}";
  return json;
}

String apiSensorsJson() {
  String json = "{";
  json += "\"environment\":{\"available\":";
  json += bmeAvailable ? "true" : "false";
  json += ",\"temperature_c\":" + String(environmentTempC, 2);
  json += ",\"humidity_percent\":" + String(environmentHumidity, 2);
  json += ",\"pressure_hpa\":" + String(environmentPressureHpa, 2);
  json += ",\"altitude_m\":" + String(environmentAltitudeM, 2) + "}";
  json += ",\"imu\":{\"available\":";
  json += imuAvailable ? "true" : "false";
  json += ",\"accel_g\":{\"x\":" + String(ax, 4) + ",\"y\":" + String(ay, 4) + ",\"z\":" + String(az, 4) + "}";
  json += ",\"gyro_dps\":{\"x\":" + String(gx, 3) + ",\"y\":" + String(gy, 3) + ",\"z\":" + String(gz, 3) + "}";
  json += ",\"pitch_deg\":" + String(orientedPitchDeg, 2) + ",\"roll_deg\":" + String(orientedRollDeg, 2);
  json += ",\"longitudinal_accel_g\":" + String(longitudinalAxisAccelG, 4) + "}";
  json += ",\"gps\":{\"data_seen\":";
  json += gpsDataSeen ? "true" : "false";
  json += ",\"location_valid\":";
  json += gpsLocationValid ? "true" : "false";
  json += ",\"speed_valid\":";
  json += gpsSpeedValid ? "true" : "false";
  json += ",\"satellites\":" + String(gpsSatellites);
  json += ",\"latitude\":" + String(gpsLatitude, 6) + ",\"longitude\":" + String(gpsLongitude, 6);
  json += ",\"raw_speed_mph\":" + String(gpsRawMph, 2) + ",\"fix_age_ms\":";
  json += lastGpsSpeedSampleMs == 0 ? "null" : String(gpsFixAgeMs);
  json += "}";
  json += ",\"speed\":{\"mph\":" + String(dashboardMph, 2);
  json += ",\"source\":\"" + jsonEscape(speedSourceLabel()) + "\"";
  json += ",\"lead_mode\":\"" + jsonEscape(speedFusionLeadModeLabel()) + "\"}";
  json += "}";
  return json;
}

String apiInputsJson() {
  String json = "{";
  json += "\"steering\":{\"valid\":";
  json += steeringInputValid ? "true" : "false";
  json += ",\"angle_deg\":" + String(steeringWheelAngleDeg);
  json += ",\"raw_pulse_us\":" + String(steeringReceiverInput.pulseWidthUs);
  json += ",\"filtered_pulse_us\":" + String(vehicleControl.steeringPulseUs);
  json += ",\"fresh\":";
  json += steeringReceiverInput.filteredPulseFresh ? "true" : "false";
  json += "},\"throttle\":{\"raw_pulse_us\":" + String(throttleReceiverInput.pulseWidthUs);
  json += ",\"filtered_pulse_us\":" + String(vehicleControl.throttlePulseUs);
  json += ",\"fresh\":";
  json += throttleReceiverInput.filteredPulseFresh ? "true" : "false";
  json += "},\"headlight\":{\"on\":";
  json += dashboardHeadlightsOn ? "true" : "false";
  json += ",\"pulse_us\":" + String(headlightInput.pulseWidthUs) + ",\"fresh\":";
  json += headlightInput.pulseFresh ? "true" : "false";
  json += "},\"sound_switch\":{\"on\":";
  json += soundSwitchOn ? "true" : "false";
  json += ",\"pulse_us\":" + String(soundSwitchInput.pulseWidthUs) + ",\"fresh\":";
  json += soundSwitchInput.pulseFresh ? "true" : "false";
  json += "}}";
  return json;
}

String apiDashboardJson() {
  String json = "{\"rpm_k\":" + String(dashboardRpmK, 2);
  json += ",\"speed_mph\":" + String(dashboardMph, 2);
  json += ",\"fuel_level\":" + String(dashboardFuelLevel, 3);
  json += ",\"gear_index\":" + String(dashboardGearIndex);
  json += ",\"odometer\":\"" + jsonEscape(dashboardOdometer) + "\"";
  json += ",\"headlights_on\":";
  json += dashboardHeadlightsOn ? "true" : "false";
  json += ",\"turn_signal\":\"" + jsonEscape(turnSignalLabel()) + "\"}";
  return json;
}

String apiAudioJson() {
  String json = "{\"horn_mode\":\"";
  json += hornWebModeValue();
  json += "\",\"horn_requested\":";
  json += hornPlaybackRequested() ? "true" : "false";
  json += ",\"horn_active\":";
  json += hornSynthIsActive() ? "true" : "false";
  json += ",\"output_ready\":";
  json += speakerI2sStarted() ? "true" : "false";
  json += ",\"storage_ready\":";
  json += browserAudioStorageReady() ? "true" : "false";
  json += ",\"file_saved\":";
  json += browserAudioFileSaved() ? "true" : "false";
  json += ",\"playing\":";
  json += browserAudioIsPlaying() ? "true" : "false";
  json += ",\"status\":\"" + jsonEscape(browserAudioStatusLabel()) + "\"}";
  return json;
}

String apiNetworkJson() {
  String json = "{\"ap_ssid\":\"" + jsonEscape(String(AP_SSID)) + "\"";
  json += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"station_status\":\"" + jsonEscape(wifiStationStatusLabel()) + "\"";
  json += ",\"station_ip\":\"" + jsonEscape(wifiStationIpLabel()) + "\"";
  json += ",\"station_ssid\":\"" + jsonEscape(wifiStaSsid) + "\"";
  json += ",\"station_saved\":";
  json += wifiStaCredentialsSaved ? "true" : "false";
  json += "}";
  return json;
}

String apiSettingsJson() {
  String json = "{\"display\":{\"brightness_percent\":" + String(screenBrightnessPercent) + "}";
  json += ",\"steering\":{\"center_us\":" + String(steeringCenterUs);
  json += ",\"left_us\":" + String(steeringLeftUs) + ",\"right_us\":" + String(steeringRightUs);
  json += ",\"turn_threshold_deg\":" + String(turnSignalThresholdDeg) + "}";
  json += ",\"tilt\":{\"orientation_deg\":" + String(tiltOrientationDeg);
  json += ",\"invert_pitch\":";
  json += invertPitchAxis ? "true" : "false";
  json += ",\"invert_roll\":";
  json += invertRollAxis ? "true" : "false";
  json += ",\"show_axis_labels\":";
  json += showTiltAxisLabels ? "true" : "false";
  json += ",\"bubble_tolerance_deg\":" + String(tiltBubbleToleranceDeg, 2) + "}";
  json += ",\"speed\":{\"lead_mode\":\"";
  json += speedFusionLeadMode == SPEED_LEAD_ACCEL ? "accel" : "gps";
  json += "\"}}";
  return json;
}

String apiStateJson() {
  String json = "{\"device\":{\"name\":\"CarDashboard\",\"api_version\":\"v1\",\"uptime_ms\":";
  json += String(millis()) + "},\"control\":" + apiControlJson();
  json += ",\"sensors\":" + apiSensorsJson();
  json += ",\"inputs\":" + apiInputsJson();
  json += ",\"dashboard\":" + apiDashboardJson();
  json += ",\"audio\":" + apiAudioJson();
  json += ",\"network\":" + apiNetworkJson();
  json += ",\"settings\":" + apiSettingsJson();
  // Temporary compatibility view used by the embedded dashboard while its
  // individual readouts are migrated to the nested public schema.
  json += ",\"legacy\":" + stateJson() + "}";
  return json;
}

void sendApiJson(AsyncWebServerRequest *request, const String &json, int code = 200) {
  request->send(code, "application/json", json);
}

void registerApiJsonPost(const char *path, ApiJsonHandler handler) {
  server.on(
    path,
    HTTP_POST,
    [handler](AsyncWebServerRequest *request) {
      String *body = static_cast<String *>(request->_tempObject);
      request->_tempObject = nullptr;
      if (body == nullptr) {
        sendApiError(request, 400, "invalid_json", "A JSON request body is required.");
        return;
      }
      if (body->length() > 0 && body->charAt(0) == '\1') {
        delete body;
        sendApiError(request, 413, "payload_too_large", "The JSON request body exceeds 4096 bytes.");
        return;
      }
      JsonDocument document;
      DeserializationError error = deserializeJson(document, *body);
      delete body;
      if (error) {
        sendApiError(request, 400, "invalid_json", String("Malformed JSON: ") + error.c_str());
        return;
      }
      JsonVariant variant = document.as<JsonVariant>();
      handler(request, variant);
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t length, size_t index, size_t total) {
      (void)index;
      if (request->_tempObject == nullptr) {
        request->_tempObject = new String();
        static_cast<String *>(request->_tempObject)->reserve(total > 4096 ? 1 : total);
      }
      String *body = static_cast<String *>(request->_tempObject);
      if (body->length() > 0 && body->charAt(0) == '\1') return;
      if (body->length() + length > 4096) {
        *body = String('\1');
        return;
      }
      body->concat(reinterpret_cast<const char *>(data), length);
    }
  );
}

void handleApiControlArm(AsyncWebServerRequest *request, JsonVariant &json) {
  (void)json;
  if (!armVehicleControl()) {
    sendApiError(request, 409, "control_unavailable", "Web control could not be armed.");
    return;
  }
  apiControlSessionId = randomHex(8);
  apiControlLastSequence = 0;
  apiControlSequenceSeen = false;
  sendApiJson(request, apiControlJson());
}

void handleApiControlCommand(AsyncWebServerRequest *request, JsonVariant &body) {
  if (!body.is<JsonObject>()) {
    sendApiError(request, 400, "invalid_json", "A JSON object is required.");
    return;
  }
  JsonObject object = body.as<JsonObject>();
  if (!object["session_id"].is<const char *>() || !object["sequence"].is<uint32_t>() ||
      !object["steering"].is<int>() || !object["throttle"].is<int>()) {
    sendApiError(request, 400, "missing_field", "session_id, sequence, steering, and throttle are required.");
    return;
  }
  if (!vehicleControl.armed) {
    sendApiError(request, 409, "control_not_armed", "Vehicle control is not armed.");
    return;
  }
  String sessionId = object["session_id"].as<String>();
  uint32_t sequence = object["sequence"].as<uint32_t>();
  int steering = object["steering"].as<int>();
  int throttle = object["throttle"].as<int>();
  if (!constantTimeEquals(sessionId, apiControlSessionId)) {
    sendApiError(request, 409, "session_mismatch", "The control session is no longer active.");
    return;
  }
  if (apiControlSequenceSeen && sequence <= apiControlLastSequence) {
    sendApiError(request, 409, "stale_sequence", "The command sequence must strictly increase.");
    return;
  }
  if (steering < -100 || steering > 100 || throttle < -100 || throttle > 100) {
    sendApiError(request, 422, "value_out_of_range", "steering and throttle must be from -100 to 100.");
    return;
  }
  apiControlLastSequence = sequence;
  apiControlSequenceSeen = true;
  setVehicleControlCommand(steering, throttle);
  vehicleControl.lastHeartbeatMs = millis();
  vehicleControl.watchdogStopped = false;
  sendApiJson(request, apiControlJson());
}

bool jsonObjectHasOnly(JsonObject object, const char *allowed[], size_t allowedCount) {
  for (JsonPair pair : object) {
    bool found = false;
    for (size_t i = 0; i < allowedCount; ++i) {
      if (strcmp(pair.key().c_str(), allowed[i]) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

void handleApiSettings(AsyncWebServerRequest *request, JsonVariant &body) {
  if (!body.is<JsonObject>()) {
    sendApiError(request, 400, "invalid_json", "A JSON object is required.");
    return;
  }
  JsonObject root = body.as<JsonObject>();
  const char *rootKeys[] = {"display", "steering", "tilt", "speed", "actions"};
  if (!jsonObjectHasOnly(root, rootKeys, 5)) {
    sendApiError(request, 422, "unknown_field", "The settings object contains an unknown field.");
    return;
  }

  int brightness = screenBrightnessPercent;
  unsigned long center = steeringCenterUs, left = steeringLeftUs, right = steeringRightUs;
  int threshold = turnSignalThresholdDeg, orientation = tiltOrientationDeg;
  bool pitchInvert = invertPitchAxis, rollInvert = invertRollAxis, labels = showTiltAxisLabels;
  float tolerance = tiltBubbleToleranceDeg;
  int speedMode = (int)speedFusionLeadMode;
  bool resetTilt = false, resetSpeed = false;

  if (root.containsKey("display")) {
    if (!root["display"].is<JsonObject>()) { sendApiError(request, 422, "invalid_settings", "display must be an object."); return; }
    JsonObject value = root["display"];
    const char *keys[] = {"brightness_percent"};
    if (!jsonObjectHasOnly(value, keys, 1) || !value["brightness_percent"].is<int>()) { sendApiError(request, 422, "invalid_settings", "brightness_percent must be an integer."); return; }
    brightness = value["brightness_percent"];
    if (brightness < SCREEN_BRIGHTNESS_MIN_PERCENT || brightness > SCREEN_BRIGHTNESS_MAX_PERCENT) { sendApiError(request, 422, "value_out_of_range", "brightness_percent is out of range."); return; }
  }
  if (root.containsKey("steering")) {
    if (!root["steering"].is<JsonObject>()) { sendApiError(request, 422, "invalid_settings", "steering must be an object."); return; }
    JsonObject value = root["steering"];
    const char *keys[] = {"center_us", "left_us", "right_us", "turn_threshold_deg"};
    if (!jsonObjectHasOnly(value, keys, 4)) { sendApiError(request, 422, "unknown_field", "steering contains an unknown field."); return; }
    if (value.containsKey("center_us")) center = value["center_us"].as<unsigned long>();
    if (value.containsKey("left_us")) left = value["left_us"].as<unsigned long>();
    if (value.containsKey("right_us")) right = value["right_us"].as<unsigned long>();
    if (value.containsKey("turn_threshold_deg")) threshold = value["turn_threshold_deg"].as<int>();
    if (right < STEERING_PULSE_MIN_VALID_US || left > STEERING_PULSE_MAX_VALID_US || !(right < center && center < left) || threshold < TURN_THRESHOLD_MIN_DEG || threshold > TURN_THRESHOLD_MAX_DEG) {
      sendApiError(request, 422, "invalid_calibration", "Steering values must be in range and ordered right < center < left."); return;
    }
  }
  if (root.containsKey("tilt")) {
    if (!root["tilt"].is<JsonObject>()) { sendApiError(request, 422, "invalid_settings", "tilt must be an object."); return; }
    JsonObject value = root["tilt"];
    const char *keys[] = {"orientation_deg", "invert_pitch", "invert_roll", "show_axis_labels", "bubble_tolerance_deg"};
    if (!jsonObjectHasOnly(value, keys, 5)) { sendApiError(request, 422, "unknown_field", "tilt contains an unknown field."); return; }
    if (value.containsKey("orientation_deg")) orientation = value["orientation_deg"].as<int>();
    if (value.containsKey("invert_pitch")) pitchInvert = value["invert_pitch"].as<bool>();
    if (value.containsKey("invert_roll")) rollInvert = value["invert_roll"].as<bool>();
    if (value.containsKey("show_axis_labels")) labels = value["show_axis_labels"].as<bool>();
    if (value.containsKey("bubble_tolerance_deg")) tolerance = value["bubble_tolerance_deg"].as<float>();
    if (!((orientation == 0) || (orientation == 90) || (orientation == 180) || (orientation == 270)) || tolerance < 0 || tolerance > 10) {
      sendApiError(request, 422, "value_out_of_range", "Tilt orientation or tolerance is invalid."); return;
    }
  }
  if (root.containsKey("speed")) {
    if (!root["speed"].is<JsonObject>()) { sendApiError(request, 422, "invalid_settings", "speed must be an object."); return; }
    String leadMode = root["speed"]["lead_mode"] | "";
    if (leadMode == "gps") speedMode = SPEED_LEAD_GPS;
    else if (leadMode == "accel") speedMode = SPEED_LEAD_ACCEL;
    else { sendApiError(request, 422, "invalid_settings", "lead_mode must be gps or accel."); return; }
  }
  if (root.containsKey("actions")) {
    resetTilt = root["actions"]["reset_tilt_reference"] | false;
    resetSpeed = root["actions"]["reset_speed_fusion"] | false;
  }

  setScreenBrightnessPercent(brightness);
  bool steeringChanged = center != steeringCenterUs || left != steeringLeftUs || right != steeringRightUs || threshold != turnSignalThresholdDeg;
  steeringCenterUs = center; steeringLeftUs = left; steeringRightUs = right; turnSignalThresholdDeg = threshold;
  if (steeringChanged) saveSteeringCalibration();
  tiltOrientationDeg = orientation; invertPitchAxis = pitchInvert; invertRollAxis = rollInvert;
  showTiltAxisLabels = labels; tiltBubbleToleranceDeg = tolerance; applyTiltOrientation();
  setSpeedFusionLeadMode(speedMode);
  if (resetTilt) resetTiltReference();
  if (resetSpeed) resetSpeedFusion();
  saveDashboardSettings();
  updateTurnSignalOutputs();
  renderCurrentScreen();
  sendApiJson(request, apiSettingsJson());
}

void handleApiNetworkConnect(AsyncWebServerRequest *request, JsonVariant &body) {
  String ssid = body["ssid"] | "";
  String password = body["password"] | "";
  ssid.trim();
  if (ssid.length() == 0) { sendApiError(request, 422, "ssid_required", "ssid is required."); return; }
  saveWifiStationCredentials(ssid, password);
  WiFi.disconnect(false, false); WiFi.mode(WIFI_AP_STA); beginWifiStation();
  sendApiJson(request, String("{\"status\":\"connecting\",\"ssid\":\"") + jsonEscape(ssid) + "\"}");
}

void handleApiHorn(AsyncWebServerRequest *request, JsonVariant &body) {
  String mode = body["mode"] | "";
  if (!setHornWebMode(mode)) { sendApiError(request, 422, "invalid_horn_mode", "mode must be off, rc, or on."); return; }
  sendApiJson(request, apiAudioJson());
}

void updateApiEvents() {
  if (apiEvents.count() == 0) return;
  unsigned long now = millis();
  if ((now - lastApiEventMs) >= API_EVENT_INTERVAL_MS) {
    lastApiEventMs = now;
    String state = apiStateJson();
    apiEvents.send(state.c_str(), "state", now);
  }
  if ((now - lastApiKeepaliveMs) >= API_EVENT_KEEPALIVE_MS) {
    lastApiKeepaliveMs = now;
    apiEvents.send("{}", "keepalive", now);
  }
}

void registerApiRoutes() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/controller", HTTP_GET, handleControllerPage);
  server.on("/state", HTTP_GET, handleState);
  server.on("/set", HTTP_ANY, handleSet);
  server.on("/controller/state", HTTP_GET, handleControllerState);
  server.on("/controller/arm", HTTP_POST, handleControllerArm);
  server.on("/controller/command", HTTP_POST, handleControllerCommand);
  server.on("/controller/stop", HTTP_POST, handleControllerStop);
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/wifi/disconnect", HTTP_POST, handleWifiDisconnect);
  server.on("/audio/horn", HTTP_POST, handleHornMode);
  server.on("/audio/horn/short", HTTP_POST, handleShortHorn);
  server.on("/audio/upload", HTTP_POST, handleBrowserAudioUploadComplete, handleBrowserAudioUpload);

  server.on("/api/v1/state", HTTP_GET, [](AsyncWebServerRequest *request) { sendApiJson(request, apiStateJson()); });
  server.on("/api/v1/sensors", HTTP_GET, [](AsyncWebServerRequest *request) { sendApiJson(request, apiSensorsJson()); });
  server.on("/api/v1/inputs", HTTP_GET, [](AsyncWebServerRequest *request) { sendApiJson(request, apiInputsJson()); });
  server.on("/api/v1/control/state", HTTP_GET, [](AsyncWebServerRequest *request) { sendApiJson(request, apiControlJson()); });
  server.on("/api/v1/control/stop", HTTP_POST, [](AsyncWebServerRequest *request) { stopVehicleControl(false); sendApiJson(request, apiControlJson()); });
  server.on("/api/v1/network/scan", HTTP_GET, handleWifiScan);
  server.on("/api/v1/network/disconnect", HTTP_POST, [](AsyncWebServerRequest *request) {
    WiFi.disconnect(false, false); WiFi.mode(WIFI_AP_STA); clearWifiStationCredentials();
    sendApiJson(request, "{\"status\":\"disconnected\"}");
  });
  server.on("/api/v1/audio/horn/short", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (hornWebMode == HornWebMode::Off) { sendApiError(request, 409, "horn_disabled", "Set horn mode to rc or on first."); return; }
    triggerShortHornSynth(WEB_SHORT_HORN_DURATION_MS); sendApiJson(request, apiAudioJson());
  });
  server.on("/api/v1/audio/upload", HTTP_POST, handleBrowserAudioUploadComplete, handleBrowserAudioUpload);

  registerApiJsonPost("/api/v1/control/arm", handleApiControlArm);
  registerApiJsonPost("/api/v1/control/command", handleApiControlCommand);
  registerApiJsonPost("/api/v1/settings", handleApiSettings);
  registerApiJsonPost("/api/v1/network/connect", handleApiNetworkConnect);
  registerApiJsonPost("/api/v1/audio/horn", handleApiHorn);

  apiEvents.onConnect([](AsyncEventSourceClient *client) {
    // The callback runs before the new client is inserted into the source's
    // client list, so any existing connection means this one is excess.
    if (apiEvents.count() >= 1) {
      client->close();
      return;
    }
    lastApiEventMs = 0;
    lastApiKeepaliveMs = millis();
  });
  server.addHandler(&apiEvents);

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) { request->send(204); return; }
    sendApiError(request, 404, "not_found", "Route not found.");
  });
}
