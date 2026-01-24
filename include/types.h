#pragma once
#include <stdint.h>

enum DeviceState {
  SLEEPING,
  AWAKENING,
  LISTENING,
  THINKING,
  SPEAKING,
  HAPPY,
  POMODORO_FOCUS,
  SAD_ERROR,
  DEBUGGING,
  CONFUSED,
  SUCCESS,
  LOW_BATTERY,
  SASSY,
  WAITING,
  LOVE,
  SURPRISED,
  LAUGHING,
  CHEERING
};
enum EventType {
    WAKE_WORD_DETECTED,
    API_RESPONSE_RECEIVED,
    UPDATE_FACE_MOOD,
    PLAY_AUDIO_CHUNK
};

struct SystemEvent {
    EventType type;
    int value;           // Optional data
    char* stringData;    // Pointer to heap data (Caution: Manual memory management!)
};

struct AppConfig {
    uint8_t volume;
    DeviceState defaultState;
    bool isMuted;
};

extern AppConfig globalConfig;