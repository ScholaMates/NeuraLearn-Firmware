#pragma once

#include "types.h"

extern DeviceState currentState;
extern AppConfig globalConfig;
extern QueueHandle_t eventQueue;
extern SemaphoreHandle_t dataMutex;

