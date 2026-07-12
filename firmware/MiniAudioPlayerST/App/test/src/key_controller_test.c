/**
  ******************************************************************************
  * @file    key_controller_test.c
  * @brief   按键控制器测试 — 按键状态 OLED 显示
  * @note    通过 BSP_Key_GetEvent() 获取消抖后的释放沿事件。
  *          一次按键 → 一次事件, 不会重复触发。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "key_controller_test.h"
#include "ssd1315.h"
#include "bsp_config.h"
#include "bsp_key.h"


void Key_Controller_Test_Init(void)
{
    BSP_DEBUG_PRINTF("TEST 0");
    SSD1315_Init();
    SSD1315_ShowString(0, 0, "Key Test Ready", 16);

    BSP_DEBUG_PRINTF("TEST 1");
}

void Key_Controller_Test_Loop(void)
{
    if (BSP_Key_GetEvent(KEY_MENU) == KEY_EDGE_RELEASE) {
        BSP_DEBUG_PRINTF("Menu pressed\r\n");
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "Menu pressed", 16);
    }

    if (BSP_Key_GetEvent(KEY_OK) == KEY_EDGE_RELEASE) {
        BSP_DEBUG_PRINTF("OK pressed\r\n");
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "OK pressed", 16);
    }

    if (BSP_Key_GetEvent(KEY_L) == KEY_EDGE_RELEASE) {
        BSP_DEBUG_PRINTF("L pressed\r\n");
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "L pressed", 16);
    }

    if (BSP_Key_GetEvent(KEY_R) == KEY_EDGE_RELEASE) {
        BSP_DEBUG_PRINTF("R pressed\r\n");
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "R pressed", 16);
    }
    SSD1315_Refresh();

}
