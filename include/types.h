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
    EVENT_BUTTON_CLICK,       
    EVENT_BUTTON_LONG_PRESS,  
    EVENT_BUTTON_DOUBLE_CLICK,
    WAKE_WORD_DETECTED,       
    
    // System Events
    API_RESPONSE_RECEIVED,    
    EVENT_AI_REPLY_RECEIVED,  
    EVENT_TELEMETRY_UPDATE,
    
    // Action Events
    UPDATE_FACE_MOOD,         
    PLAY_AUDIO_CHUNK,         
    EVENT_CAMERA_TRIGGER,      

    // Test Events
    TEST_EVENT                
};

// Events sent between tasks
struct SystemEvent {
    EventType type;
    int value;           
    char* stringData;    
};

// Configuration (Saved in website preferences)
struct AppConfig {
    uint8_t volume;
    DeviceState defaultState;
    bool isMuted;
}; 

// Shared State (Protected by Mutex/Queue architecture)
struct GlobalState {
    int batteryLevel;       // 0-100
    int wifiStrength;       // RSSI
    DeviceState mood;       // Current Face
    
    bool isConnectedToWifi;
    bool isConnectedToServer;
    
    bool isPlayingAudio;    // True = Speaking Icon, False = Muted Icon
    bool isListening;       // True = Mic Enabled Icon, False = Mic Disabled Icon
    
    time_t lastNtpSync;     
    time_t pomodoroEndTime; 
};

extern GlobalState state;
extern SemaphoreHandle_t dataMutex;
extern AppConfig globalConfig;
extern QueueHandle_t eventQueue;