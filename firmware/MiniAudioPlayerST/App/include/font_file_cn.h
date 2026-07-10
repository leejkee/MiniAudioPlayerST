#ifndef __FONT_FILE_CN_H__
#define __FONT_FILE_CN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 16x16 Chinese font for SD card filenames — 85 glyphs
 * Format: column-major, 2 pages x 16 cols = 32 bytes/glyph
 * Total: 2720 bytes
 */
#define FONT_FILE_CN_CHAR_W    16
#define FONT_FILE_CN_CHAR_H    16
#define FONT_FILE_CN_COUNT     85

extern const uint8_t font_file_cn_16x16[FONT_FILE_CN_COUNT][32];

/* Lookup: Unicode code point -> font_file_cn_16x16 index, returns 0xFF if not found */
uint8_t font_file_cn_lookup(uint16_t unicode);

#ifdef __cplusplus
}
#endif

#endif /* __FONT_FILE_CN_H__ */
