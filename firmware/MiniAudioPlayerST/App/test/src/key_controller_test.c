/**
  ******************************************************************************
  * @file    key_controller_test.c
  * @brief   按键控制器测试 — 按键状态 OLED 显示
  * @note    直接通过 BSP_Key_Read() 读取按键电平, 实时显示在 OLED 上。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "key_controller_test.h"
#include "ssd1315.h"
#include "bsp_config.h"
#include "bsp_key.h"

static uint8_t raw_pins = 0;

void Key_Controller_Test_Init(void)
{
    BSP_DEBUG_PRINTF("TEST 0");
    SSD1315_Init();

    BSP_DEBUG_PRINTF("TEST 1");
}

void Key_Controller_Test_Loop(void)
{
    /* 读取 4 个按键原始电平 */
    raw_pins = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (BSP_Key_Read((bsp_key_id_t)i) == 0) {
            raw_pins |= (1 << i);
        }
    }

    if (raw_pins & 1){
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "Menu pressed", 16);
    } else if (raw_pins & 2){
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "OK pressed", 16);
    } else if (raw_pins & 4){
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "L pressed", 16);
    } else if (raw_pins & 8){
        SSD1315_Clear();
        SSD1315_ShowString(0, 0, "R pressed", 16);
    }

    HAL_Delay(20);
}
