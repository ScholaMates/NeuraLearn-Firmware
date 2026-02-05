#pragma once

#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include "types.h"

void drawCurrentFace(TFT_eSPI& tft);
void drawHeader(TFT_eSPI& tft);
void drawBackground(TFT_eSPI& tft);
void tft_init(TFT_eSPI& tft, String FONT_FILENAME);
void uiTask(void *pvParameters);

extern AppConfig globalConfig;
extern QueueHandle_t eventQueue;
extern SemaphoreHandle_t dataMutex;
extern GlobalState state;

const char* getFaceString(DeviceState state);