float normalizeGaugeValue(float value, float minValue, float maxValue) {
  if (maxValue <= minValue) {
    return 0.0f;
  }

  return clamp01((value - minValue) / (maxValue - minValue));
}

void drawArcLine(
  TFT_eSprite &s,
  int cx,
  int cy,
  int radius,
  float startDeg,
  float endDeg,
  uint16_t color
) {
  float step = startDeg <= endDeg ? 3.0f : -3.0f;
  int lastX = polarX(cx, radius, startDeg);
  int lastY = polarY(cy, radius, startDeg);

  for (
    float deg = startDeg + step;
    (step > 0.0f) ? (deg <= endDeg) : (deg >= endDeg);
    deg += step
  ) {
    int x = polarX(cx, radius, deg);
    int y = polarY(cy, radius, deg);
    s.drawLine(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
  }
}

void drawTallCircularGauge(
  TFT_eSprite &s,
  int cx,
  int cy,
  int radius,
  float minValue,
  float maxValue,
  float value,
  const char *title
) {
  uint16_t detail = rgb565(64, 64, 64);
  float startDeg = 165.0f;
  float endDeg = 375.0f;
  float progress = normalizeGaugeValue(value, minValue, maxValue);
  float needleDeg = startDeg + ((endDeg - startDeg) * progress);

  drawArcLine(s, cx, cy, radius, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, cx, cy, radius - 1, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, cx, cy, radius - 6, startDeg, endDeg, detail);

  for (int i = 0; i <= 8; i++) {
    float p = (float)i / 8.0f;
    float deg = startDeg + ((endDeg - startDeg) * p);
    int tickInner = (i % 2 == 0) ? radius - 12 : radius - 8;
    s.drawLine(
      polarX(cx, tickInner, deg),
      polarY(cy, tickInner, deg),
      polarX(cx, radius, deg),
      polarY(cy, radius, deg),
      TFT_WHITE
    );
  }

  s.drawLine(
    cx,
    cy,
    polarX(cx, radius - 10, needleDeg),
    polarY(cy, radius - 10, needleDeg),
    TFT_RED
  );
  s.drawLine(
    cx + 1,
    cy,
    polarX(cx, radius - 10, needleDeg),
    polarY(cy, radius - 10, needleDeg),
    TFT_RED
  );
  s.fillCircle(cx, cy, 4, TFT_WHITE);
  s.fillCircle(cx, cy, 2, TFT_RED);

  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.setTextDatum(MC_DATUM);
  s.drawString(title, cx, cy - radius + 22, 2);
  s.setTextDatum(TL_DATUM);
}

void drawDownwardArcGauge(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  float progress,
  const char *startLabel,
  const char *endLabel
) {
  uint16_t detail = rgb565(64, 64, 64);
  float startDeg = 30.0f;
  float endDeg = 150.0f;
  float needleDeg = startDeg + ((endDeg - startDeg) * clamp01(progress));

  drawArcLine(s, pivotX, pivotY, radius, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, pivotX, pivotY, radius - 1, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, pivotX, pivotY, radius - 4, startDeg, endDeg, detail);

  for (int i = 0; i <= 4; i++) {
    float p = (float)i / 4.0f;
    float deg = startDeg + ((endDeg - startDeg) * p);
    int tickInner = (i == 0 || i == 4) ? radius - 8 : radius - 4;
    s.drawLine(
      polarX(pivotX, tickInner, deg),
      polarY(pivotY, tickInner, deg),
      polarX(pivotX, radius, deg),
      polarY(pivotY, radius, deg),
      TFT_WHITE
    );
  }

  s.drawLine(
    pivotX,
    pivotY,
    polarX(pivotX, radius - 8, needleDeg),
    polarY(pivotY, radius - 8, needleDeg),
    TFT_RED
  );
  s.drawLine(
    pivotX,
    pivotY + 1,
    polarX(pivotX, radius - 8, needleDeg),
    polarY(pivotY, radius - 8, needleDeg),
    TFT_RED
  );
  s.fillCircle(pivotX, pivotY, 4, TFT_WHITE);
  s.fillCircle(pivotX, pivotY, 2, TFT_RED);

  if (startLabel != nullptr || endLabel != nullptr) {
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.setTextDatum(MC_DATUM);

    if (startLabel != nullptr) {
      s.drawString(
        startLabel,
        polarX(pivotX, radius + 13, startDeg) - 5,
        polarY(pivotY, radius + 8, startDeg),
        1
      );
    }

    if (endLabel != nullptr) {
      s.drawString(
        endLabel,
        polarX(pivotX, radius + 8, endDeg) + 5,
        polarY(pivotY, radius + 8, endDeg),
        1
      );
    }

    s.setTextDatum(TL_DATUM);
  }
}

void drawFuelGauge(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  float fuelLevel
) {
  float progressDown = 1.0f - clamp01(fuelLevel);
  drawDownwardArcGauge(s, pivotX, pivotY, radius, progressDown, "F", "E");
}

void drawTemperatureGauge(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  float temperatureC,
  bool sensorAvailable
) {
  float temperatureF = (temperatureC * 1.8f) + 32.0f;
  float progressDown = sensorAvailable ? (1.0f - normalizeGaugeValue(temperatureF, 32.0f, 100.0f)) : 0.5f;
  drawDownwardArcGauge(s, pivotX, pivotY, radius, progressDown, "H", "C");
}

void drawTurnSignal(TFT_eSprite &s, int x, int y, bool left, bool active) {
  uint16_t onColor = rgb565(32, 210, 90);
  uint16_t offColor = rgb565(28, 56, 32);
  uint16_t color = active ? onColor : offColor;

  if (left) {
    s.fillTriangle(x, y + 5, x + 8, y, x + 8, y + 10, color);
    s.fillRect(x + 8, y + 3, 8, 4, color);
  } else {
    s.fillTriangle(x + 16, y + 5, x + 8, y, x + 8, y + 10, color);
    s.fillRect(x, y + 3, 8, 4, color);
  }
}

void drawGearColumn(TFT_eSprite &s, int centerX, int startY, int selectedIndex) {
  const char gears[4] = {'P', 'R', 'N', 'D'};

  s.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    int gearY = startY + (i * 15);
    if (i == selectedIndex) {
      s.fillRoundRect(centerX - 5, gearY - 4, 10, 9, 3, TFT_WHITE);
      s.setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
      s.setTextColor(rgb565(110, 110, 110), TFT_BLACK);
    }

    s.drawString(String(gears[i]), centerX, gearY, 1);
  }
  s.setTextDatum(TL_DATUM);
}

void drawOdometer(TFT_eSprite &s, int x, int y, const String &digits) {
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString("Odo", x, y + 2, 1);
  s.setTextDatum(MC_DATUM);
  int boxStartX = x + 18;

  for (int i = 0; i < 6; i++) {
    int boxX = boxStartX + (i * 10);
    String digit = "0";
    if (i < digits.length()) {
      digit = String(digits.charAt(i));
    }

    s.drawRect(boxX, y, 8, 11, TFT_WHITE);
    s.drawString(digit, boxX + 4, y + 6, 1);
  }

  s.setTextDatum(TL_DATUM);
}

void drawHeadlightIndicator(TFT_eSprite &s, int x, int y, bool active) {
  uint16_t onColor = rgb565(64, 144, 255);
  uint16_t offColor = rgb565(32, 52, 84);
  uint16_t color = active ? onColor : offColor;

  s.fillCircle(x + 6, y + 6, 4, color);
  s.drawFastVLine(x + 10, y + 2, 9, color);
  s.drawLine(x + 12, y + 3, x + 17, y + 1, color);
  s.drawLine(x + 12, y + 6, x + 18, y + 6, color);
  s.drawLine(x + 12, y + 9, x + 17, y + 11, color);
}

void drawCheckEngineIcon(TFT_eSprite &s, int x, int y, uint16_t color) {
  s.drawRoundRect(x + 1, y + 2, 12, 8, 2, color);
  s.drawRect(x + 3, y, 5, 3, color);
  s.drawRect(x + 10, y + 4, 3, 2, color);
  s.drawLine(x, y + 4, x + 1, y + 4, color);
  s.drawLine(x, y + 7, x + 1, y + 7, color);
  s.drawLine(x + 5, y + 4, x + 8, y + 7, color);
  s.drawFastVLine(x + 14, y + 4, 5, color);
}

void drawLowFuelIndicator(TFT_eSprite &s, int x, int y, bool active) {
  uint16_t accent = rgb565(255, 177, 52);
  uint16_t off = rgb565(60, 52, 28);
  uint16_t color = (active && warningOn) ? accent : off;
  drawCheckEngineIcon(s, x, y, color);
}

void drawTruckSideIcon(TFT_eSprite &s, int x, int y) {
  s.drawRect(x + 2, y + 4, 12, 5, TFT_WHITE);
  s.drawRect(x + 14, y + 2, 6, 7, TFT_WHITE);
  s.drawCircle(x + 6, y + 11, 2, TFT_WHITE);
  s.drawCircle(x + 16, y + 11, 2, TFT_WHITE);
}

void drawTruckFrontIcon(TFT_eSprite &s, int x, int y) {
  s.drawRect(x + 5, y + 2, 10, 9, TFT_WHITE);
  s.drawLine(x + 3, y + 6, x + 5, y + 6, TFT_WHITE);
  s.drawLine(x + 15, y + 6, x + 17, y + 6, TFT_WHITE);
  s.drawCircle(x + 7, y + 12, 2, TFT_WHITE);
  s.drawCircle(x + 13, y + 12, 2, TFT_WHITE);
}

void drawTiltWidget(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  bool frontView,
  float tiltValue
) {
  uint16_t detail = rgb565(64, 64, 64);
  float clamped = clamp01((tiltValue + 15.0f) / 30.0f);
  float bubbleDeg = 150.0f - (clamped * 120.0f);
  float startDeg = 150.0f;
  float endDeg = 30.0f;

  drawArcLine(s, pivotX, pivotY, radius, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, pivotX, pivotY, radius - 1, startDeg, endDeg, TFT_WHITE);
  drawArcLine(s, pivotX, pivotY, radius - 4, startDeg, endDeg, detail);

  for (int i = 0; i <= 4; i++) {
    float p = (float)i / 4.0f;
    float deg = startDeg + ((endDeg - startDeg) * p);
    int tickInner = (i == 0 || i == 4) ? radius - 8 : radius - 4;
    s.drawLine(
      polarX(pivotX, tickInner, deg),
      polarY(pivotY, tickInner, deg),
      polarX(pivotX, radius, deg),
      polarY(pivotY, radius, deg),
      TFT_WHITE
    );
  }

  int bubbleX = polarX(pivotX, radius - 5, bubbleDeg);
  int bubbleY = polarY(pivotY, radius - 5, bubbleDeg);
  s.fillCircle(bubbleX, bubbleY, 2, TFT_RED);

  if (frontView) {
    drawTruckFrontIcon(s, pivotX - 10, pivotY + 3 - radius - 4);
  } else {
    drawTruckSideIcon(s, pivotX - 10, pivotY + 3 - radius - 4);
  }
}

void renderGaugeScreen(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);

  int offsetX = 0;
  int offsetY = 10;

  bool lowFuelActive = dashboardFuelLevel <= 0.2f;
  drawFuelGauge(s, 20 + offsetX, 40 + offsetY, 17, dashboardFuelLevel);
  drawHeadlightIndicator(s, 0 + offsetX, 62 + offsetY, dashboardHeadlightsOn);

  drawTurnSignal(s, 85 + offsetX, 62 + offsetY, true, leftTurnSignalFlashing());
  drawTurnSignal(s, 132 + offsetX, 62 + offsetY, false, rightTurnSignalFlashing());
  drawGearColumn(s, 118 + offsetX, 74 + offsetY, dashboardGearIndex);

  drawTiltWidget(s, 146 + offsetX, 40 + offsetY, 17, false, pitchDeg);
  drawTiltWidget(s, 179 + offsetX, 40 + offsetY, 17, true, rollDeg);

  drawTallCircularGauge(
    s,
    50 + offsetX,
    110 + offsetY,
    45,
    0.0f,
    8.0f,
    dashboardRpmK,
    "RPM"
  );
  drawTallCircularGauge(
    s,
    185 + offsetX,
    110 + offsetY,
    45,
    0.0f,
    80.0f,
    dashboardMph,
    "MPH"
  );

  drawTemperatureGauge(s, 217 + offsetX, 40 + offsetY, 17, environmentTempC, bmeAvailable);
  // drawOdometer(s, 25 + offsetX, 118 + offsetY, dashboardOdometer);
  drawLowFuelIndicator(s, 195 + offsetX, 115 + offsetY, lowFuelActive);
}
