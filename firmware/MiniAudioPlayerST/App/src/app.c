#include "app.h"
#include "player.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"
#include "ui_render.h"
#include "ui_main_page.h"
#include "ui_exception_page.h"
#include "bsp_config.h"

#define LOG_MODULE                     "App"
#define APP_RECOVERY_RETRY_INTERVAL_MS 1000U

typedef enum
{
    APP_STATE_UNINITIALIZED = 0,
    APP_STATE_RUNNING,
    APP_STATE_RECOVERING
} app_state_t;

static app_state_t      app_state;
static player_status_t  recovery_error;
static uint32_t         last_recovery_tick;

static void App_EnterRecovery(player_status_t error)
{
    uint8_t refresh_page = ((app_state != APP_STATE_RECOVERING) || (recovery_error != error));

    app_state          = APP_STATE_RECOVERING;
    recovery_error     = error;
    last_recovery_tick = HAL_GetTick();

    if (refresh_page != 0U)
    {
        UI_ExceptionPage_SetError(error);
        UI_Render_SwitchPage(&Page_Exception);
    }
}

static void App_TryInitializePlayer(void)
{
    player_status_t status = Player_Init();

    if (status == PLAYER_OK)
    {
        app_state      = APP_STATE_RUNNING;
        recovery_error = PLAYER_OK;
        UI_Render_SwitchPage(&Page_Main);
        return;
    }

    App_EnterRecovery(status);
}

void App_Init(void)
{
    app_state          = APP_STATE_UNINITIALIZED;
    recovery_error     = PLAYER_ERR_NOT_INITIALIZED;
    last_recovery_tick = 0U;
    BSP_SSD1315_Init();
    BSP_Key_Init();
    App_TryInitializePlayer();
}

void App_Run(void)
{
    uint32_t now;

    switch (app_state)
    {
        case APP_STATE_RUNNING:
            Player_Tick();
            if (Player_GetState() == PLAYER_STATE_ERROR)
            {
                App_EnterRecovery(Player_GetLastError());
            }
            break;

        case APP_STATE_RECOVERING:
            now = HAL_GetTick();
            if ((uint32_t)(now - last_recovery_tick) >= APP_RECOVERY_RETRY_INTERVAL_MS)
            {
                App_TryInitializePlayer();
            }
            break;

        case APP_STATE_UNINITIALIZED:
        default:
            App_EnterRecovery(PLAYER_ERR_NOT_INITIALIZED);
            break;
    }

    UI_Render_Tick();
}
