#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include "esp_camera.h"
#include "pins.h"

#include "ui_renderer.h"
#include "logic_fn.h"
#include "icons_data.h"

#define FONT_FILENAME "DejaVuSansMono-Bold-40"

TFT_eSPI tft = TFT_eSPI();
QueueHandle_t eventQueue;

AppConfig globalConfig = {
    .volume = 80,
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
    config.xclk_freq_hz = 10000000;
    
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_CIF;
    
    config.jpeg_quality = 10;
    config.fb_count = 5;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;


    Serial.println("Initializing camera...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("FATAL: Camera init failed with error 0x%x\n", err);
        tft.setTextColor(TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Camera Init Failed", 160, 120);
        while(1);
    }
    Serial.println("SUCCESS: Camera initialized!");
}

void setup() {
    dataMutex = xSemaphoreCreateMutex();
    Serial.begin(115200);

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

    Serial.println("Starting LittleFS...");
    if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
    }
    Serial.println("LittleFS Mounted Successfully.");

    // camera_init();

    tft_init(tft, FONT_FILENAME);

    drawBackground(tft);

    // Create Task on Core 0
    xTaskCreatePinnedToCore(
        networkTask,   // Function
        "Network",     // Name
        8192,          // Stack size (bytes)
        NULL,          // Params
        1,             // Priority (Low)
        NULL,          // Handle
        0              // CORE 0 ID
    );

    // Create Task on Core 1
    xTaskCreatePinnedToCore(
        uiTask,        // Function
        "UI",          // Name
        4096,          // Stack size
        &tft,          // Params
        2,             // Priority (High)
        NULL,          // Handle
        1              // CORE 1 ID
    );
}

void loop() {

}
