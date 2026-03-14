#!/usr/bin/env python3

import sys
import os
import argparse
import colorsys
from PIL import Image

def rp6502_pack_tile_bpp4(p1, p2):
    # Pack two 4-bit pixels into one byte
    # Pixel 1 in high nibble, Pixel 2 in low nibble
    return ((p1 & 0x0F) << 4) | (p2 & 0x0F)
    # return (p1 & 0x0F) | ((p2 & 0x0F) << 4) 

def rp6502_rgb_sprite_bpp16(r, g, b, a):
    if a < 128:
        return 0
    else:
        # 16-bit RGB555 (1 bit alpha, 5 red, 5 green, 5 blue)
        return ((((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3)) | 1 << 5)

def convert_image(image_path, output_path, mode):
    try:
        with Image.open(image_path) as im:
            # We need the original image for Palette/Index data
            # And an RGB version for Sprite (16-bit) conversion
            rgb_im = im.convert("RGBA")
            width, height = im.size

            print(f"Processing: {image_path} ({width}x{height})")
            
            # --- BITMAP MODE (8-bit) ---
            if mode == 'bitmap':
                # ... (Keep your existing bitmap logic if needed, omitted here for brevity)
                # For safety, let's just exit if they use bitmap mode with this snippet,
                # or you can paste your old bitmap logic back here.
                print("Bitmap mode logic skipped in this fix. Use tile/sprite.")
                return

            # --- SPRITE / TILE MODES ---
            sprite_size = height
            
            if width % height != 0:
                print(f"Error: Image width ({width}) must be a multiple of height ({height}).")
                sys.exit(1)
                
            num_frames = width // height
            print(f"Layout:     {num_frames} frames of {sprite_size}x{sprite_size}")
            print(f"Output:     {output_path} [{mode}]")

            with open(output_path, "wb") as o:
                for i in range(num_frames):
                    base_x = i * sprite_size
                    
                    # === MODE: TILE (4-bit Index) ===
                    if mode == 'tile':
                        if sprite_size % 2 != 0:
                            print("Error: Sprite size must be even for Tile mode.")
                            sys.exit(1)
                        
                        # Ensure we are using indices
                        if im.mode != 'P':
                            print("WARNING: 'tile' mode requires an Indexed (Palette) PNG.")
                            print("         Attempting to auto-quantize to 16 colors...")
                            use_im = im.convert("P", palette=Image.ADAPTIVE, colors=16)
                        else:
                            use_im = im

                        for y in range(sprite_size):
                            for x in range(base_x, base_x + sprite_size, 2):
                                # Check alpha from ORIGINAL RGB image to enforce transparency
                                r1, g1, b1, a1 = rgb_im.getpixel((x, y))
                                r2, g2, b2, a2 = rgb_im.getpixel((x+1, y))

                                if a1 < 128:
                                    p1 = 0 # Force transparent
                                else:
                                    p1 = use_im.getpixel((x, y))
                                
                                if a2 < 128:
                                    p2 = 0 # Force transparent
                                else:
                                    p2 = use_im.getpixel((x+1, y))
                                
                                # Pack the indices
                                o.write(rp6502_pack_tile_bpp4(p1, p2).to_bytes(1, "little"))
                                
                    # === MODE: SPRITE (16-bit Color) ===
                    else:
                        for y in range(sprite_size):
                            for x in range(base_x, base_x + sprite_size):
                                r, g, b, a = rgb_im.getpixel((x, y))
                                val = rp6502_rgb_sprite_bpp16(r, g, b, a)
                                o.write(val.to_bytes(2, "little"))
            
            print("Done.")

    except FileNotFoundError:
        print(f"Error: File '{image_path}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"An error occurred: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Convert images to RP6502 binary format.")
    parser.add_argument("input_file", help="Input PNG image.")
    parser.add_argument("-o", "--output", help="Output BIN file.")
    parser.add_argument("--mode", choices=['sprite', 'tile', 'bitmap'], default='sprite', 
                        help="Mode: 'sprite' (16-bit), 'tile' (4-bit indices), or 'bitmap' (8-bit).")

    args = parser.parse_args()

    if not args.output:
        args.output = os.path.splitext(args.input_file)[0] + ".bin"

    convert_image(args.input_file, args.output, args.mode)

if __name__ == "__main__":
    main()