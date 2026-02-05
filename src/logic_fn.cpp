#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include <logic_fn.h>
#include "types.h"

void networkTask(void *pvParameters) {
    SystemEvent msg;

    while (true)
    {
        msg.type = TEST_EVENT;
        msg.stringData = "Hello World";

        // xQueueSend is like tx.send(). 
        // portMAX_DELAY means "wait forever if full" (blocking)
        xQueueSend(eventQueue, &msg, portMAX_DELAY); 
    }
    
    
}