void drawWarningLight(TFT_eSprite &s, bool on) {
  if (!on) {
    return;
  }

  int tx = 200;
  int ty = 26;
  int size = 16;

  int x1 = tx;
  int y1 = ty - size;
  int x2 = tx - size;
  int y2 = ty + size;
  int x3 = tx + size;
  int y3 = ty + size;

  s.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
  s.drawTriangle(x1, y1, x2, y2, x3, y3, TFT_RED);

  s.fillRect(tx - 2, ty - 6, 4, 12, TFT_BLACK);
  s.fillCircle(tx, ty + 10, 2, TFT_BLACK);
}

void drawGaugeFrame(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);

  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.setTextDatum(MC_DATUM);
  s.drawString("DASH", SCREEN_W / 2, 12, 2);

  for (float deg = START_DEG; deg <= END_DEG; deg += 2.0f) {
    int x1 = polarX(CX, R_INNER, deg);
    int y1 = polarY(CY, R_INNER, deg);
    int x2 = polarX(CX, R_OUTER, deg);
    int y2 = polarY(CY, R_OUTER, deg);
    s.drawLine(x1, y1, x2, y2, TFT_DARKGREY);
  }

  const int tickCount = 21;
  for (int i = 0; i < tickCount; i++) {
    float p = (float)i / (float)(tickCount - 1);
    float deg = START_DEG + (END_DEG - START_DEG) * p;

    bool major = (i % 5 == 0);
    int r1 = major ? (R_OUTER - 12) : (R_OUTER - 7);
    int r2 = R_OUTER;

    int x1 = polarX(CX, r1, deg);
    int y1 = polarY(CY, r1, deg);
    int x2 = polarX(CX, r2, deg);
    int y2 = polarY(CY, r2, deg);

    s.drawLine(x1, y1, x2, y2, major ? TFT_WHITE : TFT_LIGHTGREY);
  }

  s.setTextDatum(MC_DATUM);
  s.drawString(
    "0",
    polarX(CX, R_OUTER - 18, START_DEG),
    polarY(CY, R_OUTER - 18, START_DEG) - 2,
    2
  );

  s.drawString(
    "100",
    polarX(CX, R_OUTER - 22, END_DEG),
    polarY(CY, R_OUTER - 22, END_DEG) - 2,
    2
  );

  drawWarningLight(s, warningOn);
}

void drawNeedle(TFT_eSprite &s, float value) {
  float deg = START_DEG + (END_DEG - START_DEG) * value;
  int x = polarX(CX, R_NEEDLE, deg);
  int y = polarY(CY, R_NEEDLE, deg);

  s.drawLine(CX, CY, x, y, TFT_RED);
  s.drawLine(CX - 1, CY, x, y, TFT_RED);

  s.fillCircle(CX, CY, 6, TFT_WHITE);
  s.fillCircle(CX, CY, 3, TFT_RED);

  int valueInt = (int)(value * 100.0f);
  s.setTextColor(TFT_CYAN, TFT_BLACK);
  s.setTextDatum(MC_DATUM);
  s.drawString(String(valueInt), SCREEN_W / 2, 42, 4);
}

void renderGaugeScreen(TFT_eSprite &s) {
  drawGaugeFrame(s);
  drawNeedle(s, needleValue);
}

void drawBatteryIcon(TFT_eSprite &s, int x, int y, int w, int h, int percent) {
  s.drawRect(x, y, w, h, TFT_WHITE);
  s.fillRect(x + w, y + h / 3, 4, h / 3, TFT_WHITE);

  int innerPadding = 2;
  int fillW = (w - innerPadding * 2 - 2) * percent / 100;
  uint16_t fillColor = TFT_GREEN;
  if (percent < 50) {
    fillColor = TFT_YELLOW;
  }
  if (percent < 20) {
    fillColor = TFT_RED;
  }

  s.fillRect(x + innerPadding, y + innerPadding, w - innerPadding * 2, h - innerPadding * 2, TFT_BLACK);
  s.fillRect(x + innerPadding + 1, y + innerPadding + 1, fillW, h - innerPadding * 2 - 2, fillColor);
}

void drawDistanceIcon(TFT_eSprite &s, int x, int y) {
  s.drawRoundRect(x, y, 28, 20, 3, TFT_WHITE);
  s.drawLine(x + 14, y + 3, x + 12, y + 17, TFT_YELLOW);
  s.drawLine(x + 15, y + 3, x + 17, y + 17, TFT_YELLOW);
  s.drawLine(x + 6, y + 3, x + 3, y + 17, TFT_DARKGREY);
  s.drawLine(x + 22, y + 3, x + 25, y + 17, TFT_DARKGREY);
}

void drawGyroIcon(TFT_eSprite &s, int x, int y) {
  s.drawCircle(x + 14, y + 14, 12, TFT_WHITE);
  s.drawLine(x + 14, y + 2, x + 14, y + 26, TFT_DARKGREY);
  s.drawLine(x + 2, y + 14, x + 26, y + 14, TFT_DARKGREY);
  s.fillCircle(x + 14, y + 14, 3, TFT_CYAN);
}

void renderStatusScreen(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);

  s.setTextDatum(TL_DATUM);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString("STATUS", 8, 6, 2);

  s.drawRoundRect(8, 28, 68, 48, 6, TFT_DARKGREY);
  drawBatteryIcon(s, 16, 38, 28, 14, batteryPercent);
  s.setTextColor(TFT_GREEN, TFT_BLACK);
  s.drawString(String(batteryPercent) + "%", 16, 58, 2);

  s.drawRoundRect(86, 28, 68, 48, 6, TFT_DARKGREY);
  drawDistanceIcon(s, 94, 36);
  s.setTextColor(TFT_CYAN, TFT_BLACK);
  s.drawString(String(distanceMiles, 1) + " mi", 92, 58, 2);

  s.drawRoundRect(164, 28, 68, 48, 6, TFT_DARKGREY);
  drawGyroIcon(s, 182, 34);
  s.setTextColor(TFT_ORANGE, TFT_BLACK);
  s.drawString("GYRO", 178, 58, 2);

  s.drawRoundRect(8, 86, 224, 40, 6, TFT_DARKGREY);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString("X:", 16, 96, 2);
  s.drawString(String(gx, 1), 40, 96, 2);

  s.drawString("Y:", 88, 96, 2);
  s.drawString(String(gy, 1), 112, 96, 2);

  s.drawString("Z:", 160, 96, 2);
  s.drawString(String(gz, 1), 184, 96, 2);
}

void drawSegmentBar(
  TFT_eSprite &s,
  int x,
  int y,
  int segments,
  int activeSegments,
  int segmentW,
  int segmentH,
  uint16_t activeColor,
  uint16_t inactiveColor
) {
  const int gap = 3;

  for (int i = 0; i < segments; i++) {
    int segX = x + (i * (segmentW + gap));
    uint16_t fillColor = i < activeSegments ? activeColor : inactiveColor;
    s.fillRoundRect(segX, y, segmentW, segmentH, 2, fillColor);
  }
}

void drawEnvironmentTile(
  TFT_eSprite &s,
  int x,
  int y,
  int w,
  int h,
  const char *label,
  const String &value,
  uint16_t accentColor
) {
  uint16_t tileFill = rgb565(16, 22, 28);
  uint16_t detailLine = rgb565(40, 49, 58);

  s.fillRoundRect(x, y, w, h, 8, tileFill);
  s.drawRoundRect(x, y, w, h, 8, accentColor);
  s.drawFastHLine(x + 10, y + 14, w - 20, detailLine);

  s.setTextDatum(TL_DATUM);
  s.setTextColor(rgb565(141, 152, 164), tileFill);
  s.drawString(label, x + 10, y + 4, 1);

  s.setTextColor(TFT_WHITE, tileFill);
  s.drawString(value, x + 10, y + 17, 2);
}

void renderEnvironmentScreen(TFT_eSprite &s) {
  uint16_t bg = rgb565(6, 9, 14);
  uint16_t headerFill = rgb565(12, 17, 23);
  uint16_t headerLine = rgb565(45, 56, 68);
  uint16_t cardFill = rgb565(16, 19, 24);
  uint16_t cardLine = rgb565(67, 75, 85);
  uint16_t gridLine = rgb565(18, 26, 34);
  uint16_t amber = rgb565(255, 171, 66);
  uint16_t cyan = rgb565(88, 217, 255);
  uint16_t green = rgb565(77, 214, 146);
  uint16_t yellow = rgb565(255, 206, 82);
  uint16_t red = rgb565(219, 77, 62);

  s.fillSprite(bg);

  for (int y = 18; y < SCREEN_H; y += 18) {
    s.drawFastHLine(8, y, SCREEN_W - 16, gridLine);
  }

  s.fillRoundRect(8, 6, 224, 18, 8, headerFill);
  s.drawRoundRect(8, 6, 224, 18, 8, headerLine);

  s.setTextDatum(TL_DATUM);
  s.setTextColor(TFT_WHITE, headerFill);
  s.drawString("AMBIENT", 16, 10, 2);
  s.setTextColor(rgb565(148, 160, 172), headerFill);
  s.drawString("BME280", 91, 11, 1);

  uint16_t badgeFill = bmeAvailable ? rgb565(23, 95, 69) : rgb565(96, 34, 26);
  String badgeText = bmeAvailable ? bmeAddressLabel() : String("OFFLINE");
  s.fillRoundRect(176, 8, 48, 14, 6, badgeFill);
  s.setTextDatum(MC_DATUM);
  s.setTextColor(TFT_WHITE, badgeFill);
  s.drawString(badgeText, 200, 15, 1);
  s.setTextDatum(TL_DATUM);

  if (!bmeAvailable) {
    s.fillRoundRect(10, 30, 220, 95, 12, cardFill);
    s.drawRoundRect(10, 30, 220, 95, 12, red);

    s.setTextColor(red, cardFill);
    s.drawString("BME280 UNAVAILABLE", 22, 48, 2);
    s.setTextColor(TFT_WHITE, cardFill);
    s.drawString("Check power and I2C", 22, 74, 2);
    s.drawString("Tried 0x76 and 0x77", 22, 98, 2);
    return;
  }

  s.fillRoundRect(10, 30, 106, 95, 12, cardFill);
  s.drawRoundRect(10, 30, 106, 95, 12, cardLine);
  s.drawFastHLine(20, 49, 86, rgb565(46, 54, 62));

  s.setTextColor(rgb565(150, 160, 172), cardFill);
  s.drawString("CABIN TEMP", 20, 38, 1);

  s.setTextColor(amber, cardFill);
  s.drawString(String(environmentTempC, 1), 18, 55, 4);
  s.setTextColor(rgb565(255, 219, 133), cardFill);
  s.drawString("C", 85, 64, 2);

  String thermalState = "READY";
  if (environmentTempC < 18.0f) {
    thermalState = "COOL";
  } else if (environmentTempC > 27.0f) {
    thermalState = "WARM";
  }

  s.setTextColor(TFT_WHITE, cardFill);
  s.drawString(thermalState, 20, 91, 2);

  int tempSegments = (int)round(clamp01((environmentTempC + 10.0f) / 50.0f) * 8.0f);
  if (tempSegments < 0) {
    tempSegments = 0;
  }
  if (tempSegments > 8) {
    tempSegments = 8;
  }

  drawSegmentBar(
    s,
    20,
    108,
    8,
    tempSegments,
    8,
    8,
    amber,
    rgb565(43, 48, 54)
  );

  drawEnvironmentTile(
    s,
    126,
    30,
    104,
    29,
    "HUMIDITY",
    String(environmentHumidity, 1) + " %",
    cyan
  );
  drawEnvironmentTile(
    s,
    126,
    63,
    104,
    29,
    "PRESSURE",
    String(environmentPressureHpa, 0) + " hPa",
    green
  );
  drawEnvironmentTile(
    s,
    126,
    96,
    104,
    29,
    "ALTITUDE",
    String(environmentAltitudeM, 0) + " m",
    yellow
  );
}

void drawTiltBubble(TFT_eSprite &s, int cx, int cy, int radius, float pitch, float roll) {
  s.drawCircle(cx, cy, radius, TFT_WHITE);
  s.drawCircle(cx, cy, radius / 2, TFT_DARKGREY);
  s.drawLine(cx - radius, cy, cx + radius, cy, TFT_DARKGREY);
  s.drawLine(cx, cy - radius, cx, cy + radius, TFT_DARKGREY);

  float maxTilt = 30.0f;
  int bubbleX = cx + (int)((roll / maxTilt) * (radius - 8));
  int bubbleY = cy + (int)((pitch / maxTilt) * (radius - 8));

  if (bubbleX < cx - radius + 8) {
    bubbleX = cx - radius + 8;
  }
  if (bubbleX > cx + radius - 8) {
    bubbleX = cx + radius - 8;
  }
  if (bubbleY < cy - radius + 8) {
    bubbleY = cy - radius + 8;
  }
  if (bubbleY > cy + radius - 8) {
    bubbleY = cy + radius - 8;
  }

  s.fillCircle(bubbleX, bubbleY, 6, TFT_CYAN);

  if (showTiltAxisLabels) {
    String leftLabel = invertRollAxis ? "R+" : "R-";
    String rightLabel = invertRollAxis ? "R-" : "R+";
    String topLabel = invertPitchAxis ? "P+" : "P-";
    String bottomLabel = invertPitchAxis ? "P-" : "P+";

    s.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    s.setTextDatum(MC_DATUM);
    s.drawString(topLabel, cx, cy - radius - 10, 1);
    s.drawString(bottomLabel, cx, cy + radius + 10, 1);
    s.drawString(leftLabel, cx - radius - 14, cy, 1);
    s.drawString(rightLabel, cx + radius + 14, cy, 1);
  }
}

void renderTiltScreen(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);

  s.setTextDatum(TL_DATUM);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString("TILT", 8, 6, 2);

  drawTiltBubble(s, 70, 72, 38, pitchDeg, rollDeg);

  s.drawRoundRect(130, 20, 100, 90, 6, TFT_DARKGREY);

  s.setTextColor(TFT_GREEN, TFT_BLACK);
  s.drawString("Pitch:", 140, 32, 2);
  s.drawString(String(pitchDeg, 1) + " deg", 140, 50, 2);

  s.setTextColor(TFT_CYAN, TFT_BLACK);
  s.drawString("Roll:", 140, 72, 2);
  s.drawString(String(rollDeg, 1) + " deg", 140, 90, 2);
}
