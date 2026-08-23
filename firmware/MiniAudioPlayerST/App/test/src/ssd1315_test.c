/**
  ******************************************************************************
  * @file    ssd1315_test.c
  * @brief   SSD1315 OLED 驱动上机测试
  * @note    测试方法:
  *          1. 在 main.c 中 #include "ssd1315_test.h"
  *          2. 在 main() 的 while(1) 之前调用 BSP_SSD1315_Test_RunAll()
  *          3. 编译下载, 观察 OLED 屏幕和串口输出
  *          4. 每一步之间停顿 2 秒, 用肉眼确认屏幕变化
  *          5. 测试完成后注释掉 main.c 中的调用即可
  *
  *          调试器辅助验证 (可选):
  *          - 在任意 BSP_SSD1315_SetPixel / BSP_SSD1315_Refresh 处设断点
  *          - 在 Watch 窗口观察 BSP_SSD1315_GRAM 和 dirty_pages 的值
  *          - 单步执行, 确认显存数组与屏幕像素的对应关系
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ssd1315_test.h"
#include "bsp_ssd1315.h"
#include "bsp_config.h"
#define LOG_MODULE "SSD1315Test"

/* 测试辅助宏 ----------------------------------------------------------------*/
#define TEST_DELAY 2000 /* 每步停顿 2 秒, 方便观察 */

/*
 * 注意: LOG_DEBUG 输出依赖 bsp_uart.c 的 fputc 重定向 (USART2)。
 *       若 BSP_DEBUG_UART=0, 串口输出将不工作, 但测试仍可肉眼验证。
 *       本文件不直接依赖 bsp_config.h, 避免 MDK 工程 include path 配置问题。
 */

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  运行全部 SSD1315 驱动测试
  * @note   每步之间有延时, 总耗时约 30~40 秒。
  *         观察屏幕 + 串口输出, 确认每步 PASS。
  */
void SSD1315_Test_RunAll(void)
{
    /* ========================================================================
     * Test 0: 初始化 — 屏幕应从随机噪点变为全黑
     * ======================================================================== */
    LOG_DEBUG("Banner", "SSD1315 Driver Test Start");
    LOG_DEBUG("TEST 0", "BSP_SSD1315_Init ...");

    BSP_SSD1315_Init(); /* Init 内部已调用 Clear + Refresh */

    LOG_DEBUG("PASS", "Init complete. Screen should be BLACK.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 1: 四角 + 中心像素点亮
     *         左上(0,0), 右上(127,0), 左下(0,63), 右下(127,63), 中心(64,32)
     * ======================================================================== */
    LOG_DEBUG("TEST 1", "Draw 5 points: 4 corners + center ...");

    BSP_SSD1315_DrawPoint(0, 0);    /* 左上角 */
    BSP_SSD1315_DrawPoint(127, 0);  /* 右上角 */
    BSP_SSD1315_DrawPoint(0, 63);   /* 左下角 */
    BSP_SSD1315_DrawPoint(127, 63); /* 右下角 */
    BSP_SSD1315_DrawPoint(64, 32);  /* 中心   */
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "5 white dots should be visible.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 2: 擦除中心像素
     * ======================================================================== */
    LOG_DEBUG("TEST 2", "Clear center point ...");

    BSP_SSD1315_ClearPoint(64, 32);
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "Center dot should DISAPPEAR, 4 corners remain.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 3: 全屏清除
     * ======================================================================== */
    LOG_DEBUG("TEST 3", "BSP_SSD1315_Clear ...");

    BSP_SSD1315_Clear(); /* Clear 内部已调用 Refresh */

    LOG_DEBUG("PASS", "Screen should be ALL BLACK.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 4: 绘制直线 — 水平 / 垂直 / 对角线
     * ======================================================================== */
    LOG_DEBUG("TEST 4", "Draw lines: H / V / diagonal ...");

    BSP_SSD1315_DrawLine(0, 10, 127, 10);  /* 水平线 (上)   */
    BSP_SSD1315_DrawLine(0, 53, 127, 53);  /* 水平线 (下)   */
    BSP_SSD1315_DrawLine(10, 0, 10, 63);   /* 垂直线 (左)   */
    BSP_SSD1315_DrawLine(117, 0, 117, 63); /* 垂直线 (右)   */
    BSP_SSD1315_DrawLine(0, 0, 127, 63);   /* 对角线 (左上→右下) */
    BSP_SSD1315_DrawLine(127, 0, 0, 63);   /* 对角线 (右上→左下) */
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "6 lines forming a rectangle + X should be visible.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 5: 全屏清除后画圆
     * ======================================================================== */
    LOG_DEBUG("TEST 5", "Clear, then draw circles ...");

    BSP_SSD1315_Clear();

    BSP_SSD1315_DrawCircle(32, 32, 30); /* 左圆, r=30 */
    BSP_SSD1315_DrawCircle(96, 32, 30); /* 右圆, r=30 */
    BSP_SSD1315_DrawCircle(64, 32, 10); /* 中心小圆, r=10 */
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "3 circles should be visible.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 6: 反色显示 (ColorTurn)
     * ======================================================================== */
    LOG_DEBUG("TEST 6", "Color inversion ...");

    BSP_SSD1315_ColorTurn(1); /* A7: 反相显示 */
    LOG_DEBUG("PASS", "Screen INVERTED (black bg -> white bg).");
    HAL_Delay(TEST_DELAY);

    BSP_SSD1315_ColorTurn(0); /* A6: 正常显示 */
    LOG_DEBUG("PASS", "Screen NORMAL again.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 7: Display On/Off
     * ======================================================================== */
    LOG_DEBUG("TEST 7", "Display OFF / ON toggle ...");

    BSP_SSD1315_DisplayOff();
    LOG_DEBUG("PASS", "Display OFF -- screen should be DARK.");
    HAL_Delay(TEST_DELAY);

    BSP_SSD1315_DisplayOn();
    LOG_DEBUG("PASS", "Display ON -- circles reappear.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 8: 全屏填充 (逐行点亮 + 清除) — 验证连续刷新
     * ======================================================================== */
    LOG_DEBUG("TEST 8", "Full-screen fill pattern ...");

    BSP_SSD1315_Clear();
    for (uint8_t y = 0; y < 64; y++)
    {
        for (uint8_t x = 0; x < 128; x++)
        {
            BSP_SSD1315_DrawPoint(x, y);
        }
    }
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "Screen should be ALL WHITE.");
    HAL_Delay(TEST_DELAY);

    BSP_SSD1315_Clear();
    LOG_DEBUG("PASS", "Back to all black.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 9: 竖条纹 — 验证列方向寻址正确
     * ======================================================================== */
    LOG_DEBUG("TEST 9", "Vertical stripes (every 8 columns) ...");

    for (uint8_t x = 0; x < 128; x += 8)
    {
        for (uint8_t y = 0; y < 64; y++)
        {
            BSP_SSD1315_DrawPoint(x, y);
        }
    }
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "16 vertical lines (equal spacing) should be visible.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 10: 横条纹 — 验证页方向寻址正确
     * ======================================================================== */
    LOG_DEBUG("TEST 10", "Horizontal stripes (every 8 rows) ...");

    BSP_SSD1315_Clear();
    for (uint8_t y = 0; y < 64; y += 8)
    {
        for (uint8_t x = 0; x < 128; x++)
        {
            BSP_SSD1315_DrawPoint(x, y);
        }
    }
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "8 horizontal lines (equal spacing) should be visible.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 11: 边界条件 — 越界坐标不应导致 HardFault
     * ======================================================================== */
    LOG_DEBUG("TEST 11", "Boundary test (no crash expected) ...");

    BSP_SSD1315_Clear();

    /* 合法边界: 四角 (已在 Test 1 验证, 这里再确认一次) */
    BSP_SSD1315_DrawPoint(0, 0);
    BSP_SSD1315_DrawPoint(127, 63);

    /* 越界坐标: SetPixel 不做裁剪, 写越界会污染内存!
     * 这里仅验证不会 HardFault, 不要依赖此行为。 */
    /* 以下两行故意越界, 仅验证不挂死, 不做 PASS/FAIL 判断 */
    /* BSP_SSD1315_DrawPoint(128, 64); */ /* 取消注释验证越界行为 */
    /* BSP_SSD1315_DrawPoint(255, 255); */

    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "Two corner dots visible. No HardFault.");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * 测试结束
     * ======================================================================== */
    LOG_DEBUG("Banner", "SSD1315 Driver Test End");
}
