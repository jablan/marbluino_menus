#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiredDevice.h>
#include <RegisterBasedWiredDevice.h>
#include <Accelerometer.h>
#include <AccelerometerMMA8451.h>

#define DISPLAY_CS_PIN 10
#define DISPLAY_DC_PIN 9
#define DISPLAY_RS_PIN 8

#define KEY_NONE 0
#define KEY_DOWN 1
#define KEY_UP 2
#define KEY_LEFT 3
#define KEY_RIGHT 4

#define SCREEN_MENU 0
#define SCREEN_CONTRAST 1
#define SCREEN_INFO 2

#define CONTRAST_MIN 0
#define CONTRAST_MAX 20
#define CONTRAST_STEPS 5
#define CONTRAST_STEP (CONTRAST_MAX - CONTRAST_MIN) / CONTRAST_STEPS
#define CONTRAST_STEP_WIDTH 20

U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RS_PIN);
AccelerometerMMA8451 acc(0);

volatile bool gotInterrupt = false;
uint8_t max_x, max_y;

int8_t selectedItem = 0;
uint8_t itemCount = 5;
uint8_t contrast = (CONTRAST_MAX - CONTRAST_MIN) / 2;
uint8_t incomingKey = KEY_NONE;
uint8_t activeScreen = SCREEN_MENU;

char *mainMenu[] = {
  "First item",
  "Second item",
  "Third item",
  "Set contrast",
  "About"
};

void drawMenu() {
  u8g2.clearBuffer();
  const char *title = "Marbluino Menus";
  uint8_t strWidth = u8g2.getStrWidth(title);
  u8g2.drawStr((max_x - strWidth) / 2, 5, title);
  
  for (uint8_t i = 0; i < itemCount; i++) {
    if (i == selectedItem) {
      // inverted text
      u8g2.drawBox(0, 10 + i*7 + 1, 83, 7);
    }
    u8g2.drawStr(1, 10 + (i+1)*7, mainMenu[i]);
  }
  u8g2.sendBuffer();
}

void drawContrastScreen() {
  u8g2.clearBuffer();
  const char *title = "Set contrast";
  uint8_t strWidth = u8g2.getStrWidth(title);
  u8g2.drawStr((max_x - strWidth) / 2, 5, title);

  uint8_t stepHeight = 40 / CONTRAST_STEPS;
  uint8_t leftX = (max_x - CONTRAST_STEP_WIDTH) / 2;
  for (uint8_t i = 0; i < CONTRAST_STEPS; i++) {
    uint8_t topY = max_y - ((i + 1) * stepHeight);
    u8g2.drawFrame(leftX, topY, CONTRAST_STEP_WIDTH, stepHeight);
    if (contrast > CONTRAST_MIN + CONTRAST_STEP * i) {
      u8g2.drawBox(leftX + 2, topY + 2, CONTRAST_STEP_WIDTH - 4, stepHeight - 4);
    }
  }

  u8g2.sendBuffer();
}

void drawInfoScreen() {
  u8g2.clearBuffer();
  uint8_t strWidth = u8g2.getStrWidth(mainMenu[selectedItem]);
  u8g2.drawStr((max_x - strWidth) / 2, 5, mainMenu[selectedItem]);

  u8g2.sendBuffer();
}

void processPulseData(AccelerometerMMA8451::PULSE_SRCbits pulseSource) {
    Serial.print("x: ");
    Serial.print(pulseSource.AXX, HEX);
    Serial.print(" px: ");
    Serial.println(pulseSource.POLX);
    Serial.print("y: ");
    Serial.print(pulseSource.AXY, HEX);
    Serial.print(" py: ");
    Serial.println(pulseSource.POLY);
    Serial.print("z: ");
    Serial.print(pulseSource.AXZ, HEX);   
    Serial.print(" pz: ");
    Serial.println(pulseSource.POLZ);
    if (pulseSource.AXX) {
      if (pulseSource.POLX) {
        incomingKey = KEY_DOWN;
      } else {
        incomingKey = KEY_UP;
      }
    } else if (pulseSource.AXY) {
      if (pulseSource.POLY) {
        incomingKey = KEY_LEFT;
      } else {
        incomingKey = KEY_RIGHT;
      }
    }
}

void enterMenu() {
  if (strcmp(mainMenu[selectedItem], "Set contrast") == 0) {
    activeScreen = SCREEN_CONTRAST;
    drawContrastScreen();
  } else {
    activeScreen = SCREEN_INFO;
    drawInfoScreen();
  }
}

void handleMenuScreen() {
  switch (incomingKey) {
    case KEY_NONE: return;
    case KEY_DOWN:
      selectedItem++;
      if (selectedItem >= itemCount) selectedItem = 0;
      drawMenu();
      break;
    case KEY_UP:
      selectedItem--;
      if (selectedItem < 0) selectedItem = itemCount - 1;
      drawMenu();
      break;
    case KEY_RIGHT:
      enterMenu();
      break;
  }
  incomingKey = KEY_NONE;
}

void handleContrastScreen() {
  switch (incomingKey) {
    case KEY_NONE: return;
    case KEY_UP:
      if (contrast + CONTRAST_STEP < CONTRAST_MAX) {
        Serial.println("Increasing contrast");
        contrast += CONTRAST_STEP;
        u8g2.setContrast(contrast);
      }
      drawContrastScreen();
      break;
    case KEY_DOWN:
      if (contrast - CONTRAST_STEP > CONTRAST_MIN) {
        Serial.println("Decreasing contrast");
        contrast -= CONTRAST_STEP;
        u8g2.setContrast(contrast);
      }
      drawContrastScreen();
      break;
    case KEY_LEFT:
      activeScreen = SCREEN_MENU;
      drawMenu();
      break;
  }
  incomingKey = KEY_NONE;
}

void handleInfoScreen() {
  switch (incomingKey) {
    case KEY_NONE: return;
    case KEY_LEFT:
      activeScreen = SCREEN_MENU;
      drawMenu();
      break;
  }
  incomingKey = KEY_NONE;
}

void setupScreen() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_baby_tr);
  u8g2.setFontMode(1);
  u8g2.setDrawColor(2);
  u8g2.setContrast(contrast);
  max_x = u8g2.getDisplayWidth();
  max_y = u8g2.getDisplayHeight();
}

void setupPulseDetection() {
  acc.standby();
  acc.setOutputDataRate(AccelerometerMMA8451::ODR_800HZ_1_25_MS);
  acc.disableInterrupt(AccelerometerMMA8451::INT_ALL);
  acc.setDynamicRange(AccelerometerMMA8451::DR_2G);
  acc.setOversamplingMode(AccelerometerMMA8451::HI_RESOLUTION_MODS);

  acc.setPulseDetection(false, AccelerometerMMA8451::PULSE_CFG_XSPEFE | AccelerometerMMA8451::PULSE_CFG_YSPEFE, false);
  acc.setPulseThreshold(0x40, 0x40, 0x40);
  acc.setPulseFirstTimer(0x10);
  acc.setPulseLatency(0x28);

  acc.enableInterrupt(AccelerometerMMA8451::INT_PULSE);
  acc.routeInterruptToInt2(AccelerometerMMA8451::INT_PULSE);

  acc.activate();

  attachInterrupt(digitalPinToInterrupt(2), isr, FALLING);
}

void handleInterrupt() {
  AccelerometerMMA8451::INT_SOURCEbits source;

  source.value = acc.readRegister(AccelerometerMMA8451::INT_SOURCE);
  
  Serial.print("source: ");
  Serial.println(source.value, HEX);

  if (source.SRC_PULSE) {
    AccelerometerMMA8451::PULSE_SRCbits pulseSource;

    pulseSource.value = acc.readRegister(AccelerometerMMA8451::PULSE_SRC);

    processPulseData(pulseSource);
  }
}

void isr() {
  gotInterrupt = true;
}

void setup() {
  Serial.begin(115200);
  setupScreen();
  setupPulseDetection();
  drawMenu();
}

void loop() {
  if (gotInterrupt) {
    gotInterrupt = false;
    handleInterrupt();
  }
  switch (activeScreen) {
    case SCREEN_MENU:
      handleMenuScreen();
      break;
    case SCREEN_CONTRAST:
      handleContrastScreen();
      break;
    case SCREEN_INFO:
      handleInfoScreen();
      break;
  }
}
