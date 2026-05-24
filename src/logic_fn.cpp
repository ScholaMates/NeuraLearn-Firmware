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
#include "pins.h"
#include "config.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

// Audio Libraries
#include "AudioFileSource.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourceBuffer.h" 
#include <fabiocanavarro-project-1_inferencing.h>

// API Endpoints and Configurations
const char *API_HOST = "neuralearnapp.vercel.app";
const char *AUDIO_API_PATH = "/api/v1/chat/audio";
const char *VISION_API_PATH = "/api/v1/vision/analyze";
const char *TEST_USER_ID = "705c2277-75d9-4a2a-97cf-84634d3c4d4c";
String currentChatId = "";

// WiFi Manager init and configurations
WiFiManager wifiManager;
int wifiRetryCount = 0;
const int WIFI_RETRY_LIMIT = 5;
const char *WIFI_SSID = "NeuraLearn";
const char *WIFI_PASSWORD = "12345678";

// Microphone and Inference Buffers Init
int16_t *inference_buffer;
size_t chunk_size = 512;
int32_t *i2s_read_buff;

AudioGeneratorWAV *wav;
AudioFileSource *file;
AudioFileSourceBuffer *buff; 
AudioOutputI2S *out;

// This function updates the mood global state and sends an event to the UI to trigger a face redraw with the new mood
void dispatchMoodUpdate(DeviceState newMood)
{
    state.mood = newMood;
    SystemEvent ev;
    ev.type = UPDATE_FACE_MOOD;
    ev.stringData = nullptr;
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

// This function sends an event to the UI to trigger a camera countdown toast(or just floating notifications)
void dispatchNotification(const char *text)
{
    SystemEvent ev;
    ev.type = EVENT_CAMERA_TRIGGER;
    ev.stringData = strdup(text);
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

// This function sends an event to the UI to trigger an update of the telemetry icons
void dispatchTelemetryUpdate()
{
    SystemEvent ev;
    ev.type = EVENT_TELEMETRY_UPDATE;
    ev.stringData = nullptr;
    xQueueSend(eventQueue, &ev, portMAX_DELAY);
}

// This function takes the current mood from the global state and updates the face displayed on the UI accordingly, along with the text description of the mood
void handleNetworkError(const char* errorMessage) {
    Serial.printf("Error: [NETWORK] %s\n", errorMessage);
    dispatchMoodUpdate(SAD_ERROR);
    // Add a short delay to allow the user to see the error state before resetting
    vTaskDelay(pdMS_TO_TICKS(2000));
    dispatchMoodUpdate(WAITING);
}

// This function retrieves audio signal data from the microphone buffer, converts it from int16 to float, and provides it to the inference engine for processing
//> The inference engine(AI model) expects float data in the range of -1.0 to 1.0
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(inference_buffer + offset, out_ptr, length);
    return 0;
}

// This function initializes the WiFi connection using WiFiManager
//> The WifiManager here allows for better user experience in connecting to WiFi, with a web portal which is insane
void setupWiFi()
{
    wifiManager.setCaptivePortalEnable(true); 
    wifiManager.setConnectTimeout(30);
    wifiManager.setConfigPortalTimeout(120); 
    wifiManager.setBreakAfterConfig(true);

    while (!state.isConnectedToWifi)
    {
        if (!wifiManager.autoConnect(WIFI_SSID, WIFI_PASSWORD))
        {
            if (wifiRetryCount < WIFI_RETRY_LIMIT)
            {
                state.isConnectedToWifi = false;
                dispatchTelemetryUpdate();
                wifiRetryCount += 1;
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            else
            {
                wifiManager.resetSettings();
                ESP.restart();
            }
        }
        else
        {
            state.isConnectedToWifi = true;
            dispatchTelemetryUpdate();
        }
    }
    configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

// Custom AudioFileSource class to handle streaming WAV audio from HTTPS endpoints without blocking the main thread
/*>
    Since the standard AudioFileSourceHTTPStream doesn't support HTTPS or has
    blocking calls that can cuz of audio stuttering, this custom class uses
    WiFiClientSecure to stream audio data securely and efficiently from the API
    endpoint. It implements non-blocking reads and a simple seek mechanism to
    allow for smooth audio playback.
*/

/*
    > This works by maintaining an active HTTP connection to the audio stream
    > and reading data in small chunks, while also allowing for seeking by
    > discarding data until the desired position is reached. It also handles
    > timeouts and connection issues gracefully to prevent the audio playback
    > from stuttering or crashing the system.

    ? Why not use the existing AudioFileSourceHTTPStream class?
    > This class is specifically designed to work with the AudioGeneratorWAV
    > class, which expects an AudioFileSource to read audio data from. By using
    > this custom class, we can stream audio responses from the API securely
    > over HTTPS without blocking the main thread or causing performance issues.
*/
class AudioFileSourceInsecureHTTPS : public AudioFileSource
{
protected:
    HTTPClient http;
    WiFiClientSecure client;
    uint32_t size;
    uint32_t pos;
    bool opened;

public:
    /// This constructor initializes the HTTP connection to the given URL and
    /*>
        Basically like an initializer for the audio stream. It sets up the
        WiFiClientSecure to ignore certificate validation (since we're just
        streaming from a known API endpoint), and then begins the HTTP request to
    */
    AudioFileSourceInsecureHTTPS(const char *url)
    {
        client.setInsecure();
        http.begin(client, url);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            size = http.getSize();
            opened = true;
        }
        else
        {
            opened = false;
            size = 0;
        }
        pos = 0;
    }

    // This destructor ensures that the HTTP connection is properly closed when the object is destroyed to free up resources
    virtual ~AudioFileSourceInsecureHTTPS() { http.end(); }

    // This method reads audio data from the HTTP stream into the provided
    // buffer, handling timeouts and connection issues to ensure smooth playback
    virtual uint32_t read(void *data, uint32_t len) override
    {
        // If the connection isn't open, return 0 bytes read
        if (!opened) return 0;

        // --- THE ZOMBIE SOCKET FIX ---
        // If we physically received the entire payload, aggressively kill the connection.
        // This prevents the robot from hanging on the "SPEAKING" face infinitely.
        if (size > 0 && pos >= size) {
            opened = false;
            return 0;
        }

        // Get the underlying stream from the HTTP client
        WiFiClient *stream = http.getStreamPtr();
        if (!stream) return 0;

        // Read data in a loop until we've read the requested length or the connection is closed
        uint32_t bytesread = 0;
        uint8_t *buf = (uint8_t *)data;
        unsigned long timeout = millis();

        while (bytesread < len && http.connected())
        {
            int available = stream->available();
            if (available > 0)
            {
                // Maximize throughput by reading massive chunks instead of byte-by-byte
                int toread = len - bytesread;
                if (toread > available) toread = available;
                
                // Read data from the stream into the buffer
                int r = stream->read(buf + bytesread, toread);

                // If we read some data, update the bytesRead count and reset the timeout
                if (r > 0)
                {
                    bytesread += r;
                    pos += r;
                    timeout = millis();
                }
            }
            else
            {
                // Check for EOF inside the wait loop
                if (size > 0 && pos >= size) {
                    opened = false;
                    break;
                }

                // If no data is available, check for a timeout to prevent stalling
                if (millis() - timeout > 10000)
                {
                    opened = false; // KILL THE SOCKET SO IT DOES NOT LOOP FOREVER
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(1)); // 1ms delay for hyper-fast network polling
            }
        }

        // Violent server closure detection
        if (!http.connected() && bytesread == 0) {
            opened = false; 
        }

        return bytesread;
    }

    // This method provides a non-blocking way to read audio data, which can be used by the audio generator to fetch data without stalling the main thread
    virtual uint32_t readNonBlock(void *data, uint32_t len) override { return read(data, len); }

    // This method implements a simple seek mechanism by discarding data from the stream until the desired position is reached, allowing for smooth seeking without blocking
    virtual bool seek(int32_t newPos, int dir) override
    {
        // If the connection isn't open, we can't seek
        if (!opened) return false;

        // Calculate the new position based on the direction (0 = absolute, 1 = relative to current position)
        if (dir == 1) newPos = pos + newPos;

        // If the new position is before the current position, we can't seek backwards in a stream, so return false
        if (newPos < pos) return false;

        uint32_t bytesToSkip = newPos - pos;

        if (bytesToSkip == 0) return true;

        WiFiClient *stream = http.getStreamPtr();
        if (!stream) return false;

        uint8_t trash[128];
        while (bytesToSkip > 0 && http.connected())
        {
            uint32_t readLen = (bytesToSkip > sizeof(trash)) ? sizeof(trash) : bytesToSkip;
            unsigned long timeout = millis();
            while (!stream->available() && http.connected())
            {
                if (millis() - timeout > 10000) return false;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            int r = stream->read(trash, readLen);
            if (r > 0) {
                bytesToSkip -= r;
                pos += r;
            } else {
                return false;
            }
        }
        return true;
    }

    virtual bool close() override
    {
        http.end();
        opened = false;
        return true;
    }
    virtual bool isOpen() override { return opened; }
    virtual uint32_t getSize() override { return size; }
    virtual uint32_t getPos() override { return pos; }
};

// This function takes a WAV file URL, streams it using the custom
// AudioFileSourceInsecureHTTPS class, and plays it through the I2S audio output
// while allowing for dynamic volume adjustments based on the global configuration
void playAudioResponse(const char *wav_url)
{
    // CRITICAL UX SHIFT: Do NOT dispatch the SPEAKING face yet.
    // The previous network function (takePicture or recordAudio) already set the face to THINKING.
    // We maintain the THINKING state to mask the audio downloading latency.
    state.isPlayingAudio = true;
    dispatchTelemetryUpdate();

    // Audio Output Setup
    out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    out->SetPinout(I2S_SPK_BCLK, I2S_SPK_LRC, I2S_SPK_DIN);
    out->SetGain(((float)globalConfig.volume / 100.0f) * 4.0f);
    out->SetOutputModeMono(true);

    file = new AudioFileSourceInsecureHTTPS(wav_url);
    
    // SAFETY CHECK: Verify the stream is actually alive
    if (!file->isOpen()) {
        state.isPlayingAudio = false;
        dispatchTelemetryUpdate();
        dispatchMoodUpdate(WAITING);
        delete file;
        delete out;
        return;
    }

    // 128KB WAREHOUSE INJECTION (Asynchronous Stockpiling)
    // Allocates exactly 131,072 bytes (4 seconds of raw PCM) directly into the PSRAM.
    // The moment this is instantiated, an invisible FreeRTOS task begins violently 
    // sucking data out of the Wi-Fi antenna to fill the RAM.
    buff = new AudioFileSourceBuffer(file, 131072); 
    
    // PRE-FETCH LATENCY SHIELD
    // We intentionally halt the main execution thread for 2.5 seconds. 
    // Since the face is still "THINKING," the user assumes the AI is still generating.
    // In reality, the AI is done, and the ESP32 is physically stockpiling the audio payload.
    vTaskDelay(pdMS_TO_TICKS(2500));

    wav = new AudioGeneratorWAV();

    // Start streaming and playing the audio response, while monitoring the global volume for real-time adjustments
    if (wav->begin(buff, out))
    {
        // PERFECT SYNCHRONIZATION
        // Now that the 128KB warehouse is primed and the WAV engine is engaging, 
        // we forcefully snap the UI to the SPEAKING state exactly as the sound waves hit the room.
        dispatchMoodUpdate(SPEAKING);

        uint8_t last_applied_vol = globalConfig.volume;

        while (wav->isRunning())
        {
            if (!wav->loop()) {
                wav->stop();
                if (out) out->stop();
            } else {
                // If the volume has changed in the global configuration, update the audio output gain in real-time without interrupting playback
                uint8_t current_vol = globalConfig.volume;
                if (current_vol != last_applied_vol) {
                    out->SetGain(((float)current_vol / 100.0f) * 4.0f);
                    last_applied_vol = current_vol;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    } else {
        if (out) out->stop(); 
    }

    // MEMORY DEALLOCATION
    // Deleting "out" triggers the destructor which gracefully uninstalls the I2S driver.
    // This physically drops the pins to 0V, perfectly silencing the amplifier without an MMU panic.
    delete wav;
    delete buff;
    delete file;
    delete out;

    state.isPlayingAudio = false;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(WAITING);
}

// This function captures an image from the camera, uploads it to the vision API endpoint, and handles the response to trigger audio playback if an audio URL is returned in the response
void takePictureAndSend()
{
    dispatchMoodUpdate(POMODORO_FOCUS); 
    
    // UI Countdown
    for (int i = 3; i > 0; i--)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d...", i);
        dispatchNotification(buf);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    dispatchNotification("SNAP!");

    // Grab frame from camera
    camera_fb_t *fb = esp_camera_fb_get();
    
    // Clear the notification text area so "SNAP!" doesn't sit on the screen forever
    dispatchNotification(" "); 

    if (!fb)
    {
        return;
    }

    // Upload the JPEG buffer directly to the Vision endpoint
    String jsonResponse = "";
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        state.isConnectedToServer = true;
        dispatchTelemetryUpdate();
        dispatchMoodUpdate(THINKING);

        http.setTimeout(30000);
        http.begin(client, String("https://") + API_HOST + VISION_API_PATH);

        http.addHeader("Content-Type", "image/jpeg");
        
        // This is perfectly bridging with the new Route.ts patch
        http.addHeader("x-user-id", TEST_USER_ID);
        if (currentChatId.length() > 0)
            http.addHeader("x-chat-id", currentChatId);

        int httpResponseCode = http.POST(fb->buf, fb->len);
        if (httpResponseCode == 200 || httpResponseCode == 201)
        {
            jsonResponse = http.getString();
        }
        else
        {
            state.isConnectedToServer = false;
            dispatchTelemetryUpdate();
        }
        http.end();
        
        if (httpResponseCode <= 0) {
            handleNetworkError("HTTP Request Failed/Timed Out");
            return;
        }

        if (httpResponseCode != 200) {
            handleNetworkError("Server returned non-200 status");
            return;
        }
    }

    // Return the buffer back to the camera driver pool
    esp_camera_fb_return(fb);

    // Handle the enpoint response, if it contains an audio URL, trigger the audio playback function to play the response through the speaker
    if (jsonResponse.length() > 0)
    {
        // Allocated dynamically for vision payload safety
        JsonDocument resDoc; 
        if (!deserializeJson(resDoc, jsonResponse) && resDoc.containsKey("audio_url"))
        {
            playAudioResponse(resDoc["audio_url"]);
        }
    }
}

// This function is the main logic task that runs on Core 0, responsible for
// handling the wake word detection, recording audio from the microphone,
// streaming it to the API, and processing the response to trigger UI updates and audio playback
void recordAndSendAudioToAPI()
{
    state.isListening = true;
    dispatchTelemetryUpdate();

    SystemEvent wakeEv;
    wakeEv.type = WAKE_WORD_DETECTED;

    // Send the wake word detected event to the UI to trigger any UI updates
    xQueueSend(eventQueue, &wakeEv, portMAX_DELAY);

    // Check WiFi connection before attempting to stream audio, if not connected, update state and return early
    if (WiFi.status() != WL_CONNECTED)
    {
        state.isListening = false;
        dispatchTelemetryUpdate();
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    // Attempt to connect to the API server, if connection fails, update state and return early to prevent further issues
    if (!client.connect(API_HOST, 443))
    {
        state.isConnectedToServer = false;
        state.isListening = false;
        dispatchTelemetryUpdate();
        return;
    }

    state.isConnectedToServer = true;
    dispatchTelemetryUpdate();

    client.print(String("POST ") + AUDIO_API_PATH + " HTTP/1.1\r\n");
    client.print(String("Host: ") + API_HOST + "\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print(String("x-user-id: ") + TEST_USER_ID + "\r\n");

    // If there's an active chat session, include the chat ID in the headers to maintain context in the conversation with the AI
    if (currentChatId.length() > 0)
        client.print("x-chat-id: " + currentChatId + "\r\n");

    client.print("Transfer-Encoding: chunked\r\n");
    client.print("Connection: close\r\n\r\n");

    int32_t *temp_i2s_chunk = (int32_t *)malloc(chunk_size * sizeof(int32_t));
    int16_t *pcm_16_chunk = (int16_t *)malloc(chunk_size * sizeof(int16_t));

    // If memory allocation for the audio buffers fails, clean up and return early to prevent crashes
    if (!temp_i2s_chunk || !pcm_16_chunk)
    {
        if (temp_i2s_chunk) free(temp_i2s_chunk);
        if (pcm_16_chunk) free(pcm_16_chunk);
        client.stop();
        return;
    }

    unsigned long start_time = millis();
    int silent_chunks = 0;
    int max_silent_chunks = (SILENCE_TIMEOUT_MS * 16) / chunk_size;
    size_t bytes_read = 0;

    // Read audio data from the microphone in chunks, convert it to 16-bit PCM, and stream it to the API using chunked transfer encoding
    while (millis() - start_time < MAX_RECORD_TIME_MS)
    {
        i2s_read(I2S_PORT_MIC, temp_i2s_chunk, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);

        size_t samples_read = bytes_read / sizeof(int32_t);
        long sum_amplitude = 0;

        // Convert the 32-bit I2S audio data to 16-bit PCM format
        for (size_t i = 0; i < samples_read; i++)
        {
            pcm_16_chunk[i] = (int16_t)(temp_i2s_chunk[i] >> 14);
            sum_amplitude += abs(pcm_16_chunk[i]);
        }

        int avg_volume = sum_amplitude / samples_read;
        if (avg_volume < SILENCE_THRESHOLD)
            silent_chunks++;
        else
            silent_chunks = 0;

        size_t bytes_to_send = samples_read * sizeof(int16_t);
        client.print(String(bytes_to_send, HEX) + "\r\n");
        client.write((uint8_t *)pcm_16_chunk, bytes_to_send);
        client.print("\r\n");

        if (silent_chunks >= max_silent_chunks)
            break;
    }

    client.print("0\r\n\r\n");

    state.isListening = false;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(THINKING);

    String jsonResponse = "";
    unsigned long timeout = millis();

    // Read the response from the endpoint, looking for a JSON payload that contains the AI's response, and update the current chat ID if provided in the response for maintaining conversation context
    while (client.connected() && millis() - timeout < 20000)
    {
        if (client.available())
        {
            String line = client.readStringUntil('\n');
            if (line.startsWith("{"))
                jsonResponse = line;
            timeout = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    client.stop();
    free(temp_i2s_chunk);
    free(pcm_16_chunk);

    bool shouldTakePicture = false;

    if (jsonResponse.length() > 0)
    {
        JsonDocument resDoc;
        DeserializationError error = deserializeJson(resDoc, jsonResponse);
        if (!error)
        {
            if (resDoc.containsKey("chat_id"))
                currentChatId = resDoc["chat_id"].as<String>();
            if (resDoc.containsKey("action") && resDoc["action"] == "TAKE_PICTURE")
                shouldTakePicture = true;
            if (resDoc.containsKey("audio_url"))
                playAudioResponse(resDoc["audio_url"]);
        }
    }
    else
    {
        state.isConnectedToServer = false;
        dispatchTelemetryUpdate();
    }

    if (shouldTakePicture)
        takePictureAndSend();

    dispatchMoodUpdate(WAITING);
}

// This is the main network task that runs on Core 0, responsible for
// initializing the WiFi connection, setting up the I2S microphone input,
// running the wake word detection loop, and handling the logic for recording
// and sending audio to the API when the wake word is detected
void networkTask(void *pvParameters)
{
    setupWiFi();
    dispatchMoodUpdate(WAITING);

    // I2S Microphone Setup
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = EI_CLASSIFIER_FREQUENCY,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 16,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = -1,
        .data_in_num = I2S_MIC_SD};

    i2s_driver_install(I2S_PORT_MIC, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT_MIC, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT_MIC);

    inference_buffer = (int16_t *)malloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
    i2s_read_buff = (int32_t *)malloc(chunk_size * sizeof(int32_t));

    unsigned long last_trigger_time = 0;
    size_t bytes_read = 0;

    // The main loop continuously reads audio data from the I2S microphone,
    // processes it through the wake word detection model, and triggers the
    // recording and sending of audio to the API when a wake word is detected
    // with sufficient confidence, while also implementing a cooldown mechanism
    // to prevent multiple triggers in quick succession
    while (true)
    {
        i2s_read(I2S_PORT_MIC, i2s_read_buff, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        int samples_read = bytes_read / sizeof(int32_t);

        // Shift the existing audio data in the inference buffer to make room
        // for the new samples, effectively creating a sliding window of audio data for
        // the wake word detection model
        numpy::roll(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -samples_read);

        // A pointer to the start of the new data in the inference buffer, where
        // the latest audio samples will be written after conversion from 32-bit
        // I2S format to 16-bit PCM format
        int16_t *new_data_start = inference_buffer + (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - samples_read);

        // Convert the 32-bit I2S audio data to 16-bit PCM format
        for (int i = 0; i < samples_read; i++)
        {
            new_data_start[i] = (int16_t)(i2s_read_buff[i] >> 14);
        }

        // Calculate the average amplitude of the audio samples in the inference
        // buffer to use as a simple measure of volume, which can be used to
        // filter out low-volume noise and prevent false triggers of the wake
        // word detection
        long sum_amplitude = 0;
        for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++)
        {
            sum_amplitude += abs(inference_buffer[i]);
        }
        int avg_buffer_volume = sum_amplitude / EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

        // Set up the signal structure for the wake word detection model, providing a pointer to the inference buffer and the function to retrieve audio data for processing
        signal_t signal;
        signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        signal.get_data = &microphone_audio_signal_get_data;

        ei_impulse_result_t result = {0};
        EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);

        // If the inference engine returns an error, skip this iteration and
        // wait for the next batch of audio data to process, while also adding a
        // small delay to prevent spamming the CPU with continuous inference
        // attempts in case of persistent errors
        if (r != EI_IMPULSE_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Iterate through the classification results to check for the presence
        // of the wake word with sufficient confidence, and if detected, trigger
        // the recording and sending of audio to the API, while also
        // implementing a cooldown mechanism to prevent multiple triggers in
        // quick succession
        //> Sliding window approach with cooldown to prevent multiple triggers from a single wake word utterance
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
        {
            if (strcmp(result.classification[ix].label, "wake_word") == 0 ||
                strcmp(result.classification[ix].label, "hey_buddy") == 0)
            {

                if (result.classification[ix].value > CONFIDENCE_THRESHOLD && avg_buffer_volume > SILENCE_THRESHOLD)
                {
                    unsigned long now = millis();
                    if (now - last_trigger_time > COOLDOWN_MS)
                    {
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

void hardwareTask(void *pvParameters)
{
    Serial.println("Info: [Hardware] Hardware Task started on Core 1");

    pinMode(BUTTON_1, INPUT_PULLDOWN);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    while (!state.isConnectedToWifi)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    unsigned long network_connect_time = millis();
    vTaskDelay(pdMS_TO_TICKS(2000));

    float ema_vol = analogRead(VOLUME_POT_PIN);

    unsigned long button_press_start = 0;
    unsigned long button_last_high = 0;

    bool shield_dropped_notified = false;
    unsigned long last_busy_toast_time = 0;

    while (true)
    {
        int batt_pct = 100;

        if (abs(batt_pct - state.batteryLevel) >= 2)
        {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            state.batteryLevel = batt_pct;
            xSemaphoreGive(dataMutex);
            dispatchTelemetryUpdate();

            if (batt_pct <= 5 && state.mood != LOW_BATTERY)
            {
                dispatchMoodUpdate(LOW_BATTERY);
                Serial.println("Warning: [POWER] Critical voltage detected! PB IC kill-switch imminent.");
            }
        }

        int raw_vol = analogRead(VOLUME_POT_PIN);

        ema_vol = (0.2 * raw_vol) + (0.8 * ema_vol);
        int vol_pct = map((int)ema_vol, 0, 4095, 100, 0);
        vol_pct = constrain(vol_pct, 0, 100);

        if (abs(vol_pct - globalConfig.volume) >= 3)
        {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            globalConfig.volume = vol_pct;
            xSemaphoreGive(dataMutex);
            dispatchTelemetryUpdate();
        }

        bool isSystemBusy = state.isListening || state.isConnectedToServer || state.isPlayingAudio;
        bool isShieldUp = (millis() - network_connect_time <= 8000);

        if (!isShieldUp && !shield_dropped_notified)
        {
            dispatchNotification("System Ready!");
            shield_dropped_notified = true;
        }

        if (!isShieldUp && !isSystemBusy)
        {
            if (digitalRead(BUTTON_1) == HIGH)
            {
                if (button_press_start == 0)
                {
                    button_press_start = millis();
                    Serial.println("Debug: [SYSTEM] Button just went HIGH. Setting the chronological anchor.");
                }
                button_last_high = millis();

                if (millis() - button_press_start >= 5000)
                {
                    Serial.println("Info: [SYSTEM] 5-Second Active-High hold verified. Awaiting physical release...");

                    dispatchMoodUpdate(SLEEPING);

                    unsigned long trap_start = millis();
                    bool valid_release = false;

                    while (digitalRead(BUTTON_1) == HIGH)
                    {
                        vTaskDelay(pdMS_TO_TICKS(50));

                        if (millis() - trap_start > 10000)
                        {
                            Serial.println("Error: [SYSTEM] Pin HIGH for >10s. Hardware short detected! Aborting.");
                            break;
                        }
                    }

                    if (digitalRead(BUTTON_1) == LOW)
                    {
                        valid_release = true;
                    }

                    if (valid_release)
                    {
                        Serial.println("Info: [SYSTEM] Button released cleanly. Executing hardware kill-switch.");
                        vTaskDelay(pdMS_TO_TICKS(100));

                        gpio_hold_en((gpio_num_t)BUTTON_1);
                        gpio_deep_sleep_hold_en();
                        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 1);
                        esp_deep_sleep_start();
                    }
                    else
                    {
                        button_press_start = 0;
                        dispatchMoodUpdate(WAITING);
                    }
                }
            }
            else
            {
                if (millis() - button_last_high > 250)
                {
                    if (button_press_start != 0)
                    {
                        Serial.println("Debug: [SYSTEM] Anchor vaporized (Button Released).");
                    }
                    button_press_start = 0; 
                }
            }
        }
        else
        {
            button_press_start = 0;
            button_last_high = millis();

            if (digitalRead(BUTTON_1) == HIGH)
            {
                if (millis() - last_busy_toast_time > 3000)
                {
                    last_busy_toast_time = millis();
                    if (isShieldUp)
                    {
                        dispatchNotification("Booting... Please Wait.");
                    }
                    else if (isSystemBusy)
                    {
                        dispatchNotification("System Busy!");
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}