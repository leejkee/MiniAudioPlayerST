#ifndef __BSP_SSD1315_H__
#define __BSP_SSD1315_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

    typedef enum
    {
        FONT_TYPE_SYSTEM = 0,
        FONT_TYPE_FILE   = 1
    } CnFontType;

    // SSD1315 API functions
    void BSP_SSD1315_Init(void);

    void BSP_SSD1315_ColorTurn(uint8_t i);

    void BSP_SSD1315_DisplayTurn(uint8_t orientation);

    void BSP_SSD1315_Clear(void);

    /**
  * @brief  仅清空软件显存，不立即向 OLED 发送黑屏数据
  * @note   绘制完整帧后调用 BSP_SSD1315_Refresh() 一次性提交。
  */
    void BSP_SSD1315_ClearBuffer(void);

    /**
  * @brief  清空软件显存中的指定矩形，不立即刷新 OLED
  */
    void BSP_SSD1315_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);

    void BSP_SSD1315_DisplayOn(void);

    void BSP_SSD1315_DisplayOff(void);

    void BSP_SSD1315_Refresh(void);

    void BSP_SSD1315_SetPixel(uint8_t x, uint8_t y, uint8_t color);

    void BSP_SSD1315_DrawPoint(uint8_t x, uint8_t y);

    void BSP_SSD1315_ClearPoint(uint8_t x, uint8_t y);

    void BSP_SSD1315_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

    void BSP_SSD1315_DrawCircle(uint8_t x, uint8_t y, uint8_t r);

    void BSP_SSD1315_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size);

    void BSP_SSD1315_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);

    void BSP_SSD1315_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);

    void BSP_SSD1315_ShowChinese(uint8_t x, uint8_t y, uint16_t unicode, CnFontType type);

    /**
  * @brief  显示固定 16x16 单元中的 Unicode 图标
  * @param  large 0 使用居中的 12x12 字模，非 0 使用 16x16 字模
  */
    void BSP_SSD1315_ShowIcon(uint8_t x, uint8_t y, uint32_t unicode, uint8_t large);

    void BSP_SSD1315_ScrollDisplay(uint8_t num, uint8_t space);

#ifdef __cplusplus
}
#endif

#endif  // __BSP_SSD1315_H__
