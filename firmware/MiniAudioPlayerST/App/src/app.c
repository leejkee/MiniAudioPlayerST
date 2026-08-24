#include "app.h"
#include "player.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"
#include "ui_render.h"
#include "ui_main_page.h"
#include "ui_exception_page.h"
#include "bsp_config.h"

#define LOG_MODULE "App"
static uint8_t app_initialized;

static void    App_EnterFatalError(player_status_t error)
{
    UI_ExceptionPage_SetError(error);
    UI_Render_SwitchPage(&Page_Exception);
}

void App_Init(void)
{
    player_status_t player_status;

    app_initialized = 0U;
    BSP_SSD1315_Init();
    BSP_Key_Init();
    player_status = Player_Init();

    if (player_status == PLAYER_OK)
    {
        app_initialized = 1U;
        UI_Render_SwitchPage(&Page_Main);
    }
    else
    {
        App_EnterFatalError(player_status);
    }
}

void App_Run(void)
{
    static uint8_t loop_reported;
    if (loop_reported == 0U)
    {
        LOG_DEBUG("Run", "Main loop entered");
        loop_reported = 1U;
    }

    if (app_initialized != 0U)
    {
        Player_Tick();
    }
    UI_Render_Tick();
}
