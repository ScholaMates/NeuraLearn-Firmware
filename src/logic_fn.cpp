#include "logic_fn.h"
#include "types.h"
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

#include <fabiocanavarro-project-1_inferencing.h> 

// --- NeuraLearn Backend API ---
const char* API_HOST = "neuralearnapp.vercel.app";
const char* AUDIO_API_PATH = "/api/v1/chat/audio";
const char* TEST_USER_ID = "705c2277-75d9-4a2a-97cf-84634d3c4d4c"; 

// --- I2S Microphone Pins ---
#define I2S_MIC_SCK 39
#define I2S_MIC_WS  38
#define I2S_MIC_SD  47
#define I2S_PORT    I2S_NUM_0

// --- Reactive Recording Tuning ---
#define CONFIDENCE_THRESHOLD 0.60f   
#define COOLDOWN_MS 3000 
#define MAX_RECORD_TIME_MS 15000   // Absolute maximum cut-off
#define SILENCE_TIMEOUT_MS 1500    // Stop recording after x amount seconds of silence
#define SILENCE_THRESHOLD 150      // Volume threshold (0-32768). Adjust if it cuts off too early!

// ML Buffers
int16_t *inference_buffer; 
size_t chunk_size = 512;
int32_t *i2s_read_buff;

// Edge Impulse Callback
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(inference_buffer + offset, out_ptr, length);
    return 0;
}

// Wifi Manager
WiFiManager wifiManager;
WiFiServer server(80);
bool wifiConnected = false;
int wifiRetryCount = 0;
const int WIFI_RETRY_LIMIT = 5;
const char *WIFI_SSID = "NeuraLearn";
const char *WIFI_PASSWORD = "12345678";


void setupWiFi()
{
    Serial.println("Event: Connecting to WiFi");
    wifiManager.setConfigPortalTimeout(1000000000);
    wifiManager.setBreakAfterConfig(false);

    while (!wifiConnected) {
        if (!wifiManager.autoConnect(WIFI_SSID, WIFI_PASSWORD))
        {
            if (wifiRetryCount < WIFI_RETRY_LIMIT)
            {
                Serial.println("Warning: Failed to connect to WiFi, retrying...");
                wifiConnected = false;
                wifiRetryCount +=1;
                delay(2000);
                continue;
            }
            else
            {
                Serial.println("Warning: Failed to connect");
                Serial.println("Important Event: Resetting WiFi settings and restarting ESP...");
                wifiManager.resetSettings();
                ESP.restart();
            }
        }
        else 
        {
            wifiConnected = true;
        }
    }
    Serial.println("Event: WiFi connected");
    Serial.println("\tIP address: " + WiFi.localIP().toString());
    server.begin();
}

// Reactive Audio Streamer
void recordAndSendAudioToAPI() {
    Serial.println("\n[SYSTEM] -> Wake word triggered! Reactive stream started...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ERROR] WiFi Disconnected!");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); 

    if (!client.connect(API_HOST, 443)) {
        Serial.println("[ERROR] Connection to server failed!");
        return;
    }

    // --- 1. CHUNKED HTTP HEADERS ---
    // We do NOT send Content-Length. We tell Vercel we are streaming chunk by chunk.
    client.print(String("POST ") + AUDIO_API_PATH + " HTTP/1.1\r\n");
    client.print(String("Host: ") + API_HOST + "\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print(String("X-User-Id: ") + TEST_USER_ID + "\r\n");
    client.print("Transfer-Encoding: chunked\r\n"); // THE MAGIC KEY
    client.print("Connection: close\r\n\r\n"); 

    int32_t *temp_i2s_chunk = (int32_t*)malloc(chunk_size * sizeof(int32_t));
    int16_t *pcm_16_chunk = (int16_t*)malloc(chunk_size * sizeof(int16_t));
    
    if (!temp_i2s_chunk || !pcm_16_chunk) {
        Serial.println("[ERROR] Failed to allocate streaming chunks!");
        if(temp_i2s_chunk) free(temp_i2s_chunk);
        if(pcm_16_chunk) free(pcm_16_chunk);
        client.stop();
        return;
    }

    unsigned long start_time = millis();
    int silent_chunks = 0;
    
    // Math: 16000 samples per sec = 16 samples per millisecond.
    // How many chunks make up our silence timeout?
    int max_silent_chunks = (SILENCE_TIMEOUT_MS * 16) / chunk_size;
    
    Serial.println("[SYSTEM] -> Recording & Streaming live (Speak now!)...");

    size_t bytes_read = 0;

    // --- 2. DYNAMIC STREAMING LOOP ---
    while (millis() - start_time < MAX_RECORD_TIME_MS) {
        i2s_read(I2S_PORT, temp_i2s_chunk, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);

        long sum_amplitude = 0;

        for (size_t i = 0; i < samples_read; i++) {
            pcm_16_chunk[i] = (int16_t)(temp_i2s_chunk[i] >> 14); 
            sum_amplitude += abs(pcm_16_chunk[i]); // Calculate absolute volume
        }
        
        // --- VOICE ACTIVITY DETECTION (VAD) ---
        int avg_volume = sum_amplitude / samples_read;
        if (avg_volume < SILENCE_THRESHOLD) {
            silent_chunks++;
        } else {
            silent_chunks = 0; // Reset if user speaks again!
        }

        // --- CHUNKED TRANSFER ENCODING PROTOCOL ---
        // Format: [Size in HEX]\r\n [Binary Data] \r\n
        size_t bytes_to_send = samples_read * sizeof(int16_t);
        client.print(String(bytes_to_send, HEX) + "\r\n");
        client.write((uint8_t*)pcm_16_chunk, bytes_to_send);
        client.print("\r\n");

        // --- EXIT CONDITION ---
        if (silent_chunks >= max_silent_chunks) {
            Serial.printf("[SYSTEM] -> Silence detected (avg vol: %d). Stopping stream.\n", avg_volume);
            break;
        }
    }

    if (millis() - start_time >= MAX_RECORD_TIME_MS) {
        Serial.println("[SYSTEM] -> Maximum time reached. Stopping stream.");
    }

    // --- 3. END THE STREAM ---
    // A 0-byte chunk tells the server "I am done talking."
    client.print("0\r\n\r\n");

    Serial.println("[SYSTEM] -> Stream closed. Waiting for AI response...");

    // --- 4. Read the Backend Response ---
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 20000) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println(line);
            timeout = millis();
        }
        
        // THE CRITICAL FIX: Yield to FreeRTOS so the Watchdog doesn't starve
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    Serial.println("\n========================================");
    Serial.println("[NeuraLearn API Streaming Cycle Complete]");

    client.stop();
    free(temp_i2s_chunk);
    free(pcm_16_chunk);
    
    Serial.println("[SYSTEM] -> Resuming Wake Word listening...\n");
}

void networkTask(void *pvParameters) {
    Serial.println("Logic Task started on Core 0");

    setupWiFi();

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

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);

    inference_buffer = (int16_t*)malloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
    i2s_read_buff = (int32_t*)malloc(chunk_size * sizeof(int32_t));

    unsigned long last_trigger_time = 0;
    size_t bytes_read = 0;

    Serial.println("Listening for Wake Word...");

    while (true) {
        i2s_read(I2S_PORT, i2s_read_buff, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
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
                        
                        Serial.printf("\nWAKE DETECTED! (Score: %.2f)\n", result.classification[ix].value);
                        
                        recordAndSendAudioToAPI();

                        memset(inference_buffer, 0, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
                        i2s_zero_dma_buffer(I2S_PORT);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}