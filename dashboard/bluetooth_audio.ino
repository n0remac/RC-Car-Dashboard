#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// ----------------------
// Bluetooth audio config
// ----------------------
const char *BT_SPEAKER_NAME = "ESP32-TDisplay-Speaker";
static const int BT_SPEAKER_VOLUME = 80;
static const int BT_I2S_LRC_PIN = 13;
static const int BT_I2S_BCLK_PIN = 17;
static const int BT_I2S_DOUT_PIN = 15;
static const int BT_OWNED_GPS_RX_PIN = 27;
static const uint32_t BT_MIN_FREE_HEAP_BEFORE_START = 80000;
static const uint32_t BT_MIN_MAX_ALLOC_HEAP_BEFORE_START = 45000;

I2SStream speakerI2s;
BluetoothA2DPSink bluetoothSpeaker(speakerI2s);

bool speakerI2sReady = false;
String speakerI2sStatus = "Not started";
bool btSpeakerStarted = false;
String btSpeakerStatus = "Not started";
int btConnectionState = 0;
int btAudioState = 0;
bool btAvrcConnected = false;
uint16_t btSampleRate = 0;

bool initSpeakerI2sForFormat(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);

bool bluetoothPinMatches(int pin, int ownedPin) {
  return pin >= 0 && pin == ownedPin;
}

bool bluetoothI2sPinConflicts() {
  const int pins[] = {
    BT_I2S_LRC_PIN,
    BT_I2S_BCLK_PIN,
    BT_I2S_DOUT_PIN
  };

  for (int i = 0; i < 3; i++) {
    int pin = pins[i];
    if (bluetoothPinMatches(pin, SDA_PIN) ||
        bluetoothPinMatches(pin, SCL_PIN) ||
        bluetoothPinMatches(pin, LEFT_TURN_LED_PIN) ||
        bluetoothPinMatches(pin, RIGHT_TURN_LED_PIN) ||
        bluetoothPinMatches(pin, SCREEN_BACKLIGHT_PIN) ||
        bluetoothPinMatches(pin, STEERING_INPUT_PIN) ||
        bluetoothPinMatches(pin, BT_OWNED_GPS_RX_PIN)) {
      return true;
    }
  }

  return false;
}

String bluetoothSpeakerName() {
  return String(BT_SPEAKER_NAME);
}

String bluetoothSpeakerPinsLabel() {
  return String("LRC ") + BT_I2S_LRC_PIN +
    ", BCLK " + BT_I2S_BCLK_PIN +
    ", DIN " + BT_I2S_DOUT_PIN;
}

bool speakerI2sStarted() {
  return speakerI2sReady;
}

String speakerI2sStatusLabel() {
  return speakerI2sStatus;
}

bool bluetoothSpeakerStarted() {
  return btSpeakerStarted;
}

String bluetoothSpeakerStatusLabel() {
  return btSpeakerStatus;
}

const char *bluetoothConnectionStateText(int state) {
  switch (state) {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      return "Disconnected";
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      return "Connecting";
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      return "Connected";
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      return "Disconnecting";
    default:
      return "Unknown";
  }
}

String bluetoothConnectionStateLabel() {
  return String(bluetoothConnectionStateText(btConnectionState));
}

const char *bluetoothAudioStateText(int state) {
  switch (state) {
    case ESP_A2D_AUDIO_STATE_STARTED:
      return "Streaming";
    case ESP_A2D_AUDIO_STATE_SUSPEND:
      return "Idle";
    default:
      return "Unknown";
  }
}

String bluetoothAudioStateLabel() {
  return String(bluetoothAudioStateText(btAudioState));
}

bool bluetoothAvrcConnected() {
  return btAvrcConnected;
}

uint16_t bluetoothSampleRate() {
  return btSampleRate;
}

void logBluetoothHeap(const char *label) {
  Serial.print(label);
  Serial.print(" free=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" min=");
  Serial.print(ESP.getMinFreeHeap());
  Serial.print(" max_alloc=");
  Serial.println(ESP.getMaxAllocHeap());
}

void onBluetoothAvrcConnectionChanged(bool connected) {
  btAvrcConnected = connected;
  Serial.print("Bluetooth AVRCP: ");
  Serial.println(connected ? "Connected" : "Disconnected");
}

void onBluetoothSampleRateChanged(uint16_t rate) {
  btSampleRate = rate;
  Serial.print("Bluetooth sample rate: ");
  Serial.println(rate);
}

bool initSpeakerI2s() {
  if (speakerI2sReady) {
    return true;
  }

  return initSpeakerI2sForFormat(44100, 2, 16);
}

bool initSpeakerI2sForFormat(uint32_t sampleRate, uint8_t channels, uint8_t bitsPerSample) {
  if (bluetoothI2sPinConflicts()) {
    speakerI2sReady = false;
    speakerI2sStatus = "Pin conflict";
    Serial.print("Speaker I2S not started: pin conflict (");
    Serial.print(bluetoothSpeakerPinsLabel());
    Serial.println(")");
    return false;
  }

  if (bitsPerSample != 16) {
    speakerI2sReady = false;
    speakerI2sStatus = "Unsupported WAV";
    Serial.println("Speaker I2S not started: only 16-bit WAV is supported");
    return false;
  }

  if (channels != 1 && channels != 2) {
    speakerI2sReady = false;
    speakerI2sStatus = "Unsupported WAV";
    Serial.println("Speaker I2S not started: WAV must be mono or stereo");
    return false;
  }

  auto cfg = speakerI2s.defaultConfig(TX_MODE);
  cfg.pin_bck = BT_I2S_BCLK_PIN;
  cfg.pin_ws = BT_I2S_LRC_PIN;
  cfg.pin_data = BT_I2S_DOUT_PIN;

  cfg.sample_rate = sampleRate;
  cfg.bits_per_sample = bitsPerSample;
  cfg.channels = channels;

  if (!speakerI2s.begin(cfg)) {
    speakerI2sReady = false;
    speakerI2sStatus = "I2S failed";
    Serial.println("Speaker I2S not started: begin failed");
    return false;
  }

  speakerI2sReady = true;
  speakerI2sStatus = "Ready";

  Serial.print("Speaker I2S pins: ");
  Serial.println(bluetoothSpeakerPinsLabel());

  Serial.print("Speaker I2S format: ");
  Serial.print(sampleRate);
  Serial.print(" Hz, ");
  Serial.print(bitsPerSample);
  Serial.print("-bit, ");
  Serial.print(channels);
  Serial.println(channels == 1 ? " channel" : " channels");

  return true;
}

bool prepareSpeakerI2sForLocalAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
) {
  if (bluetoothI2sPinConflicts()) {
    speakerI2sReady = false;
    speakerI2sStatus = "Pin conflict";
    return false;
  }

  speakerI2s.end();
  speakerI2sReady = false;

  return initSpeakerI2sForFormat(sampleRate, channels, bitsPerSample);
}

void initBluetoothSpeaker() {
  Serial.println("Starting ESP32 Bluetooth Speaker...");
  logBluetoothHeap("BT heap before start");

  if (!initSpeakerI2s()) {
    btSpeakerStarted = false;
    btSpeakerStatus = speakerI2sStatus;
    Serial.print("Bluetooth speaker not started: ");
    Serial.println(btSpeakerStatus);
    return;
  }

  if (ESP.getFreeHeap() < BT_MIN_FREE_HEAP_BEFORE_START ||
      ESP.getMaxAllocHeap() < BT_MIN_MAX_ALLOC_HEAP_BEFORE_START) {
    btSpeakerStarted = false;
    btSpeakerStatus = "Low heap";
    Serial.println("Bluetooth speaker not started: low heap");
    logBluetoothHeap("BT low heap");
    return;
  }

  bluetoothSpeaker.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *) {
    btConnectionState = (int)state;
    Serial.print("Bluetooth A2DP connection: ");
    Serial.println(bluetoothConnectionStateText(btConnectionState));
  });
  bluetoothSpeaker.set_on_audio_state_changed([](esp_a2d_audio_state_t state, void *) {
    btAudioState = (int)state;
    Serial.print("Bluetooth A2DP audio: ");
    Serial.println(bluetoothAudioStateText(btAudioState));
  });
  bluetoothSpeaker.set_avrc_connection_state_callback(onBluetoothAvrcConnectionChanged);
  bluetoothSpeaker.set_sample_rate_callback(onBluetoothSampleRateChanged);
  bluetoothSpeaker.set_volume(BT_SPEAKER_VOLUME);
  bluetoothSpeaker.start(BT_SPEAKER_NAME);

  btSpeakerStarted = true;
  btSpeakerStatus = "Ready";

  Serial.print("Pair your phone with: ");
  Serial.println(BT_SPEAKER_NAME);
  Serial.print("Bluetooth speaker I2S pins: ");
  Serial.println(bluetoothSpeakerPinsLabel());
  logBluetoothHeap("BT heap after start");
}
