#pragma once

#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include <logic_fn.h>
#include "types.h"

void networkTask(void *pvParameters) {
    SystemEvent msg;
    msg.type = API_RESPONSE_RECEIVED;
    msg.stringData = "Hello World"; // In reality, strdup() this to heap

    // xQueueSend is like tx.send(). 
    // portMAX_DELAY means "wait forever if full" (blocking)
    xQueueSend(eventQueue, &msg, portMAX_DELAY); 
}