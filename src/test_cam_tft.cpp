#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- Starting Camera to TFT Live Viewfinder Test ---");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("FATAL: Camera init failed with error 0x%x\n", err);
    tft.setTextColor(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Camera Init Failed", 160, 120);
    while(1);
  }
  Serial.println("SUCCESS: Camera initialized!");
}

void loop() {
    Serial.println("Capturing frame...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
Serial.println("Frame captured, displaying...");
  tft.startWrite();
  Serial.println("Pushing image to TFT...");
  tft.pushImage(0, 0, fb->width, fb->height, (uint16_t *)fb->buf);
  Serial.println("Image displayed.");
  tft.endWrite();
  Serial.println("TFT write complete.");

  Serial.println("Returning frame buffer to camera...");
  esp_camera_fb_return(fb);

  delay(1);
}

