#pragma once

#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>

enum DeviceState;

void drawCurrentFace(TFT_eSPI& tft);
void drawHeader(TFT_eSPI& tft);
void drawBackground(TFT_eSPI& tft);
void tft_init(TFT_eSPI& tft);

const char* getFaceString(DeviceState state);