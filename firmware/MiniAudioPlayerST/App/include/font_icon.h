#ifndef FONT_ICON_H
#define FONT_ICON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* Two-state monochrome Unicode icon font.
 * Format: page-major then column-major, bit 0 = top row of each page.
 * Both arrays use the same Unicode index and fixed 16x16 storage cell.
 */
#define FONT_ICON_GLYPH_COUNT       6
#define FONT_ICON_NOT_FOUND         UINT16_MAX
#define FONT_ICON_CELL_WIDTH        16
#define FONT_ICON_CELL_HEIGHT       16
#define FONT_ICON_CELL_PAGES        2
#define FONT_ICON_BYTES_PER_GLYPH   32
#define FONT_ICON_SMALL_VISUAL_SIZE 12
#define FONT_ICON_LARGE_VISUAL_SIZE 16

    extern const uint8_t font_icon_small_16x16[FONT_ICON_GLYPH_COUNT][FONT_ICON_BYTES_PER_GLYPH];
    extern const uint8_t font_icon_large_16x16[FONT_ICON_GLYPH_COUNT][FONT_ICON_BYTES_PER_GLYPH];

    /* Returns the glyph index, or FONT_ICON_NOT_FOUND. */
    uint16_t             font_icon_lookup(uint32_t unicode);

#ifdef __cplusplus
}
#endif

#endif /* FONT_ICON_H */
