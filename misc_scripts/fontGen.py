import os
import sys
import time
import logging
from PIL import Image, ImageFont, ImageDraw

# --- Configuration ---
FONT_FILE = "/home/Fabio/main/Downloads/unifont-17.0.04.otf"
FONT_SIZE = 36
OUTPUT_FILE = "src/faces_data.h"

TEXT_COLOR = (203, 166, 247) 
BG_COLOR = (0, 0, 0) # TFT_BLACK

FACES = {
    "SLEEPING": "(ᴗ˳ᴗ)ᶻzZ",
    "AWAKENING": "(≖‿‿≖)",
    "LISTENING": "( ⚆_⚆) ၊၊||၊",
    "THINKING": "(✜‿‿✜)",
    "SPEAKING": "(◕‿‿◕)🎤︎︎",
    "HAPPY": "( ˶ᵔ ᵕ ᵔ˶)",
    "POMODORO_FOCUS": "(╭ರ_•́) ⌕",
    "SAD_ERROR": "(⁠╥⁠﹏⁠╥⁠) ⚠︎",
    "CONFUSED": "(·•᷄ࡇ•᷅ )",
    "SUCCESS": "ദ്ദി◝ ⩊ ◜.ᐟ",
    "LOW_BATTERY": "( ꩜ ᯅ ꩜;)⁭ᵎᵎ",
    "SASSY": "(≖. ≖\")",
    "WAITING": "( •̯́ ₃ •̯̀)…",
    "LOVE": "₍₍⚞(˶>ᗜ<˶)⚟⁾⁾",
    "SURPRISED": "(˶°ㅁ°)!!",
    "LAUGHING": "ꉂ(˵˃ ᗜ ˂˵)",
    "CHEERING": "(੭˃ᴗ˂)੭",
    "DEBUGGING": "(#__#)",
    "FALLBACK": "(☓‿‿☓)"
}

# --- Strict Logger ---
class StrictFormatter(logging.Formatter):
    FORMATS = {
        logging.DEBUG: "Debug: %(message)s",
        logging.INFO: "Info: %(message)s",
        logging.WARNING: "Warning: %(message)s",
        logging.ERROR: "Error: %(message)s",
        logging.CRITICAL: "Critical: %(message)s"
    }
    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)

logger = logging.getLogger("AssetGen")
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setFormatter(StrictFormatter())
logger.addHandler(ch)

# --- Zero-Dependency Progress Bar ---
def print_progress_bar(iteration, total, prefix='Progress:', suffix='Complete', decimals=1, length=50, fill='█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filled_length = int(length * iteration // total)
    bar = fill * filled_length + '-' * (length - filled_length)
    sys.stdout.write(f'\r{prefix} |{bar}| {percent}% {suffix}')
    sys.stdout.flush()
    if iteration == total:
        print()

def main():
    logger.info("Initializing JIT Sprite Rasterizer...")
    
    if not os.path.exists(FONT_FILE):
        logger.error(f"Cannot find '{FONT_FILE}'. Please drop a .ttf file into the directory.")
        return

    try:
        font = ImageFont.truetype(FONT_FILE, FONT_SIZE)
    except Exception as e:
        logger.error(f"Font loading failed: {e}")
        return

    # 1. Calculate the Universal Bounding Box (Crucial to prevent UI ghosting)
    logger.debug("Calculating maximum memory bounds for DMA alignment...")
    dummy_img = Image.new("RGB", (1, 1), BG_COLOR)
    draw = ImageDraw.Draw(dummy_img)
    
    max_w, max_h = 0, 0
    for face in FACES.values():
        bbox = draw.textbbox((0, 0), face, font=font)
        w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        if w > max_w: max_w = w
        if h > max_h: max_h = h

    # Pad the edges and ensure even numbers for memory alignment
    max_w += 16
    max_h += 16
    if max_w % 2 != 0: max_w += 1
    if max_h % 2 != 0: max_h += 1

    logger.info(f"Universal Bounding Box locked at: {max_w}x{max_h} pixels.")

    # 2. Render and Serialize
    sprite_data = {}
    total_faces = len(FACES)
    
    for i, (name, face) in enumerate(FACES.items()):
        # Render perfectly centered into the fixed boundary
        img = Image.new("RGB", (max_w, max_h), BG_COLOR)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), face, font=font)
        text_w, text_h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        x = (max_w - text_w) // 2 - bbox[0]
        y = (max_h - text_h) // 2 - bbox[1]
        
        draw.text((x, y), face, font=font, fill=TEXT_COLOR)

        # Extract to 16-bit RGB565 memory format
        rgb565_array = []
        for (r, g, b) in img.getdata():
            val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            rgb565_array.append(f"0x{val:04X}")
            
        sprite_data[name] = rgb565_array
        
        print_progress_bar(i + 1, total_faces, prefix='Rasterizing:', suffix=f'({name})', length=40)
        time.sleep(0.02) # Fluid terminal animation

    # 3. Compile the C++ Header File
    logger.debug(f"Compiling memory arrays to {OUTPUT_FILE}...")
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    
    try:
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            f.write("#pragma once\n#include <stdint.h>\n\n")
            f.write(f"// Auto-Generated RGB565 Kaomoji Sprites (Catppuccin Mocha Mauve)\n")
            f.write(f"// Memory Layout: {max_w}x{max_h} pixels\n")
            f.write(f"#define FACE_WIDTH {max_w}\n")
            f.write(f"#define FACE_HEIGHT {max_h}\n\n")

            for name, array in sprite_data.items():
                f.write(f"const uint16_t image_face_{name.lower()}[{len(array)}] PROGMEM = {{\n    ")
                for i, hex_val in enumerate(array):
                    f.write(f"{hex_val}, ")
                    if (i + 1) % 12 == 0:
                        f.write("\n    ")
                f.write("\n};\n\n")
        logger.info(f"Success! {len(FACES)} sprites compiled to {OUTPUT_FILE}.")
    except Exception as e:
        logger.error(f"Failed to write header file: {e}")

if __name__ == "__main__":
    main()