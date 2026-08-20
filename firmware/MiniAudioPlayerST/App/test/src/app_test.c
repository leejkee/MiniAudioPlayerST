/*
1) 页面按键功能测试，主界面，菜单界面，功能子页面
2）音乐播放功能测试，播放，暂停，切歌
*/
#include "app_test.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"
#include "bsp_vs1003.h"
#include "player.h"
#include "ui_render.h"
#include "ui_main_page.h"
#include "bsp_config.h"

#define LOG_MODULE "AppTest"

static void _PrintVS1003Register(const char *name, uint8_t address)
{
    bsp_vs1003_status_t status;
    uint16_t value = 0U;

    status = BSP_VS1003_ReadRegister(address, &value);
    LOG_DEBUG("VS1003 Reg Read", "%s status=%d value=0x%04X",
                     name,
                     (int)status,
                     (unsigned int)value);
}

static void _PrintVS1003Failure(void)
{
    bsp_vs1003_verify_diag_t diag;
    uint8_t ready = BSP_VS1003_IsReady();

    LOG_DEBUG("VS1003", "state=%d XRESET=%u XCS=%u XDCS=%u DREQ=%u",
        (int)BSP_VS1003_GetState(),
        (unsigned int)HAL_GPIO_ReadPin(VS1003_XRESET_GPIO_Port,
                                      VS1003_XRESET_Pin),
        (unsigned int)HAL_GPIO_ReadPin(VS1003_XCS_GPIO_Port,
                                      VS1003_XCS_Pin),
        (unsigned int)HAL_GPIO_ReadPin(VS1003_XDCS_GPIO_Port,
                                      VS1003_XDCS_Pin),
        (unsigned int)ready);

    if (BSP_VS1003_GetLastVerifyDiag(&diag) != 0U) {
        LOG_DEBUG("VS1003 Verify", "reg=0x%02X expected=0x%04X actual=0x%04X mask=0x%04X",
            (unsigned int)diag.address,
            (unsigned int)diag.expected,
            (unsigned int)diag.actual,
            (unsigned int)diag.mask);
    }

    if (ready != 0U) {
        _PrintVS1003Register("MODE", BSP_VS1003_REG_MODE);
        _PrintVS1003Register("STATUS", BSP_VS1003_REG_STATUS);
        _PrintVS1003Register("CLOCKF", BSP_VS1003_REG_CLOCKF);
        _PrintVS1003Register("VOL", BSP_VS1003_REG_VOL);
    }
}

void App_TestInit(void)
{
    player_status_t player_status;

    BSP_SSD1315_Init();
    LOG_DEBUG("Init", "SSD1315");
    BSP_Key_Init();
    LOG_DEBUG("Init", "Key");
    player_status = Player_Init();
    LOG_DEBUG("Init", "Player_Init=%d state=%d error=%d tracks=%lu",
                     (int)player_status,
                     (int)Player_GetState(),
                     (int)Player_GetLastError(),
                     (unsigned long)Player_GetTrackCount());
    if (player_status == PLAYER_ERR_DECODER) {
        _PrintVS1003Failure();
    }
    UI_Render_SwitchPage(&Page_Main);
    LOG_DEBUG("Init", "Main page");
}

void App_TestRun(void)
{
    static uint8_t loop_reported;
    static uint8_t previous_pressed = 0xFFU;
    uint8_t pressed = 0U;
    bsp_key_id_t key;

    if (loop_reported == 0U) {
        LOG_DEBUG("Run", "Main loop entered");
        loop_reported = 1U;
    }

    Player_Tick();
    UI_Render_Tick();

    for (key = KEY_OK; key < KEY_COUNT; key++) {
        if (BSP_Key_Read(key) == 0U) {
            pressed |= (uint8_t)(1U << key);
        }
    }
    if (pressed != previous_pressed) {
        LOG_DEBUG("Key", "pressed=0x%02X state=%d error=%d",
                         (unsigned int)pressed,
                         (int)Player_GetState(),
                         (int)Player_GetLastError());
        previous_pressed = pressed;
    }
}
