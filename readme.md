# NeuraLearn-Firmware

<p>
<img alt="License" src="https://img.shields.io/badge/license-MIT-blue">
<img alt="Platform" src=https://img.shields.io/badge/platform-ESP32--S3-orange">
</p>

<p align="center">
  <strong>The open-source firmware for a distraction-free, AI-powered learning companion.</strong><br>
  <em>A project by <a href="https://github.com/Synapse-Labs">Synapse Labs</a></em>
</p>


## 🚀 The Problem
In a world of constant digital distraction, students need tools that help them focus, not pull them away. While phones and laptops offer access to powerful AI assistants, they are also gateways to social media, games, and notifications.

NeuraLearn is the hardware solution to this problem. It's a small, friendly device that sits on a student's desk, providing on-demand academic assistance without the distractions of a multi-purpose device. This repository contains the firmware that brings the NeuraLearn hardware to life.

## ✨ Core Features
This firmware enables the NeuraLearn device with a rich set of features designed for a focused learning experience:

- Voice-Activated Interface: Hands-free interaction initiated by a custom wake word.

- Switchable AI Tutor Personalities: Select different AI interaction modes, such as a "Socratic Tutor" that asks guiding questions or a "Direct Answer" mode.

- Dynamic Facial Expressions: A color TFT screen displays rich, animated faces to convey status (listening, thinking, speaking), making the interaction more engaging.

- Integrated Pomodoro Timer: Actively coaches students on time management and focus skills.

- Visual Problem Solving: Utilizes an onboard camera to interpret problems from physical documents and get AI-powered help.

- Automated Note-Taking: Seamlessly integrates with applications like Notion, Obsidian, or Google Docs to automatically log conversations.

- Companion App Integration: Communicates with a backend to sync history and settings with a companion web portal.

## 🛠️ Hardware Requirements
To build a NeuraLearn device using this firmware, you will need the following core components:

- MCU: An ESP32-S3 development board with a camera connector (specifically a version with 16MB Flash and 8MB PSRAM, like the N16R8).

- Display: A 2.4" or 2.8" SPI TFT Color Display with an ILI9341 driver chip.

- Camera: An OV2640 camera module with a compatible ribbon cable.

- Audio Input: An I2S Microphone module (e.g., INMP441).

- Audio Output: An I2S Amplifier module (e.g., MAX98357A) connected to a small speaker.

## ⚙️ Getting Started

#### 1. Clone the Repository
```bash
git clone [https://github.com/Synapse-Labs/NeuraLearn-Firmware.git](https://github.com/Synapse-Labs/NeuraLearn-Firmware.git)
cd NeuraLearn-Firmware
```

#### 2. Configure the Display Driver
This firmware is configured for a specific hardware setup. The most critical step is configuring the display library.

- Navigate to the `.pio/libdeps/[your_env]/TFT_eSPI/` folder.

- Open the `User_Setup.h` file.

- Ensure your pin definitions are uncommented in the ESP32 section and that `#define USE_HSPI_PORT` is also uncommented.

#### 3. Add Your Credentials
In the src folder, you will likely need to create a credentials.h file storing your Wi-Fi SSID, password, and any API keys.

#### 4. Upload Filesystem and Firmware
Place your image assets (e.g., happy.bmp, sleep.bmp) into the data folder.

Using the PlatformIO sidebar:

- Run the `"Upload Filesystem Image"` task.

- Run the `"Upload"` task to flash the main firmware.

## 🤝 Contributing
This is a student-led, open-source project. We welcome contributions of all kinds! Whether you're helping us squash bugs, improve our facial animations, or add new features, we'd love to have you. Please feel free to open an issue or submit a pull request.

## 📜 License
This project is licensed under the MIT License. See the LICENSE file for details.
