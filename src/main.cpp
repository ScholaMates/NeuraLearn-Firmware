#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>

#include "ui_renderer.h"
#include "logic_fn.h"

#define FONT_FILENAME "DejaVuSansMono-Bold-40"

TFT_eSPI tft = TFT_eSPI();
QueueHandle_t eventQueue;

AppConfig globalConfig = {
    .volume = 80,
    .defaultState = SLEEPING,
    .isMuted = false
};

void setup() {
  Serial.begin(115200);

 globalConfig.defaultState = LISTENING;
 eventQueue = xQueueCreate(10, sizeof(SystemEvent)); 

  if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
  }
  Serial.println("LittleFS Mounted Successfully.");

  tft_init(tft, FONT_FILENAME);

  drawCurrentFace(tft);
}

void loop() {

}

