/**
  ******************************************************************************
  * @file    bsp_key.h
  * @brief   BSP 按键模块 — 硬件抽象层
  * @note    四个按键: MENU(PA9) / OK(PA10) / L(PA11) / R(PA12)
  *          全部 GPIO 输入 + 内部上拉, 按下接 GND (低电平有效)。
  *          消抖方案: TIM1 每 10ms 触发 BSP_Key_Poll(), 连续 3 次读数一致
  *          才确认状态变化 (30ms 消抖窗口)。
  ******************************************************************************
  */

#ifndef __BSP_KEY_H__
#define __BSP_KEY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 消抖参数 ------------------------------------------------------------------*/
#define BSP_KEY_DEBOUNCE_CNT  3   /* 连续 3 次一致才确认 (3 × 10ms = 30ms) */

/* 按键 ID -------------------------------------------------------------------*/
typedef enum {
    KEY_MENU = 0,
    KEY_OK,
    KEY_L,
    KEY_R,
    KEY_COUNT           /* 按键总数, 非有效 ID */
} bsp_key_id_t;

/* 按键边沿类型 --------------------------------------------------------------*/
typedef enum {
    KEY_EDGE_NONE    = 0,  /* 无事件 */
    KEY_EDGE_RELEASE = 1,  /* 释放沿 (按下→松开, 消抖确认) */
} key_edge_type_t;

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  读取单个按键的消抖后电平
  * @param  id: 按键 ID
  * @retval 0: 按下 (低电平), 1: 释放 (高电平)
  * @note   返回的是消抖后的稳定状态, 由 BSP_Key_Poll() 在定时器中断中维护。
  */
uint8_t BSP_Key_Read(bsp_key_id_t id);

/**
  * @brief  按键消抖扫描 (由 HAL_TIM_PeriodElapsedCallback 每 10ms 调用)
  * @note   读取所有按键 GPIO, 运行消抖状态机。
  *         消抖确认状态变化时设置该键的边沿事件 (sticky, 等待应用层消费)。
  */
void BSP_Key_Poll(void);

/**
  * @brief  获取并消费按键边沿事件
  * @param  id: 按键 ID
  * @retval KEY_EDGE_RELEASE: 释放沿事件 (按下后松开), KEY_EDGE_NONE: 无事件
  * @note   每次调用消费一个事件 (sticky flag 清零)。
  *         一次按键 → 一次事件, 不会重复触发。
  */
key_edge_type_t BSP_Key_GetEvent(bsp_key_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEY_H__ */
