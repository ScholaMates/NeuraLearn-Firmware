# NeuraLearn

NeuraLearn is an intelligent, ESP32-S3 based hardware assistant that integrates on-device machine learning with cloud-based AI endpoints. It features wake-word detection, interactive facial UI rendering, continuous audio streaming, and computer vision analysis.

## Hardware Specifications
- **Microcontroller**: Freenove ESP32-S3 WROOM
- **Display**: ILI9488 SPI TFT Display
- **Microphone**: INMP441 (I2S)
- **Speaker**: MAX98357A (I2S)
- **Camera**: OV2640 (connected via standard ESP32 DVP pins)
- **Input**: Push button (hardware kill-switch) and Potentiometer (volume control)

## Core Features
- **On-Device AI**: Utilizes the Edge Impulse SDK for continuous wake-word detection.
- **Cloud AI Integration**: Securely streams 16-bit PCM audio and JPEG image buffers to external APIs.
- **Interactive UI**: Multi-threaded display rendering, dynamically updating faces and telemetry data (WiFi strength, battery level, server status).
- **Robust Hardware Control**: Features an exponential moving average (EMA) filtered volume control and a chronological hardware kill-switch to safely manage deep sleep states.

## Software Architecture
- **Core 0 (Logic & Network Task)**: Handles intensive networking tasks, audio recording, HTTPS chunked streaming, and computer vision uploads.
- **Core 1 (UI & Hardware Task)**: Dedicated to real-time UI rendering and critical hardware polling (battery monitoring and user inputs).

## Setup and Installation
1. Clone this repository to your local machine.
2. Ensure you have the necessary dependencies installed via PlatformIO (e.g., `TFT_eSPI`, `WiFiManager`, `ArduinoJson`, `ESP8266Audio`).
3. Configure the `TFT_eSPI` library by modifying `User_Setup.h` to match the ILI9488 driver and assigned HSPI pins.
4. Flash the LittleFS filesystem image to upload the static binary UI assets (e.g., `bg.bin`).
5. Compile and upload the firmware to the ESP32-S3.
6. On the first boot, the device will host a captive portal. Connect to the "NeuraLearn" WiFi network to provision your local network credentials.

## API Communication
The device communicates with backend services hosted at `neuralearnapp.vercel.app`. It utilizes chunked transfer encoding for real-time audio processing and handles asynchronous JSON responses to determine subsequent system actions (e.g., triggering the camera module or playing an audio response).