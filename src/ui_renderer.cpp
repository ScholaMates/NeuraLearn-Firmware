#include <SPI.h>
#include <LittleFS.h>
#include <time.h>
#include "icons_data.h"
#include "faces_data.h"
#include "ui_renderer.h"
#include "types.h"
#include "config.h"

// Cache the last mood to prevent redundant redraws
static DeviceState lastMood = (DeviceState)-1;

// Utility to convert DeviceState enum to human-readable string
const char *getStateName(DeviceState state)
{
    switch (state)
    {
    case SLEEPING:
        return "Sleeping";
    case AWAKENING:
        return "Booting Up";
    case LISTENING:
        return "Listening...";
    case THINKING:
        return "Thinking...";
    case SPEAKING:
        return "Speaking";
    case HAPPY:
        return "Happy";
    case POMODORO_FOCUS:
        return "Focus Mode";
    case SAD_ERROR:
        return "System Error";
    case CONFUSED:
        return "Confused";
    case SUCCESS:
        return "Connected";
    case LOW_BATTERY:
        return "Low Battery";
    case SASSY:
        return "Sassy";
    case WAITING:
        return "Idle";
    case LOVE:
        return "Grateful";
    case SURPRISED:
        return "Surprised";
    case LAUGHING:
        return "Laughing";
    case CHEERING:
        return "Cheering";
    case DEBUGGING:
        return "Debug Mode";
    default:
        return "Unknown";
    }
}

void drawCurrentFaceAndState(TFT_eSPI &tft, GlobalState &state)
{
    if (state.mood == lastMood)
        return;

    int face_x = (TFT_WIDTH / 2) - (FACE_WIDTH / 2);
    int face_y = (TFT_HEIGHT / 2) - (FACE_HEIGHT / 2);

    // Wipe the specific face block
    tft.fillRect(0, face_y, TFT_WIDTH, FACE_HEIGHT, TFT_BLACK);

    switch (state.mood)
    {
    case SLEEPING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sleeping);
        break;
    case AWAKENING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_awakening);
        break;
    case LISTENING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_listening);
        break;
    case THINKING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_thinking);
        break;
    case SPEAKING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_speaking);
        break;
    case HAPPY:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_happy);
        break;
    case POMODORO_FOCUS:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_pomodoro_focus);
        break;
    case SAD_ERROR:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sad_error);
        break;
    case CONFUSED:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_confused);
        break;
    case SUCCESS:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_success);
        break;
    case LOW_BATTERY:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_low_battery);
        break;
    case SASSY:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_sassy);
        break;
    case WAITING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_waiting);
        break;
    case LOVE:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_love);
        break;
    case SURPRISED:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_surprised);
        break;
    case LAUGHING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_laughing);
        break;
    case CHEERING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_cheering);
        break;
    case DEBUGGING:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_debugging);
        break;
    default:
        tft.pushImage(face_x, face_y, FACE_WIDTH, FACE_HEIGHT, image_face_fallback);
        break;
    }

    tft.setTextColor(TFT_MAUVE, TFT_BLACK);

    // Hardware "Squeegee" Wipe
    //> Instead of setTextPadding, we physically blank the entire horizontal band where the text lives.
    //> This perfectly erases the dropping tails of 'p' or 'g' from previous strings.
    int text_y_center = (TFT_HEIGHT / 2) - (FACE_HEIGHT / 2) - 30;
    tft.fillRect(0, text_y_center - 20, TFT_WIDTH, 40, TFT_BLACK);

    String stateText = "State: " + String(getStateName(state.mood));
    tft.drawString(stateText, TFT_WIDTH / 2, text_y_center);

    lastMood = state.mood;
}

// This function updates the status icons in the top right corner based on the current global state
void updateTelemetryIcons(TFT_eSPI &tft, GlobalState &state)
{

    static bool lastWifi = !state.isConnectedToWifi;
    static bool lastServer = !state.isConnectedToServer;
    static bool lastMic = !state.isListening;
    static bool lastAudio = !state.isPlayingAudio;
    static int lastBatt = -1;
    static int lastVolume = -1;

    // WiFi UI Logic (Anchored perfectly to X:377)
    if (state.isConnectedToWifi != lastWifi)
    {
        if (state.isConnectedToWifi)
        {
            // "Stamp Eraser": Wipe the massive 43x44 disabled bounding box first!
            tft.fillRect(377, 20, 43, 44, TFT_BLACK);
            tft.drawBitmap(377, 20, image_wifi_bits, 19, 16, 0xFFFF);
        }
        else
        {
            tft.pushImage(377, 20, 43, 44, image_Wifi_disabled_pixels);
        }
        lastWifi = state.isConnectedToWifi;
    }

    // Battery UI Logic (Anchored to X:409)
    if (state.batteryLevel != lastBatt)
    {
        // Wipe the old static battery bounding box (X:409, Y:20, W:24, H:16)
        tft.fillRect(409, 20, 30, 16, TFT_BLACK);

        // Draw physical battery chassis
        tft.drawRect(409, 20, 24, 14, 0xFFFF); // Main cylinder
        tft.fillRect(433, 24, 3, 6, 0xFFFF);   // Positive terminal nub

        // Calculate internal liquid fill mapping (Width 0 to 20 pixels max)
        int fillWidth = map(state.batteryLevel, 0, 100, 0, 20);

        uint16_t battColor = TFT_MAUVE;
        if (state.batteryLevel <= 20)
            battColor = TFT_RED;
        else if (state.batteryLevel <= 40)
            battColor = TFT_ORANGE;

        // Render the fluid block
        if (fillWidth > 0)
        {
            tft.fillRect(411, 22, fillWidth, 10, battColor);
        }
        lastBatt = state.batteryLevel;
    }

    // Cellular/Server UI Logic (Anchored to X:349)
    if (state.isConnectedToServer != lastServer)
    {
        if (state.isConnectedToServer)
        {
            // Hide the cellular disabled icon when connected to server
            tft.fillRect(349, 20, 15, 16, TFT_BLACK);
        }
        else
        {
            tft.pushImage(349, 20, 15, 16, image_Cellular_data_Disabled_pixels);
        }
        lastServer = state.isConnectedToServer;
    }

    // Microphone UI Logic (Anchored to X:225 / X:227)
    if (state.isListening != lastMic)
    {
        if (state.isListening)
        {
            // The enabled icon is 26x30, disabled is a massive 236x30 band! Wipe it!
            tft.fillRect(225, 268, 236, 30, TFT_BLACK);
            tft.pushImage(227, 268, 26, 30, image_Microphone_Enabled_pixels);
        }
        else
        {
            tft.pushImage(225, 268, 236, 30, image_Microphone_disabled_pixels);
        }
        lastMic = state.isListening;
    }

    // Volume/Audio UI Logic (Anchored to X:114)
    if (state.isPlayingAudio != lastAudio || globalConfig.volume != lastVolume)
    {
        // Wipe 20x16 box (The max width of the loud volume icon)
        tft.fillRect(114, 21, 20, 16, TFT_BLACK);

        if (globalConfig.volume <= 5 || !state.isPlayingAudio)
        {
            // Muted or Volume knob turned all the way down
            tft.drawBitmap(114, 21, image_volume_muted_bits, 18, 16, 0xFFFF);
        }
        else
        {
            // Actively playing and volume is up
            tft.drawBitmap(114, 21, image_volume_normal_bits, 18, 16, 0xFFFF);
        }
        lastAudio = state.isPlayingAudio;
        lastVolume = globalConfig.volume;
    }

    // Clock & Date UI Logic
    static int lastMinute = -1;

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Only update if the minute changed AND NTP actually synced (Year > 1970)
    if (timeinfo.tm_min != lastMinute && timeinfo.tm_year > 100)
    {
        // Squeegee wipe the specific 75x28 area where Date/Time lives
        tft.fillRect(47, 11, 75, 28, TFT_BLACK);

        uint8_t oldDatum = tft.getTextDatum();
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(0xFFFF);

        // Draw Date (Standard 8x8 font)
        tft.setFreeFont(NULL);
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%04d/%02d/%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        tft.drawString(dateStr, 47, 11);

        // Draw Time (FreeMono9)
        tft.setFreeFont(&FreeMono9pt7b);
        char timeStr[8];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        tft.drawString(timeStr, 47, 23);

        // Safely restore hardware state
        tft.setFreeFont(NULL);
        tft.setTextDatum(oldDatum);
        lastMinute = timeinfo.tm_min;
    }

    // Podomoro Timer UI Logic (Anchored to X:179, Y:235)
    static int lastPomoSec = -1;
    int currentPomoSec = 0;

    if (state.pomodoroEndTime > 0 && now < state.pomodoroEndTime)
    {
        currentPomoSec = state.pomodoroEndTime - now;
    }
    else if (state.pomodoroEndTime > 0 && now >= state.pomodoroEndTime)
    {
        currentPomoSec = 0; // Timer finished
    }
    else
    {
        currentPomoSec = -1; // Timer off
    }

    if (currentPomoSec != lastPomoSec && currentPomoSec >= 0)
    {
        // Squeegee wipe the Pomodoro text area to prevent ghosting
        tft.fillRect(179, 235, 115, 20, TFT_BLACK);

        uint8_t oldDatum = tft.getTextDatum();
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(0xFFFF);
        tft.setFreeFont(&FreeMono12pt7b);

        int h = currentPomoSec / 3600;
        int m = (currentPomoSec % 3600) / 60;
        int s = currentPomoSec % 60;

        char pomoStr[16];
        snprintf(pomoStr, sizeof(pomoStr), "%02d:%02d:%02d", h, m, s);
        tft.drawString(pomoStr, 179, 235);

        tft.setFreeFont(NULL);
        tft.setTextDatum(oldDatum);
        lastPomoSec = currentPomoSec;
    }
}

// This function draws the static background elements (non-changing UI components) from a binary file on LittleFS at the beginning of the program
//> I have chosen to store the background in LittleFS since, it only needs to be drawn once at startup
void drawBackground(TFT_eSPI &tft)
{
    fs::File f = LittleFS.open("/bg.bin", "r");
    if (!f || f.size() == 0)
    {
        if (f)
            f.close();
        return;
    }

    size_t len = f.size();
    if (len != EXPECTED_BG_SIZE)
    {
        f.close();
        return;
    }

    uint16_t *bgBuffer = (uint16_t *)ps_malloc(len);
    if (bgBuffer)
    {
        f.read((uint8_t *)bgBuffer, len);
        f.close();
        tft.pushImage(BG_X, BG_Y, BG_WIDTH, BG_HEIGHT, bgBuffer);
        free(bgBuffer);
    }
    else
    {
        f.close();
    }
}

// This function renders the static icons that don't change with state (Battery outline, Cellular outline, Static Text, etc.)
void renderStaticIcons(TFT_eSPI &tft)
{
    uint8_t oldDatum = tft.getTextDatum();
    tft.setTextDatum(TL_DATUM);

    // Static Battery Loading
    tft.pushImage(409, 20, 24, 16, image_Battery_Charging_pixels);

    // Initial Time Display
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.drawString("15:10", 47, 23);

    // Initial Date Display
    tft.setTextSize(1);
    tft.setFreeFont(NULL); // Fallback to standard 8x8 GFX font for the small date
    tft.drawString("yyyy/mm/dd", 47, 11);

    // Podomoro_Bounding_Box
    tft.drawRect(162, 233, 144, 21, 0xFFFF);

    // Podomoro_Time
    tft.setFreeFont();
    tft.drawString("00:10:20", 179, 235);

    tft.setFreeFont();          // Release FreeFont lock
    tft.setTextDatum(oldDatum); // Restore the original Middle-Center anchor
}

// This function initializes the TFT display, sets rotation, loads fonts, and draws the static background and icons
void tft_init(TFT_eSPI &tft, String FONT_FILENAME)
{
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(-1);

    drawBackground(tft);

    renderStaticIcons(tft);

    tft.setTextDatum(MC_DATUM);
    tft.loadFont(FONT_FILENAME, LittleFS);
    if (!tft.fontLoaded)
        tft.setTextFont(4);
}

// The main UI Task that runs on Core 1, responsible for rendering the face, state text, and telemetry icons based on events received from logic_fn.cpp
void uiTask(void *pvParameters)
{
    TFT_eSPI tft = *(TFT_eSPI *)pvParameters;
    Serial.println("Info: [UI] UI Task Started on Core 1.");

    if (pvParameters == NULL)
        vTaskDelete(NULL);
    SystemEvent msg;

    drawCurrentFaceAndState(tft, state);
    updateTelemetryIcons(tft, state);

    while (true)
    {
        // Sleep Core 1 until logic_fn.cpp fires an event, OR 1 second passes (1000 ticks)
        if (xQueueReceive(eventQueue, &msg, pdMS_TO_TICKS(1000)))
        {
            switch (msg.type)
            {
            case UPDATE_FACE_MOOD:
                drawCurrentFaceAndState(tft, state);
                break;

            case EVENT_TELEMETRY_UPDATE:
                updateTelemetryIcons(tft, state);
                break;

            case EVENT_CAMERA_TRIGGER:
                if (msg.stringData != nullptr)
                {

                    // Moved from dead center to Top-Center (Y=70) in Yellow.
                    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    tft.setTextPadding(150);
                    tft.drawString(msg.stringData, TFT_WIDTH / 2, 70);

                    // Released the padding lock
                    tft.setTextPadding(0);

                    // Free the string data after use to prevent memory leaks lmao
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

        // This runs unconditionally every time the queue times out (1Hz/ technically every 1000ms),
        // allowing the clock and Pomodoro timer to physically tick on the screen!
        updateTelemetryIcons(tft, state);
    }
}