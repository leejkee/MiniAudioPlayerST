#include "ui_file_page.h"
#include "file_manager.h"
#include "bsp_ssd1315.h"
#include "font_en.h"

/* OLED layout constants (128x64, 8 pages) */
#define OLED_W       128
#define ROW_H        16   /* 2 pages × 8 pixels each */
#define CJK_CHAR_W   16
#define CURSOR_X     0
#define ENTRY_X      FONT_EN_CHAR_W  /* indent after cursor arrow */

/* -------------------------------------------------------------------------- */
/*  WCHAR string renderer — handles ASCII + CJK via BSP public API            */
/* -------------------------------------------------------------------------- */
static void _ShowWideString(uint8_t x, uint8_t y, const WCHAR *wstr)
{
    while (*wstr != L'\0') {
        if (x >= OLED_W) {
            break;
        }

        if (*wstr < 128) {
            /* ASCII — 8 px wide */
            if (x + FONT_EN_CHAR_W > OLED_W) {
                break;
            }
            BSP_SSD1315_ShowChar(x, y, (char)*wstr, 1);
            x += FONT_EN_CHAR_W;
        } else {
            /* CJK / wide character — 16 px wide */
            if (x + CJK_CHAR_W > OLED_W) {
                break;
            }
            BSP_SSD1315_ShowChinese(x, y, (uint16_t)*wstr, FONT_TYPE_FILE);
            x += CJK_CHAR_W;
        }
        wstr++;
    }
}

/* -------------------------------------------------------------------------- */
/*  Full-screen render: clear → draw entries → highlight cursor → refresh     */
/* -------------------------------------------------------------------------- */
static void _Render(void)
{
    uint8_t visible    = FileManager_GetVisibleCount();
    uint8_t cursor_row = FileManager_GetCursorRow();

    BSP_SSD1315_ClearBuffer();
    for (uint8_t row = 0; row < visible; row++) {
        const WCHAR *name = FileManager_GetVisibleEntry(row);
        if (name == NULL) {
            continue;
        }

        uint8_t y = row * ROW_H;

        if (row == cursor_row) {
            /* cursor row: draw ">" prefix + file name */
            BSP_SSD1315_ShowChar(CURSOR_X, y, '>', 1);
            _ShowWideString(ENTRY_X, y, name);
        } else {
            _ShowWideString(CURSOR_X, y, name);
        }
    }
    BSP_SSD1315_Refresh();
}

static void _Page_Enter(void)
{
    FileManager_Init();
    _Render();
}

static void _Page_Key(bsp_key_id_t key)
{
    switch (key) {
        case KEY_L:
            FileManager_MoveCursorUp();
            _Render();
            break;
        case KEY_R:
            FileManager_MoveCursorDown();
            _Render();
            break;
        default:
            break;
    }
}

PageHandler_t Page_File = {.on_enter = _Page_Enter, .on_key = _Page_Key, .on_tick = NULL};
