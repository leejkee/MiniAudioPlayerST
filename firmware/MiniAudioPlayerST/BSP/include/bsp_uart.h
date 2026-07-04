/**
  ******************************************************************************
  * @file    bsp_uart.h
  * @brief   BSP 调试串口模块 — 硬件抽象层
  * @note    USART2 由 CubeMX 配置为常驻外设 (usart.c)。
  *          BSP_DEBUG_UART 仅控制 printf 重定向的编译开关。
  ******************************************************************************
  */

#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  调试串口初始化
  * @note   仅在 BSP_DEBUG_UART=1 时激活 printf 重定向，
  *         USART2 硬件初始化由 CubeMX 生成的 MX_USART2_UART_Init() 负责。
  */
void BSP_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_UART_H__ */
