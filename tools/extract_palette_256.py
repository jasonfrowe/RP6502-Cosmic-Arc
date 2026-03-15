#!/usr/bin/env python3
"""Extract a 256-entry RP6502 palette from an image.

Usage:
  python3 ./tools/extract_palette_256.py Sprites/MainScreen_tiles.png
"""

import argparse
import sys
from PIL import Image


def rp6502_encode_color(r: int, g: int, b: int) -> int:
    # RP6502 RGB555 with alpha bit set.
    return (((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3) | (1 << 5))


def palette_from_p_mode(image: Image.Image) -> list[tuple[int, int, int]]:
    raw = image.getpalette()
    if not raw:
        raise ValueError("Image is P mode but does not contain palette data")

    colors = []
    for i in range(256):
        base = i * 3
        if base + 2 < len(raw):
            colors.append((raw[base], raw[base + 1], raw[base + 2]))
        else:
            colors.append((0, 0, 0))
    return colors


def palette_from_non_p_mode(image: Image.Image) -> list[tuple[int, int, int]]:
    # Quantize to 256 colors so output is deterministic and directly usable.
    paletted = image.convert("RGBA").convert("P", palette=Image.ADAPTIVE, colors=256)
    return palette_from_p_mode(paletted)


def emit_c_palette(colors: list[tuple[int, int, int]], image_path: str) -> None:
    print(f"// 256-color palette extracted from {image_path}")
    print("// Copy into your init code")
    print("#define PALETTE_ADDR 0xFF58")
    print()
    print("uint16_t tile_palette[256] = {")

    for i, (r, g, b) in enumerate(colors):
        encoded = rp6502_encode_color(r, g, b)
        suffix = ""
        if i == 0:
            suffix = " // Index 0"
        print(f"    0x{encoded:04X},{suffix}")

    print("};")
    print()
    print("RIA.addr0 = PALETTE_ADDR;")
    print("RIA.step0 = 1;")
    print("for (int i = 0; i < 256; i++) {")
    print("    RIA.rw0 = tile_palette[i] & 0xFF;")
    print("    RIA.rw0 = tile_palette[i] >> 8;")
    print("}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract a 256-color RP6502 palette from an image")
    parser.add_argument("image", help="Path to source image")
    args = parser.parse_args()

    try:
        image = Image.open(args.image)
    except Exception as exc:  # pragma: no cover - CLI error path
        print(f"Error opening {args.image}: {exc}", file=sys.stderr)
        return 1

    try:
        if image.mode == "P":
            colors = palette_from_p_mode(image)
        else:
            print("Warning: image is not paletted (P). Quantizing to 256 colors.", file=sys.stderr)
            colors = palette_from_non_p_mode(image)
    except Exception as exc:  # pragma: no cover - CLI error path
        print(f"Error extracting palette: {exc}", file=sys.stderr)
        return 1

    emit_c_palette(colors, args.image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
