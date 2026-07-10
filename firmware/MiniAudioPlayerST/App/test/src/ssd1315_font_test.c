#include "font_cn.h"
#include "font_file_cn.h"
#include "ssd1315.h"
#include "ssd1315_font_test.h"
#include "bsp_config.h"

#define SSD1315_FONT_TEST_DELAY  1000    /* 1s delay between steps */


void SSD1315_Font_Test_RunAll(void)
{
    BSP_DEBUG_PRINTF("\r\n========== SSD1315 Font Test Start ==========\r\n");
    BSP_DEBUG_PRINTF("[TEST 0] Show English font ...\r\n");
    SSD1315_Init();

    BSP_DEBUG_PRINTF("[PASS] Init complete. Screen should be BLACK.\r\n");
    HAL_Delay(SSD1315_FONT_TEST_DELAY);

    BSP_DEBUG_PRINTF("[TEST 1] Show ASCII characters (size 16) ...\r\n");
    SSD1315_ShowChar(0, 0, 'A', 16);
    SSD1315_ShowChar(16, 0, 'B', 16);
    SSD1315_ShowChar(32, 0, 'C', 16);
    SSD1315_ShowChar(48, 0, 'D', 16);
    SSD1315_ShowChar(64, 0, '~', 16);
    SSD1315_ShowChar(80, 0, '!', 16);
    SSD1315_ShowString(0, 16, "Hello, SSD1315!", 16);
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] ASCII characters should be visible.\r\n");
    HAL_Delay(SSD1315_FONT_TEST_DELAY);

    BSP_DEBUG_PRINTF("[TEST 2] Show Chinese chars via font_cn (FONT_TYPE_SYSTEM) ...\r\n");
    /* 9 UI-label glyphs from font_cn_16x16 (59 total):
     *   Line 3 (y=32): ge qu bo fang qi  (Song Player)
     *   Line 4 (y=48): zheng zai bo fang  (Now Playing)
     */
    {
        uint8_t idx;

        /* ---- Line 3: U+6B4C U+66F2 U+64AD U+653E U+5668 ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+6B4C (ge) ... ");
        idx = font_cn_lookup(0x6B4C);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  32, 0x6B4C, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+66F2 (qu) ... ");
        idx = font_cn_lookup(0x66F2);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 32, 0x66F2, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+64AD (bo) ... ");
        idx = font_cn_lookup(0x64AD);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(32, 32, 0x64AD, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+653E (fang) ... ");
        idx = font_cn_lookup(0x653E);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(48, 32, 0x653E, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+5668 (qi) ... ");
        idx = font_cn_lookup(0x5668);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(64, 32, 0x5668, FONT_TYPE_SYSTEM);

        /* ---- Line 4: U+6B63 U+5728 U+64AD U+653E ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+6B63 (zheng) ... ");
        idx = font_cn_lookup(0x6B63);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  48, 0x6B63, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+5728 (zai) ... ");
        idx = font_cn_lookup(0x5728);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 48, 0x5728, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+64AD (bo) ... ");
        idx = font_cn_lookup(0x64AD);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(32, 48, 0x64AD, FONT_TYPE_SYSTEM);

        BSP_DEBUG_PRINTF("  [DEBUG] U+653E (fang) ... ");
        idx = font_cn_lookup(0x653E);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(48, 48, 0x653E, FONT_TYPE_SYSTEM);
    }

    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] Chinese chars should be visible.\r\n");
    HAL_Delay(SSD1315_FONT_TEST_DELAY);

    BSP_DEBUG_PRINTF("[TEST 3] Show song names via font_file_cn (FONT_TYPE_FILE) ...\r\n");
    /* Simulate SD-card song filenames from font_file_cn_16x16 (85 glyphs):
     *   Line 1 (y=0):  shi nian         (Ten Years)
     *   Line 2 (y=16): yue guang         (Moonlight)
     *   Line 3 (y=32): wo de meng        (My Dream)
     *   Line 4 (y=48): shi guang         (Time)
     */
    SSD1315_Clear();
    {
        uint8_t idx;

        /* ---- Line 1: U+5341 U+5E74 ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+5341 (shi) ... ");
        idx = font_file_cn_lookup(0x5341);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  0, 0x5341, FONT_TYPE_FILE);

        BSP_DEBUG_PRINTF("  [DEBUG] U+5E74 (nian) ... ");
        idx = font_file_cn_lookup(0x5E74);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 0, 0x5E74, FONT_TYPE_FILE);

        /* ---- Line 2: U+6708 U+5149 ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+6708 (yue) ... ");
        idx = font_file_cn_lookup(0x6708);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  16, 0x6708, FONT_TYPE_FILE);

        BSP_DEBUG_PRINTF("  [DEBUG] U+5149 (guang) ... ");
        idx = font_file_cn_lookup(0x5149);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 16, 0x5149, FONT_TYPE_FILE);

        /* ---- Line 3: U+6211 U+7684 U+68A6 ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+6211 (wo) ... ");
        idx = font_file_cn_lookup(0x6211);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  32, 0x6211, FONT_TYPE_FILE);

        BSP_DEBUG_PRINTF("  [DEBUG] U+7684 (de) ... ");
        idx = font_file_cn_lookup(0x7684);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 32, 0x7684, FONT_TYPE_FILE);

        BSP_DEBUG_PRINTF("  [DEBUG] U+68A6 (meng) ... ");
        idx = font_file_cn_lookup(0x68A6);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(32, 32, 0x68A6, FONT_TYPE_FILE);

        /* ---- Line 4: U+65F6 U+5149 ---- */
        BSP_DEBUG_PRINTF("  [DEBUG] U+65F6 (shi) ... ");
        idx = font_file_cn_lookup(0x65F6);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(0,  48, 0x65F6, FONT_TYPE_FILE);

        BSP_DEBUG_PRINTF("  [DEBUG] U+5149 (guang) ... ");
        idx = font_file_cn_lookup(0x5149);
        BSP_DEBUG_PRINTF("%s (idx=%u)\r\n", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        SSD1315_ShowChinese(16, 48, 0x5149, FONT_TYPE_FILE);
    }
    SSD1315_Refresh();

    BSP_DEBUG_PRINTF("[PASS] Song names should be visible.\r\n");
    HAL_Delay(SSD1315_FONT_TEST_DELAY);

}
