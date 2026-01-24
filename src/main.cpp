#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>

#include "ui_renderer.h"

#define FONT_FILENAME "DejaVuSansMono-Bold-40"

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
  }
  Serial.println("LittleFS Mounted Successfully.");

  tft_init(tft);

  drawCurrentFace(tft);
}

void loop() {

}

