#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include "esp_camera.h"
#include "pins.h"

#include "ui_renderer.h"
#include "logic_fn.h"
#include "icons_data.h"

#define FONT_FILENAME "DejaVuSansMono-Bold-10"

TFT_eSPI tft = TFT_eSPI();
QueueHandle_t eventQueue;

AppConfig globalConfig = {
    .volume = 100,
    .defaultState = SLEEPING,
    .isMuted = false,
};

SemaphoreHandle_t dataMutex;
GlobalState state;

void camera_init() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    
    // THE VIP SETTINGS (Copied directly from your successful isolated test)
    config.xclk_freq_hz = 10000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    Serial.println("Info: [SYSTEM] Initializing camera to claim PSRAM VIP space...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Error: [SYSTEM] FATAL: Camera init failed with error 0x%x\n", err);
        Serial.println("Error: [SYSTEM] System halting to protect data integrity.");
        while(1);
    }
    Serial.println("Info: [SYSTEM] SUCCESS: Camera initialized and memory claimed!");
}

void setup() {
    Serial.begin(115200);
    delay(500); // Brief hardware stabilization delay
    Serial.println("\nInfo: [SYSTEM] Booting NeuraLearn OS...");

    // 1. HARDWARE ORCHESTRATION: Camera must boot FIRST to claim contiguous PSRAM
    camera_init();

    dataMutex = xSemaphoreCreateMutex();

    DeviceState currentState = globalConfig.defaultState;
    state.batteryLevel = 80;
    state.mood = currentState;
    state.isConnectedToWifi = false;
    state.isConnectedToServer = false;
    state.isListening = false;
    state.isPlayingAudio = false;
    state.lastNtpSync = 0;
    state.pomodoroEndTime = 0;
    state.wifiStrength = 0; 

    eventQueue = xQueueCreate(10, sizeof(SystemEvent)); 

    Serial.println("Info: [FS] Starting LittleFS...");
    if (!LittleFS.begin()) {
        Serial.println("Error: [FS] FATAL: LittleFS Mount Failed.");
        return;
    }
    Serial.println("Info: [FS] LittleFS Mounted Successfully.");

    tft_init(tft, FONT_FILENAME);
    drawBackground(tft);

    // Create Task on Core 0 (Network & AI Processing)
    xTaskCreatePinnedToCore(
        networkTask,   // Function
        "Network",     // Name
        8192,          // Stack size (bytes)
        NULL,          // Params
        1,             // Priority (Low)
        NULL,          // Handle
        0              // CORE 0 ID
    );

    // Create Task on Core 1 (UI Rendering)
    xTaskCreatePinnedToCore(
        uiTask,        // Function
        "UI",          // Name
        4096,          // Stack size
        &tft,          // Params
        2,             // Priority (High)
        NULL,          // Handle
        1              // CORE 1 ID
    );

    // Create Lightweight Hardware Polling Task (Core 1)
    xTaskCreatePinnedToCore(
        hardwareTask,  // Function
        "Hardware",    // Name
        2048,          // Stack size
        NULL,          // Params
        3,             // Priority (Highest - Needs instant response)
        NULL,          // Handle
        1              // CORE 1 ID
    );
}

void loop() {
    // Left empty. FreeRTOS tasks handle everything.
}