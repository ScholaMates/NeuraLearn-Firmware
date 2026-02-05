#pragma once

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

#define BATTERY_PIN       1  // Connected to Voltage Divider
#define VOLUME_POT_PIN    2  // Connected to Potentiometer Wiper

// --- I2S Audio: Speaker (MAX98357A) ---
#define I2S_SPK_BCLK      41
#define I2S_SPK_LRC       42  // Word Select
#define I2S_SPK_DIN       40  // Data In

// --- I2S Audio: Microphone (INMP441) ---
#define I2S_MIC_SCK       39  // Bit Clock
#define I2S_MIC_WS        38  // Word Select
#define I2S_MIC_SD        47  // Serial Data

// --- SPI Display (ILI9341) ---
/*

# define ILI9488_DRIVER     // WARNING: Do not connect ILI9488 display SDO to MISO if other devices share the SPI bus (TFT SDO does NOT tristate when CS is high)
- Use 9488 FUCK ITS BEEN A WHOLE FUCKING GHOUR ABSHJFJHIOELGVJEIQLO:
- Anyways, use hspi ports

#define TFT_MOSI          48
#define TFT_SCLK          45
#define TFT_CS            14  // Chip Select
#define TFT_DC            21  // Data/Command
#define TFT_RST           46  // Reset
#define TFT_BL            3   // Backlight (PWM capable)
*/

// --- User Inputs ---
#define BUTTON_1          0   // Built-in Boot Button (Use as input)
#define BUTTON_2          43  // Extra button (Optional)