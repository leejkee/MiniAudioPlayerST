#!/usr/bin/env python3
"""
Generate Chinese 16x16 font C arrays for SSD1315 OLED.

Scans C source files for Chinese characters, renders each with Pillow +
system font (SimSun), and outputs column-major font data.

Usage:
    python tools/generate_cn_font.py                    # scan App/src/ for CJK chars
    python tools/generate_cn_font.py --scan main.c      # scan specific files
    python tools/generate_cn_font.py --chars "播放暂停"  # explicit character list
    python tools/generate_cn_font.py --preview           # write preview.txt

Output files:
    App/include/font_cn.h  — header + lookup function
    App/src/font_cn.c      — font_cn_16x16[N][32] + code→index table

Format: column-major, 16×16 (2 pages × 16 columns = 32 bytes/glyph).
    bytes 0~15:  upper page (rows 0~7),  each byte = 8 vertical pixels
    bytes 16~31: lower page (rows 8~15), each byte = 8 vertical pixels
    bit 0 = top of page, bit 7 = bottom of page
"""

import argparse
import os
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: Pillow required for Chinese font rendering.")
    print("       pip install Pillow")
    sys.exit(1)

# ---- Font config -----------------------------------------------------------

FONT_PATH = "C:\\Windows\\Fonts\\simsun.ttc"   # 宋体
FONT_INDEX = 0                                  # ttc index 0 = SimSun
CHAR_W = 16
CHAR_H = 16
PAGES = CHAR_H // 8                             # 2
BYTES_PER_GLYPH = PAGES * CHAR_W                # 32

# CJK Unified Ideographs range (simplified Chinese is mostly here)
CJK_RE = re.compile(r'[一-鿿㐀-䶿豈-﫿]')

# UI strings that must be included regardless of source scan
REQUIRED_CHARS = set(
    "播放暂停音量调节模式菜单歌曲名称时间文件列表"
    "上一曲下一曲顺序循环随机单曲"
    "无误卡解码器错误拔出挂载失败"
    "正在读取扫描初始化完成等待"
)


# ---- Core ---------------------------------------------------------------

def scan_cjk_chars(file_paths):
    """Extract unique CJK characters from given files (UTF-8 assumed)."""
    chars = set()
    for path in file_paths:
        try:
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
        except (OSError, UnicodeDecodeError):
            print(f"  [skip] {path}")
            continue
        found = set(CJK_RE.findall(text))
        chars.update(found)
        if found:
            print(f"  {path}: {len(found)} CJK chars")
    return chars


def find_c_files(base_dir):
    """Walk directory tree and return .c/.h file paths, skipping generated font files."""
    skip = {"font_en.c", "font_en.h", "font_cn.c", "font_cn.h"}
    paths = []
    for root, _dirs, files in os.walk(base_dir):
        for f in files:
            if f.endswith((".c", ".h")) and f not in skip:
                paths.append(os.path.join(root, f))
    return paths


def render_char(char, font, scale=1):
    """Render a CJK character at 16x16, return column-major bitmap.

    When scale > 1, uses supersampling: render at scale× larger then
    downscale with LANCZOS. Set scale=1 for direct rendering (recommended
    for CJK since thin strokes disappear with downscaling).
    """
    if scale > 1:
        big_w = CHAR_W * scale
        big_h = CHAR_H * scale
        canvas = Image.new("L", (big_w, big_h), 0)
        draw = ImageDraw.Draw(canvas)
        bbox = draw.textbbox((0, 0), char, font=font)
        gw = bbox[2] - bbox[0]
        gh = bbox[3] - bbox[1]
        x_off = (big_w - gw) // 2 - bbox[0]
        y_off = (big_h - gh) // 2 - bbox[1]
        draw.text((x_off, y_off), char, font=font, fill=255)
        img = canvas.resize((CHAR_W, CHAR_H), Image.LANCZOS)
    else:
        img = Image.new("L", (CHAR_W, CHAR_H), 0)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), char, font=font)
        gw = bbox[2] - bbox[0]
        gh = bbox[3] - bbox[1]
        x_off = (CHAR_W - gw) // 2 - bbox[0]
        y_off = (CHAR_H - gh) // 2 - bbox[1]
        draw.text((x_off, y_off), char, font=font, fill=255)

    # Convert to column-major, per-page format
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


# ---- C code generation --------------------------------------------------

def generate_header(count):
    return f"""#ifndef __FONT_CN_H__
#define __FONT_CN_H__

#ifdef __cplusplus
extern "C" {{
#endif

#include <stdint.h>

/* 16x16 Chinese font, {count} glyphs
 * Format: column-major, 2 pages x 16 cols = 32 bytes/glyph
 * Total: {count * BYTES_PER_GLYPH} bytes
 */
#define FONT_CN_CHAR_W    16
#define FONT_CN_CHAR_H    16
#define FONT_CN_COUNT     {count}

extern const uint8_t font_cn_16x16[FONT_CN_COUNT][{BYTES_PER_GLYPH}];

/* Lookup: Unicode code point -> font_cn_16x16 index, returns 0xFF if not found */
uint8_t font_cn_lookup(uint16_t unicode);

#ifdef __cplusplus
}}
#endif

#endif /* __FONT_CN_H__ */
"""


def generate_source(glyphs):
    """Generate font_cn.c with bitmap data and sorted lookup table."""
    # glyphs: list of (code_point, bitmap_bytes) sorted by code point
    n = len(glyphs)

    lines = []
    lines.append('#include "font_cn.h"')
    lines.append("")
    lines.append(f"/* 16x16 Chinese font, {n} glyphs */")
    lines.append("")

    # Bitmap array
    lines.append(
        f"const uint8_t font_cn_16x16[FONT_CN_COUNT][{BYTES_PER_GLYPH}] = {{"
    )
    for i, (cp, bitmap) in enumerate(glyphs):
        ch = chr(cp)
        lines.append(f"    /* {i:3d}  U+{cp:04X} '{ch}' */")
        # Flatten all 32 bytes into a single {byte0, ..., byte31} initializer
        all_bytes = ", ".join(f"0x{b:02X}" for b in bitmap)
        lines.append(f"    {{{all_bytes}}},")
    lines.append("};")
    lines.append("")

    # Sorted lookup table: array of {code_point, index} pairs
    lines.append("/* Lookup table: sorted by Unicode code point for binary search */")
    lines.append("static const struct { uint16_t code; uint8_t idx; } cn_lut[] = {")
    for i, (cp, _) in enumerate(glyphs):
        ch = chr(cp)
        lines.append(f"    {{0x{cp:04X}, {i:3d}}},  /* U+{cp:04X} '{ch}' */")
    lines.append("};")
    lines.append("")
    lines.append("#define CN_LUT_SIZE (sizeof(cn_lut) / sizeof(cn_lut[0]))")
    lines.append("")

    # Binary search lookup function
    lines.append("""uint8_t font_cn_lookup(uint16_t unicode)
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


def write_preview_txt(path, glyphs):
    """Write ASCII-art preview of all rendered glyphs (no deps)."""
    chars_per_row = 8
    n = len(glyphs)

    with open(path, "w", encoding="utf-8") as f:
        f.write(f"Chinese font preview: {n} glyphs, 16x16\n\n")

        for block_start in range(0, n, chars_per_row):
            block = glyphs[block_start : block_start + chars_per_row]
            bn = len(block)

            # Labels
            for cp, _ in block:
                f.write(f"  U+{cp:04X}     ")
            f.write("\n")
            for cp, _ in block:
                f.write(f"   '{chr(cp)}'       ")
            f.write("\n")

            # Pixel rows
            for row in range(CHAR_H):
                page = row // 8
                bit = row % 8
                for cp, bitmap in block:
                    for col in range(CHAR_W):
                        byte_val = bitmap[page * CHAR_W + col]
                        on = (byte_val >> bit) & 1
                        f.write("█" if on else "·")
                    f.write(" ")
                f.write("\n")
            f.write("\n")

    print(f"[OK] Preview: {path}")


# ---- Main ---------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate SSD1315 Chinese 16x16 font C arrays"
    )
    parser.add_argument(
        "--scan", nargs="*", default=None,
        help="File paths or dirs to scan for CJK chars (default: App/src/)"
    )
    parser.add_argument(
        "--no-scan", action="store_true",
        help="Skip source scanning, use only --chars and REQUIRED_CHARS"
    )
    parser.add_argument(
        "--chars", default=None,
        help="Additional explicit characters to include"
    )
    parser.add_argument(
        "--font", default=FONT_PATH,
        help=f"TrueType/TrueTypeCollection font path (default: {FONT_PATH})"
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

    # Resolve output dir
    out_dir = args.out_dir or os.path.join(
        repo_root, "firmware", "MiniAudioPlayerST", "App"
    )
    out_dir = os.path.normpath(out_dir)

    # ---- Collect characters ----
    chars = set(REQUIRED_CHARS)

    if args.no_scan:
        pass  # skip all file scanning
    elif args.scan is not None:
        for target in args.scan:
            target_path = os.path.normpath(target)
            if not os.path.isabs(target_path):
                target_path = os.path.join(repo_root, target_path)
            if os.path.isdir(target_path):
                files = find_c_files(target_path)
            else:
                files = [target_path] if os.path.exists(target_path) else []
            print(f"Scanning {len(files)} files from: {target_path}")
            chars.update(scan_cjk_chars(files))
    else:
        # Default: scan App/ directory
        app_src = os.path.join(
            repo_root, "firmware", "MiniAudioPlayerST", "App"
        )
        files = find_c_files(app_src)
        print(f"Scanning {len(files)} files from: {app_src}")
        chars.update(scan_cjk_chars(files))

    if args.chars:
        chars.update(args.chars)

    # Filter to CJK only
    chars = {c for c in chars if CJK_RE.match(c)}

    if not chars:
        print("ERROR: no CJK characters found. Use --chars to specify.")
        sys.exit(1)

    # Sort by code point for binary search
    sorted_chars = sorted(chars, key=ord)
    n = len(sorted_chars)

    print(f"\nTotal unique CJK chars: {n}")
    print(f"Flash usage: {n * BYTES_PER_GLYPH} bytes (bitmap) + {n * 3} bytes (LUT)")
    print(f"Chars: {''.join(sorted_chars)}")
    print()

    # ---- Render ----
    if not os.path.exists(args.font):
        print(f"ERROR: font not found: {args.font}")
        print("  Install SimSun or specify --font <path>")
        sys.exit(1)

    font = ImageFont.truetype(args.font, index=FONT_INDEX, size=14)  # 14pt fits 16x16 cell best

    glyphs = []  # (code_point, bitmap)
    for ch in sorted_chars:
        bitmap = render_char(ch, font, scale=1)
        glyphs.append((ord(ch), bitmap))
        print(f"  Rendered U+{ord(ch):04X} '{ch}'")

    # ---- Output ----
    include_dir = os.path.join(out_dir, "include")
    src_dir = os.path.join(out_dir, "src")
    os.makedirs(include_dir, exist_ok=True)
    os.makedirs(src_dir, exist_ok=True)

    h_path = os.path.join(include_dir, "font_cn.h")
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_header(n))
    print(f"\n[OK] {h_path}")

    c_path = os.path.join(src_dir, "font_cn.c")
    with open(c_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_source(glyphs))
    print(f"[OK] {c_path}")

    if args.preview:
        preview_path = os.path.join(out_dir, "font_cn_preview.txt")
        write_preview_txt(preview_path, glyphs)

    print(f"\nDone. {n} glyphs, {n * BYTES_PER_GLYPH} bytes bitmap data.")


if __name__ == "__main__":
    main()
