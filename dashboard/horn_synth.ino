#include <Arduino.h>
#include <math.h>
#include "AudioTools.h"

// ----------------------
// Direct I2S horn synth
// ----------------------
static const uint32_t HORN_SAMPLE_RATE = 22050;
static const uint8_t HORN_CHANNELS = 1;
static const uint8_t HORN_BITS_PER_SAMPLE = 16;
static const float HORN_TONE_A_HZ = 342.5f;
static const float HORN_TONE_B_HZ = 387.5f;
static const float HORN_LOWPASS_HZ = 1800.0f;
static const float HORN_VOLUME = 0.35f;
static const uint16_t HORN_ATTACK_MS = 6;
static const uint16_t HORN_RELEASE_MS = 45;
static const size_t HORN_BUFFER_FRAMES = 128;
static const float HORN_TWO_PI = 6.28318530718f;

enum class HornSynthState : uint8_t {
  Idle,
  Attack,
  Sustain,
  Release
};

extern I2SStream speakerI2s;

bool prepareSpeakerI2sForBrowserAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);
bool speakerI2sStarted();
String speakerI2sStatusLabel();
bool browserAudioIsPlaying();
void stopBrowserAudioPlayback(const String &status);

HornSynthState hornSynthState = HornSynthState::Idle;
bool hornSynthHeld = false;
uint32_t hornSynthShortUntilMs = 0;
float hornSynthPhaseA = 0.0f;
float hornSynthPhaseB = 0.0f;
float hornSynthLowpassState = 0.0f;
float hornSynthEnvelope = 0.0f;
float hornSynthLowpassAlpha = 0.0f;
float hornSynthAttackStep = 0.0f;
float hornSynthReleaseStep = 0.0f;
String hornSynthStatus = "Horn idle";

bool hornSynthShortIsActive(uint32_t now);
bool hornSynthBegin(String &message);
void updateHornSynthState();
int16_t nextHornSynthSample();
float advanceHornSynthSaw(float &phase, float frequencyHz);

bool initHornSynth() {
  hornSynthState = HornSynthState::Idle;
  hornSynthHeld = false;
  hornSynthShortUntilMs = 0;
  hornSynthPhaseA = 0.0f;
  hornSynthPhaseB = 0.0f;
  hornSynthLowpassState = 0.0f;
  hornSynthEnvelope = 0.0f;
  hornSynthLowpassAlpha = 1.0f - expf(
    (-HORN_TWO_PI * HORN_LOWPASS_HZ) / (float)HORN_SAMPLE_RATE
  );
  hornSynthAttackStep = 1.0f /
    ((float)HORN_SAMPLE_RATE * ((float)HORN_ATTACK_MS / 1000.0f));
  hornSynthReleaseStep = 1.0f /
    ((float)HORN_SAMPLE_RATE * ((float)HORN_RELEASE_MS / 1000.0f));
  hornSynthStatus = "Horn idle";
  return true;
}

bool startHornSynth(String &message) {
  hornSynthHeld = true;
  return hornSynthBegin(message);
}

void stopHornSynth() {
  hornSynthHeld = false;
  if (!hornSynthShortIsActive(millis()) && hornSynthState != HornSynthState::Idle) {
    hornSynthState = HornSynthState::Release;
    hornSynthStatus = "Horn releasing";
  }
}

void triggerShortHornSynth(uint16_t durationMs) {
  if (durationMs == 0) {
    return;
  }

  hornSynthShortUntilMs = millis() + durationMs;
  String message;
  hornSynthBegin(message);
}

void cancelHornSynth() {
  hornSynthHeld = false;
  hornSynthShortUntilMs = 0;
  if (hornSynthState != HornSynthState::Idle) {
    hornSynthState = HornSynthState::Release;
    hornSynthStatus = "Horn releasing";
  }
}

void updateHornSynth() {
  updateHornSynthState();
  if (hornSynthState == HornSynthState::Idle) {
    return;
  }

  int16_t buffer[HORN_BUFFER_FRAMES];
  for (size_t i = 0; i < HORN_BUFFER_FRAMES; i++) {
    buffer[i] = nextHornSynthSample();
  }

  size_t written = speakerI2s.write(
    reinterpret_cast<const uint8_t *>(buffer),
    sizeof(buffer)
  );
  if (written != sizeof(buffer)) {
    hornSynthStatus = "Horn I2S write failed";
  }
}

bool hornSynthIsActive() {
  return hornSynthState != HornSynthState::Idle;
}

String hornSynthStatusLabel() {
  return hornSynthStatus;
}

bool hornSynthShortIsActive(uint32_t now) {
  return hornSynthShortUntilMs != 0 &&
    (int32_t)(hornSynthShortUntilMs - now) > 0;
}

bool hornSynthBegin(String &message) {
  if (hornSynthState == HornSynthState::Idle) {
    if (browserAudioIsPlaying()) {
      stopBrowserAudioPlayback("Stopped for horn");
    }

    if (!prepareSpeakerI2sForBrowserAudio(
          HORN_SAMPLE_RATE,
          HORN_CHANNELS,
          HORN_BITS_PER_SAMPLE
        )) {
      hornSynthStatus = String("Horn unavailable: ") + speakerI2sStatusLabel();
      message = hornSynthStatus;
      return false;
    }

    hornSynthPhaseA = 0.0f;
    hornSynthPhaseB = 0.0f;
    hornSynthLowpassState = 0.0f;
    hornSynthEnvelope = 0.0f;
  } else if (!speakerI2sStarted()) {
    if (!prepareSpeakerI2sForBrowserAudio(
          HORN_SAMPLE_RATE,
          HORN_CHANNELS,
          HORN_BITS_PER_SAMPLE
        )) {
      hornSynthStatus = String("Horn unavailable: ") + speakerI2sStatusLabel();
      message = hornSynthStatus;
      return false;
    }
  }

  hornSynthState = HornSynthState::Attack;
  hornSynthStatus = "Horn attack";
  message = hornSynthStatus;
  return true;
}

void updateHornSynthState() {
  uint32_t now = millis();
  bool shortActive = hornSynthShortIsActive(now);
  if (!shortActive) {
    hornSynthShortUntilMs = 0;
  }

  if (hornSynthHeld || shortActive) {
    if (hornSynthState == HornSynthState::Release) {
      hornSynthState = HornSynthState::Attack;
      hornSynthStatus = "Horn attack";
    }
    return;
  }

  if (hornSynthState == HornSynthState::Attack ||
      hornSynthState == HornSynthState::Sustain) {
    hornSynthState = HornSynthState::Release;
    hornSynthStatus = "Horn releasing";
  }
}

int16_t nextHornSynthSample() {
  if (hornSynthState == HornSynthState::Attack) {
    hornSynthEnvelope += hornSynthAttackStep;
    if (hornSynthEnvelope >= 1.0f) {
      hornSynthEnvelope = 1.0f;
      hornSynthState = HornSynthState::Sustain;
      hornSynthStatus = "Horn synth";
    }
  } else if (hornSynthState == HornSynthState::Release) {
    hornSynthEnvelope -= hornSynthReleaseStep;
    if (hornSynthEnvelope <= 0.0f) {
      hornSynthEnvelope = 0.0f;
      hornSynthState = HornSynthState::Idle;
      hornSynthStatus = "Horn idle";
      return 0;
    }
  }

  float mixedSample = (advanceHornSynthSaw(hornSynthPhaseA, HORN_TONE_A_HZ) +
    advanceHornSynthSaw(hornSynthPhaseB, HORN_TONE_B_HZ)) * 0.5f;
  hornSynthLowpassState +=
    hornSynthLowpassAlpha * (mixedSample - hornSynthLowpassState);

  float outputSample = hornSynthLowpassState * hornSynthEnvelope * HORN_VOLUME;
  if (outputSample > 1.0f) {
    outputSample = 1.0f;
  } else if (outputSample < -1.0f) {
    outputSample = -1.0f;
  }
  return (int16_t)(outputSample * 32767.0f);
}

float advanceHornSynthSaw(float &phase, float frequencyHz) {
  float sample = (phase * 2.0f) - 1.0f;
  phase += frequencyHz / (float)HORN_SAMPLE_RATE;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return sample;
}
