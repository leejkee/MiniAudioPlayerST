/**
  ******************************************************************************
  * @file    bsp_key.c
  * @brief   BSP 按键模块 — 硬件抽象层实现
  * @note    封装 GPIO 读取 + 软件消抖。
  *          BSP_Key_Poll() 由 TIM1 中断每 10ms 调用, 维护消抖后的稳定状态。
  *          BSP_Key_Read() 返回的是消抖后的电平。
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
    [KEY_OK]   = { GPIOA, GPIO_PIN_10 },
    [KEY_L]    = { GPIOA, GPIO_PIN_11 },
    [KEY_R]    = { GPIOA, GPIO_PIN_12 },
};

/* 消抖上下文 ----------------------------------------------------------------*/
static struct {
    uint8_t stable_state;           /* 消抖后的稳定状态 (bitmap, 1=按下) */
    uint8_t debounce_cnt[KEY_COUNT]; /* 每个按键的消抖计数 */
} key_ctx;

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  读取单个按键的消抖后电平
  * @param  id: 按键 ID
  * @retval 0: 按下 (低电平), 1: 释放 (高电平)
  */
uint8_t BSP_Key_Read(bsp_key_id_t id)
{
    if (id >= KEY_COUNT) {
        return 1; /* 无效 ID, 视为释放 */
    }

    /* 返回消抖后的稳定状态: bit=1 → 按下 → 返回 0 */
    return (key_ctx.stable_state & (1 << id)) ? 0 : 1;
}

/**
  * @brief  按键消抖扫描 (由 HAL_TIM_PeriodElapsedCallback 每 10ms 调用)
  * @note   消抖算法:
  *         1. 读取所有按键当前 raw 电平
  *         2. 对比 stable_state:
  *            - raw == stable → 清零该键计数器
  *            - raw != stable → 计数器 +1
  *         3. 计数器 >= BSP_KEY_DEBOUNCE_CNT → 确认变化, 更新 stable_state
  */
void BSP_Key_Poll(void)
{
    uint8_t current_raw = 0;

    /* 1. 读取所有按键当前电平 */
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        const bsp_key_pin_t *kp = &key_pins[i];
        if (HAL_GPIO_ReadPin(kp->port, kp->pin) == GPIO_PIN_RESET) {
            current_raw |= (1 << i); /* bit=1 表示按下 */
        }
    }

    /* 2. 逐键运行消抖状态机 */
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        uint8_t raw_bit = (current_raw >> i) & 1;
        uint8_t stable_bit = (key_ctx.stable_state >> i) & 1;

        if (raw_bit == stable_bit) {
            /* 与稳定状态一致 → 清零计数 */
            key_ctx.debounce_cnt[i] = 0;
        } else {
            /* 不一致 → 累加计数 */
            key_ctx.debounce_cnt[i]++;

            /* 连续 N 次一致 → 确认变化 */
            if (key_ctx.debounce_cnt[i] >= BSP_KEY_DEBOUNCE_CNT) {
                /* 翻转稳定状态位 */
                key_ctx.stable_state ^= (1 << i);
                key_ctx.debounce_cnt[i] = 0;
            }
        }
    }
}
