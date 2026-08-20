#include "font_cn.h"
#include "font_file_cn.h"
#include "bsp_ssd1315.h"
#include "ssd1315_font_test.h"
#include "bsp_config.h"
#define LOG_MODULE "SSD1315FontTest"

#define BSP_SSD1315_FONT_TEST_DELAY  1000    /* 1s delay between steps */


void BSP_SSD1315_Font_Test_RunAll(void)
{
    LOG_DEBUG("Banner", "SSD1315 Font Test Start");
    LOG_DEBUG("TEST 0", "Show English font ...");
    BSP_SSD1315_Init();

    LOG_DEBUG("PASS", "Init complete. Screen should be BLACK.");
    HAL_Delay(BSP_SSD1315_FONT_TEST_DELAY);

    LOG_DEBUG("TEST 1", "Show ASCII characters (size 16) ...");
    BSP_SSD1315_ShowChar(0, 0, 'A', 16);
    BSP_SSD1315_ShowChar(16, 0, 'B', 16);
    BSP_SSD1315_ShowChar(32, 0, 'C', 16);
    BSP_SSD1315_ShowChar(48, 0, 'D', 16);
    BSP_SSD1315_ShowChar(64, 0, '~', 16);
    BSP_SSD1315_ShowChar(80, 0, '!', 16);
    BSP_SSD1315_ShowString(0, 16, "Hello, SSD1315!", 16);
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "ASCII characters should be visible.");
    HAL_Delay(BSP_SSD1315_FONT_TEST_DELAY);

    LOG_DEBUG("TEST 2", "Show Chinese chars via font_cn (FONT_TYPE_SYSTEM) ...");
    /* 9 UI-label glyphs from font_cn_16x16 (59 total):
     *   Line 3 (y=32): ge qu bo fang qi  (Song Player)
     *   Line 4 (y=48): zheng zai bo fang  (Now Playing)
     */
    {
        uint8_t idx;

        /* ---- Line 3: U+6B4C U+66F2 U+64AD U+653E U+5668 ---- */
        LOG_DEBUG("DEBUG", "U+6B4C (ge) ... ");
        idx = font_cn_lookup(0x6B4C);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  32, 0x6B4C, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+66F2 (qu) ... ");
        idx = font_cn_lookup(0x66F2);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 32, 0x66F2, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+64AD (bo) ... ");
        idx = font_cn_lookup(0x64AD);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(32, 32, 0x64AD, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+653E (fang) ... ");
        idx = font_cn_lookup(0x653E);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(48, 32, 0x653E, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+5668 (qi) ... ");
        idx = font_cn_lookup(0x5668);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(64, 32, 0x5668, FONT_TYPE_SYSTEM);

        /* ---- Line 4: U+6B63 U+5728 U+64AD U+653E ---- */
        LOG_DEBUG("DEBUG", "U+6B63 (zheng) ... ");
        idx = font_cn_lookup(0x6B63);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  48, 0x6B63, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+5728 (zai) ... ");
        idx = font_cn_lookup(0x5728);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 48, 0x5728, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+64AD (bo) ... ");
        idx = font_cn_lookup(0x64AD);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(32, 48, 0x64AD, FONT_TYPE_SYSTEM);

        LOG_DEBUG("DEBUG", "U+653E (fang) ... ");
        idx = font_cn_lookup(0x653E);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(48, 48, 0x653E, FONT_TYPE_SYSTEM);
    }

    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "Chinese chars should be visible.");
    HAL_Delay(BSP_SSD1315_FONT_TEST_DELAY);

    LOG_DEBUG("TEST 3", "Show song names via font_file_cn (FONT_TYPE_FILE) ...");
    /* Simulate SD-card song filenames from font_file_cn_16x16 (85 glyphs):
     *   Line 1 (y=0):  shi nian         (Ten Years)
     *   Line 2 (y=16): yue guang         (Moonlight)
     *   Line 3 (y=32): wo de meng        (My Dream)
     *   Line 4 (y=48): shi guang         (Time)
     */
    BSP_SSD1315_Clear();
    {
        uint8_t idx;

        /* ---- Line 1: U+5341 U+5E74 ---- */
        LOG_DEBUG("DEBUG", "U+5341 (shi) ... ");
        idx = font_file_cn_lookup(0x5341);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  0, 0x5341, FONT_TYPE_FILE);

        LOG_DEBUG("DEBUG", "U+5E74 (nian) ... ");
        idx = font_file_cn_lookup(0x5E74);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 0, 0x5E74, FONT_TYPE_FILE);

        /* ---- Line 2: U+6708 U+5149 ---- */
        LOG_DEBUG("DEBUG", "U+6708 (yue) ... ");
        idx = font_file_cn_lookup(0x6708);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  16, 0x6708, FONT_TYPE_FILE);

        LOG_DEBUG("DEBUG", "U+5149 (guang) ... ");
        idx = font_file_cn_lookup(0x5149);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 16, 0x5149, FONT_TYPE_FILE);

        /* ---- Line 3: U+6211 U+7684 U+68A6 ---- */
        LOG_DEBUG("DEBUG", "U+6211 (wo) ... ");
        idx = font_file_cn_lookup(0x6211);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  32, 0x6211, FONT_TYPE_FILE);

        LOG_DEBUG("DEBUG", "U+7684 (de) ... ");
        idx = font_file_cn_lookup(0x7684);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 32, 0x7684, FONT_TYPE_FILE);

        LOG_DEBUG("DEBUG", "U+68A6 (meng) ... ");
        idx = font_file_cn_lookup(0x68A6);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(32, 32, 0x68A6, FONT_TYPE_FILE);

        /* ---- Line 4: U+65F6 U+5149 ---- */
        LOG_DEBUG("DEBUG", "U+65F6 (shi) ... ");
        idx = font_file_cn_lookup(0x65F6);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(0,  48, 0x65F6, FONT_TYPE_FILE);

        LOG_DEBUG("DEBUG", "U+5149 (guang) ... ");
        idx = font_file_cn_lookup(0x5149);
        LOG_DEBUG("Message", "%s (idx=%u)", idx != 0xFF ? "ok" : "NOT FOUND", idx);
        BSP_SSD1315_ShowChinese(16, 48, 0x5149, FONT_TYPE_FILE);
    }
    BSP_SSD1315_Refresh();

    LOG_DEBUG("PASS", "Song names should be visible.");
    HAL_Delay(BSP_SSD1315_FONT_TEST_DELAY);

}
