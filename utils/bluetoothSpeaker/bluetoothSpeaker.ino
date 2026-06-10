#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// Bluetooth name shown on your phone
const char *BT_SPEAKER_NAME = "ESP32-TDisplay-Speaker";

// ESP32 -> MAX98357 pins
#define I2S_BCLK 26   // GPIO26 -> MAX98357 BCLK / BCK
#define I2S_LRC  25   // GPIO25 -> MAX98357 LRC / WS
#define I2S_DOUT 22   // GPIO22 -> MAX98357 DIN

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Starting ESP32 Bluetooth Speaker...");

  // Configure I2S output to the MAX98357
  auto cfg = i2s.defaultConfig(TX_MODE);

  cfg.pin_bck  = I2S_BCLK;
  cfg.pin_ws   = I2S_LRC;
  cfg.pin_data = I2S_DOUT;

  // Normal Bluetooth audio settings
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = 16;
  cfg.channels = 2;

  i2s.begin(cfg);

  // Optional volume, range is usually 0-127
  a2dp_sink.set_volume(80);

  // Start Bluetooth A2DP receiver
  a2dp_sink.start(BT_SPEAKER_NAME);

  Serial.print("Pair your phone with: ");
  Serial.println(BT_SPEAKER_NAME);
}

void loop() {
  // Audio runs in the background
}