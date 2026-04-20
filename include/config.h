#pragma once

// -- ui_renderer Configuration

// Display CCnfiguration
#define TFT_WIDTH 480
#define TFT_HEIGHT 320
#define BG_X 10
#define BG_Y 22
#define BG_WIDTH 460
#define BG_HEIGHT 276
#define EXPECTED_BG_SIZE (BG_WIDTH * BG_HEIGHT * 2) 
#define TFT_MAUVE 0xCD3E 


// -- logic_fn Configuration

// AI Inference Config
#define CONFIDENCE_THRESHOLD 0.80f   

// Device Behavior Config
#define COOLDOWN_MS 5000 

// Audio Recording Config
#define MAX_RECORD_TIME_MS 15000   
#define SILENCE_TIMEOUT_MS 1500    
#define SILENCE_THRESHOLD 150      
