#include "AudioTools.h"

// ----------------------
// Browser audio output
// ----------------------
static const int AUDIO_I2S_LRC_PIN = 13;
static const int AUDIO_I2S_BCLK_PIN = 17;
static const int AUDIO_I2S_DOUT_PIN = 15;
static const int AUDIO_OWNED_GPS_RX_PIN = 27;

I2SStream speakerI2s;
VolumeStream browserAudioVolume(speakerI2s);

bool speakerI2sReady = false;
String speakerI2sStatus = "Not started";

bool audioPinMatches(int pin, int ownedPin) {
  return pin >= 0 && pin == ownedPin;
}

bool audioI2sPinConflicts() {
  const int pins[] = {
    AUDIO_I2S_LRC_PIN,
    AUDIO_I2S_BCLK_PIN,
    AUDIO_I2S_DOUT_PIN
  };

  for (int i = 0; i < 3; i++) {
    int pin = pins[i];
    if (audioPinMatches(pin, SDA_PIN) ||
        audioPinMatches(pin, SCL_PIN) ||
        audioPinMatches(pin, LEFT_TURN_LED_PIN) ||
        audioPinMatches(pin, RIGHT_TURN_LED_PIN) ||
        audioPinMatches(pin, SCREEN_BACKLIGHT_PIN) ||
        audioPinMatches(pin, HEADLIGHT_INPUT_PIN) ||
        audioPinMatches(pin, STEERING_CONTROL_PIN) ||
        audioPinMatches(pin, AUDIO_OWNED_GPS_RX_PIN)) {
      return true;
    }
  }

  return false;
}

String speakerI2sPinsLabel() {
  return String("LRC ") + AUDIO_I2S_LRC_PIN +
    ", BCLK " + AUDIO_I2S_BCLK_PIN +
    ", DIN " + AUDIO_I2S_DOUT_PIN;
}

bool speakerI2sStarted() {
  return speakerI2sReady;
}

String speakerI2sStatusLabel() {
  return speakerI2sStatus;
}

bool initSpeakerI2sForFormat(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample,
  bool lowLatency
) {
  if (audioI2sPinConflicts()) {
    speakerI2sReady = false;
    speakerI2sStatus = "Pin conflict";
    Serial.print("Speaker I2S not started: pin conflict (");
    Serial.print(speakerI2sPinsLabel());
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
  cfg.pin_bck = AUDIO_I2S_BCLK_PIN;
  cfg.pin_ws = AUDIO_I2S_LRC_PIN;
  cfg.pin_data = AUDIO_I2S_DOUT_PIN;

  cfg.sample_rate = sampleRate;
  cfg.bits_per_sample = bitsPerSample;
  cfg.channels = channels;
  if (lowLatency) {
    // Two 64-frame DMA buffers are about 5.8 ms at the horn's 22.05 kHz
    // mono format. The continuous horn task can keep them fed without
    // inserting a perceptible queue of silence before a trigger.
    cfg.buffer_count = 2;
    cfg.buffer_size = 64;
  }

  if (!speakerI2s.begin(cfg)) {
    speakerI2sReady = false;
    speakerI2sStatus = "I2S failed";
    Serial.println("Speaker I2S not started: begin failed");
    return false;
  }

  speakerI2sReady = true;
  speakerI2sStatus = "Ready";

  Serial.print("Speaker I2S pins: ");
  Serial.println(speakerI2sPinsLabel());

  Serial.print("Speaker I2S format: ");
  Serial.print(sampleRate);
  Serial.print(" Hz, ");
  Serial.print(bitsPerSample);
  Serial.print("-bit, ");
  Serial.print(channels);
  Serial.println(channels == 1 ? " channel" : " channels");

  return true;
}

bool prepareSpeakerI2sForBrowserAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
) {
  if (audioI2sPinConflicts()) {
    speakerI2sReady = false;
    speakerI2sStatus = "Pin conflict";
    return false;
  }

  speakerI2s.end();
  speakerI2sReady = false;

  return initSpeakerI2sForFormat(sampleRate, channels, bitsPerSample, false);
}

bool prepareSpeakerI2sForHorn(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
) {
  if (audioI2sPinConflicts()) {
    speakerI2sReady = false;
    speakerI2sStatus = "Pin conflict";
    return false;
  }

  speakerI2s.end();
  speakerI2sReady = false;

  return initSpeakerI2sForFormat(sampleRate, channels, bitsPerSample, true);
}
