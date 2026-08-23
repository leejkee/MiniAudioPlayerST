#include "playlist_test.h"
#include "player.h"
#include "bsp_config.h"
#define LOG_MODULE "PlaylistTest"
#include "bsp_ssd1315.h"
#include "bsp_key.h"

static uint8_t playlist_test_ready = 0;

void           Playlist_Test_Init(void)
{
    player_status_t status;

    // OLED
    BSP_SSD1315_Init();
    BSP_SSD1315_Clear();
    LOG_DEBUG("PL TEST", "OLED Init OK");

    // player
    status = Player_Init();
    if (status != PLAYER_OK)
    {
        LOG_DEBUG("PL TEST", "Player_Init failed: %d", (int)status);
        return;
    }

    LOG_DEBUG("PL TEST", "Player_Init OK");

    // key
    BSP_Key_Init();
    LOG_DEBUG("PL TEST", "Key Init OK");

    status = Player_Play(0U);
    if (status != PLAYER_OK)
    {
        LOG_DEBUG("PL TEST", "Player_Play(0) failed: %d", (int)status);
        return;
    }

    playlist_test_ready = 1U;

    LOG_DEBUG("PL TEST", "Ready, Playing track 0");

    LOG_DEBUG("PL TEST", "OK=pause/resume, L=previous, R=next, MENU=stop");
}

void Playlist_Test_Run(void)
{
    static uint32_t last_report_time;
    player_status_t status;

    if (playlist_test_ready == 0U)
    {
        return;
    }

    Player_Tick();

    // OK = pause/resume
    if (BSP_Key_GetEvent(KEY_OK) == KEY_EDGE_RELEASE)
    {
        Player_TogglePause();
    }

    if (BSP_Key_GetEvent(KEY_L) == KEY_EDGE_RELEASE)
    {
        status = Player_Previous();
        if (status != PLAYER_OK)
        {
            LOG_DEBUG("PL TEST", "Previous failed: %d", (int)status);
        }
    }

    if (BSP_Key_GetEvent(KEY_R) == KEY_EDGE_RELEASE)
    {
        status = Player_Next();
        if (status != PLAYER_OK)
        {
            LOG_DEBUG("PL TEST", "Next failed: %d", (int)status);
        }
    }

    if (BSP_Key_GetEvent(KEY_MENU) == KEY_EDGE_RELEASE)
    {
        Player_Stop();
    }

    if ((HAL_GetTick() - last_report_time) >= 1000U)
    {
        last_report_time = HAL_GetTick();

        LOG_DEBUG("PL TEST", "state=%d track=%lu pos=%lu/%lu progress=%u%% error=%d",
                  (int)Player_GetState(), (unsigned long)Player_GetCurrentIndex(),
                  (unsigned long)Player_GetFilePosition(), (unsigned long)Player_GetFileSize(),
                  (unsigned int)Player_GetProgressPercent(), (int)Player_GetLastError());
    }
}
