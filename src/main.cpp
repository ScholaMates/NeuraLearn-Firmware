#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>

#include "ui_renderer.h"
#include "logic_fn.h"

#define FONT_FILENAME "DejaVuSansMono-Bold-40"

TFT_eSPI tft = TFT_eSPI();
QueueHandle_t eventQueue;

AppConfig globalConfig = {
    .volume = 80,
    .defaultState = SLEEPING,
    .isMuted = false
};

SemaphoreHandle_t dataMutex;

void setup() {
    dataMutex = xSemaphoreCreateMutex();
    Serial.begin(115200);

    DeviceState currentState = globalConfig.defaultState;

    eventQueue = xQueueCreate(10, sizeof(SystemEvent)); 

    if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed.");
    return;
    }
    Serial.println("LittleFS Mounted Successfully.");

    tft_init(tft, FONT_FILENAME);

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
        NULL,          // Params
        2,             // Priority (High)
        NULL,          // Handle
        1              // CORE 1 ID
    );
}

void loop() {

}

