#include "logic_fn.h"
#include "types.h"
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_camera.h"

// --- Audio Libraries ---
#include "AudioFileSource.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorWAV.h" 
#include "AudioOutputI2S.h"
#include <fabiocanavarro-project-1_inferencing.h> 

const char* API_HOST = "neuralearnapp.vercel.app";
const char* AUDIO_API_PATH = "/api/v1/chat/audio";
const char* VISION_API_PATH = "/api/v1/vision/analyze"; 
const char* TEST_USER_ID = "705c2277-75d9-4a2a-97cf-84634d3c4d4c"; 
String currentChatId = ""; 

// --- Config & Pins ---
#define I2S_MIC_SCK 39
#define I2S_MIC_WS  38
#define I2S_MIC_SD  47
#define I2S_PORT_MIC I2S_NUM_0
#define I2S_SPK_BCLK 41
#define I2S_SPK_LRC  42  
#define I2S_SPK_DIN  40  

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

#define CONFIDENCE_THRESHOLD 0.80f   
#define COOLDOWN_MS 3000 
#define MAX_RECORD_TIME_MS 15000   
#define SILENCE_TIMEOUT_MS 1500    
#define SILENCE_THRESHOLD 150      

int16_t *inference_buffer; 
size_t chunk_size = 512;
int32_t *i2s_read_buff;

AudioGeneratorWAV *wav; 
AudioFileSource *file; 
AudioOutputI2S *out;

void dispatchMoodUpdate(DeviceState newMood) {
    state.mood = newMood;
    SystemEvent ev;
    ev.type = UPDATE_FACE_MOOD;
    ev.stringData = nullptr;
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

void dispatchCameraCountdown(const char* text) {
    SystemEvent ev;
    ev.type = EVENT_CAMERA_TRIGGER;
    ev.stringData = strdup(text); 
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

// NEW: Dispatches a signal to Core 1 to check the telemetry booleans
void dispatchTelemetryUpdate() {
    SystemEvent ev;
    ev.type = EVENT_TELEMETRY_UPDATE;
    ev.stringData = nullptr;
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(inference_buffer + offset, out_ptr, length);
    return 0;
}

WiFiManager wifiManager;
WiFiServer server(80);
int wifiRetryCount = 0;
const int WIFI_RETRY_LIMIT = 5;
const char *WIFI_SSID = "NeuraLearn";
const char *WIFI_PASSWORD = "12345678";

void setupWiFi() {
    Serial.println("Info: [NETWORK] Connecting to WiFi");
    wifiManager.setConfigPortalTimeout(1000000000);
    wifiManager.setBreakAfterConfig(false);

    while (!state.isConnectedToWifi) {
        if (!wifiManager.autoConnect(WIFI_SSID, WIFI_PASSWORD)) {
            if (wifiRetryCount < WIFI_RETRY_LIMIT) {
                state.isConnectedToWifi = false;
                dispatchTelemetryUpdate();
                wifiRetryCount +=1;
                delay(2000);
            } else {
                wifiManager.resetSettings();
                ESP.restart();
            }
        } else {
            // FIRE ALARM: WiFi Connected! Tell the UI.
            state.isConnectedToWifi = true;
            dispatchTelemetryUpdate(); 
        }
    }
    Serial.println("Info: [NETWORK] WiFi connected!");
    
    // --- NTP TIME SYNCHRONIZATION ---
    Serial.println("Info: [NETWORK] Syncing NTP (Singapore Time)...");
    // 28800 seconds = GMT+8 (Singapore time offset)
    configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

class AudioFileSourceInsecureHTTPS : public AudioFileSource {
protected:
    HTTPClient http;
    WiFiClientSecure client;
    uint32_t size;
    uint32_t pos;
    bool opened;

public:
    AudioFileSourceInsecureHTTPS(const char* url) {
        client.setInsecure(); 
        http.begin(client, url);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            size = http.getSize();
            opened = true;
        } else {
            opened = false;
            size = 0;
        }
        pos = 0;
    }
    virtual ~AudioFileSourceInsecureHTTPS() { http.end(); }
    virtual uint32_t read(void *data, uint32_t len) override {
        if (!opened) return 0;
        WiFiClient *stream = http.getStreamPtr();
        if (!stream) return 0;
        uint32_t bytesRead = 0;
        uint8_t *buf = (uint8_t*)data;
        unsigned long timeout = millis();
        while (bytesRead < len && http.connected()) {
            if (stream->available()) {
                int r = stream->read(buf + bytesRead, len - bytesRead);
                if (r > 0) { bytesRead += r; pos += r; timeout = millis(); }
            } else {
                if (millis() - timeout > 5000) break;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
        return bytesRead;
    }
    virtual uint32_t readNonBlock(void *data, uint32_t len) override { return read(data, len); }
    virtual bool seek(int32_t newPos, int dir) override {
        if (!opened) return false;
        if (dir == 1) newPos = pos + newPos;
        if (newPos < pos) return false;
        uint32_t bytesToSkip = newPos - pos;
        if (bytesToSkip == 0) return true;
        WiFiClient *stream = http.getStreamPtr();
        if (!stream) return false;
        uint8_t trash[128];
        while (bytesToSkip > 0 && http.connected()) {
            uint32_t readLen = (bytesToSkip > sizeof(trash)) ? sizeof(trash) : bytesToSkip;
            unsigned long timeout = millis();
            while (!stream->available() && http.connected()) {
                if (millis() - timeout > 5000) return false;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            int r = stream->read(trash, readLen);
            if (r > 0) { bytesToSkip -= r; pos += r; } else { return false; }
        }
        return true;
    }
    virtual bool close() override { http.end(); opened = false; return true; }
    virtual bool isOpen() override { return opened; }
    virtual uint32_t getSize() override { return size; }
    virtual uint32_t getPos() override { return pos; }
};

void playAudioResponse(const char* wav_url) {
    Serial.println("\nInfo: [SYSTEM] -> 🔊 Playing Audio Response...");
    
    // FIRE ALARM: We are speaking. Toggle Volume icon.
    state.isPlayingAudio = true;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(SPEAKING);

    out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S); 
    out->SetPinout(I2S_SPK_BCLK, I2S_SPK_LRC, I2S_SPK_DIN);
    out->SetGain(0.8); 
    out->SetOutputModeMono(true);

    file = new AudioFileSourceInsecureHTTPS(wav_url);
    wav = new AudioGeneratorWAV(); 
    
    if (wav->begin(file, out)) {
        while (wav->isRunning()) {
            if (!wav->loop()) wav->stop();
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    delete wav;
    delete file;
    delete out;

    // FIRE ALARM: Done speaking. Mute Volume icon.
    state.isPlayingAudio = false;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(WAITING);
}

void takePictureAndSend() {
    Serial.println("\nInfo: [CAMERA] Waking up hardware via Just-In-Time Allocation...");
    dispatchMoodUpdate(POMODORO_FOCUS); 

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
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_VGA; 
    config.pixel_format = PIXFORMAT_JPEG; 
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM; 
    config.jpeg_quality = 12; 
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) return;
    
    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 3; i > 0; i--) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d...", i);
        dispatchCameraCountdown(buf);
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    dispatchCameraCountdown("SNAP!");

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        esp_camera_deinit();
        return;
    }

    String jsonResponse = "";
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        // FIRE ALARM: Attempting server connection
        state.isConnectedToServer = true;
        dispatchTelemetryUpdate();
        dispatchMoodUpdate(THINKING);

        http.setTimeout(30000); 
        http.begin(client, String("https://") + API_HOST + VISION_API_PATH);
        
        http.addHeader("Content-Type", "image/jpeg");
        http.addHeader("x-user-id", TEST_USER_ID);
        if (currentChatId.length() > 0) http.addHeader("x-chat-id", currentChatId);

        int httpResponseCode = http.POST(fb->buf, fb->len);
        if (httpResponseCode == 200 || httpResponseCode == 201) {
            jsonResponse = http.getString();
        } else {
            state.isConnectedToServer = false;
            dispatchTelemetryUpdate();
        }
        http.end();
    }

    esp_camera_fb_return(fb);
    esp_camera_deinit();

    if (jsonResponse.length() > 0) {
        JsonDocument resDoc;
        if (!deserializeJson(resDoc, jsonResponse) && resDoc.containsKey("audio_url")) {
            playAudioResponse(resDoc["audio_url"]);
        }
    }
}

void recordAndSendAudioToAPI() {
    Serial.println("\nInfo: [SYSTEM] -> Wake word triggered! Reactive stream started...");
    
    // FIRE ALARM: Mic is active!
    state.isListening = true;
    dispatchTelemetryUpdate();

    SystemEvent wakeEv;
    wakeEv.type = WAKE_WORD_DETECTED;
    xQueueSend(eventQueue, &wakeEv, portMAX_DELAY);

    if (WiFi.status() != WL_CONNECTED) {
        state.isListening = false;
        dispatchTelemetryUpdate();
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); 

    if (!client.connect(API_HOST, 443)) {
        state.isConnectedToServer = false;
        state.isListening = false;
        dispatchTelemetryUpdate();
        return;
    }

    // FIRE ALARM: We successfully opened a TCP socket to Vercel
    state.isConnectedToServer = true;
    dispatchTelemetryUpdate();

    client.print(String("POST ") + AUDIO_API_PATH + " HTTP/1.1\r\n");
    client.print(String("Host: ") + API_HOST + "\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print(String("x-user-id: ") + TEST_USER_ID + "\r\n");
    
    if (currentChatId.length() > 0) client.print("x-chat-id: " + currentChatId + "\r\n");

    client.print("Transfer-Encoding: chunked\r\n"); 
    client.print("Connection: close\r\n\r\n"); 

    int32_t *temp_i2s_chunk = (int32_t*)malloc(chunk_size * sizeof(int32_t));
    int16_t *pcm_16_chunk = (int16_t*)malloc(chunk_size * sizeof(int16_t));
    
    if (!temp_i2s_chunk || !pcm_16_chunk) {
        if(temp_i2s_chunk) free(temp_i2s_chunk);
        if(pcm_16_chunk) free(pcm_16_chunk);
        client.stop();
        return;
    }

    unsigned long start_time = millis();
    int silent_chunks = 0;
    int max_silent_chunks = (SILENCE_TIMEOUT_MS * 16) / chunk_size;
    size_t bytes_read = 0;

    while (millis() - start_time < MAX_RECORD_TIME_MS) {
        i2s_read(I2S_PORT_MIC, temp_i2s_chunk, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);
        long sum_amplitude = 0;

        for (size_t i = 0; i < samples_read; i++) {
            pcm_16_chunk[i] = (int16_t)(temp_i2s_chunk[i] >> 14); 
            sum_amplitude += abs(pcm_16_chunk[i]); 
        }
        
        int avg_volume = sum_amplitude / samples_read;
        if (avg_volume < SILENCE_THRESHOLD) silent_chunks++;
        else silent_chunks = 0; 

        size_t bytes_to_send = samples_read * sizeof(int16_t);
        client.print(String(bytes_to_send, HEX) + "\r\n");
        client.write((uint8_t*)pcm_16_chunk, bytes_to_send);
        client.print("\r\n");

        if (silent_chunks >= max_silent_chunks) break;
    }

    client.print("0\r\n\r\n");
    Serial.println("Info: [SYSTEM] -> Stream closed. Waiting for AI response...");

    // FIRE ALARM: Stopped listening to user
    state.isListening = false;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(THINKING);

    String jsonResponse = "";
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 20000) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.startsWith("{")) jsonResponse = line;
            timeout = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    client.stop();
    free(temp_i2s_chunk);
    free(pcm_16_chunk);
    
    bool shouldTakePicture = false;

    if (jsonResponse.length() > 0) {
        JsonDocument resDoc;
        DeserializationError error = deserializeJson(resDoc, jsonResponse);
        if (!error) {
            if (resDoc.containsKey("chat_id")) currentChatId = resDoc["chat_id"].as<String>();
            if (resDoc.containsKey("action") && resDoc["action"] == "TAKE_PICTURE") shouldTakePicture = true;
            if (resDoc.containsKey("audio_url")) playAudioResponse(resDoc["audio_url"]);
        }
    } else {
        // AI Failed to respond
        state.isConnectedToServer = false;
        dispatchTelemetryUpdate();
    }

    if (shouldTakePicture) takePictureAndSend();

    i2s_zero_dma_buffer(I2S_PORT_MIC);
    dispatchMoodUpdate(WAITING);
}

void networkTask(void *pvParameters) {
    Serial.println("Info: Logic Task started on Core 0");

    setupWiFi();
    dispatchMoodUpdate(WAITING); 

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = EI_CLASSIFIER_FREQUENCY, 
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = -1,
        .data_in_num = I2S_MIC_SD
    };

    i2s_driver_install(I2S_PORT_MIC, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT_MIC, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT_MIC);

    inference_buffer = (int16_t*)malloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
    i2s_read_buff = (int32_t*)malloc(chunk_size * sizeof(int32_t));

    unsigned long last_trigger_time = 0;
    size_t bytes_read = 0;

    while (true) {
        i2s_read(I2S_PORT_MIC, i2s_read_buff, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        int samples_read = bytes_read / sizeof(int32_t);

        numpy::roll(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -samples_read);

        int16_t *new_data_start = inference_buffer + (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - samples_read);
        for (int i = 0; i < samples_read; i++) {
            new_data_start[i] = (int16_t)(i2s_read_buff[i] >> 14); 
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        signal.get_data = &microphone_audio_signal_get_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false); 
        
        if (r != EI_IMPULSE_OK) {
            vTaskDelay(pdMS_TO_TICKS(10)); 
            continue;
        }

        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            if (strcmp(result.classification[ix].label, "wake_word") == 0 || 
                strcmp(result.classification[ix].label, "hey_buddy") == 0) {
                
                if (result.classification[ix].value > CONFIDENCE_THRESHOLD) {
                    unsigned long now = millis();
                    if (now - last_trigger_time > COOLDOWN_MS) {
                        last_trigger_time = now;
                        recordAndSendAudioToAPI();
                        memset(inference_buffer, 0, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
                        i2s_zero_dma_buffer(I2S_PORT_MIC);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}