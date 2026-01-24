#pragma once

#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include "icons_data.h"
#include "ui_renderer.h"

enum DeviceState {
  SLEEPING,
  AWAKENING,
  LISTENING,
  THINKING,
  SPEAKING,
  HAPPY,
  POMODORO_FOCUS,
  SAD_ERROR,
  DEBUGGING
};

DeviceState currentState = SLEEPING;

const char* getFaceString(DeviceState state) {
  switch (state) {
    case SLEEPING:
      return "(⇀‿‿↼)";
    case AWAKENING:
      return "(≖‿‿≖)";
    case LISTENING:
      return "( ⚆_⚆)";
    case THINKING:
      return "(✜‿‿✜)";
    case SPEAKING:
      return "(◕‿‿◕)";
    case HAPPY:
      return "(•‿‿•)";
    case POMODORO_FOCUS:
      return "(⌐■_■)";
    case SAD_ERROR:
      return "(╥☁╥ )";
    case DEBUGGING:
      return "(#__#)";
    default:
      return "(☓‿‿☓)"; // A "broken" face for any unknown state
  }
}

void drawCurrentFace(TFT_eSPI& tft) {
  const char* face = getFaceString(currentState);

  tft.fillRect(0, 50, 320, 140, TFT_BLACK);

  tft.setTextColor(TFT_WHITE);

  tft.drawString(face, 76, 102);
}

void drawHeader(TFT_eSPI& tft) {
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Device State Display", 160, 25);
}

void drawBackground(TFT_eSPI& tft) {
    fs::File f = LittleFS.open("/bg.bin", "r");
    size_t len = f.size();
    
    // Allocate buffer in PSRAM (not internal RAM)
    uint16_t* bgBuffer = (uint16_t*) ps_malloc(len);
    
    if (bgBuffer) {
        f.read((uint8_t*)bgBuffer, len);
        f.close();
        
        tft.pushImage(0, 0, 320, 240, bgBuffer);
        
        free(bgBuffer); 
    }
}

void tft_init(TFT_eSPI& tft) { 
 
  tft.init();
  Serial.println("TFT Initialized.");

  tft.fillScreen(0x0);
  tft.setRotation(0);

  drawBackground(tft);
  Serial.println("Background initialized.");
  
  tft.setTextDatum(MC_DATUM);
  Serial.println("TFT Text Datum Set.");

  tft.loadFont(FONT_FILENAME, LittleFS);

  if (tft.fontLoaded) {
    Serial.println("Smooth Font loaded successfully!");
  } else {
    Serial.println("FATAL: Failed to load font file " FONT_FILENAME);
    tft.setTextFont(4);
  }
}

