#include <Arduino.h>
#include <math.h>
#include "AudioTools.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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
static const uint16_t HORN_ATTACK_MS = 8;
static const uint16_t HORN_RELEASE_MS = 45;

// Keep the I2S queue shorter than 6 ms. The audio task continuously fills it
// with PCM silence while idle, so a horn request reaches the speaker promptly.
static const size_t HORN_BUFFER_FRAMES = 64;
static const size_t HORN_PRIME_FRAMES = 128;
static const uint16_t HORN_AUDIO_TASK_STACK_BYTES = 4096;
static const UBaseType_t HORN_AUDIO_TASK_PRIORITY = 3;
static const float HORN_TWO_PI = 6.28318530718f;
static const float HORN_TONE_A_START_PHASE = 0.25f;
static const float HORN_TONE_B_START_PHASE = 0.75f;

enum class HornSynthState : uint8_t {
  Idle,
  Attack,
  Sustain,
  Release
};

enum class HornSynthStatus : uint8_t {
  Idle,
  Attack,
  Synth,
  Releasing,
  Unavailable,
  PrimeFailed,
  WriteFailed,
  TaskFailed
};

extern I2SStream speakerI2s;

bool prepareSpeakerI2sForHorn(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);
bool speakerI2sStarted();
String speakerI2sStatusLabel();
bool browserAudioIsPlaying();
void stopBrowserAudioPlayback(const String &status);

volatile HornSynthState hornSynthState = HornSynthState::Idle;
volatile HornSynthStatus hornSynthStatus = HornSynthStatus::Idle;
volatile bool hornSynthHeld = false;
volatile uint32_t hornSynthShortUntilMs = 0;
volatile uint32_t hornSynthStartSequence = 0;
volatile bool hornSynthOutputPrepared = false;

float hornSynthPhaseA = HORN_TONE_A_START_PHASE;
float hornSynthPhaseB = HORN_TONE_B_START_PHASE;
float hornSynthLowpassState = 0.0f;
float hornSynthEnvelope = 0.0f;
float hornSynthAttackProgress = 0.0f;
float hornSynthAttackStartEnvelope = 0.0f;
float hornSynthLowpassAlpha = 0.0f;
float hornSynthAttackStep = 0.0f;
float hornSynthReleaseStep = 0.0f;

TaskHandle_t hornSynthAudioTaskHandle = nullptr;
SemaphoreHandle_t hornSynthOutputMutex = nullptr;

bool hornSynthShortIsActive(uint32_t now);
bool hornSynthRequestIsActive();
bool hornSynthBegin(String &message, bool startsNewRequest);
bool prepareHornSynthOutput(String &message);
void updateHornSynthState();
void resetHornSynthVoice();
void fillHornSynthBuffer(int16_t *buffer, size_t frameCount);
int16_t nextHornSynthSample();
float advanceHornSynthSaw(float &phase, float frequencyHz);
void hornSynthAudioTask(void *parameter);
void wakeHornSynthAudioTask();

bool initHornSynth() {
  hornSynthState = HornSynthState::Idle;
  hornSynthStatus = HornSynthStatus::Idle;
  hornSynthHeld = false;
  hornSynthShortUntilMs = 0;
  hornSynthStartSequence = 0;
  hornSynthOutputPrepared = false;
  hornSynthLowpassAlpha = 1.0f - expf(
    (-HORN_TWO_PI * HORN_LOWPASS_HZ) / (float)HORN_SAMPLE_RATE
  );
  hornSynthAttackStep = 1.0f /
    ((float)HORN_SAMPLE_RATE * ((float)HORN_ATTACK_MS / 1000.0f));
  hornSynthReleaseStep = 1.0f /
    ((float)HORN_SAMPLE_RATE * ((float)HORN_RELEASE_MS / 1000.0f));
  resetHornSynthVoice();

  if (hornSynthOutputMutex == nullptr) {
    hornSynthOutputMutex = xSemaphoreCreateMutex();
  }
  if (hornSynthOutputMutex == nullptr) {
    hornSynthStatus = HornSynthStatus::TaskFailed;
    return false;
  }

  if (hornSynthAudioTaskHandle == nullptr) {
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
    const BaseType_t taskCore = 0;
#else
    const BaseType_t taskCore = 1;
#endif
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
      hornSynthAudioTask,
      "HornAudio",
      HORN_AUDIO_TASK_STACK_BYTES,
      nullptr,
      HORN_AUDIO_TASK_PRIORITY,
      &hornSynthAudioTaskHandle,
      taskCore
    );
    if (taskCreated != pdPASS) {
      hornSynthAudioTaskHandle = nullptr;
      hornSynthStatus = HornSynthStatus::TaskFailed;
      return false;
    }
  }

  String setupMessage;
  if (!prepareHornSynthOutput(setupMessage)) {
    return false;
  }

  hornSynthStatus = HornSynthStatus::Idle;
  return true;
}

bool startHornSynth(String &message) {
  bool wasRequested = hornSynthRequestIsActive();
  hornSynthHeld = true;
  return hornSynthBegin(message, !wasRequested);
}

void stopHornSynth() {
  hornSynthHeld = false;
  wakeHornSynthAudioTask();
}

void triggerShortHornSynth(uint16_t durationMs) {
  if (durationMs == 0) {
    return;
  }

  bool wasRequested = hornSynthRequestIsActive();
  hornSynthShortUntilMs = millis() + durationMs;
  String message;
  hornSynthBegin(message, !wasRequested);
}

void cancelHornSynth() {
  hornSynthHeld = false;
  hornSynthShortUntilMs = 0;
  wakeHornSynthAudioTask();
}

// Browser WAV playback owns the I2S driver while it is active. Prevent the
// idle-silence task from writing while that owner tears down or reconfigures it.
void invalidateHornSynthOutput() {
  hornSynthOutputPrepared = false;
  wakeHornSynthAudioTask();

  if (hornSynthOutputMutex != nullptr &&
      xSemaphoreTake(hornSynthOutputMutex, portMAX_DELAY) == pdTRUE) {
    xSemaphoreGive(hornSynthOutputMutex);
  }
}

bool hornSynthIsActive() {
  return hornSynthRequestIsActive() || hornSynthState != HornSynthState::Idle;
}

String hornSynthStatusLabel() {
  switch (hornSynthStatus) {
    case HornSynthStatus::Attack:
      return "Horn attack";
    case HornSynthStatus::Synth:
      return "Horn synth";
    case HornSynthStatus::Releasing:
      return "Horn releasing";
    case HornSynthStatus::Unavailable:
      return String("Horn unavailable: ") + speakerI2sStatusLabel();
    case HornSynthStatus::PrimeFailed:
      return "Horn I2S prime failed";
    case HornSynthStatus::WriteFailed:
      return "Horn I2S write failed";
    case HornSynthStatus::TaskFailed:
      return "Horn audio task failed";
    case HornSynthStatus::Idle:
    default:
      return "Horn idle";
  }
}

bool hornSynthShortIsActive(uint32_t now) {
  uint32_t shortUntilMs = hornSynthShortUntilMs;
  return shortUntilMs != 0 && (int32_t)(shortUntilMs - now) > 0;
}

bool hornSynthRequestIsActive() {
  return hornSynthHeld || hornSynthShortIsActive(millis());
}

bool hornSynthBegin(String &message, bool startsNewRequest) {
  if (browserAudioIsPlaying()) {
    stopBrowserAudioPlayback("Stopped for horn");
    invalidateHornSynthOutput();
  }

  if (!hornSynthOutputPrepared || !speakerI2sStarted()) {
    if (!prepareHornSynthOutput(message)) {
      return false;
    }
  }

  if (startsNewRequest) {
    hornSynthStartSequence++;
  }
  hornSynthStatus = HornSynthStatus::Attack;
  message = hornSynthStatusLabel();
  wakeHornSynthAudioTask();
  return true;
}

bool prepareHornSynthOutput(String &message) {
  hornSynthOutputPrepared = false;
  wakeHornSynthAudioTask();

  if (hornSynthOutputMutex == nullptr ||
      xSemaphoreTake(hornSynthOutputMutex, portMAX_DELAY) != pdTRUE) {
    hornSynthStatus = HornSynthStatus::TaskFailed;
    message = hornSynthStatusLabel();
    return false;
  }

  bool prepared = prepareSpeakerI2sForHorn(
    HORN_SAMPLE_RATE,
    HORN_CHANNELS,
    HORN_BITS_PER_SAMPLE
  );
  if (!prepared) {
    hornSynthStatus = HornSynthStatus::Unavailable;
    message = hornSynthStatusLabel();
    xSemaphoreGive(hornSynthOutputMutex);
    return false;
  }

  int16_t silence[HORN_PRIME_FRAMES] = {};
  size_t written = speakerI2s.write(
    reinterpret_cast<const uint8_t *>(silence),
    sizeof(silence)
  );
  xSemaphoreGive(hornSynthOutputMutex);

  if (written != sizeof(silence)) {
    hornSynthStatus = HornSynthStatus::PrimeFailed;
    message = hornSynthStatusLabel();
    return false;
  }

  hornSynthOutputPrepared = true;
  wakeHornSynthAudioTask();
  return true;
}

void updateHornSynthState() {
  static uint32_t appliedStartSequence = 0;
  uint32_t requestedStartSequence = hornSynthStartSequence;
  if (requestedStartSequence != appliedStartSequence) {
    appliedStartSequence = requestedStartSequence;
    if (hornSynthState == HornSynthState::Release && hornSynthEnvelope > 0.0f) {
      hornSynthAttackStartEnvelope = hornSynthEnvelope;
      hornSynthAttackProgress = 0.0f;
    } else {
      resetHornSynthVoice();
    }
    hornSynthState = HornSynthState::Attack;
    hornSynthStatus = HornSynthStatus::Attack;
  }

  uint32_t now = millis();
  bool shortActive = hornSynthShortIsActive(now);
  if (!shortActive && hornSynthShortUntilMs != 0) {
    hornSynthShortUntilMs = 0;
  }

  if (hornSynthHeld || shortActive) {
    if (hornSynthState == HornSynthState::Release) {
      hornSynthAttackStartEnvelope = hornSynthEnvelope;
      hornSynthAttackProgress = 0.0f;
      hornSynthState = HornSynthState::Attack;
      hornSynthStatus = HornSynthStatus::Attack;
    }
    return;
  }

  if (hornSynthState == HornSynthState::Attack ||
      hornSynthState == HornSynthState::Sustain) {
    hornSynthState = HornSynthState::Release;
    hornSynthStatus = HornSynthStatus::Releasing;
  }
}

void resetHornSynthVoice() {
  hornSynthPhaseA = HORN_TONE_A_START_PHASE;
  hornSynthPhaseB = HORN_TONE_B_START_PHASE;
  hornSynthLowpassState = 0.0f;
  hornSynthEnvelope = 0.0f;
  hornSynthAttackProgress = 0.0f;
  hornSynthAttackStartEnvelope = 0.0f;
}

void fillHornSynthBuffer(int16_t *buffer, size_t frameCount) {
  updateHornSynthState();
  if (hornSynthState == HornSynthState::Idle) {
    memset(buffer, 0, frameCount * sizeof(int16_t));
    return;
  }

  for (size_t i = 0; i < frameCount; i++) {
    buffer[i] = nextHornSynthSample();
  }
}

int16_t nextHornSynthSample() {
  if (hornSynthState == HornSynthState::Attack) {
    hornSynthAttackProgress += hornSynthAttackStep;
    if (hornSynthAttackProgress >= 1.0f) {
      hornSynthAttackProgress = 1.0f;
      hornSynthEnvelope = 1.0f;
      hornSynthState = HornSynthState::Sustain;
      hornSynthStatus = HornSynthStatus::Synth;
    } else {
      float smoothProgress = hornSynthAttackProgress * hornSynthAttackProgress *
        (3.0f - (2.0f * hornSynthAttackProgress));
      hornSynthEnvelope = hornSynthAttackStartEnvelope +
        ((1.0f - hornSynthAttackStartEnvelope) * smoothProgress);
    }
  } else if (hornSynthState == HornSynthState::Release) {
    hornSynthEnvelope -= hornSynthReleaseStep;
    if (hornSynthEnvelope <= 0.0f) {
      hornSynthEnvelope = 0.0f;
      hornSynthState = HornSynthState::Idle;
      hornSynthStatus = HornSynthStatus::Idle;
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

void hornSynthAudioTask(void *parameter) {
  (void)parameter;
  int16_t buffer[HORN_BUFFER_FRAMES];

  while (true) {
    if (!hornSynthOutputPrepared) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
      continue;
    }

    fillHornSynthBuffer(buffer, HORN_BUFFER_FRAMES);

    if (xSemaphoreTake(hornSynthOutputMutex, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    size_t written = 0;
    if (hornSynthOutputPrepared) {
      written = speakerI2s.write(
        reinterpret_cast<const uint8_t *>(buffer),
        sizeof(buffer)
      );
    }
    xSemaphoreGive(hornSynthOutputMutex);

    if (written != sizeof(buffer) && hornSynthOutputPrepared) {
      hornSynthStatus = HornSynthStatus::WriteFailed;
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

void wakeHornSynthAudioTask() {
  if (hornSynthAudioTaskHandle != nullptr) {
    xTaskNotifyGive(hornSynthAudioTaskHandle);
  }
}
