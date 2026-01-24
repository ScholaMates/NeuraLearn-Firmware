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

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
  }
  Serial.println("LittleFS Mounted Successfully.");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);

  tft.loadFont(FONT_FILENAME, LittleFS);

  // Now, check if the font was loaded successfully.
  if (tft.fontLoaded) {
    Serial.println("Smooth Font loaded successfully!");
  } else {
    Serial.println("FATAL: Failed to load font file " FONT_FILENAME);
    // As a backup, fall back to a built-in legacy font
    tft.setTextFont(4);
  }

  // Initial face draw
  drawCurrentFace();
}

// A variable to track time for the non-blocking delay
unsigned long previousMillis = 0;
const long interval = 5000; // 5 seconds

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int nextState = (int)currentState + 1;
    if (nextState > DEBUGGING) {
      nextState = SLEEPING;
    }
    
    currentState = (DeviceState)nextState;
    
    drawCurrentFace();
  }
}

