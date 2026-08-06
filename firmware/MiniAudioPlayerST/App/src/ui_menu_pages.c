#include "ui_menu_pages.h"

#include "bsp_ssd1315.h"
#include "player.h"
#include "ui_file_page.h"

#define UI_ROW_HEIGHT       16U
#define UI_CN_CHAR_WIDTH    16U
#define MENU_ITEM_COUNT      3U

typedef struct
{
    const uint16_t *label;
    uint8_t label_length;
    PageHandler_t *page;
} menu_item_t;

static const uint16_t volume_label[] = {0x97F3U, 0x91CFU, 0x8C03U, 0x8282U};
static const uint16_t mode_label[] = {0x64ADU, 0x653EU, 0x6A21U, 0x5F0FU};
static const uint16_t file_label[] = {0x6587U, 0x4EF6U, 0x5217U, 0x8868U};
static const uint16_t repeat_all_label[] = {0x5217U, 0x8868U, 0x5FAAU, 0x73AFU};
static const uint16_t repeat_one_label[] = {0x5355U, 0x66F2U, 0x5FAAU, 0x73AFU};

static const menu_item_t menu_items[MENU_ITEM_COUNT] = {
    {volume_label, 4U, &Page_Volume},
    {mode_label, 4U, &Page_PlayMode},
    {file_label, 4U, &Page_File}
};

static uint8_t menu_selection;
static player_mode_t selected_mode;

static void _ShowChineseText(uint8_t x,
                             uint8_t y,
                             const uint16_t *text,
                             uint8_t length)
{
    uint8_t index;

    for (index = 0U; index < length; index++) {
        BSP_SSD1315_ShowChinese(x, y, text[index], FONT_TYPE_SYSTEM);
        x = (uint8_t)(x + UI_CN_CHAR_WIDTH);
    }
}

static void _RenderMenu(void)
{
    uint8_t index;

    BSP_SSD1315_Clear();
    BSP_SSD1315_ShowString(0U, 0U, "Menu", 1U);

    for (index = 0U; index < MENU_ITEM_COUNT; index++) {
        uint8_t y = (uint8_t)((index + 1U) * UI_ROW_HEIGHT);

        if (index == menu_selection) {
            BSP_SSD1315_ShowChar(0U, y, '>', 1U);
        }
        _ShowChineseText(16U,
                         y,
                         menu_items[index].label,
                         menu_items[index].label_length);
    }
    BSP_SSD1315_Refresh();
}

static void _MenuEnter(void)
{
    if (menu_selection >= MENU_ITEM_COUNT) {
        menu_selection = 0U;
    }
    _RenderMenu();
}

static void _MenuKey(bsp_key_id_t key)
{
    switch (key) {
        case KEY_OK:
            (void)UI_Render_PushPage(menu_items[menu_selection].page);
            break;
        case KEY_L:
            menu_selection = (menu_selection == 0U)
                ? MENU_ITEM_COUNT - 1U
                : menu_selection - 1U;
            _RenderMenu();
            break;
        case KEY_R:
            menu_selection++;
            if (menu_selection >= MENU_ITEM_COUNT) {
                menu_selection = 0U;
            }
            _RenderMenu();
            break;
        case KEY_MENU:
        default:
            break;
    }
}

static void _RenderVolume(void)
{
    uint8_t volume = Player_GetVolume();
    uint8_t digits = (volume == 100U) ? 3U : ((volume >= 10U) ? 2U : 1U);
    uint8_t value_x = (uint8_t)((128U - ((uint8_t)(digits + 1U) * 8U)) / 2U);

    BSP_SSD1315_Clear();
    _ShowChineseText(32U, 0U, volume_label, 4U);
    BSP_SSD1315_ShowChar(8U, 32U, '<', 1U);
    BSP_SSD1315_ShowNum(value_x, 32U, volume, digits, 1U);
    BSP_SSD1315_ShowChar((uint8_t)(value_x + digits * 8U), 32U, '%', 1U);
    BSP_SSD1315_ShowChar(112U, 32U, '>', 1U);
    BSP_SSD1315_Refresh();
}

static void _VolumeEnter(void)
{
    _RenderVolume();
}

static void _VolumeKey(bsp_key_id_t key)
{
    uint8_t volume = Player_GetVolume();

    switch (key) {
        case KEY_L:
            volume = (volume >= 10U) ? (uint8_t)(volume - 10U) : 0U;
            (void)Player_SetVolume(volume);
            _RenderVolume();
            break;
        case KEY_R:
            volume = (volume <= 90U) ? (uint8_t)(volume + 10U) : 100U;
            (void)Player_SetVolume(volume);
            _RenderVolume();
            break;
        case KEY_OK:
        case KEY_MENU:
        default:
            break;
    }
}

static const uint16_t *_ModeLabel(player_mode_t mode)
{
    return (mode == PLAYER_MODE_REPEAT_ONE)
        ? repeat_one_label
        : repeat_all_label;
}

static void _RenderPlayMode(void)
{
    BSP_SSD1315_Clear();
    _ShowChineseText(32U, 0U, mode_label, 4U);
    BSP_SSD1315_ShowChar(8U, 32U, '<', 1U);
    _ShowChineseText(32U, 32U, _ModeLabel(selected_mode), 4U);
    BSP_SSD1315_ShowChar(112U, 32U, '>', 1U);
    BSP_SSD1315_Refresh();
}

static void _PlayModeEnter(void)
{
    selected_mode = Player_GetMode();
    _RenderPlayMode();
}

static void _PlayModeKey(bsp_key_id_t key)
{
    switch (key) {
        case KEY_L:
        case KEY_R:
            selected_mode = (selected_mode == PLAYER_MODE_REPEAT_ALL)
                ? PLAYER_MODE_REPEAT_ONE
                : PLAYER_MODE_REPEAT_ALL;
            _RenderPlayMode();
            break;
        case KEY_OK:
            (void)Player_SetMode(selected_mode);
            (void)UI_Render_PopPage();
            break;
        case KEY_MENU:
        default:
            break;
    }
}

PageHandler_t Page_Menu = {
    .on_enter = _MenuEnter,
    .on_key = _MenuKey,
    .on_tick = NULL
};

PageHandler_t Page_Volume = {
    .on_enter = _VolumeEnter,
    .on_key = _VolumeKey,
    .on_tick = NULL
};

PageHandler_t Page_PlayMode = {
    .on_enter = _PlayModeEnter,
    .on_key = _PlayModeKey,
    .on_tick = NULL
};
