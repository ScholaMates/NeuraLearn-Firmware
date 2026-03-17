#pragma once
#include <stdint.h>

/* Battery Icons */
extern const uint16_t image_Battery_1_pixels[];
extern const uint16_t image_Battery_2_pixels[];
extern const uint16_t image_Battery_4_pixels[];
extern const uint16_t image_Battery_Charging_pixels[];
extern const uint16_t image_Battery_Dead_pixels[];
extern const uint16_t image_Battery_full_pixels[];
extern const uint16_t image_Cellular_data_Disabled_pixels[];
extern const unsigned char image_wifi_bits[];
extern const uint16_t image_Wifi_disabled_pixels[];
extern const uint16_t image_Microphone_disabled_pixels[];
extern const uint16_t image_Microphone_Enabled_pixels[];
extern const unsigned char image_volume_loud_bits[];
extern const unsigned char image_volume_low_bits[];
extern const unsigned char image_volume_muted_bits[];
extern const unsigned char image_volume_normal_bits[];

/* show location of each icon
void drawScreen_2(void) {
    tft.fillScreen(0x0);
    // Time
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.drawString("15:10", 47, 23);
    // Podomoro_Time
    tft.setFreeFont(&FreeMono12pt7b);
    tft.drawString("00:10:20", 179, 235);
    // Cellular_data_Disabled
    tft.pushImage(349, 20, 15, 16, image_Cellular_data_Disabled_pixels);
    // Wifi_disabled
    tft.pushImage(377, 20, 43, 44, image_Wifi_disabled_pixels);
    // wifi
    tft.drawBitmap(377, 20, image_wifi_bits, 19, 16, 0xFFFF);
    // Battery_Charging
    tft.pushImage(409, 20, 24, 16, image_Battery_Charging_pixels);
    // Battery_full
    tft.pushImage(409, 21, 24, 16, image_Battery_full_pixels);
    // Battery_4
    tft.pushImage(409, 21, 24, 16, image_Battery_4_pixels);
    // Battery_2
    tft.pushImage(409, 21, 24, 16, image_Battery_2_pixels);
    // Battery_1
    tft.pushImage(409, 20, 24, 16, image_Battery_1_pixels);
    // Battery_Dead
    tft.pushImage(409, 22, 24, 45, image_Battery_Dead_pixels);
    // MIcrophone_disabled
    tft.pushImage(225, 268, 236, 30, image_Microphone_disabled_pixels);
    // Podomoro_Bounding_Box
    tft.drawRect(162, 233, 144, 21, 0xFFFF);
    // Microphone_Enabled
    tft.pushImage(227, 268, 26, 30, image_Microphone_Enabled_pixels);
    // Face
    tft.setTextSize(2);
    tft.setFreeFont(&FreeMono18pt7b);
    tft.drawString("( >,,< )", 72, 135);
    // volume_muted
    tft.drawBitmap(114, 21, image_volume_muted_bits, 18, 16, 0xFFFF);
    // volume_low
    tft.drawBitmap(114, 21, image_volume_low_bits, 18, 16, 0xFFFF);
    // volume_loud
    tft.drawBitmap(114, 21, image_volume_loud_bits, 20, 16, 0xFFFF);
    // volume_normal
    tft.drawBitmap(114, 21, image_volume_normal_bits, 18, 16, 0xFFFF);
    // Layer 23
    tft.setTextSize(1);
    tft.setFreeFont();
    tft.drawString("yyyy/mm/dd", 47, 11);
    // Layer 23
    tft.pushImage(10, 22, 460, 276, image_Layer_23_pixels);
}

*/