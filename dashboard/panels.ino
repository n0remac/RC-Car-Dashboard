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

void drawDashboardShell(TFT_eSprite &s) {
  uint16_t detail = rgb565(64, 64, 64);

  s.drawLine(1, 1, 239, 1, TFT_WHITE);
  s.drawLine(1, 1, 20, 134, TFT_WHITE);
  s.drawLine(239, 1, 212, 134, TFT_WHITE);
  s.drawLine(20, 134, 212, 134, TFT_WHITE);
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

void drawFuelGauge(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  float fuelLevel
) {
  uint16_t detail = rgb565(64, 64, 64);
  float startDeg = -60.0f;
  float endDeg = 60.0f;
  float progressDown = 1.0f - clamp01(fuelLevel);
  float needleDeg = startDeg + ((endDeg - startDeg) * progressDown);

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

  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.setTextDatum(MC_DATUM);
  s.drawString("F", pivotX + radius - 9, pivotY - radius - 5, 1);
  s.drawString("E", pivotX + radius - 9, pivotY + radius + 8, 1);
  s.setTextDatum(TL_DATUM);
}

void drawTemperatureGauge(
  TFT_eSprite &s,
  int pivotX,
  int pivotY,
  int radius,
  float temperatureC,
  bool sensorAvailable
) {
  uint16_t detail = rgb565(64, 64, 64);
  float startDeg = 120.0f;
  float endDeg = 240.0f;
  float progressDown = sensorAvailable ? (1.0f - normalizeGaugeValue(temperatureC, 0.0f, 50.0f)) : 0.5f;
  float needleDeg = startDeg + ((endDeg - startDeg) * progressDown);

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
  int x,
  int y,
  int w,
  bool frontView,
  float tiltValue
) {
  float clamped = clamp01((tiltValue + 15.0f) / 30.0f);
  float bubbleDeg = 180.0f - (clamped * 180.0f);
  int bubbleRadius = 10;
  int bubbleCx = x + (w / 2);
  int bubbleCy = y + 10;

  drawArcLine(s, bubbleCx, bubbleCy, bubbleRadius, 180.0f, 0.0f, TFT_WHITE);
  drawArcLine(s, bubbleCx, bubbleCy, bubbleRadius - 1, 180.0f, 0.0f, TFT_WHITE);

  int bubbleX = polarX(bubbleCx, bubbleRadius - 3, bubbleDeg);
  int bubbleY = polarY(bubbleCy, bubbleRadius - 3, bubbleDeg);
  s.fillCircle(bubbleX, bubbleY, 3, TFT_WHITE);

  if (frontView) {
    drawTruckFrontIcon(s, x + 4, y - 5);
  } else {
    drawTruckSideIcon(s, x + 2, y - 5);
  }
}

void renderGaugeScreen(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);
  drawDashboardShell(s);

  bool lowFuelActive = dashboardFuelLevel <= 0.2f;
  drawFuelGauge(s, 20, 40, 17, dashboardFuelLevel);
  drawHeadlightIndicator(s, 37, 7, dashboardHeadlightsOn);

  drawTurnSignal(s, 92, 47, true, warningOn);
  drawTurnSignal(s, 126, 47, false, warningOn);
  drawGearColumn(s, 118, 74, dashboardGearIndex);

  drawTiltWidget(s, 145, 4, 24, false, pitchDeg);
  drawTiltWidget(s, 177, 4, 24, true, rollDeg);

  drawTallCircularGauge(
    s,
    66,
    100,
    45,
    0.0f,
    8.0f,
    dashboardRpmK,
    "RPM"
  );
  drawTallCircularGauge(
    s,
    167,
    100,
    45,
    0.0f,
    80.0f,
    dashboardMph,
    "MPH"
  );

  drawTemperatureGauge(s, 220, 40, 17, environmentTempC, bmeAvailable);
  drawOdometer(s, 25, 118, dashboardOdometer);
  drawLowFuelIndicator(s, 192, 118, lowFuelActive);
}
