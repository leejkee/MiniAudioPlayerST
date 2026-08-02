#include "ui_main_page.h"

#include "bsp_ssd1315.h"
#include "file_manager.h"
#include "font_en.h"
#include "player.h"

#include <string.h>

#define OLED_WIDTH              128U
#define MAIN_ROW_HEIGHT         16U
#define MAIN_NAME_CAPACITY      128U
#define MAIN_REFRESH_INTERVAL_MS 1000U
#define MAIN_WIDE_CHAR_WIDTH    16U
#define MAIN_ICON_PREVIOUS_X     16U
#define MAIN_ICON_ACTION_X       56U
#define MAIN_ICON_NEXT_X         96U
#define MAIN_ICON_PREVIOUS       0x23EEUL
#define MAIN_ICON_NEXT           0x23EDUL
#define MAIN_ICON_PAUSE          0x23F8UL
#define MAIN_ICON_STOP           0x23F9UL
#define MAIN_ICON_PLAY           0x25B6UL

typedef struct
{
    player_mode_t mode;
    player_state_t state;
    uint32_t track_index;
    uint32_t elapsed_seconds;
    uint32_t duration_seconds;
    uint8_t volume;
    uint8_t pressed_keys;
    WCHAR name[MAIN_NAME_CAPACITY];
} main_meta_t;

static main_meta_t main_meta;
static uint32_t last_refresh_tick;

static uint8_t _AsciiToLower(WCHAR character)
{
    if ((character >= (WCHAR)'A') && (character <= (WCHAR)'Z')) {
        return (uint8_t)(character + ((WCHAR)'a' - (WCHAR)'A'));
    }
    return (uint8_t)character;
}

static void _CopyDisplayName(WCHAR *destination,
                             uint16_t capacity,
                             const WCHAR *source)
{
    const WCHAR *name = source;
    uint16_t length = 0U;

    if ((destination == NULL) || (capacity == 0U)) {
        return;
    }
    destination[0] = 0U;
    if (source == NULL) {
        return;
    }

    while (*source != 0U) {
        if ((*source == (WCHAR)'/') || (*source == (WCHAR)'\\')) {
            name = source + 1;
        }
        source++;
    }

    while ((name[length] != 0U) && (length + 1U < capacity)) {
        destination[length] = name[length];
        length++;
    }
    destination[length] = 0U;

    if ((length >= 4U)
        && (destination[length - 4U] == (WCHAR)'.')
        && (_AsciiToLower(destination[length - 3U]) == (uint8_t)'m')
        && (_AsciiToLower(destination[length - 2U]) == (uint8_t)'p')
        && (_AsciiToLower(destination[length - 1U]) == (uint8_t)'3')) {
        destination[length - 4U] = 0U;
    }
}

static void _UpdateName(void)
{
    static const WCHAR no_music[] = {
        'N', 'o', ' ', 'M', 'u', 's', 'i', 'c', 0
    };
    const WCHAR *name = Player_GetCurrentPath();

    if ((name == NULL) && (FileManager_GetFileCount() > 0U)) {
        name = FileManager_GetVisibleEntry(FileManager_GetCursorRow());
    }
    if (name == NULL) {
        name = no_music;
    }

    _CopyDisplayName(main_meta.name,
                     (uint16_t)(sizeof(main_meta.name)
                                / sizeof(main_meta.name[0])),
                     name);
}

static void _ShowWideString(uint8_t x, uint8_t y, const WCHAR *text)
{
    while ((text != NULL) && (*text != 0U) && (x < OLED_WIDTH)) {
        if (*text < 128U) {
            if (x > OLED_WIDTH - FONT_EN_CHAR_W) {
                break;
            }
            BSP_SSD1315_ShowChar(x, y, (char)*text, 1U);
            x += FONT_EN_CHAR_W;
        } else {
            if (x > OLED_WIDTH - MAIN_WIDE_CHAR_WIDTH) {
                break;
            }
            BSP_SSD1315_ShowChinese(x,
                                    y,
                                    (uint16_t)*text,
                                    FONT_TYPE_FILE);
            x += MAIN_WIDE_CHAR_WIDTH;
        }
        text++;
    }
}

static void _ShowSystemText(uint8_t x,
                            uint8_t y,
                            const uint16_t *text,
                            uint8_t count)
{
    uint8_t index;

    for (index = 0U; index < count; index++) {
        BSP_SSD1315_ShowChinese(x,
                                y,
                                text[index],
                                FONT_TYPE_SYSTEM);
        x += MAIN_WIDE_CHAR_WIDTH;
    }
}

static uint8_t _DecimalDigits(uint8_t value)
{
    if (value >= 100U) {
        return 3U;
    }
    if (value >= 10U) {
        return 2U;
    }
    return 1U;
}

static void _ShowTime(uint8_t x, uint32_t seconds)
{
    uint32_t minutes;

    BSP_SSD1315_ShowChar(x, 3U * MAIN_ROW_HEIGHT, '[', 1U);
    if (seconds > 5999U) {
        seconds = 5999U;
    }
    minutes = seconds / 60U;
    BSP_SSD1315_ShowNum(x + 8U,
                        3U * MAIN_ROW_HEIGHT,
                        minutes,
                        2U,
                        1U);
    BSP_SSD1315_ShowChar(x + 24U, 3U * MAIN_ROW_HEIGHT, ':', 1U);
    BSP_SSD1315_ShowNum(x + 32U,
                        3U * MAIN_ROW_HEIGHT,
                        seconds % 60U,
                        2U,
                        1U);
    BSP_SSD1315_ShowChar(x + 48U, 3U * MAIN_ROW_HEIGHT, ']', 1U);
}

static void _RenderHeader(void)
{
    static const uint16_t repeat_all[] = {0x5FAAU, 0x73AFU};
    static const uint16_t repeat_one[] = {0x5355U, 0x66F2U};
    uint8_t digits;

    BSP_SSD1315_ShowString(0U, 0U, "MP3", 1U);
    BSP_SSD1315_ShowChar(32U, 0U, 'V', 1U);
    BSP_SSD1315_ShowNum(40U, 0U, main_meta.volume, 1U, 1U);
    digits = _DecimalDigits(main_meta.volume);
    BSP_SSD1315_ShowChar((uint8_t)(40U + digits * FONT_EN_CHAR_W),
                         0U,
                         '%',
                         1U);

    if (main_meta.mode == PLAYER_MODE_REPEAT_ONE) {
        _ShowSystemText(88U, 0U, repeat_one, 2U);
    } else {
        _ShowSystemText(88U, 0U, repeat_all, 2U);
    }
}

static uint8_t _IconLarge(bsp_key_id_t key)
{
    return ((main_meta.pressed_keys & (1U << key)) != 0U)
        ? 1U
        : 0U;
}

static void _RenderControls(void)
{
    uint32_t action_icon = MAIN_ICON_PLAY;

    if (main_meta.state == PLAYER_STATE_PLAYING) {
        action_icon = MAIN_ICON_PAUSE;
    } else if (main_meta.state == PLAYER_STATE_ERROR) {
        action_icon = MAIN_ICON_STOP;
    }

    BSP_SSD1315_ShowIcon(MAIN_ICON_PREVIOUS_X,
                         2U * MAIN_ROW_HEIGHT,
                         MAIN_ICON_PREVIOUS,
                         _IconLarge(KEY_L));
    BSP_SSD1315_ShowIcon(MAIN_ICON_ACTION_X,
                         2U * MAIN_ROW_HEIGHT,
                         action_icon,
                         _IconLarge(KEY_OK));
    BSP_SSD1315_ShowIcon(MAIN_ICON_NEXT_X,
                         2U * MAIN_ROW_HEIGHT,
                         MAIN_ICON_NEXT,
                         _IconLarge(KEY_R));
}

static uint8_t _ReadPressedKeys(void)
{
    uint8_t pressed = 0U;
    bsp_key_id_t key;

    for (key = KEY_OK; key < KEY_COUNT; key++) {
        if (BSP_Key_Read(key) == 0U) {
            pressed |= (uint8_t)(1U << key);
        }
    }
    return pressed;
}

static void _Render(void)
{
    BSP_SSD1315_Clear();
    _RenderHeader();
    _ShowWideString(0U, MAIN_ROW_HEIGHT, main_meta.name);
    _RenderControls();
    _ShowTime(0U, main_meta.elapsed_seconds);
    BSP_SSD1315_ShowChar(56U, 3U * MAIN_ROW_HEIGHT, '/', 1U);
    _ShowTime(64U, main_meta.duration_seconds);
    BSP_SSD1315_Refresh();
}

static uint8_t _UpdateMeta(uint8_t force)
{
    player_mode_t mode = Player_GetMode();
    player_state_t state = Player_GetState();
    uint32_t track_index = Player_GetCurrentIndex();
    uint32_t elapsed_seconds = Player_GetElapsedSeconds();
    uint32_t duration_seconds = Player_GetDurationSeconds();
    uint8_t volume = Player_GetVolume();
    uint8_t pressed_keys = _ReadPressedKeys();
    uint8_t changed = force;

    if ((main_meta.mode != mode)
        || (main_meta.state != state)
        || (main_meta.track_index != track_index)
        || (main_meta.elapsed_seconds != elapsed_seconds)
        || (main_meta.duration_seconds != duration_seconds)
        || (main_meta.volume != volume)
        || (main_meta.pressed_keys != pressed_keys)) {
        changed = 1U;
    }

    if ((force != 0U) || (main_meta.track_index != track_index)) {
        _UpdateName();
    }
    main_meta.mode = mode;
    main_meta.state = state;
    main_meta.track_index = track_index;
    main_meta.elapsed_seconds = elapsed_seconds;
    main_meta.duration_seconds = duration_seconds;
    main_meta.volume = volume;
    main_meta.pressed_keys = pressed_keys;
    return changed;
}

static void _Refresh(uint8_t force)
{
    if (_UpdateMeta(force) != 0U) {
        _Render();
    }
    last_refresh_tick = HAL_GetTick();
}

static void _Page_Enter(void)
{
    memset(&main_meta, 0, sizeof(main_meta));
    _Refresh(1U);
}

static void _Page_Key(bsp_key_id_t key)
{
    switch (key) {
        case KEY_OK:
            Player_TogglePause();
            _Refresh(1U);
            break;
        case KEY_L:
            (void)Player_Previous();
            _Refresh(1U);
            break;
        case KEY_R:
            (void)Player_Next();
            _Refresh(1U);
            break;
        case KEY_MENU:
        default:
            break;
    }
}

static void _Page_Tick(void)
{
    uint32_t now = HAL_GetTick();

    if ((_ReadPressedKeys() != main_meta.pressed_keys)
        || ((uint32_t)(now - last_refresh_tick)
            >= MAIN_REFRESH_INTERVAL_MS)) {
        _Refresh(0U);
    }
}

PageHandler_t Page_Main = {
    .on_enter = _Page_Enter,
    .on_key = _Page_Key,
    .on_tick = _Page_Tick
};
