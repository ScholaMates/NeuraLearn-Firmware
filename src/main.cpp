#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>

#define FONT_FILENAME "DejaVuSansMono-Bold-40"

TFT_eSPI tft = TFT_eSPI();

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

void drawCurrentFace() {
  const char* face = getFaceString(currentState);

  tft.fillRect(0, 50, 320, 140, TFT_BLACK);

  tft.setTextColor(TFT_WHITE);

  tft.drawString(face, 76, 102);
}

void drawHeader() {
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Device State Display", 160, 25);
}

void drawBackground() {
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

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
  }
  Serial.println("LittleFS Mounted Successfully.");

  tft.init();
  Serial.println("TFT Initialized.");

  tft.fillScreen(0x0);
  tft.setRotation(0);

  drawBackground();
  Serial.println("Background initialized.");
  
  tft.setTextDatum(MC_DATUM);
  Serial.println("TFT Text Datum Set.");

  // Loading Smooth Font from LittleFS
  tft.loadFont(FONT_FILENAME, LittleFS);

  if (tft.fontLoaded) {
    Serial.println("Smooth Font loaded successfully!");
  } else {
    Serial.println("FATAL: Failed to load font file " FONT_FILENAME);
    tft.setTextFont(4);
  }

  drawCurrentFace();
}

void loop() {

}

