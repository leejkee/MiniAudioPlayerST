#ifndef __FONT_CN_H__
#define __FONT_CN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 16x16 Chinese font, 59 glyphs
 * Format: column-major, 2 pages x 16 cols = 32 bytes/glyph
 * Total: 1888 bytes
 */
#define FONT_CN_CHAR_W    16
#define FONT_CN_CHAR_H    16
#define FONT_CN_COUNT     59

extern const uint8_t font_cn_16x16[FONT_CN_COUNT][32];

/* Lookup: Unicode code point -> font_cn_16x16 index, returns 0xFF if not found */
uint8_t font_cn_lookup(uint16_t unicode);

#ifdef __cplusplus
}
#endif

#endif /* __FONT_CN_H__ */
