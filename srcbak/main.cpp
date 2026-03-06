#include <Arduino.h>
#include <driver/i2s.h>

#include <fabiocanavarro-project-1_inferencing.h> 

// --- I2S Microphone Pins (From your pins_config.h) ---
#define I2S_MIC_SCK 39
#define I2S_MIC_WS  38
#define I2S_MIC_SD  47
#define I2S_PORT    I2S_NUM_0

// --- Tuning ---
#define CONFIDENCE_THRESHOLD 0.9f   // 70% sure it heard the wake word
#define COOLDOWN_MS 2000             // Wait 2 seconds before triggering again

// Global buffers
int16_t *inference_buffer; 
size_t chunk_size = 512;
int32_t *i2s_read_buff;
unsigned long last_trigger_time = 0;

// Edge Impulse Callback: The AI asks for audio data here
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(inference_buffer + offset, out_ptr, length);
    return 0;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Emergency Wake Word Script...");

    // 1. Setup I2S for INMP441 Microphone
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = EI_CLASSIFIER_FREQUENCY, // Automatically pulls 16000 from your AI model
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 is 24-bit padded to 32
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

    // 2. Allocate memory for the audio buffers
    inference_buffer = (int16_t*)malloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(int16_t));
    i2s_read_buff = (int32_t*)malloc(chunk_size * sizeof(int32_t));

    if (!inference_buffer || !i2s_read_buff) {
        Serial.println("❌ Memory allocation failed!");
        while(1); // Halt
    }

    Serial.println("✅ Microphone ready. Say the wake word!");
}

void loop() {
    size_t bytes_read = 0;
    
    // 1. Read Raw Audio (This blocks until the 512-sample chunk is full)
    i2s_read(I2S_PORT, i2s_read_buff, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    int samples_read = bytes_read / sizeof(int32_t);

    // 2. Slide the buffer left (Delete old audio, make room for new)
    numpy::roll(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -samples_read);

    // 3. Convert 32-bit I2S data to 16-bit PCM for the AI
    int16_t *new_data_start = inference_buffer + (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - samples_read);
    for (int i = 0; i < samples_read; i++) {
        new_data_start[i] = (int16_t)(i2s_read_buff[i] >> 14); // Standard INMP441 bit shift
    }

    // 4. Wrap the buffer in an Edge Impulse signal object
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;

    // 5. Run the AI Model!
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false); 
    
    if (r != EI_IMPULSE_OK) {
        Serial.printf("❌ AI failed with code: %d\n", r);
        return;
    }

    // 6. Check if it heard you
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        // We check for 'wake_word' or whatever you named it. Change this string if needed!
        if (strcmp(result.classification[ix].label, "wake_word") == 0 || 
            strcmp(result.classification[ix].label, "hey_buddy") == 0) {
            
            if (result.classification[ix].value > CONFIDENCE_THRESHOLD) {
                unsigned long now = millis();
                if (now - last_trigger_time > COOLDOWN_MS) {
                    last_trigger_time = now;
                    
                    // THIS IS YOUR SUCCESS MOMENT
                    Serial.printf("\n============================\n");
                    Serial.printf("✨ WAKE DETECTED! (Score: %.2f)\n", result.classification[ix].value);
                    Serial.printf("============================\n\n");
                }
            }
        }
    }
}