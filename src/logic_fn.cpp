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

// Audio Libraries
#include "AudioFileSource.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
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
void dispatchCameraCountdown(const char *text)
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
    Serial.println("Info: [NETWORK] Connecting to WiFi");
    wifiManager.setConfigPortalTimeout(1000000000);
    wifiManager.setBreakAfterConfig(false);

    while (!state.isConnectedToWifi)
    {
        if (!wifiManager.autoConnect(WIFI_SSID, WIFI_PASSWORD))
        {
            if (wifiRetryCount < WIFI_RETRY_LIMIT)
            {
                state.isConnectedToWifi = false;
                dispatchTelemetryUpdate();
                wifiRetryCount += 1;
                delay(2000);
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
    Serial.println("Info: [NETWORK] WiFi connected!");
    Serial.println("Info: [NETWORK] Syncing NTP (Singapore Time)...");
    configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

// Custom AudioFileSource class to handle streaming WAV audio from HTTPS endpoints without blocking the main thread
/*>
    Since the standard AudioFileSourceHTTPStream doesn't support HTTPS or has
    blocking calls that can cause audio stuttering, this custom class uses
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
        if (!opened)
            return 0;

        // Get the underlying stream from the HTTP client
        WiFiClient *stream = http.getStreamPtr();
        if (!stream)
        {
            return 0;
        }

        // Read data in a loop until we've read the requested length or the connection is closed
        uint32_t bytesRead = 0;
        uint8_t *buf = (uint8_t *)data;
        unsigned long timeout = millis();

        while (bytesRead < len && http.connected())
        {
            if (stream->available())
            {
                // Read data from the stream into the buffer
                int r = stream->read(buf + bytesRead, len - bytesRead);

                // If we read some data, update the bytesRead count and reset the timeout
                if (r > 0)
                {
                    bytesRead += r;
                    pos += r;
                    timeout = millis();
                }
            }
            else
            {
                // If no data is available, check for a timeout to prevent stalling
                if (millis() - timeout > 5000)
                    break;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
        return bytesRead;
    }

    // This method provides a non-blocking way to read audio data, which can be used by the audio generator to fetch data without stalling the main thread
    virtual uint32_t readNonBlock(void *data, uint32_t len) override { return read(data, len); }

    // This method implements a simple seek mechanism by discarding data from the stream until the desired position is reached, allowing for smooth seeking without blocking
    virtual bool seek(int32_t newPos, int dir) override
    {
        // If the connection isn't open, we can't seek
        if (!opened)
            return false;

        // Calculate the new position based on the direction (0 = absolute, 1 = relative to current position)
        if (dir == 1)
            newPos = pos + newPos;

        // If the new position is before the current position, we can't seek backwards in a stream, so return false
        if (newPos < pos)
            return false;

        uint32_t bytesToSkip = newPos - pos;

        if (bytesToSkip == 0)
            return true;

        WiFiClient *stream = http.getStreamPtr();
        if (!stream)
            return false;

        uint8_t trash[128];
        while (bytesToSkip > 0 && http.connected())
        {
            uint32_t readLen = (bytesToSkip > sizeof(trash)) ? sizeof(trash) : bytesToSkip;
            unsigned long timeout = millis();
            while (!stream->available() && http.connected())
            {
                if (millis() - timeout > 5000)
                    return false;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            int r = stream->read(trash, readLen);
            if (r > 0)
            {
                bytesToSkip -= r;
                pos += r;
            }
            else
            {
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
    Serial.println("\nInfo: [SYSTEM] Playing Audio Response...");
    state.isPlayingAudio = true;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(SPEAKING);

    // Audio Output Setup
    out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    out->SetPinout(I2S_SPK_BCLK, I2S_SPK_LRC, I2S_SPK_DIN);

    // Initial Volume Setup (Float mapping 0.0 to 1.0)
    float initial_vol = (float)globalConfig.volume / 100.0f;
    out->SetGain(initial_vol);
    out->SetOutputModeMono(true);

    file = new AudioFileSourceInsecureHTTPS(wav_url);
    wav = new AudioGeneratorWAV();

    // Start streaming and playing the audio response, while monitoring the global volume for real-time adjustments
    if (wav->begin(file, out))
    {
        uint8_t last_applied_vol = globalConfig.volume;

        while (wav->isRunning())
        {
            if (!wav->loop())
            {
                wav->stop();
            }
            else
            {
                uint8_t current_vol = globalConfig.volume;

                // If the volume has changed in the global configuration, update the audio output gain in real-time without interrupting playback
                if (current_vol != last_applied_vol)
                {
                    out->SetGain((float)current_vol / 100.0f);
                    last_applied_vol = current_vol;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    delete wav;
    delete file;
    delete out;

    state.isPlayingAudio = false;
    dispatchTelemetryUpdate();
    dispatchMoodUpdate(WAITING);
}

// This function captures an image from the camera, uploads it to the vision API endpoint, and handles the response to trigger audio playback if an audio URL is returned in the response
void takePictureAndSend()
{
    Serial.println("\nInfo: [CAMERA] Capturing image and sending to Vision API...");
    dispatchMoodUpdate(POMODORO_FOCUS);

    // UI Countdown
    for (int i = 3; i > 0; i--)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d...", i);
        dispatchCameraCountdown(buf);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    dispatchCameraCountdown("SNAP!");

    // Grab frame from camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Error: [CAMERA] Frame buffer grab failed. Memory might be corrupted.");
        return;
    }
    Serial.printf("Debug: [CAMERA] Captured frame successfully: %dx%d, %d bytes\n", fb->width, fb->height, fb->len);

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
        http.addHeader("x-user-id", TEST_USER_ID);
        if (currentChatId.length() > 0)
            http.addHeader("x-chat-id", currentChatId);

        Serial.println("Info: [NETWORK] Uploading JPEG payload to Vercel...");
        int httpResponseCode = http.POST(fb->buf, fb->len);
        if (httpResponseCode == 200 || httpResponseCode == 201)
        {
            jsonResponse = http.getString();
            Serial.println("Info: [NETWORK] Vision API processed successfully.");
        }
        else
        {
            Serial.printf("Error: [NETWORK] Vision API failed with code: %d\n", httpResponseCode);
            state.isConnectedToServer = false;
            dispatchTelemetryUpdate();
        }
        http.end();
    }

    // Return the buffer back to the camera driver pool
    esp_camera_fb_return(fb);

    // Handle the enpoint response, if it contains an audio URL, trigger the audio playback function to play the response through the speaker 
    if (jsonResponse.length() > 0)
    {
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
    Serial.println("\nInfo: [SYSTEM] Wake word detected, starting audio recording...");

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
        if (temp_i2s_chunk)
            free(temp_i2s_chunk);
        if (pcm_16_chunk)
            free(pcm_16_chunk);
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
        //> The portMAX_DELAY means the task will wait indefinitely until data is avaliable
        i2s_read(I2S_PORT_MIC, temp_i2s_chunk, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY); 

        size_t samples_read = bytes_read / sizeof(int32_t);
        long sum_amplitude = 0;

        // Convert the 32-bit I2S audio data to 16-bit PCM format expected by my the website endpoint
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
    Serial.println("Info: [SYSTEM] -> Stream closed. Waiting for AI response...");

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

    i2s_zero_dma_buffer(I2S_PORT_MIC);
    dispatchMoodUpdate(WAITING);
}

// This is the main network task that runs on Core 0, responsible for
// initializing the WiFi connection, setting up the I2S microphone input,
// running the wake word detection loop, and handling the logic for recording
// and sending audio to the API when the wake word is detected
void networkTask(void *pvParameters)
{
    Serial.println("Info: [SYSTEM] Logic Task started on Core 0");

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
        .dma_buf_count = 8,
        .dma_buf_len = 512,
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

                if (result.classification[ix].value > CONFIDENCE_THRESHOLD)
                {
                    unsigned long now = millis();
                    if (now - last_trigger_time > COOLDOWN_MS)
                    {
                        last_trigger_time = now;

                        Serial.printf("\nWAKE DETECTED! (Score: %.2f)\n", result.classification[ix].value);

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

// This function is the main hardware task that runs on Core 1
void hardwareTask(void *pvParameters)
{
    Serial.println("Info: [Hardware] Hardware Task started on Core 1");

    // ADC Tuning for 3.3V range
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // Initialize the exponential moving average variables for battery and
    // volume readings to stabilize the initial readings before the looping begins
    float ema_batt = analogRead(BATTERY_PIN);
    float ema_vol = analogRead(VOLUME_POT_PIN);

    // The main loop continuously reads the battery voltage and volume
    // potentiometer values, applies an exponential moving average filter for
    // stability, maps the raw ADC values to percentage levels, and updates the
    // global state and UI when significant changes are detected, while also
    // implementing a hardware protection mechanism to warn the user of critical
    // battery levels before the power bank cuts off power
    while (true)
    {
        /**
            A fully charged 18650 battery is 4.2V. Sliced in half by two 100k resistors = 2.1V.
            2.1V on an ESP32 ADC (11dB attenuation) reads roughly 2700-2800.
            A dead 18650 is ~3.2V. Sliced in half = 1.6V.
            1.6V on the ADC reads roughly 2100.
        */

        int raw_batt = analogRead(BATTERY_PIN);
        ema_batt = (0.05 * raw_batt) + (0.95 * ema_batt);

        // Map the smoothed battery voltage reading to a percentage value
        // between 0 and 100, based on the expected voltage range of the
        // battery, and constrain it to ensure it stays within valid bounds
        int batt_pct = map((int)ema_batt, 2100, 2750, 0, 100);
        batt_pct = constrain(batt_pct, 0, 100);

        // If the battery percentage has changed significantly (by 2% or more)
        // since the last update, update the global state and signal the UI to
        // redraw the telemetry icons, while also checking for critical battery
        // levels to trigger a mood update and warning message
        if (abs(batt_pct - state.batteryLevel) >= 2)
        {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            state.batteryLevel = batt_pct;
            xSemaphoreGive(dataMutex);
            dispatchTelemetryUpdate();

            // Warn user before Power Bank IC abruptly cuts 5V power
            if (batt_pct <= 5 && state.mood != LOW_BATTERY)
            {
                dispatchMoodUpdate(LOW_BATTERY);
                Serial.println("Warning: [POWER] Critical voltage detected! PB IC kill-switch imminent.");
            }
        }

        // Read the volume potentiometer value, apply an exponential moving
        // average filter for stability, map it to a percentage value, and
        // update the global state and UI if it has changed significantly (by 3%
        // or more) since the last update
        int raw_vol = analogRead(VOLUME_POT_PIN);

        // Apply an exponential moving average filter to the raw volume reading
        // to stabilize it and prevent rapid fluctuations in the volume level
        // due to noise or small adjustments of the potentiometer
        /*>
            By the way, EMA(Exponential Moving Average) is a filtering technique
            that gives more weight to recent data points, making it more
            responsive to changes while still smoothing out noise. The alpha
            value (0.2 in this case) determines how much weight is given to the
            new data versus the previous EMA value. A higher alpha makes it more
            responsive, while a lower alpha makes it smoother.
            
            The formula for EMA is:
            EMA_current = (alpha * New_Data) + ((1 - alpha) * EMA_previous
        */
        ema_vol = (0.2 * raw_vol) + (0.8 * ema_vol);

        int vol_pct = map((int)ema_vol, 0, 4095, 0, 100);
        vol_pct = constrain(vol_pct, 0, 100);

        if (abs(vol_pct - globalConfig.volume) >= 3)
        {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            globalConfig.volume = vol_pct;
            xSemaphoreGive(dataMutex);
            dispatchTelemetryUpdate();
        }

        // Yield to the UI renderer (20Hz Polling Rate/ 50ms delay)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}