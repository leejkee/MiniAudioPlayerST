#ifndef __SSD1315_H__
#define __SSD1315_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// SSD1315 API functions
void SSD1315_Init(void);

void SSD1315_ColorTurn(uint8_t i);

void SSD1315_DisplayTurn(uint8_t orientation);

void SSD1315_Clear(void);

void SSD1315_DisplayOn(void);

void SSD1315_DisplayOff(void);

void SSD1315_Refresh(void);

void SSD1315_SetPixel(uint8_t x, uint8_t y, uint8_t color);

void SSD1315_DrawPoint(uint8_t x, uint8_t y);

void SSD1315_ClearPoint(uint8_t x, uint8_t y);

void SSD1315_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

void SSD1315_DrawCircle(uint8_t x, uint8_t y, uint8_t r);

void SSD1315_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size);

void SSD1315_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);

void SSD1315_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);

void SSD1315_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size);

void SSD1315_ScrollDisplay(uint8_t num, uint8_t space);


#ifdef __cplusplus
}
#endif

#endif // __SSD1315_H__

