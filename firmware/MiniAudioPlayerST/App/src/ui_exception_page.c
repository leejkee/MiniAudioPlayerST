#include "ui_exception_page.h"

#include "bsp_ssd1315.h"
#include "font_en.h"

#define EXCEPTION_ROW_HEIGHT 16U
#define EXCEPTION_OLED_WIDTH 128U

static player_status_t exception_error = PLAYER_ERR_NOT_INITIALIZED;

static const char     *_GetErrorText(player_status_t error)
{
    switch (error)
    {
        case PLAYER_ERR_PARAM:
            return "Invalid Param";
        case PLAYER_ERR_NOT_INITIALIZED:
            return "Not Initialized";
        case PLAYER_ERR_MOUNT:
            return "FS Mount Error";
        case PLAYER_ERR_NO_FILES:
            return "No Music Files";
        case PLAYER_ERR_OPEN:
            return "File Open Error";
        case PLAYER_ERR_READ:
            return "File Read Error";
        case PLAYER_ERR_DECODER:
            return "Decoder Error";
        case PLAYER_OK:
        default:
            return "Unknown Error";
    }
}

static uint8_t _CenteredX(const char *text)
{
    uint8_t length = 0U;

    while ((length < (EXCEPTION_OLED_WIDTH / FONT_EN_CHAR_W)) && (text[length] != '\0'))
    {
        length++;
    }
    return (uint8_t)((EXCEPTION_OLED_WIDTH - length * FONT_EN_CHAR_W) / 2U);
}

static void _Page_Enter(void)
{
    const char *error_text = _GetErrorText(exception_error);

    BSP_SSD1315_ShowString(_CenteredX("Player Error"), 0U, "Player Error", 1U);
    BSP_SSD1315_ShowString(_CenteredX("Reason:"), EXCEPTION_ROW_HEIGHT, "Reason:", 1U);
    BSP_SSD1315_ShowString(_CenteredX(error_text), 2U * EXCEPTION_ROW_HEIGHT, error_text, 1U);
    BSP_SSD1315_ShowString(_CenteredX("Retrying..."), 3U * EXCEPTION_ROW_HEIGHT, "Retrying...",
                           1U);
}

void UI_ExceptionPage_SetError(player_status_t error)
{
    exception_error = error;
}

PageHandler_t Page_Exception = {.on_enter = _Page_Enter, .on_key = NULL, .on_tick = NULL};
