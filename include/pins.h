#pragma once

// -- Camera Pins for the Freenove ESP32 S-3 WROOM
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

// -- I2S Audio: Speaker (MAX98357A)
#define I2S_SPK_BCLK      41
#define I2S_SPK_LRC       42  // Word Select
#define I2S_SPK_DIN       40  // Data In

// -- I2S Audio: Microphone (INMP441)
#define I2S_MIC_SCK       39  // Bit Clock
#define I2S_MIC_WS        38  // Word Select
#define I2S_MIC_SD        47  // Serial Data
#define I2S_PORT_MIC I2S_NUM_0

// -- User Inputs
#define BUTTON_1          1   // Built-in Boot Button (Use as input)
#define VOLUME_POT_PIN    2  // Connected to Potentiometer Wiper

// -- Battery voltage monitoring pin
// #define BATTERY_PIN         // Connected to Voltage Divider

// -- SPI Display (ILI9488)
//* Edit the file in .pio/libdeps/{Your model}/TFT_eSPI/User_Setup.h
//* Mine is in .pio/libdeps/freenove_esp32_s3_wroom/TFT_eSPI/User_Setup.h
//? Why the fuck isnt this shit automatic, cuz its annoying
/*
    # define ILI9488_DRIVER     // WARNING: Do not connect ILI9488 display SDO to MISO if other devices share the SPI bus (TFT SDO does NOT tristate when CS is high)
    ! A Warning from me, use 9488, FUCK ITS BEEN A WHOLE FUCKING GHOUR ABSHJFJHIOELGVJEIQLO:
    * Anyways, use HSPI ports also

    #define TFT_MOSI          48
    #define TFT_SCLK          45
    #define TFT_CS            14  // Chip Select
    #define TFT_DC            21  // Data/Command
    #define TFT_RST           46  // Reset
    #define TFT_BL            3   // Backlight (PWM capable)
*/