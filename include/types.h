#pragma once
#include <stdint.h>
#include <Arduino.h>

// States that the faces can be in
enum DeviceState {
  SLEEPING,        // Low Power
  AWAKENING,       // Boot / Wake
  LISTENING,       // VAD Active
  THINKING,        // API Request in flight
  SPEAKING,        // TTS Playing
  HAPPY,           // Generic Good
  POMODORO_FOCUS,  // Timer Running
  SAD_ERROR,       // WiFi/API Fail
  DEBUGGING,       // Engineering Mode
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

// Types of events that can be sent between tasks
enum EventType {
    // Input Events
    EVENT_BUTTON_CLICK,       // Short Press
    EVENT_BUTTON_LONG_PRESS,  // For Sleep
    EVENT_BUTTON_DOUBLE_CLICK,// For Manual Camera
    WAKE_WORD_DETECTED,       // "Hey Buddy"
    
    // System Events
    API_RESPONSE_RECEIVED,    // JSON parsed
    EVENT_AI_REPLY_RECEIVED,  // Ready to speak
    
    // Action Events
    UPDATE_FACE_MOOD,         // Command UI to change face
    PLAY_AUDIO_CHUNK,         // Command Audio to play
    EVENT_CAMERA_TRIGGER,      // Command Camera to snap

    // Test Events
    TEST_EVENT                // For Debugging
};

// Events sent between tasks
struct SystemEvent {
    EventType type;
    int value;           // Optional: Battery %, Error Code, etc.
    char* stringData;    // Optional: Heap-allocated string (e.g., TTS Text)
};

// Configuration (Saved in website preferences)
struct AppConfig {
    uint8_t volume;
    DeviceState defaultState;
    bool isMuted;
}; 

// Shared State (Should be Protected by Mutex)
// Updated every 30 seconds by Logic Task
struct GlobalState {
    int batteryLevel;       // 0-100
    int wifiStrength;       // RSSI
    DeviceState mood;       // Current Face
    
    bool isConnectedToWifi;
    bool isConnectedToServer;
    
    bool isPlayingAudio;    // Lock: Logic sets true, UI shows speaking mouth
    bool isListening;       // Lock: Logic sets true, UI shows ear icon
    
    time_t lastNtpSync;     // Unix Timestamp
    time_t pomodoroEndTime; // 0 = Off. Timestamp = Target End Time.
};

// Global States and Queues

extern GlobalState state;
extern SemaphoreHandle_t dataMutex;
extern AppConfig globalConfig;
extern QueueHandle_t eventQueue;