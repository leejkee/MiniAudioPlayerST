/**
  ******************************************************************************
  * @file    bsp_key.h
  * @brief   BSP 按键模块 — 硬件抽象层
  * @note    仅封装 GPIO 引脚读取, 不包含消抖/长按等业务逻辑。
  *          四个按键: MENU(PA9) / OK(PA10) / L(PA11) / R(PA12)
  *          全部 GPIO 输入 + 内部上拉, 按下接 GND (低电平有效)。
  ******************************************************************************
  */

#ifndef __BSP_KEY_H__
#define __BSP_KEY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 按键 ID -------------------------------------------------------------------*/
typedef enum {
    KEY_MENU = 0,
    KEY_OK,
    KEY_L,
    KEY_R,
    KEY_COUNT           /* 按键总数, 非有效 ID */
} bsp_key_id_t;

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  读取单个按键的原始 GPIO 电平
  * @param  id: 按键 ID
  * @retval 0: 按下 (低电平), 1: 释放 (高电平)
  * @note   无消抖, 直接返回当前引脚电平, 由上层 (App/key_scanner) 负责去抖
  */
uint8_t BSP_Key_Read(bsp_key_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEY_H__ */
