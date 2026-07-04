/**
  ******************************************************************************
  * @file    bsp_key.c
  * @brief   BSP 按键模块 — 硬件抽象层实现
  * @note    仅封装 GPIO 引脚电平读取, 不包含任何消抖/业务逻辑。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp_key.h"

/* 按键引脚查找表 ------------------------------------------------------------*/
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} bsp_key_pin_t;

static const bsp_key_pin_t key_pins[KEY_COUNT] = {
    [KEY_MENU] = { GPIOA, GPIO_PIN_9  },
    [KEY_OK]   = { GPIOA, GPIO_PIN_10  },
    [KEY_L]    = { GPIOA, GPIO_PIN_11 },
    [KEY_R]    = { GPIOA, GPIO_PIN_12 },
};

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  读取单个按键的原始 GPIO 电平
  * @param  id: 按键 ID
  * @retval 0: 按下 (低电平), 1: 释放 (高电平)
  */
uint8_t BSP_Key_Read(bsp_key_id_t id)
{
    if (id >= KEY_COUNT) {
        return 1; /* 无效 ID, 视为释放 */
    }

    const bsp_key_pin_t *kp = &key_pins[id];

    /* 上拉 + 按下接 GND → 低电平有效, HAL_GPIO_ReadPin 返回 GPIO_PIN_RESET (0) 表示按下 */
    return (HAL_GPIO_ReadPin(kp->port, kp->pin) == GPIO_PIN_RESET) ? 0 : 1;
}
