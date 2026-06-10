#include <Arduino.h>
#include <TFT_eSPI.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include <math.h>
#include "esp_system.h"

TFT_eSPI tft = TFT_eSPI();

// -------------------- I2S pins --------------------
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// -------------------- Joystick --------------------
const int JOY_SW_PIN = 27;

// GPIO32 = ADC1_CHANNEL_4
// GPIO33 = ADC1_CHANNEL_5
const adc1_channel_t JOY_X_ADC = ADC1_CHANNEL_4;
const adc1_channel_t JOY_Y_ADC = ADC1_CHANNEL_5;

// -------------------- Display layout --------------------
const int SCREEN_W = 240;
const int SCREEN_H = 135;

const int BOX_X = 40;
const int BOX_Y = 14;
const int BOX_W = 160;
const int BOX_H = 70;

const int DOT_RADIUS = 5;

// -------------------- Audio settings --------------------
const int SAMPLE_RATE = 44100;
const int BUFFER_FRAMES = 128;

const float MIN_FREQ = 110.0;
const float MAX_FREQ = 1760.0;
const float MAX_AMP  = 0.15;

// Global audio buffer.
int16_t audioSamples[BUFFER_FRAMES * 2];

// -------------------- Shared audio control --------------------
// These are written by loop() and read by audioTask().
volatile float targetFreq = 440.0;
volatile float targetAmp = 0.0;

// Mutex for safe shared access.
portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED;

// -------------------- Audio task state --------------------
TaskHandle_t audioTaskHandle = NULL;
bool i2sReady = false;

// -------------------- Joystick calibration --------------------
int centerX = 2048;
int centerY = 2048;

// -------------------- UI/log timers --------------------
unsigned long lastLog = 0;
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("================================");
  Serial.println("Joystick I2S TFT Synth Threaded");
  Serial.println("================================");

  printResetReason();

  pinMode(JOY_SW_PIN, INPUT_PULLUP);

  setupLegacyAdc();
  setupDisplay();

  Serial.println("[1] Calibrating joystick...");
  showCalibrationScreen();
  calibrateJoystick();
  Serial.println("[1] Joystick calibrated.");

  Serial.println("[2] Setting up I2S...");
  esp_err_t result = setupI2S();

  Serial.print("[2] I2S setup result: ");
  Serial.println((int)result);

  if (result == ESP_OK) {
    i2sReady = true;
    Serial.println("[2] I2S ready.");
  } else {
    i2sReady = false;
    Serial.println("[2] I2S failed. Continuing with display only.");
  }

  if (i2sReady) {
    Serial.println("[3] Starting audio task...");

    xTaskCreatePinnedToCore(
      audioTask,
      "audioTask",
      4096,
      NULL,
      2,
      &audioTaskHandle,
      0
    );

    Serial.println("[3] Audio task started.");
  }

  tft.fillScreen(TFT_BLACK);
  drawStaticUi();

  Serial.println("[4] Entering main loop.");
}

void loop() {
  int rawX = readJoyX();
  int rawY = readJoyY();
  bool pressed = digitalRead(JOY_SW_PIN) == LOW;

  int normalizedX = normalizeAxis(rawX, centerX);
  int normalizedY = normalizeAxis(rawY, centerY);

  // Corrected so left = lower pitch, right = higher pitch.
  int correctedX = -normalizedX;

  float pitchAmount = (correctedX + 1000) / 2000.0;
  pitchAmount = constrain(pitchAmount, 0.0, 1.0);

  float newFreq = MIN_FREQ * pow(MAX_FREQ / MIN_FREQ, pitchAmount);

  float ampAmount = (normalizedY + 1000) / 2000.0;
  ampAmount = constrain(ampAmount, 0.0, 1.0);

  float newAmp = ampAmount * MAX_AMP;

  if (pressed) {
    newAmp = 0.0;
  }

  // Update shared audio targets.
  portENTER_CRITICAL(&audioMux);
  targetFreq = newFreq;
  targetAmp = newAmp;
  portEXIT_CRITICAL(&audioMux);

  // Screen is updated slowly so it does not interfere much.
  if (millis() - lastDisplayUpdate > 150) {
    lastDisplayUpdate = millis();

    drawJoystickUi(
      rawX,
      rawY,
      normalizedX,
      normalizedY,
      newFreq,
      newAmp,
      pressed
    );
  }

  if (millis() - lastLog > 1000) {
    lastLog = millis();

    Serial.print("[LOOP] rawX=");
    Serial.print(rawX);
    Serial.print(" rawY=");
    Serial.print(rawY);
    Serial.print(" normX=");
    Serial.print(normalizedX);
    Serial.print(" normY=");
    Serial.print(normalizedY);
    Serial.print(" freq=");
    Serial.print(newFreq, 1);
    Serial.print(" amp=");
    Serial.print(newAmp, 2);
    Serial.print(" button=");
    Serial.print(pressed ? "MUTE" : "PLAY");
    Serial.print(" heap=");
    Serial.println(ESP.getFreeHeap());
  }

  delay(5);
}

// -------------------- Audio task --------------------

void audioTask(void *parameter) {
  Serial.println("[AUDIO] Task running.");

  float phase = 0.0;
  float currentFreq = 440.0;
  float currentAmp = 0.0;

  while (true) {
    float localTargetFreq;
    float localTargetAmp;

    portENTER_CRITICAL(&audioMux);
    localTargetFreq = targetFreq;
    localTargetAmp = targetAmp;
    portEXIT_CRITICAL(&audioMux);

    // Smooth inside the audio thread.
    // This reduces zipper noise/clicks when the joystick changes values.
    currentFreq = currentFreq * 0.995 + localTargetFreq * 0.005;
    currentAmp  = currentAmp  * 0.995 + localTargetAmp  * 0.005;

    writeSineChunk(currentFreq, currentAmp, phase);
  }
}

// -------------------- Display --------------------

void setupDisplay() {
  Serial.println("[TFT] Initializing display...");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Joystick I2S Synth");
  tft.setCursor(5, 20);
  tft.println("Booting...");

  Serial.println("[TFT] Display ready.");
}

void showCalibrationScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(10, 20);
  tft.println("Joystick I2S Synth");

  tft.setCursor(10, 45);
  tft.println("Calibrating joystick...");

  tft.setCursor(10, 62);
  tft.println("Do not touch joystick.");

  tft.drawRect(10, 90, SCREEN_W - 20, 8, TFT_WHITE);
}

void drawStaticUi() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(5, 2);
  tft.println("Joystick I2S Synth");

  drawGrid();

  tft.setCursor(5, 122);
  tft.print("X=pitch Y=volume SW=mute");
}

void drawGrid() {
  tft.drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, TFT_WHITE);

  tft.drawLine(
    BOX_X + BOX_W / 2,
    BOX_Y,
    BOX_X + BOX_W / 2,
    BOX_Y + BOX_H,
    TFT_DARKGREY
  );

  tft.drawLine(
    BOX_X,
    BOX_Y + BOX_H / 2,
    BOX_X + BOX_W,
    BOX_Y + BOX_H / 2,
    TFT_DARKGREY
  );
}

void drawJoystickUi(
  int rawX,
  int rawY,
  int normalizedX,
  int normalizedY,
  float frequency,
  float amplitude,
  bool pressed
) {
  int dotX = map(normalizedX, -1000, 1000, BOX_X + BOX_W, BOX_X);
  int dotY = map(normalizedY, -1000, 1000, BOX_Y + BOX_H, BOX_Y);

  dotX = constrain(dotX, BOX_X, BOX_X + BOX_W);
  dotY = constrain(dotY, BOX_Y, BOX_Y + BOX_H);

  tft.fillRect(BOX_X + 1, BOX_Y + 1, BOX_W - 2, BOX_H - 2, TFT_BLACK);
  drawGrid();

  tft.fillCircle(dotX, dotY, DOT_RADIUS, pressed ? TFT_RED : TFT_GREEN);

  tft.fillRect(0, 88, SCREEN_W, 47, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(5, 90);
  tft.print("Pitch:");
  tft.print(frequency, 1);
  tft.print("Hz");

  tft.setCursor(5, 104);
  tft.print("Vol:");
  tft.print(amplitude, 2);

  tft.print(" X:");
  tft.print(rawX);

  tft.print(" Y:");
  tft.print(rawY);

  tft.setCursor(5, 118);
  tft.print("SW:");
  tft.print(pressed ? "MUTE " : "PLAY ");
  tft.print("Heap:");
  tft.print(ESP.getFreeHeap());
}

// -------------------- ADC --------------------

void setupLegacyAdc() {
  Serial.println("[ADC] Setting up legacy ADC1");

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(JOY_X_ADC, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(JOY_Y_ADC, ADC_ATTEN_DB_11);

  Serial.println("[ADC] Legacy ADC1 ready");
}

int readJoyX() {
  return adc1_get_raw(JOY_X_ADC);
}

int readJoyY() {
  return adc1_get_raw(JOY_Y_ADC);
}

// -------------------- Diagnostics --------------------

void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();

  Serial.print("Reset reason code: ");
  Serial.println((int)reason);

  switch (reason) {
    case ESP_RST_POWERON:
      Serial.println("Reset reason: power-on");
      break;
    case ESP_RST_EXT:
      Serial.println("Reset reason: external reset");
      break;
    case ESP_RST_SW:
      Serial.println("Reset reason: software reset");
      break;
    case ESP_RST_PANIC:
      Serial.println("Reset reason: panic/crash");
      break;
    case ESP_RST_INT_WDT:
      Serial.println("Reset reason: interrupt watchdog");
      break;
    case ESP_RST_TASK_WDT:
      Serial.println("Reset reason: task watchdog");
      break;
    case ESP_RST_WDT:
      Serial.println("Reset reason: watchdog");
      break;
    case ESP_RST_BROWNOUT:
      Serial.println("Reset reason: brownout");
      break;
    default:
      Serial.println("Reset reason: other/unknown");
      break;
  }

  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
}

// -------------------- Joystick --------------------

void calibrateJoystick() {
  const int samples = 120;
  long sumX = 0;
  long sumY = 0;

  Serial.println("[CAL] Do not touch joystick.");

  for (int i = 0; i < samples; i++) {
    int x = readJoyX();
    int y = readJoyY();

    sumX += x;
    sumY += y;

    if (i % 20 == 0) {
      Serial.print("[CAL] sample=");
      Serial.print(i);
      Serial.print(" x=");
      Serial.print(x);
      Serial.print(" y=");
      Serial.println(y);
    }

    int progressW = map(i, 0, samples - 1, 0, SCREEN_W - 20);
    tft.fillRect(10, 90, progressW, 8, TFT_GREEN);

    delay(15);
  }

  centerX = sumX / samples;
  centerY = sumY / samples;

  Serial.print("[CAL] centerX=");
  Serial.println(centerX);
  Serial.print("[CAL] centerY=");
  Serial.println(centerY);
}

int normalizeAxis(int raw, int center) {
  int value = raw - center;
  const int deadzone = 80;

  if (abs(value) < deadzone) {
    return 0;
  }

  if (value > 0) {
    int positiveRange = 4095 - center - deadzone;
    if (positiveRange <= 0) return 0;

    return constrain(
      map(value, deadzone, 4095 - center, 0, 1000),
      0,
      1000
    );
  } else {
    int negativeRange = center - deadzone;
    if (negativeRange <= 0) return 0;

    return constrain(
      map(value, -deadzone, -center, 0, -1000),
      -1000,
      0
    );
  }
}

// -------------------- I2S --------------------

esp_err_t setupI2S() {
  i2s_driver_uninstall(I2S_NUM_0);

  i2s_config_t i2s_config = {};

  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = SAMPLE_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_I2S_MSB;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len = BUFFER_FRAMES;
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = true;
  i2s_config.fixed_mclk = 0;

  i2s_pin_config_t pin_config = {};

  pin_config.bck_io_num = I2S_BCLK;
  pin_config.ws_io_num = I2S_LRC;
  pin_config.data_out_num = I2S_DOUT;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t installResult = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

  Serial.print("[I2S] install: ");
  Serial.println((int)installResult);

  if (installResult != ESP_OK) {
    return installResult;
  }

  esp_err_t pinResult = i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.print("[I2S] set_pin: ");
  Serial.println((int)pinResult);

  if (pinResult != ESP_OK) {
    return pinResult;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);

  return ESP_OK;
}

void writeSineChunk(float frequency, float amplitude, float &phase) {
  const float twoPi = 2.0 * PI;
  float phaseIncrement = twoPi * frequency / SAMPLE_RATE;

  for (int i = 0; i < BUFFER_FRAMES; i++) {
    float value = sin(phase) * amplitude;
    int16_t sample = (int16_t)(value * 32767);

    audioSamples[i * 2] = sample;
    audioSamples[i * 2 + 1] = sample;

    phase += phaseIncrement;

    if (phase >= twoPi) {
      phase -= twoPi;
    }
  }

  size_t bytesWritten = 0;

  // In the dedicated audio task, blocking is good:
  // it keeps the I2S stream paced continuously.
  i2s_write(
    I2S_NUM_0,
    audioSamples,
    sizeof(audioSamples),
    &bytesWritten,
    portMAX_DELAY
  );
}