#pragma once
#include <stdint.h>

extern const uint16_t image_Battery_1_pixels[];
extern const uint16_t image_Battery_2_pixels[];
extern const uint16_t image_Battery_4_pixels[];
extern const uint16_t image_Battery_Charging_pixels[];
extern const uint16_t image_Battery_Dead_pixels[];
extern const uint16_t image_Battery_full_pixels[];
extern const uint16_t image_Cellular_data_Disabled_pixels[];
extern const uint16_t image_download_pixels[];
extern const uint16_t image_Microphone_Enabled_pixels[];
extern const uint16_t image_Wifi_disabled_pixels[];
extern const unsigned char image_wifi_bits[];

/* show location of each icon
void draw(void) {
    // Time
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.drawString("15:10", 47, 21);
    // Podomoro_Time
    tft.setFreeFont(&FreeMono12pt7b);
    tft.drawString("00:10:20", 106, 163);
    // Edges
    tft.pushImage(7, 6, 305, 229, image_Edges_pixels);
    // Microphone_Enabled
    tft.pushImage(145, 195, 30, 32, image_Microphone_Enabled_pixels);
    // Cellular_data_Disabled
    tft.pushImage(227.5, 20, 15, 16, image_Cellular_data_Disabled_pixels);
    // Horizontal_Line_Break
    tft.drawRect(21, 36, 275, 3, 0xFFFF);
    // Wifi_disabled
    tft.pushImage(247, 20, 41, 44, image_Wifi_disabled_pixels);
    // wifi
    tft.drawBitmap(247, 20, image_wifi_bits, 19, 16, 0xFFFF);
    // Battery_Charging
    tft.pushImage(271, 21, 24, 16, image_Battery_Charging_pixels);
    // Battery_full
    tft.pushImage(271, 21, 24, 16, image_Battery_full_pixels);
    // Face
    tft.setFreeFont(&FreeMono18pt7b);
    tft.drawString("( >,,< )", 76, 102);
    // Battery_4
    tft.pushImage(271, 21, 24, 16, image_Battery_4_pixels);
    // Podomoro_Bounding_Box
    tft.drawRect(88, 161, 144, 21, 0xFFFF);
    // Battery_2
    tft.pushImage(271, 21, 24, 16, image_Battery_2_pixels);
    // Battery_1
    tft.pushImage(271, 21, 24, 16, image_Battery_1_pixels);
    // download
    tft.pushImage(145, 195, 30, 32, image_download_pixels);
    // Battery_Dead
    tft.pushImage(271, 22, 24, 45, image_Battery_Dead_pixels);
}
*/