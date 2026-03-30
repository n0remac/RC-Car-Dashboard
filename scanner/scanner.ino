#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <TFT_eSPI.h>

#define SDA_PIN 21
#define SCL_PIN 22

// change if scanner showed 0x77
#define BME_ADDRESS 0x76

TFT_eSPI tft = TFT_eSPI();
Adafruit_BME280 bme;

void drawValue(const char* label, float value, const char* unit, int y, uint16_t color) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setCursor(10, y);
  tft.print(label);
  tft.print(": ");

  tft.print(value, 1);
  tft.print(" ");
  tft.println(unit);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);

  tft.setCursor(10, 10);
  tft.println("BME280");

  Wire.begin(SDA_PIN, SCL_PIN);

  bool status = bme.begin(BME_ADDRESS);

  if (!status) {
    tft.setTextColor(TFT_RED, TFT_BLACK);

    tft.setCursor(10, 40);
    tft.println("Sensor not found");

    tft.setCursor(10, 70);
    tft.println("Check wiring");

    while (1) {
      delay(1000);
    }
  }

  delay(1000);
}

void loop() {

  float tempC = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;

  // sea level pressure may vary by location
  float altitude = bme.readAltitude(1013.25);

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(10, 5);
  tft.println("ENVIRONMENT");

  drawValue("Temp", tempC, "C", 35, TFT_ORANGE);

  drawValue("Humidity", humidity, "%", 60, TFT_CYAN);

  drawValue("Pressure", pressure, "hPa", 85, TFT_GREEN);

  drawValue("Altitude", altitude, "m", 110, TFT_YELLOW);

  delay(1000);
}