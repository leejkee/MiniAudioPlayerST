#!/usr/bin/env python3
r"""
Scan a Windows directory for filenames, extract unique CJK characters,
and generate SSD1315-compatible 16x16 Chinese font C arrays.

Usage:
    python tools/scan_dir_font.py E:\music                    # scan directory
    python tools/scan_dir_font.py E:\music --preview           # + preview
    python tools/scan_dir_font.py E:\music E:\more_music       # multiple dirs
    python tools/scan_dir_font.py --chars "阴天快乐"            # explicit list

Output:
    App/include/font_file_cn.h  — header + lookup function
    App/src/font_file_cn.c      — font_file_cn_16x16[N][32] + lookup table

This is separate from font_cn (UI labels) — intended for SD card filenames.
At runtime, if a character is not found in font_cn, fall back to font_file_cn.
"""

import argparse
import os
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: Pillow required.")
    print("       pip install Pillow")
    sys.exit(1)

# ---- Config ---------------------------------------------------------------

FONT_PATH = "C:\\Windows\\Fonts\\simsun.ttc"
FONT_INDEX = 0
CHAR_W = 16
CHAR_H = 16
PAGES = CHAR_H // 8
BYTES_PER_GLYPH = PAGES * CHAR_W

CJK_RE = re.compile(r'[一-鿿㐀-䶿豈-﫿]')

# ---- Core rendering (shared with generate_cn_font.py) ---------------------

def render_char(char, font, scale=1):
    """Render a CJK character at 16x16, return column-major bitmap (32 bytes)."""
    if scale > 1:
        big_w = CHAR_W * scale
        big_h = CHAR_H * scale
        canvas = Image.new("L", (big_w, big_h), 0)
        draw = ImageDraw.Draw(canvas)
        bbox = draw.textbbox((0, 0), char, font=font)
        gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        x_off = (big_w - gw) // 2 - bbox[0]
        y_off = (big_h - gh) // 2 - bbox[1]
        draw.text((x_off, y_off), char, font=font, fill=255)
        img = canvas.resize((CHAR_W, CHAR_H), Image.LANCZOS)
    else:
        img = Image.new("L", (CHAR_W, CHAR_H), 0)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), char, font=font)
        gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        x_off = (CHAR_W - gw) // 2 - bbox[0]
        y_off = (CHAR_H - gh) // 2 - bbox[1]
        draw.text((x_off, y_off), char, font=font, fill=255)

    bitmap = bytearray(BYTES_PER_GLYPH)
    for col in range(CHAR_W):
        for page in range(PAGES):
            byte_val = 0
            for bit in range(8):
                row = page * 8 + bit
                if img.getpixel((col, row)) > 128:
                    byte_val |= 1 << bit
            bitmap[page * CHAR_W + col] = byte_val
    return bytes(bitmap)


# ---- Directory scanning ---------------------------------------------------

def scan_dir_cjk(dirs):
    """Walk directories and extract unique CJK chars from all filenames."""
    chars = set()
    file_count = 0
    for d in dirs:
        if not os.path.isdir(d):
            print(f"  [skip] not a directory: {d}")
            continue
        for root, _dirs, files in os.walk(d):
            for fname in files:
                file_count += 1
                found = set(CJK_RE.findall(fname))
                if found:
                    chars.update(found)
        print(f"  {d}: {file_count} files scanned")
    return chars, file_count


# ---- C code generation ---------------------------------------------------

def generate_header(count):
    return f"""#ifndef __FONT_FILE_CN_H__
#define __FONT_FILE_CN_H__

#ifdef __cplusplus
extern "C" {{
#endif

#include <stdint.h>

/* 16x16 Chinese font for SD card filenames — {count} glyphs
 * Format: column-major, 2 pages x 16 cols = 32 bytes/glyph
 * Total: {count * BYTES_PER_GLYPH} bytes
 */
#define FONT_FILE_CN_CHAR_W    16
#define FONT_FILE_CN_CHAR_H    16
#define FONT_FILE_CN_COUNT     {count}

extern const uint8_t font_file_cn_16x16[FONT_FILE_CN_COUNT][{BYTES_PER_GLYPH}];

/* Lookup: Unicode code point -> font_file_cn_16x16 index, returns 0xFF if not found */
uint8_t font_file_cn_lookup(uint16_t unicode);

#ifdef __cplusplus
}}
#endif

#endif /* __FONT_FILE_CN_H__ */
"""


def generate_source(glyphs):
    """Generate font_file_cn.c with bitmap data and sorted lookup table."""
    n = len(glyphs)

    lines = []
    lines.append('#include "font_file_cn.h"')
    lines.append("")
    lines.append(f"/* 16x16 Chinese font for SD card filenames — {n} glyphs */")
    lines.append("")

    # Bitmap array
    lines.append(
        f"const uint8_t font_file_cn_16x16[FONT_FILE_CN_COUNT][{BYTES_PER_GLYPH}] = {{"
    )
    for i, (cp, bitmap) in enumerate(glyphs):
        ch = chr(cp)
        lines.append(f"    /* {i:3d}  U+{cp:04X} '{ch}' */")
        # Flatten all 32 bytes into a single {byte0, ..., byte31} initializer
        all_bytes = ", ".join(f"0x{b:02X}" for b in bitmap)
        lines.append(f"    {{{all_bytes}}},")
    lines.append("};")
    lines.append("")

    # Sorted lookup table
    lines.append("/* Lookup table: sorted by Unicode code point for binary search */")
    lines.append("static const struct { uint16_t code; uint8_t idx; } cn_lut[] = {")
    for i, (cp, _) in enumerate(glyphs):
        ch = chr(cp)
        lines.append(f"    {{0x{cp:04X}, {i:3d}}},  /* U+{cp:04X} '{ch}' */")
    lines.append("};")
    lines.append("")
    lines.append("#define CN_LUT_SIZE (sizeof(cn_lut) / sizeof(cn_lut[0]))")
    lines.append("")

    # Binary search
    lines.append("""uint8_t font_file_cn_lookup(uint16_t unicode)
{
    uint8_t lo = 0, hi = CN_LUT_SIZE - 1;
    while (lo <= hi) {
        uint8_t mid = (lo + hi) / 2;
        if (cn_lut[mid].code == unicode) return cn_lut[mid].idx;
        if (cn_lut[mid].code < unicode) lo = mid + 1;
        else                           hi = mid - 1;
    }
    return 0xFF; /* not found */
}""")

    return "\n".join(lines) + "\n"


# ---- Main ---------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Scan directory filenames → Chinese font C arrays for SSD1315"
    )
    parser.add_argument(
        "dirs", nargs="*", default=None,
        help="Directories to scan for filenames"
    )
    parser.add_argument(
        "--chars", default=None,
        help="Additional explicit characters to include"
    )
    parser.add_argument(
        "--font", default=FONT_PATH,
        help=f"TrueType font path (default: {FONT_PATH})"
    )
    parser.add_argument(
        "--out-dir", default=None,
        help="Output dir (default: ../firmware/MiniAudioPlayerST/App)"
    )
    parser.add_argument(
        "--preview", action="store_true",
        help="Write preview.txt"
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.join(script_dir, "..")

    out_dir = args.out_dir or os.path.join(
        repo_root, "firmware", "MiniAudioPlayerST", "App"
    )
    out_dir = os.path.normpath(out_dir)

    # ---- Collect characters from filenames ----
    chars = set()

    if args.chars:
        chars.update(CJK_RE.findall(args.chars))

    if args.dirs:
        found, file_count = scan_dir_cjk(args.dirs)
        chars.update(found)
    elif not args.chars:
        print("ERROR: specify at least one directory or use --chars")
        sys.exit(1)

    if not chars:
        print("ERROR: no CJK characters found in filenames")
        sys.exit(1)

    sorted_chars = sorted(chars, key=ord)
    n = len(sorted_chars)

    print(f"\nCJK chars from filenames: {n}")
    print(f"Flash usage: {n * BYTES_PER_GLYPH} bytes (bitmap) + {n * 3} bytes (LUT)")
    print(f"Chars: {''.join(sorted_chars)}")
    print()

    # ---- Render ----
    if not os.path.exists(args.font):
        print(f"ERROR: font not found: {args.font}")
        sys.exit(1)

    font = ImageFont.truetype(args.font, index=FONT_INDEX, size=14)

    glyphs = []
    for ch in sorted_chars:
        bitmap = render_char(ch, font, scale=1)
        glyphs.append((ord(ch), bitmap))
        print(f"  Rendered U+{ord(ch):04X} '{ch}'")

    # ---- Output ----
    include_dir = os.path.join(out_dir, "include")
    src_dir = os.path.join(out_dir, "src")
    os.makedirs(include_dir, exist_ok=True)
    os.makedirs(src_dir, exist_ok=True)

    h_path = os.path.join(include_dir, "font_file_cn.h")
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_header(n))
    print(f"\n[OK] {h_path}")

    c_path = os.path.join(src_dir, "font_file_cn.c")
    with open(c_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_source(glyphs))
    print(f"[OK] {c_path}")

    if args.preview:
        # ASCII-art preview
        preview_path = os.path.join(out_dir, "font_file_cn_preview.txt")
        chars_per_row = 8
        with open(preview_path, "w", encoding="utf-8") as f:
            f.write(f"Filename font preview: {n} glyphs\n\n")
            for bs in range(0, n, chars_per_row):
                block = glyphs[bs:bs + chars_per_row]
                for cp, _ in block:
                    f.write(f"  U+{cp:04X}     ")
                f.write("\n")
                for cp, _ in block:
                    f.write(f"   '{chr(cp)}'       ")
                f.write("\n")
                for row in range(CHAR_H):
                    page, bit = row // 8, row % 8
                    for cp, bitmap in block:
                        for col in range(CHAR_W):
                            on = (bitmap[page * CHAR_W + col] >> bit) & 1
                            f.write("#" if on else ".")
                        f.write(" ")
                    f.write("\n")
                f.write("\n")
        print(f"[OK] Preview: {preview_path}")

    print(f"\nDone. {n} glyphs from filenames, {n * BYTES_PER_GLYPH} bytes.")


if __name__ == "__main__":
    main()
