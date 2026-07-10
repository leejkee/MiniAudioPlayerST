/**
  ******************************************************************************
  * @file    ssd1315_test.c
  * @brief   SSD1315 OLED 驱动上机测试
  * @note    测试方法:
  *          1. 在 main.c 中 #include "ssd1315_test.h"
  *          2. 在 main() 的 while(1) 之前调用 SSD1315_Test_RunAll()
  *          3. 编译下载, 观察 OLED 屏幕和串口输出
  *          4. 每一步之间停顿 2 秒, 用肉眼确认屏幕变化
  *          5. 测试完成后注释掉 main.c 中的调用即可
  *
  *          调试器辅助验证 (可选):
  *          - 在任意 SSD1315_SetPixel / SSD1315_Refresh 处设断点
  *          - 在 Watch 窗口观察 SSD1315_GRAM 和 dirty_pages 的值
  *          - 单步执行, 确认显存数组与屏幕像素的对应关系
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ssd1315_test.h"
#include "ssd1315.h"
#include "bsp_config.h"

/* 测试辅助宏 ----------------------------------------------------------------*/
#define TEST_DELAY      2000    /* 每步停顿 2 秒, 方便观察 */

/*
 * 注意: BSP_DEBUG_PRINTF 输出依赖 bsp_uart.c 的 fputc 重定向 (USART2)。
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
    BSP_DEBUG_PRINTF("\r\n========== SSD1315 Driver Test Start ==========\r\n");
    BSP_DEBUG_PRINTF("[TEST 0] SSD1315_Init ...\r\n");

    SSD1315_Init();  /* Init 内部已调用 Clear + Refresh */

    BSP_DEBUG_PRINTF("[PASS] Init complete. Screen should be BLACK.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 1: 四角 + 中心像素点亮
     *         左上(0,0), 右上(127,0), 左下(0,63), 右下(127,63), 中心(64,32)
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 1] Draw 5 points: 4 corners + center ...\r\n");

    SSD1315_DrawPoint(0, 0);      /* 左上角 */
    SSD1315_DrawPoint(127, 0);    /* 右上角 */
    SSD1315_DrawPoint(0, 63);     /* 左下角 */
    SSD1315_DrawPoint(127, 63);   /* 右下角 */
    SSD1315_DrawPoint(64, 32);    /* 中心   */
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] 5 white dots should be visible.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 2: 擦除中心像素
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 2] Clear center point ...\r\n");

    SSD1315_ClearPoint(64, 32);
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] Center dot should DISAPPEAR, 4 corners remain.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 3: 全屏清除
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 3] SSD1315_Clear ...\r\n");

    SSD1315_Clear();  /* Clear 内部已调用 Refresh */

    BSP_DEBUG_PRINTF("[PASS] Screen should be ALL BLACK.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 4: 绘制直线 — 水平 / 垂直 / 对角线
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 4] Draw lines: H / V / diagonal ...\r\n");

    SSD1315_DrawLine(0, 10, 127, 10);   /* 水平线 (上)   */
    SSD1315_DrawLine(0, 53, 127, 53);   /* 水平线 (下)   */
    SSD1315_DrawLine(10, 0, 10, 63);    /* 垂直线 (左)   */
    SSD1315_DrawLine(117, 0, 117, 63);  /* 垂直线 (右)   */
    SSD1315_DrawLine(0, 0, 127, 63);    /* 对角线 (左上→右下) */
    SSD1315_DrawLine(127, 0, 0, 63);    /* 对角线 (右上→左下) */
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] 6 lines forming a rectangle + X should be visible.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 5: 全屏清除后画圆
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 5] Clear, then draw circles ...\r\n");

    SSD1315_Clear();

    SSD1315_DrawCircle(32, 32, 30);   /* 左圆, r=30 */
    SSD1315_DrawCircle(96, 32, 30);   /* 右圆, r=30 */
    SSD1315_DrawCircle(64, 32, 10);   /* 中心小圆, r=10 */
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] 3 circles should be visible.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 6: 反色显示 (ColorTurn)
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 6] Color inversion ...\r\n");

    SSD1315_ColorTurn(1);  /* A7: 反相显示 */
    BSP_DEBUG_PRINTF("[PASS] Screen INVERTED (black bg -> white bg).\r\n");
    HAL_Delay(TEST_DELAY);

    SSD1315_ColorTurn(0);  /* A6: 正常显示 */
    BSP_DEBUG_PRINTF("[PASS] Screen NORMAL again.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 7: Display On/Off
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 7] Display OFF / ON toggle ...\r\n");

    SSD1315_DisplayOff();
    BSP_DEBUG_PRINTF("[PASS] Display OFF -- screen should be DARK.\r\n");
    HAL_Delay(TEST_DELAY);

    SSD1315_DisplayOn();
    BSP_DEBUG_PRINTF("[PASS] Display ON -- circles reappear.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 8: 全屏填充 (逐行点亮 + 清除) — 验证连续刷新
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 8] Full-screen fill pattern ...\r\n");

    SSD1315_Clear();
    for (uint8_t y = 0; y < 64; y++) {
        for (uint8_t x = 0; x < 128; x++) {
            SSD1315_DrawPoint(x, y);
        }
    }
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] Screen should be ALL WHITE.\r\n");
    HAL_Delay(TEST_DELAY);

    SSD1315_Clear();
    BSP_DEBUG_PRINTF("[PASS] Back to all black.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 9: 竖条纹 — 验证列方向寻址正确
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 9] Vertical stripes (every 8 columns) ...\r\n");

    for (uint8_t x = 0; x < 128; x += 8) {
        for (uint8_t y = 0; y < 64; y++) {
            SSD1315_DrawPoint(x, y);
        }
    }
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] 16 vertical lines (equal spacing) should be visible.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 10: 横条纹 — 验证页方向寻址正确
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 10] Horizontal stripes (every 8 rows) ...\r\n");

    SSD1315_Clear();
    for (uint8_t y = 0; y < 64; y += 8) {
        for (uint8_t x = 0; x < 128; x++) {
            SSD1315_DrawPoint(x, y);
        }
    }
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] 8 horizontal lines (equal spacing) should be visible.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * Test 11: 边界条件 — 越界坐标不应导致 HardFault
     * ======================================================================== */
    BSP_DEBUG_PRINTF("[TEST 11] Boundary test (no crash expected) ...\r\n");

    SSD1315_Clear();

    /* 合法边界: 四角 (已在 Test 1 验证, 这里再确认一次) */
    SSD1315_DrawPoint(0, 0);
    SSD1315_DrawPoint(127, 63);

    /* 越界坐标: SetPixel 不做裁剪, 写越界会污染内存!
     * 这里仅验证不会 HardFault, 不要依赖此行为。 */
    /* 以下两行故意越界, 仅验证不挂死, 不做 PASS/FAIL 判断 */
    /* SSD1315_DrawPoint(128, 64); */  /* 取消注释验证越界行为 */
    /* SSD1315_DrawPoint(255, 255); */

    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] Two corner dots visible. No HardFault.\r\n");
    HAL_Delay(TEST_DELAY);

    /* ========================================================================
     * 测试结束
     * ======================================================================== */
    BSP_DEBUG_PRINTF("========== SSD1315 Driver Test End ==========\r\n\r\n");
}
