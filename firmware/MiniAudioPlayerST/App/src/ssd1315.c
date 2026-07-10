#include "ssd1315.h"
#include "bsp_oled.h"
#include <stdlib.h>

/*
 * ============================================================================
 * GDDRAM 显存模型 (SSD1315, 128x64 单色 OLED)
 * ============================================================================
 *
 *  显存数组: uint8_t SSD1315_GRAM[8][128];  共 1024 字节
 *            [页 0~7][列 0~127]
 *
 *  物理映射 (默认: A0h + C0h, 未做重映射):
 *
 *    列方向: SEG0 (左) ────────────── SEG127 (右)
 *    行方向: COM0  (上) ────────────── COM63  (下)
 *
 *  内存布局 — 一维视角 (1024 字节连续存放):
 *
 *    byte   0 ── byte 127  │ byte 128 ── byte 255  │ ... │ byte 896 ── byte
 * 1023 ├─ PAGE0 × 128 列 ────┼─ PAGE1 × 128 列 ────┼─ ... ─┼─ PAGE7 × 128 列
 * ──┤ COM0~COM7             COM8~COM15                   COM56~COM63
 *
 *  每个字节 = 一列中同一页的 8 个像素:
 *
 *    SSD1315_GRAM[page][col]:
 *
 *      屏幕坐标 (col, page*8 + bit)
 *      ┌───┐
 *      │ █ │ ← bit 0 = COM(page*8+0) = 该页最上一行
 *      │ █ │ ← bit 1 = COM(page*8+1)
 *      │ █ │ ← bit 2 = COM(page*8+2)
 *      │ █ │ ← bit 3 = COM(page*8+3)
 *      │ █ │ ← bit 4 = COM(page*8+4)
 *      │ █ │ ← bit 5 = COM(page*8+5)
 *      │ █ │ ← bit 6 = COM(page*8+6)
 *      │ █ │ ← bit 7 = COM(page*8+7) = 该页最下一行
 *      └───┘
 *      SEG(col)
 *
 *  各页覆盖的 COM 范围:
 *
 *    PAGE0: COM0  ~ COM7   (屏幕第  1 ~  8 行)  — 左上角
 *    PAGE1: COM8  ~ COM15  (屏幕第  9 ~ 16 行)
 *    PAGE2: COM16 ~ COM23  (屏幕第 17 ~ 24 行)
 *    PAGE3: COM24 ~ COM31  (屏幕第 25 ~ 32 行)
 *    PAGE4: COM32 ~ COM39  (屏幕第 33 ~ 40 行)
 *    PAGE5: COM40 ~ COM47  (屏幕第 41 ~ 48 行)
 *    PAGE6: COM48 ~ COM55  (屏幕第 49 ~ 56 行)
 *    PAGE7: COM56 ~ COM63  (屏幕第 57 ~ 64 行)  — 右下角
 *
 *  重映射命令的影响:
 *
 *    命令      默认值    翻转后     效果
 *    ─────────────────────────────────────────
 *    A0h/A1h   SEG0 左   SEG127 左  左右镜像
 *    C0h/C8h   COM0 上   COM0 下    上下翻转
 *
 *  本工程 Init 中使用 A1h + C8h, 即 COM0 在屏幕下方, SEG127 在左。
 *  显存数组的读写方式不受重映射影响, 只改变物理像素和 GDDRAM 地址的对应关系。
 *
 *  像素坐标换算 (x = 列, y = 行):
 *
 *    page = y / 8;
 *    bit  = y % 8;
 *
 *    写像素: SSD1315_GRAM[page][x] |=  (1 << bit);  // 点亮
 *    擦像素: SSD1315_GRAM[page][x] &= ~(1 << bit);  // 熄灭
 *
 * ============================================================================
 */

static uint8_t SSD1315_GRAM[8][128];

static uint8_t dirty_pages; // 1 字节，每个 bit 标记一页是否脏

void SSD1315_Init(void) {
  BSP_OLED_WriteCmd(0xAE); // 1. 先关显示

  BSP_OLED_WriteCmd(0xD5); // 2. 时钟分频 / 振荡频率
  BSP_OLED_WriteCmd(0x80);

  BSP_OLED_WriteCmd(0xA8); // 3. MUX 比例 = 63 (64 行), 最大分辨率
  BSP_OLED_WriteCmd(0x3F);

  BSP_OLED_WriteCmd(0xD3); // 4. 显示偏移 = 0
  BSP_OLED_WriteCmd(0x00);

  BSP_OLED_WriteCmd(0x40); // 5. 显示起始行 = 0

  BSP_OLED_WriteCmd(0x8D); // 6. 使能内部电荷泵
  BSP_OLED_WriteCmd(0x14); //    7.5V

  BSP_OLED_WriteCmd(0x20); // 7. 寻址模式 = 水平
  BSP_OLED_WriteCmd(0x00);

  BSP_OLED_WriteCmd(0xA1); // 8. Segment 重映射 (左右镜像)
  BSP_OLED_WriteCmd(0xC8); // 9. COM 扫描方向 (上下翻转)

  BSP_OLED_WriteCmd(0xDA); // 10. COM 引脚配置
  BSP_OLED_WriteCmd(0x12);

  BSP_OLED_WriteCmd(0x81); // 11. 对比度
  BSP_OLED_WriteCmd(0x7F);

  BSP_OLED_WriteCmd(0xD9); // 12. 预充电周期
  BSP_OLED_WriteCmd(0xF1);

  BSP_OLED_WriteCmd(0xDB); // 13. VCOMH 电压
  BSP_OLED_WriteCmd(0x40);

  BSP_OLED_WriteCmd(0xA4); // 14. 恢复 GDDRAM 显示
  BSP_OLED_WriteCmd(0xA6); // 15. 正常显示 (非反色)

  SSD1315_Clear(); // 16. 清显存 + 刷屏

  BSP_OLED_WriteCmd(0xAF); // 17. 开显示
}

void SSD1315_ColorTurn(uint8_t i) {
  // 设置 OLED 颜色翻转
  BSP_OLED_WriteCmd(0xA6 | (i & 0x01)); // A6: 正常显示, A7: 反相显示
}

void SSD1315_DisplayTurn(uint8_t orientation) {

  BSP_OLED_WriteCmd(0xC0 | ((orientation & 0x01) << 3)); // 设置扫描方向
  BSP_OLED_WriteCmd(0xA0 | (orientation & 0x02));        // 设置列地址映射
}

void SSD1315_ClearPoint(uint8_t x, uint8_t y) {
  SSD1315_SetPixel(x, y, 0); // 擦除像素点
}

void SSD1315_Clear(void) {
  // 清空显存数组
  for (uint8_t page = 0; page < 8; page++) {
    for (uint8_t col = 0; col < 128; col++) {
      SSD1315_GRAM[page][col] = 0x00;
    }
  }
  dirty_pages = 0xFF; // 全部页脏, 确保 Refresh 将全黑数据发送到 OLED
  SSD1315_Refresh();  // 刷新显示
}

void SSD1315_DisplayOn(void) {
  BSP_OLED_WriteCmd(0xAF); // 开显示
}

void SSD1315_DisplayOff(void) {
  BSP_OLED_WriteCmd(0xAE); // 关显示
}

void SSD1315_DrawPoint(uint8_t x, uint8_t y) {
  SSD1315_SetPixel(x, y, 1); // 点亮像素点
}

void SSD1315_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  // 使用 Bresenham 算法绘制直线
  int dx = abs(x2 - x1);
  int dy = -abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx + dy;
  while (1) {
    SSD1315_SetPixel(x1, y1, 1);
    if (x1 == x2 && y1 == y2)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x1 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void SSD1315_DrawCircle(uint8_t x, uint8_t y, uint8_t r)
{
  int a, b, num;
  a = 0;
  b = r;
  while (2 * b * b >= r * r) {
    SSD1315_SetPixel(x + a, y - b, 1);
    SSD1315_SetPixel(x - a, y - b, 1);
    SSD1315_SetPixel(x - a, y + b, 1);
    SSD1315_SetPixel(x + a, y + b, 1);
    SSD1315_SetPixel(x + b, y + a, 1);
    SSD1315_SetPixel(x + b, y - a, 1);
    SSD1315_SetPixel(x - b, y - a, 1);
    SSD1315_SetPixel(x - b, y + a, 1);
    a++;
    num = (a * a + b * b) - r * r; // 计算当前点到圆心的距离与半径的差值
    if (num > 0) {
      b--;
      a--;
    }

  }
}

void SSD1315_SetPixel(uint8_t x, uint8_t y, uint8_t color) {
  uint8_t page = y / 8;
  uint8_t bit = y % 8;
  if (color) {
    SSD1315_GRAM[page][x] |= (1 << bit);
  } else {
    SSD1315_GRAM[page][x] &= ~(1 << bit);
  }
  dirty_pages |= (1 << page);
}

void SSD1315_Refresh(void) {
  for (uint8_t p = 0; p < 8; p++) {
    // 只刷新脏页
    if (dirty_pages & (1 << p)) {
      BSP_OLED_WriteCmd(0x21);
      BSP_OLED_WriteCmd(0x00);
      BSP_OLED_WriteCmd(0x7F);
      BSP_OLED_WriteCmd(0x22);
      BSP_OLED_WriteCmd(p);
      BSP_OLED_WriteCmd(p);
      BSP_OLED_WriteData(SSD1315_GRAM[p], 128);
    }
  }
  dirty_pages = 0; // 清零，下一次重新标记
}
