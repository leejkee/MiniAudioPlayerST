/**
  ******************************************************************************
  * @file    main_page_test.c
  * @brief   主界面 播放器测试
  * @note    IIC OLED 显示UI，USART2 同时输出播放器元数据。
  ******************************************************************************
  */

#include "main_page_test.h"

#include "bsp_config.h"
#define LOG_MODULE "MainPageTest"
#include "bsp_key.h"
#include "bsp_ssd1315.h"
#include "file_manager.h"
#include "font_icon.h"
#include "player.h"
#include "ui_main_page.h"
#include "ui_render.h"

#define MAIN_PAGE_TEST_REPORT_MS 1000U
#define MAIN_PAGE_NAME_UTF8_SIZE 384U

static uint8_t     test_initialized;

static const char *_StateName(player_state_t state)
{
    static const char *const names[] = {"UNINIT", "STOPPED", "PLAYING", "PAUSED", "ERROR"};

    return (state <= PLAYER_STATE_ERROR) ? names[state] : "UNKNOWN";
}

static const char *_ModeName(player_mode_t mode)
{
    return (mode == PLAYER_MODE_REPEAT_ONE) ? "REPEAT_ONE" : "REPEAT_ALL";
}

static const WCHAR *_DisplayedName(void)
{
    const WCHAR *name = Player_GetCurrentPath();

    if ((name == NULL) && (FileManager_GetFileCount() > 0U))
    {
        name = FileManager_GetVisibleEntry(FileManager_GetCursorRow());
    }
    return name;
}

static void _NameToUtf8(const WCHAR *source, char *output, uint16_t capacity)
{
    const WCHAR *name = source;
    const WCHAR *end;
    uint16_t     write = 0U;

    if ((output == NULL) || (capacity == 0U))
    {
        return;
    }
    if (source == NULL)
    {
        output[0] = 0;
        return;
    }

    while (*source != 0U)
    {
        if ((*source == (WCHAR)'/') || (*source == (WCHAR)'\\'))
        {
            name = source + 1;
        }
        source++;
    }
    end = source;
    if (((end - name) >= 4) && (end[-4] == (WCHAR)'.') &&
        ((end[-3] == (WCHAR)'m') || (end[-3] == (WCHAR)'M')) &&
        ((end[-2] == (WCHAR)'p') || (end[-2] == (WCHAR)'P')) && (end[-1] == (WCHAR)'3'))
    {
        end -= 4;
    }
    source = name;

    while ((source < end) && (write + 1U < capacity))
    {
        uint32_t codepoint = *source++;

        if ((codepoint >= 0xD800U) && (codepoint <= 0xDBFFU) && (source < end) &&
            (*source >= 0xDC00U) && (*source <= 0xDFFFU))
        {
            codepoint = 0x10000UL + ((codepoint - 0xD800U) << 10) + (*source++ - 0xDC00U);
        }

        if (codepoint < 0x80U)
        {
            if (write + 1U >= capacity)
            {
                break;
            }
            output[write++] = (char)codepoint;
        }
        else if (codepoint < 0x800U)
        {
            if (write + 2U >= capacity)
            {
                break;
            }
            output[write++] = (char)(0xC0U | (codepoint >> 6));
            output[write++] = (char)(0x80U | (codepoint & 0x3FU));
        }
        else if (codepoint < 0x10000U)
        {
            if (write + 3U >= capacity)
            {
                break;
            }
            output[write++] = (char)(0xE0U | (codepoint >> 12));
            output[write++] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
            output[write++] = (char)(0x80U | (codepoint & 0x3FU));
        }
        else
        {
            if (write + 4U >= capacity)
            {
                break;
            }
            output[write++] = (char)(0xF0U | (codepoint >> 18));
            output[write++] = (char)(0x80U | ((codepoint >> 12) & 0x3FU));
            output[write++] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
            output[write++] = (char)(0x80U | (codepoint & 0x3FU));
        }
    }
    output[write] = 0;
}

static uint32_t _ClampDisplaySeconds(uint32_t seconds)
{
    return (seconds > 5999U) ? 5999U : seconds;
}

static uint8_t _PressedBitmap(void)
{
    uint8_t      pressed = 0U;
    bsp_key_id_t key;

    for (key = KEY_OK; key < KEY_COUNT; key++)
    {
        if (BSP_Key_Read(key) == 0U)
        {
            pressed |= (uint8_t)(1U << key);
        }
    }
    return pressed;
}

static uint8_t _ExpectedIconSize(uint8_t pressed, bsp_key_id_t key)
{
    return ((pressed & (1U << key)) != 0U) ? FONT_ICON_LARGE_VISUAL_SIZE
                                           : FONT_ICON_SMALL_VISUAL_SIZE;
}

static void _ReportSnapshot(const char *reason)
{
    static char name_utf8[MAIN_PAGE_NAME_UTF8_SIZE];
    uint32_t    elapsed     = _ClampDisplaySeconds(Player_GetElapsedSeconds());
    uint32_t    duration    = _ClampDisplaySeconds(Player_GetDurationSeconds());
    uint32_t    track_count = Player_GetTrackCount();
    uint32_t    track_index = Player_GetCurrentIndex();

    _NameToUtf8(_DisplayedName(), name_utf8, (uint16_t)sizeof(name_utf8));
    LOG_DEBUG("Snapshot", "reason=%s state=%s mode=%s volume=%u%% track=%lu/%lu error=%d", reason,
              _StateName(Player_GetState()), _ModeName(Player_GetMode()),
              (unsigned int)Player_GetVolume(),
              (unsigned long)((track_count > 0U) ? track_index + 1U : 0U),
              (unsigned long)track_count, (int)Player_GetLastError());
    LOG_DEBUG("Metadata", "name=%s duration=%lu sec", (name_utf8[0] != 0) ? name_utf8 : "No Music",
              (unsigned long)Player_GetDurationSeconds());
    LOG_DEBUG("Time", "%02lu:%02lu/%02lu:%02lu", (unsigned long)(elapsed / 60U),
              (unsigned long)(elapsed % 60U), (unsigned long)(duration / 60U),
              (unsigned long)(duration % 60U));
}

static void _ReportIconSizes(uint8_t pressed)
{
    LOG_DEBUG("Icon", "L=%ux%u OK=%ux%u R=%ux%u; pressed bitmap=0x%02X",
              (unsigned int)_ExpectedIconSize(pressed, KEY_L),
              (unsigned int)_ExpectedIconSize(pressed, KEY_L),
              (unsigned int)_ExpectedIconSize(pressed, KEY_OK),
              (unsigned int)_ExpectedIconSize(pressed, KEY_OK),
              (unsigned int)_ExpectedIconSize(pressed, KEY_R),
              (unsigned int)_ExpectedIconSize(pressed, KEY_R), (unsigned int)pressed);
}

void MainPage_Test_Init(void)
{
    player_status_t status;

    LOG_DEBUG("Banner", "Main Page Integration Test");
    BSP_SSD1315_Init();
    BSP_Key_Init();
    status = Player_Init();
    UI_Render_SwitchPage(&Page_Main);
    test_initialized = 1U;

    LOG_DEBUG("Init", "Player_Init=%d", (int)status);
    LOG_DEBUG("Step", "OK: play/pause; L/R: previous/next track.");
    LOG_DEBUG("Check", "Hold L/OK/R: icon must grow 12x12 -> 16x16.");
    LOG_DEBUG("Check", "Release key: icon returns to 12x12 and action runs.");
    LOG_DEBUG("Check", "Compare OLED song/time with UART metadata/time lines.");
    _ReportSnapshot("INIT");
    _ReportIconSizes(_PressedBitmap());
}

void MainPage_Test_Run(void)
{
    static uint32_t       last_report_tick;
    static uint32_t       previous_track_index = UINT32_MAX;
    static player_state_t previous_state       = PLAYER_STATE_UNINIT;
    static uint8_t        previous_pressed     = 0xFFU;
    uint32_t              now;
    uint32_t              track_index;
    player_state_t        state;
    uint8_t               pressed;

    if (test_initialized == 0U)
    {
        return;
    }

    Player_Tick();
    UI_Render_Tick();

    now         = HAL_GetTick();
    track_index = Player_GetCurrentIndex();
    state       = Player_GetState();
    pressed     = _PressedBitmap();

    if (pressed != previous_pressed)
    {
        _ReportIconSizes(pressed);
        previous_pressed = pressed;
    }
    if ((track_index != previous_track_index) || (state != previous_state))
    {
        _ReportSnapshot("CHANGE");
        previous_track_index = track_index;
        previous_state       = state;
        last_report_tick     = now;
    }
    else if ((uint32_t)(now - last_report_tick) >= MAIN_PAGE_TEST_REPORT_MS)
    {
        _ReportSnapshot("TICK");
        last_report_tick = now;
    }
}
