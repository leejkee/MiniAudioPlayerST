#ifndef __FONT_EN_H__
#define __FONT_EN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 8x16 ASCII font — standard VGA 8x16 console bitmap
 * 95 glyphs (U+0020 to U+007E), 1520 bytes total
 * Format: column-major per GRAM page, bit 0 = top row of page
 */
#define FONT_EN_CHAR_W    8
#define FONT_EN_CHAR_H    16
#define FONT_EN_FIRST     0x20
#define FONT_EN_LAST      0x7E
#define FONT_EN_COUNT     95

extern const uint8_t font_en_8x16[FONT_EN_COUNT][16];

#ifdef __cplusplus
}
#endif

#endif /* __FONT_EN_H__ */
