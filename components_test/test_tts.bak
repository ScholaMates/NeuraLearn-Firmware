#include <Arduino.h>
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <WiFiManager.h>
#include <LittleFS.h>
#include "AudioFileSourceLittleFS.h"

// --- Configuration ---

// The sentence you want the AI to say
const char* textToSpeak = "Hello, I am the NeuraLearn Study Buddy. I am ready to help you focus and learn.";

// NOTE: This example uses a free, public TTS service for testing.
// In your final project, you would replace this with a more robust service
// like Google Cloud TTS or OpenAI TTS, which require API keys.
const char* ttsUrl_base = "https://translate.google.com/translate_tts?ie=UTF-8&q=";
const char* ttsUrl_end = "&tl=en&client=tw-ob";

// Speaker Pin Configuration (using the safe pins from our last test)
#define I2S_SPEAKER_BIT_CLOCK   GPIO_NUM_41
#define I2S_SPEAKER_WORD_SELECT GPIO_NUM_42 // Also called LRC
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_40

// --- Global Objects ---
AudioGeneratorMP3 *mp3;
AudioFileSourceLittleFS *file;
AudioOutputI2S *out;

void setup() {
  Serial.begin(115200);
  WiFiManager wm;
  delay(1000);

  // --- Initialize LittleFS ---
  if (!LittleFS.begin()) {
    Serial.println("FATAL: LittleFS Mount Failed. Halting.");
    while(1);
  }

  bool res;
  res = wm.autoConnect("NeuraLearn","NeuraLearn123");

  if(!res) {
      Serial.println("Failed to connect");
      ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }

 // --- Initialize Audio Output ---
  out = new AudioOutputI2S(1, 0); 
  out->SetPinout(I2S_SPEAKER_BIT_CLOCK, I2S_SPEAKER_WORD_SELECT, I2S_SPEAKER_SERIAL_DATA);
  
  // --- Construct the Full URL ---
  String urlEncodedText = String(textToSpeak);
  urlEncodedText.replace(" ", "%20");
  String fullUrl = String(ttsUrl_base) + urlEncodedText + String(ttsUrl_end);

  // --- Download MP3 to LittleFS ---
  HTTPClient http;
  Serial.println("Starting HTTP request to download MP3...");
  if (http.begin(fullUrl)) {
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      // Open a file in LittleFS for writing
      fs::File f = LittleFS.open("/tts.mp3", "w");
      if (!f) {
          Serial.println("FATAL: Failed to open file for writing");
          return;
      }
      
      // Write the content from the stream to the file
      http.writeToStream(&f);
      f.close();
      Serial.println("MP3 file downloaded and saved to LittleFS.");

      // --- Start Playback from the File ---
      Serial.println("Starting playback from filesystem...");
      file = new AudioFileSourceLittleFS("/tts.mp3");
      mp3 = new AudioGeneratorMP3();
      mp3->begin(file, out);

    } else {
      Serial.printf("Error: HTTP request failed with code %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Error: Unable to begin HTTP connection.");
  }
}

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("Playback finished. Restarting in 10 seconds...");
      delay(10000);
      ESP.restart();
    }
  }
}
