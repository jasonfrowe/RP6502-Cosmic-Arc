#!/usr/bin/env python3
import sys
import argparse
from PIL import Image

def rp6502_encode_color(r, g, b):
    # RP6502 16-bit color format: A BBBBB GGGGG RRRRR
    # Alpha is bit 5 (usually set to 1 for opaque colors in palette)
    return (((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3) | (1 << 5))

def extract_palette(image_path):
    try:
        im = Image.open(image_path)
    except Exception as e:
        print(f"Error opening {image_path}: {e}")
        return

    colors = []
    
    if im.mode == 'P':
        # Get raw palette (flat list [r,g,b, r,g,b...])
        raw_pal = im.getpalette()
        
        # Calculate how many actual colors are in the file
        if raw_pal:
            num_entries = len(raw_pal) // 3
            # Read up to 16 colors, or however many exist
            limit = min(16, num_entries)
            
            for i in range(limit):
                r = raw_pal[i*3]
                g = raw_pal[i*3+1]
                b = raw_pal[i*3+2]
                colors.append((r, g, b))
        else:
            print("Error: Image is P mode but has no palette data.")
            return
            
    else:
        # RGB Mode fallback
        print("Warning: Image is RGB. Extracting unique colors...")
        unique = im.getcolors(maxcolors=256)
        if not unique:
             print("Error: Could not determine colors.")
             return
        
        # Format is (count, (r,g,b))
        for i in range(min(16, len(unique))):
             colors.append(unique[i][1])

    # --- PADDING ---
    # Fill remaining slots with Black (0,0,0) up to 16
    while len(colors) < 16:
        colors.append((0,0,0))

    # --- GENERATE C CODE ---
    print(f"// Palette extracted from {image_path}")
    print(f"// Place in xram_map.h:")
    print(f"#define PALETTE_ADDR 0xFF58")
    print(f"")
    print(f"// Place in your init function:")
    print(f"uint16_t tile_palette[16] = {{")
    
    for idx, c in enumerate(colors):
        r, g, b = c[0], c[1], c[2]
        val = rp6502_encode_color(r, g, b)
        
        comment = ""
        if idx == 0: comment = " // Index 0 (Transparent)"
        elif idx >= len(colors) - (16 - len(colors)) and r==0 and g==0 and b==0:
            comment = " // Padding"
            
        print(f"    0x{val:04X}, {comment}")
    
    print(f"}};")
    print(f"")
    print(f"RIA.addr0 = PALETTE_ADDR;")
    print(f"RIA.step0 = 1;")
    print(f"for (int i = 0; i < 16; i++) {{")
    print(f"    RIA.rw0 = tile_palette[i] & 0xFF;")
    print(f"    RIA.rw0 = tile_palette[i] >> 8;")
    print(f"}}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: ./extract_palette.py <image.png>")
    else:
        extract_palette(sys.argv[1])