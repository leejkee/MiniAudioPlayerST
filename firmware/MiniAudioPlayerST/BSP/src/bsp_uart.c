/**
  ******************************************************************************
  * @file    bsp_uart.c
  * @brief   BSP 调试串口模块 — printf 重定向层
  * @note    硬件初始化由 CubeMX 生成的 usart.c 负责 (USART2, PA2/PA3)。
  *          USART2 作为常驻外设始终初始化，不受 BSP_DEBUG_UART 影响。
  *          BSP_DEBUG_UART 仅控制 printf 重定向的编译开关。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp_uart.h"
#include "bsp_config.h"
#include "usart.h"

/* -------------------------------------------------------------------------- */
#if BSP_DEBUG_UART
/* -------------------------------------------------------------------------- */

/**
  * @brief  printf 重定向到 USART2 (被 printf / BSP_DEBUG_PRINTF 底层调用)
  * @note   依赖 CubeMX 生成的 huart2 句柄
  */
int fputc(int ch, FILE *f)
{
    (void)f;
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/**
  * @brief  调试串口初始化入口
  * @note   USART2 硬件初始化由 MX_USART2_UART_Init() 完成 (main.c 中调用)。
  *         本函数仅保留为 API 占位，供上层模块调用。
  */
void BSP_UART_Init(void)
{
    /* USART2 硬件已在 main.c 中由 MX_USART2_UART_Init() 初始化 */
}

/* -------------------------------------------------------------------------- */
#else  /* BSP_DEBUG_UART == 0 */
/* -------------------------------------------------------------------------- */

/**
  * @brief  调试串口禁用时的空实现
  * @note   USART2 硬件仍然初始化 (CubeMX 常驻外设)，
  *         但 printf 重定向不编译，所有 BSP_DEBUG_PRINTF 宏展开为空。
  */
void BSP_UART_Init(void)
{
    /* nothing */
}

/* -------------------------------------------------------------------------- */
#endif /* BSP_DEBUG_UART */
/* -------------------------------------------------------------------------- */
