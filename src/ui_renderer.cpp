#include <SPI.h>
#include <LittleFS.h>
#include "icons_data.h"
#include "ui_renderer.h"
#include "types.h"



const char* getFaceString(DeviceState state) {
  switch (state) {
    case SLEEPING:
      return "(ᴗ˳ᴗ)ᶻ𝗓𐰁";        // Screen off / Low power
    case AWAKENING:
      return "(˶˃ ᵕ ˂˶)";       // Just turned on
    case LISTENING:
      return "(⸝⸝ •ᴗ•⸝⸝) ၊၊||၊"; // Waiting for user voice
    case THINKING:
      return "( ╹ -╹)?";        // Processing / Talking to server
    case SPEAKING:
      return "(•0•)🎤︎︎";         // Replying
    case HAPPY:
      return "( ˶ᵔ ᵕ ᵔ˶)";       // General positive interaction
    case POMODORO_FOCUS:
      return "(╭ರ_•́) ⌕";        // Serious study time
    case SAD_ERROR:
      return "(⁠╥⁠﹏⁠╥⁠) ⚠︎";      // WiFi failed or API error
    case CONFUSED:
      return "(·•᷄ࡇ•᷅ )";       // Didn't understand command
    case SUCCESS:
      return "ദ്ദി◝ ⩊ ◜.ᐟ";       // Task completed / WiFi Connected
    case LOW_BATTERY:
      return "( ꩜ ᯅ ꩜;)⁭ᵎᵎ";     // Needs charging
    case SASSY:
      return "(≖. ≖\")";        // Smart aleck answer / Smug
    case WAITING:
      return "( •̯́ ₃ •̯̀)…";       // Idle / Chilling
    case LOVE:
      return "₍₍⚞(˶>ᗜ<˶)⚟⁾⁾";    // Good job / Grateful
    case SURPRISED:
      return "(˶°ㅁ°)!!";        // Shocked
    case LAUGHING:
      return "ꉂ(˵˃ ᗜ ˂˵)";       // Laughing
    case CHEERING:
      return "(੭˃ᴗ˂)੭";         // Cheering
    case DEBUGGING:
      return "(#__#)";           // Legacy debug face
    default:
      return "(☓‿‿☓)";           // Fallback for unknown states
  }
}

void drawCurrentFace(TFT_eSPI& tft, GlobalState& state) {
  const char* face = getFaceString(state.mood);
  
  tft.setTextColor(TFT_WHITE);

  tft.drawString(face, 72, 135);
}

void drawBackground(TFT_eSPI& tft) {
    fs::File f = LittleFS.open("/bg.bin", "r");
    if (!f || f.size() == 0) return;
    size_t len = f.size();
    
    uint16_t* bgBuffer = (uint16_t*) ps_malloc(len);
    Serial.println("Background buffer allocated.");
    
    if (bgBuffer) {
        f.read((uint8_t*)bgBuffer, len);
        f.close();
        
        tft.pushImage(10, 22, 460, 276, bgBuffer);
        
        Serial.println("Background drawn to TFT.");
        free(bgBuffer); 
    }
}

void tft_init(TFT_eSPI& tft, String FONT_FILENAME) { 
 
  tft.init();
  Serial.println("TFT Initialized.");
  Serial.flush();

  tft.fillScreen(0x0);
  tft.setRotation(-1);

  drawBackground(tft);
  Serial.println("Background initialized.");
  
  tft.setTextDatum(MC_DATUM);
  Serial.println("TFT Text Datum Set.");

  tft.loadFont(FONT_FILENAME, LittleFS);

  if (tft.fontLoaded) {
    Serial.println("Smooth Font loaded successfully!");
  } else {
    Serial.println("FATAL: Failed to load font file " + FONT_FILENAME);
    tft.setTextFont(4);
  }
}

void render_battery_icon(TFT_eSPI& tft, GlobalState& state) {
    switch (state.batteryLevel) {
        case 0 ... 10:
            tft.pushImage(409, 22, 24, 45, image_Battery_Dead_pixels);
            break;
        case 11 ... 30:
            tft.pushImage(409, 20, 24, 16, image_Battery_1_pixels);
            break;
        case 31 ... 60:
            tft.pushImage(409, 20, 24, 16, image_Battery_2_pixels);
            break;
        case 61 ... 90:
            tft.pushImage(409, 20, 24, 16, image_Battery_4_pixels);
            break;
        case 91 ... 100:
            tft.pushImage(409, 22, 24, 45, image_Battery_full_pixels);
            break;
        default:
            tft.pushImage(409, 22, 24, 45, image_Battery_Dead_pixels);
            break;
    }
}

void uiTask(void *pvParameters) {
  TFT_eSPI tft = *(TFT_eSPI*)pvParameters;
    Serial.println("UI Task Started.");
    if (pvParameters == NULL) {
        Serial.println("UI Task: pvParameters is NULL");
        vTaskDelete(NULL);
    }
    SystemEvent msg;
    while (true) {
      render_battery_icon(tft, state);
      drawCurrentFace(tft, state);
      Serial.println("UI Task waiting for events...");
        if (xQueueReceive(eventQueue, &msg, portMAX_DELAY)) {
            Serial.println("UI Task received event.");
            switch(msg.type) {
                case UPDATE_FACE_MOOD:
                    drawCurrentFace(*(TFT_eSPI*)pvParameters, state);
                    break;
                case PLAY_AUDIO_CHUNK:
                    break;
                case EVENT_CAMERA_TRIGGER:
                    break;
                case TEST_EVENT:
                    Serial.println(String("UI Task received test event: ") + msg.stringData);
                    break;
                case WAKE_WORD_DETECTED:
                    state.mood = LISTENING;
                    Serial.println("UI Task: Wake word detected.");
                    break;
                default:
                    Serial.println("UI Task: Unknown event type received.");
                    break;
            }
        }
    }
}
