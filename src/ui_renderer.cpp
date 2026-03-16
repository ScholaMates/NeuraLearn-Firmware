#include <SPI.h>
#include <LittleFS.h>
#include "icons_data.h"
#include "faces_data.h" 
#include "ui_renderer.h"
#include "types.h"

// --- Display Configuration ---
#define TFT_WIDTH 480
#define TFT_HEIGHT 320
#define BG_X 10
#define BG_Y 22
#define BG_WIDTH 460
#define BG_HEIGHT 276
#define EXPECTED_BG_SIZE (BG_WIDTH * BG_HEIGHT * 2) 

#define TFT_MAUVE 0xCD3E 

static DeviceState lastMood = (DeviceState)-1;

const char* getStateName(DeviceState state) {
  switch (state) {
    case SLEEPING:       return "Sleeping";
    case AWAKENING:      return "Booting Up";
    case LISTENING:      return "Listening...";
    case THINKING:       return "Thinking...";
    case SPEAKING:       return "Speaking";
    case HAPPY:          return "Happy";
    case POMODORO_FOCUS: return "Focus Mode";
    case SAD_ERROR:      return "System Error";
    case CONFUSED:       return "Confused";
    case SUCCESS:        return "Connected";
    case LOW_BATTERY:    return "Low Battery";
    case SASSY:          return "Sassy";
    case WAITING:        return "Idle";
    case LOVE:           return "Grateful";
    case SURPRISED:      return "Surprised";
    case LAUGHING:       return "Laughing";
    case CHEERING:       return "Cheering";
    case DEBUGGING:      return "Debug Mode";
    default:             return "Unknown";
  }
}

void drawCurrentFaceAndState(TFT_eSPI& tft, GlobalState& state) {
    // Only redraw if the mood actually changed
    if (state.mood == lastMood) return;

    // Calculate physical center anchor
    int face_x = (TFT_WIDTH / 2) - (FACE_WIDTH / 2);
    int face_y = (TFT_HEIGHT / 2) - (FACE_HEIGHT / 2);

    // We only wipe the horizontal band where the face/text lives to preserve the background.
    tft.fillRect(0, face_y, TFT_WIDTH, FACE_HEIGHT, TFT_BLACK);

    // Draw the pre-rendered Face
    switch (state.mood) {
        case SLEEPING:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sleeping); break;
        case AWAKENING:      tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_awakening); break;
        case LISTENING:      tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_listening); break;
        case THINKING:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_thinking); break;
        case SPEAKING:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_speaking); break;
        case HAPPY:          tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_happy); break;
        case POMODORO_FOCUS: tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_pomodoro_focus); break;
        case SAD_ERROR:      tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sad_error); break;
        case CONFUSED:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_confused); break;
        case SUCCESS:        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_success); break;
        case LOW_BATTERY:    tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_low_battery); break;
        case SASSY:          tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sassy); break;
        case WAITING:        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_waiting); break;
        case LOVE:           tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_love); break;
        case SURPRISED:      tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_surprised); break;
        case LAUGHING:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_laughing); break;
        case CHEERING:       tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_cheering); break;
        case DEBUGGING:      tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_debugging); break;
        default:             tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_fallback); break;
    }

    // Draw the text state at the bottom of the screen
    tft.setTextColor(TFT_MAUVE, TFT_BLACK); 
    
    // Setting padding to 300 pixels forces the TFT driver to automatically fill
    // the trailing space with black, preventing the "Sticky Paint" ghosting artifact.
    tft.setTextPadding(300); 
    
    String stateText = "State: " + String(getStateName(state.mood));
    tft.drawString(stateText, TFT_WIDTH / 2, TFT_HEIGHT - 35);
    
    // Release the padding lock so it doesn't affect other text draws
    tft.setTextPadding(0); 

    lastMood = state.mood;
}

void drawBackground(TFT_eSPI& tft) {
    fs::File f = LittleFS.open("/bg.bin", "r");
    if (!f || f.size() == 0) {
        Serial.println("Error: [UI] bg.bin not found or empty!");
        if (f) f.close();
        return;
    }
    
    size_t len = f.size();
    
    if (len != EXPECTED_BG_SIZE) {
        Serial.printf("Error: [UI] bg.bin size mismatch! Expected %d bytes, got %d.\n", EXPECTED_BG_SIZE, len);
        f.close();
        return;
    }
    
    uint16_t* bgBuffer = (uint16_t*) ps_malloc(len);
    
    if (bgBuffer) {
        f.read((uint8_t*)bgBuffer, len);
        f.close();
        tft.pushImage(BG_X, BG_Y, BG_WIDTH, BG_HEIGHT, bgBuffer);
        free(bgBuffer); 
    } else {
        Serial.println("Error: [UI] Failed to allocate PSRAM for background!");
        f.close();
    }
}

void renderStaticIcons(TFT_eSPI& tft) {
    // Pushed exactly to the coordinates from your icons_data.h mapping
    
    // Bottom Bar Icons
    tft.pushImage(145, 195, 30, 32, image_Microphone_Enabled_pixels);
    
    // Top Bar Network Icons
    tft.pushImage(227, 20, 15, 16, image_Cellular_data_Disabled_pixels);
    tft.drawBitmap(247, 20, image_wifi_bits, 19, 16, 0xFFFF);
    
    // Static Battery Loading (Using 'Charging' as the permanent placeholder)
    tft.pushImage(271, 21, 24, 16, image_Battery_Charging_pixels);
}

void tft_init(TFT_eSPI& tft, String FONT_FILENAME) { 
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(-1); 
    
    drawBackground(tft);
    renderStaticIcons(tft); // Inject static overlays once
    
    tft.setTextDatum(MC_DATUM); 
    tft.loadFont(FONT_FILENAME, LittleFS);
    if (!tft.fontLoaded) tft.setTextFont(4);
}

void uiTask(void *pvParameters) {
    TFT_eSPI tft = *(TFT_eSPI*)pvParameters;
    Serial.println("Info: [UI] UI Task Started on Core 1.");
    
    if (pvParameters == NULL) vTaskDelete(NULL);
    SystemEvent msg;

    // Force an initial render before dropping into the queue loop
    drawCurrentFaceAndState(tft, state);

    while (true) {
        // Sleep Core 1 until logic_fn.cpp fires an event
        if (xQueueReceive(eventQueue, &msg, portMAX_DELAY)) {
            
            switch(msg.type) {
                case UPDATE_FACE_MOOD:
                    drawCurrentFaceAndState(tft, state);
                    break;
                    
                case EVENT_CAMERA_TRIGGER:
                    if (msg.stringData != nullptr) {
                        tft.setTextColor(TFT_CYAN, TFT_BLACK); 
                        
                        // Hardware padding strictly erases the old numbers ("3" -> "2")
                        // so we don't get overlapping artifacts during the countdown.
                        tft.setTextPadding(200); 
                        
                        tft.drawString(msg.stringData, TFT_WIDTH / 2, TFT_HEIGHT / 2);
                        
                        tft.setTextPadding(0); // Release padding lock
                        lastMood = (DeviceState)-1; // Invalidate cache so face redraws after
                        free(msg.stringData); 
                    }
                    break;
                    
                case WAKE_WORD_DETECTED:
                    state.mood = LISTENING;
                    drawCurrentFaceAndState(tft, state);
                    break;
                    
                default:
                    break;
            }
        }
    }
}