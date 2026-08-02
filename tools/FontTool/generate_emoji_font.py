#!/usr/bin/env python3
"""Generate multi-size monochrome Unicode icon fonts for SSD1315 OLED.

The generated bitmaps use the same page/column layout as the existing font
tools.  One glyph is addressed by one Unicode scalar value; emoji sequences
that contain ZWJ or combining characters are intentionally unsupported.

Examples:
    python tools/FontTool/generate_emoji_font.py
    python tools/FontTool/generate_emoji_font.py --small-size 12 --large-size 16
    python tools/FontTool/generate_emoji_font.py --codepoints U+25B6 U+23F8
    python tools/FontTool/generate_emoji_font.py --codepoints U+23EE U+23EF U+23ED
    python tools/FontTool/generate_emoji_font.py --font C:\\path\\icons.ttf

Output:
    tmp/FontTool/include/font_icon.h
    tmp/FontTool/src/font_icon.c

Every glyph uses a 16x16 cell and occupies exactly two OLED pages. The selected
visual size is centered inside that cell. Bytes are ordered by page, then
column; bit 0 is the top pixel in each 8-row page.
"""

import argparse
import os
import sys
import unicodedata

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: Pillow is required for Unicode icon rendering.")
    print("       pip install Pillow")
    sys.exit(1)


DEFAULT_CODEPOINTS = (
    0x23EE,  # last track
    0x23EF,  # play/pause
    0x23F8,  # pause
    0x23F9,  # stop
    0x23ED,  # next track
    0x25B6,  # play
)
CELL_WIDTH = 16
CELL_HEIGHT = 16
CELL_PAGES = CELL_HEIGHT // 8
BYTES_PER_GLYPH = CELL_PAGES * CELL_WIDTH
DEFAULT_SMALL_SIZE = 12
DEFAULT_LARGE_SIZE = 16
FONT_CANDIDATES = (
    r"C:\Windows\Fonts\seguisym.ttf",  # monochrome Segoe UI Symbol
    r"C:\Windows\Fonts\seguiemj.ttf",  # Segoe UI Emoji fallback
)
IGNORED_SELECTORS = {0xFE0E, 0xFE0F}
ZWJ = 0x200D
EMOJI_MODIFIERS = range(0x1F3FB, 0x1F400)
REGIONAL_INDICATORS = range(0x1F1E6, 0x1F200)
TAG_CHARACTERS = range(0xE0020, 0xE0080)


def parse_codepoint(value):
    """Parse U+25B6, 0x25B6, or 25B6 as a Unicode code point."""
    raw = value.strip().upper()
    if raw.startswith("U+"):
        raw = raw[2:]
    elif raw.startswith("0X"):
        raw = raw[2:]
    try:
        codepoint = int(raw, 16)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"invalid Unicode code point: {value}"
        ) from exc
    if codepoint > 0x10FFFF or 0xD800 <= codepoint <= 0xDFFF:
        raise argparse.ArgumentTypeError(
            f"not a Unicode scalar value: {value}"
        )
    return codepoint


def codepoint_label(codepoint):
    return f"U+{codepoint:04X}"


def collect_codepoints(chars, explicit_codepoints):
    codepoints = set()
    candidates = list(explicit_codepoints or ())
    for char in chars or "":
        candidates.append(ord(char))

    for codepoint in candidates:
        if codepoint in IGNORED_SELECTORS:
            continue
        char = chr(codepoint)
        if (codepoint == ZWJ or unicodedata.combining(char) or
                codepoint in EMOJI_MODIFIERS or
                codepoint in REGIONAL_INDICATORS or
                codepoint in TAG_CHARACTERS):
            raise ValueError(
                "emoji sequences are unsupported; provide one standalone "
                "Unicode code point per icon"
            )
        codepoints.add(codepoint)
    return sorted(codepoints)


def find_default_font():
    for path in FONT_CANDIDATES:
        if os.path.isfile(path):
            return path
    return FONT_CANDIDATES[0]


def validate_visual_sizes(small_size, large_size):
    """Validate two visual bounds within the fixed 16x16 storage cell."""
    if not 1 <= small_size <= CELL_WIDTH:
        raise ValueError("small size must be between 1 and 16")
    if not 1 <= large_size <= CELL_WIDTH:
        raise ValueError("large size must be between 1 and 16")
    if small_size >= large_size:
        raise ValueError("small size must be less than large size")


def render_icon(codepoint, font_path, font_index, visual_size, padding,
                threshold):
    """Render one Unicode scalar centered in a fixed 16x16 cell."""
    inner_size = visual_size - padding * 2
    if inner_size <= 0:
        raise ValueError(
            f"padding {padding} leaves no pixels in size {visual_size}"
        )

    # Render large first, crop the actual ink bounds, then scale to a stable
    # pixel box. This makes the requested small/large size describe centered
    # visual bounds, independent of a font's ascent/descent metrics.
    render_size = max(64, visual_size * 4)
    font = ImageFont.truetype(font_path, index=font_index, size=render_size)
    canvas_side = render_size * 3
    canvas = Image.new("RGBA", (canvas_side, canvas_side), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    text = chr(codepoint)
    bbox = draw.textbbox((0, 0), text, font=font)
    x = (canvas_side - (bbox[2] - bbox[0])) // 2 - bbox[0]
    y = (canvas_side - (bbox[3] - bbox[1])) // 2 - bbox[1]
    # Ask Pillow for the font's monochrome outline. Rendering embedded color
    # layers and keeping only alpha would turn colored button emoji into solid
    # rectangles and lose their inner symbol.
    draw.text((x, y), text, font=font, fill=(255, 255, 255, 255))

    alpha = canvas.getchannel("A")
    ink_bbox = alpha.getbbox()
    if ink_bbox is None:
        raise ValueError(
            f"font produced an empty glyph for {codepoint_label(codepoint)}"
        )
    ink = alpha.crop(ink_bbox)
    scale = min(inner_size / ink.width, inner_size / ink.height)
    target_w = max(1, round(ink.width * scale))
    target_h = max(1, round(ink.height * scale))
    resampling = getattr(Image, "Resampling", Image)
    ink = ink.resize((target_w, target_h), resampling.LANCZOS)

    image = Image.new("L", (CELL_WIDTH, CELL_HEIGHT), 0)
    image.paste(ink, (
        (CELL_WIDTH - target_w) // 2,
        (CELL_HEIGHT - target_h) // 2,
    ))

    bitmap = bytearray(BYTES_PER_GLYPH)
    for page in range(CELL_PAGES):
        for col in range(CELL_WIDTH):
            value = 0
            for bit in range(8):
                row = page * 8 + bit
                if image.getpixel((col, row)) >= threshold:
                    value |= 1 << bit
            bitmap[page * CELL_WIDTH + col] = value
    if not any(bitmap):
        raise ValueError(
            f"threshold removed every pixel for {codepoint_label(codepoint)}"
        )
    return bytes(bitmap)


def generate_header(codepoints, small_size, large_size):
    return f"""#ifndef FONT_ICON_H
#define FONT_ICON_H

#ifdef __cplusplus
extern \"C\" {{
#endif

#include <stdint.h>

/* Two-state monochrome Unicode icon font.
 * Format: page-major then column-major, bit 0 = top row of each page.
 * Both arrays use the same Unicode index and fixed 16x16 storage cell.
 */
#define FONT_ICON_GLYPH_COUNT {len(codepoints)}
#define FONT_ICON_NOT_FOUND   UINT16_MAX
#define FONT_ICON_CELL_WIDTH  {CELL_WIDTH}
#define FONT_ICON_CELL_HEIGHT {CELL_HEIGHT}
#define FONT_ICON_CELL_PAGES  {CELL_PAGES}
#define FONT_ICON_BYTES_PER_GLYPH {BYTES_PER_GLYPH}
#define FONT_ICON_SMALL_VISUAL_SIZE {small_size}
#define FONT_ICON_LARGE_VISUAL_SIZE {large_size}

extern const uint8_t
font_icon_small_16x16[FONT_ICON_GLYPH_COUNT][FONT_ICON_BYTES_PER_GLYPH];
extern const uint8_t
font_icon_large_16x16[FONT_ICON_GLYPH_COUNT][FONT_ICON_BYTES_PER_GLYPH];

/* Returns the glyph index, or FONT_ICON_NOT_FOUND. */
uint16_t font_icon_lookup(uint32_t unicode);

#ifdef __cplusplus
}}
#endif

#endif /* FONT_ICON_H */
"""


def format_bitmap(bitmap):
    return ", ".join(f"0x{value:02X}" for value in bitmap)


def append_bitmap_array(lines, symbol, codepoints, glyphs):
    lines.append(
        f"const uint8_t {symbol}"
        "[FONT_ICON_GLYPH_COUNT][FONT_ICON_BYTES_PER_GLYPH] = {"
    )
    for index, codepoint in enumerate(codepoints):
        lines.append(
            f"    /* {index:3d}  {codepoint_label(codepoint)} */"
        )
        lines.append(f"    {{{format_bitmap(glyphs[codepoint])}}},")
    lines.extend(("};", ""))


def generate_source(codepoints, small_glyphs, large_glyphs, header_filename):
    lines = [f'#include "{header_filename}"', ""]
    append_bitmap_array(
        lines, "font_icon_small_16x16", codepoints, small_glyphs
    )
    append_bitmap_array(
        lines, "font_icon_large_16x16", codepoints, large_glyphs
    )

    lines.append("static const uint32_t font_icon_codepoints[FONT_ICON_GLYPH_COUNT] = {")
    for codepoint in codepoints:
        lines.append(f"    0x{codepoint:08X}UL,")
    lines.extend(("};", ""))

    lines.append("""uint16_t font_icon_lookup(uint32_t unicode)
{
    int32_t lo = 0;
    int32_t hi = (int32_t)FONT_ICON_GLYPH_COUNT - 1;

    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        uint32_t codepoint = font_icon_codepoints[mid];
        if (codepoint == unicode) { return (uint16_t)mid; }
        if (codepoint < unicode) { lo = mid + 1; }
        else                     { hi = mid - 1; }
    }
    return FONT_ICON_NOT_FOUND;
}""")
    return "\n".join(lines) + "\n"


def bitmap_pixel(bitmap, x, y):
    return (bitmap[(y // 8) * CELL_WIDTH + x] >> (y % 8)) & 1


def write_preview(path, codepoints, sizes, glyphs):
    with open(path, "w", encoding="utf-8", newline="\n") as output:
        for size in sizes:
            output.write(
                f"Unicode icon preview: {size}x{size} centered in 16x16\n\n"
            )
            for codepoint in codepoints:
                output.write(f"{codepoint_label(codepoint)}\n")
                bitmap = glyphs[size][codepoint]
                for row in range(CELL_HEIGHT):
                    output.write("".join(
                        "#" if bitmap_pixel(bitmap, col, row) else "."
                        for col in range(CELL_WIDTH)
                    ))
                    output.write("\n")
                output.write("\n")


def main():
    parser = argparse.ArgumentParser(
        description="Generate small/large SSD1315 Unicode icon font arrays"
    )
    parser.add_argument(
        "--chars", default=None,
        help="standalone Unicode icon characters (variation selectors ignored)"
    )
    parser.add_argument(
        "--codepoints", nargs="*", type=parse_codepoint, default=None,
        help="Unicode values such as U+25B6, 0x23F8, or 1F50A"
    )
    parser.add_argument(
        "--small-size", type=int, default=DEFAULT_SMALL_SIZE,
        help="small visual size centered in the 16x16 cell (default: 12)"
    )
    parser.add_argument(
        "--large-size", type=int, default=DEFAULT_LARGE_SIZE,
        help="large visual size centered in the 16x16 cell (default: 16)"
    )
    parser.add_argument(
        "--font", default=None,
        help="TrueType/OpenType font path (default: Segoe UI Symbol/Emoji)"
    )
    parser.add_argument(
        "--font-index", type=int, default=0,
        help="face index for a font collection (default: 0)"
    )
    parser.add_argument(
        "--padding", type=int, default=0,
        help="blank border inside the selected visual size (default: 0)"
    )
    parser.add_argument(
        "--threshold", type=int, default=128,
        help="1-bit alpha threshold, 1..255 (default: 128)"
    )
    parser.add_argument(
        "--out-dir", default=None,
        help="output root directory (default: tmp/FontTool)"
    )
    parser.add_argument(
        "--output-basename", default="font_icon",
        help="generated .h/.c basename (default: font_icon)"
    )
    parser.add_argument(
        "--preview", action="store_true",
        help="write <output-basename>_preview.txt in the output root"
    )
    args = parser.parse_args()

    try:
        validate_visual_sizes(args.small_size, args.large_size)
        if not args.output_basename.isidentifier() or not args.output_basename.isascii():
            raise ValueError(
                "output basename must be an ASCII C identifier"
            )
        if args.padding < 0:
            raise ValueError("padding cannot be negative")
        if not 1 <= args.threshold <= 255:
            raise ValueError("threshold must be between 1 and 255")
        if args.chars is None and args.codepoints is None:
            codepoints = collect_codepoints(None, DEFAULT_CODEPOINTS)
        else:
            codepoints = collect_codepoints(args.chars, args.codepoints)
        if not codepoints:
            raise ValueError("no icon code points were specified")
    except ValueError as exc:
        parser.error(str(exc))

    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_dir = args.out_dir or os.path.join(
        repo_root, "tmp", "FontTool"
    )
    out_dir = os.path.abspath(os.path.normpath(out_dir))
    font_path = os.path.abspath(args.font or find_default_font())
    if not os.path.isfile(font_path):
        parser.error(f"font not found: {font_path}; use --font <path>")

    small_glyphs = {}
    large_glyphs = {}
    try:
        for codepoint in codepoints:
            small_glyphs[codepoint] = render_icon(
                codepoint, font_path, args.font_index, args.small_size,
                args.padding, args.threshold
            )
            large_glyphs[codepoint] = render_icon(
                codepoint, font_path, args.font_index, args.large_size,
                args.padding, args.threshold
            )
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    include_dir = os.path.join(out_dir, "include")
    source_dir = os.path.join(out_dir, "src")
    os.makedirs(include_dir, exist_ok=True)
    os.makedirs(source_dir, exist_ok=True)
    header_path = os.path.join(include_dir, args.output_basename + ".h")
    source_path = os.path.join(source_dir, args.output_basename + ".c")

    with open(header_path, "w", encoding="utf-8", newline="\n") as output:
        output.write(generate_header(
            codepoints, args.small_size, args.large_size
        ))
    with open(source_path, "w", encoding="utf-8", newline="\n") as output:
        output.write(generate_source(
            codepoints, small_glyphs, large_glyphs,
            os.path.basename(header_path)
        ))

    print(f"[OK] Header: {header_path}")
    print(f"[OK] Source: {source_path}")
    for label, size in (
            ("small", args.small_size), ("large", args.large_size)):
        print(
            f"  {label}: visual {size}x{size} in 16x16 cell, "
            f"{BYTES_PER_GLYPH} bytes/glyph, "
            f"{BYTES_PER_GLYPH * len(codepoints)} bytes total"
        )

    if args.preview:
        preview_path = os.path.join(
            out_dir, args.output_basename + "_preview.txt"
        )
        write_preview(
            preview_path, codepoints,
            (args.small_size, args.large_size),
            {
                args.small_size: small_glyphs,
                args.large_size: large_glyphs,
            },
        )
        print(f"[OK] Preview: {preview_path}")


if __name__ == "__main__":
    main()
