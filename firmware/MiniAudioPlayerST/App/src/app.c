#include "app.h"
#include "player.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"
#include "ui_render.h"
#include "ui_main_page.h"
#include "bsp_config.h"

#define LOG_MODULE "App"
void App_EnterFatalError()
{
    // Show Msg on OLED
    while (1)
    {
    }
}

void App_Init()
{
    player_status_t player_status;
    BSP_SSD1315_Init();
    BSP_Key_Init();
    player_status = Player_Init();

    if (player_status == PLAYER_OK)
    {
        UI_Render_SwitchPage(&Page_Main);
    }
    else
    {
        App_EnterFatalError();
    }
}

void App_Run()
{
    static uint8_t loop_reported;
    if (loop_reported == 0U)
    {
        LOG_DEBUG("Run", "Main loop entered");
        loop_reported = 1U;
    }

    Player_Tick();
    UI_Render_Tick();
}
