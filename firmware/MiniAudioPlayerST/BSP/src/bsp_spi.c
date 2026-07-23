/**
  ******************************************************************************
  * @file    bsp_spi.c
  * @brief   BSP SPI 模块实现 — 平台相关代码唯一集中点
  * @note    本文件使用 STM32 HAL + hspi1 + PA4。
  *          移植到 GD32 等平台只需重写本文件, 接口 (bsp_spi.h) 不变。
  ******************************************************************************
  */

#include "bsp_spi.h"
#include "spi.h"   /* hspi1 */

/* ========================================================================= */
/*                           SPI 收发                                         */
/* ========================================================================= */

uint8_t BSP_SPI_RW(uint8_t tx_data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi1, &tx_data, &rx_data, 1, HAL_MAX_DELAY);
    return rx_data;
}

void BSP_SPI_Tx(const uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

void BSP_SPI_Rx(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
}

void BSP_SPI_SetSpeed(uint8_t speed)
{
    uint32_t prescaler;

    if (speed == BSP_SPI_SPEED_LOW) {
        prescaler = SPI_BAUDRATEPRESCALER_128;  /* ~375kHz @ 48MHz */
    } else {
        prescaler = SPI_BAUDRATEPRESCALER_2;    /* ~24MHz  @ 48MHz */
    }

    hspi1.Init.BaudRatePrescaler = prescaler;
    HAL_SPI_Init(&hspi1);
}

/* ========================================================================= */
/*                       平台抽象: CS 引脚控制 (PA4)                           */
/* ========================================================================= */

void BSP_SPI_CS_Low(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}

void BSP_SPI_CS_High(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

/* ========================================================================= */
/*                       平台抽象: 系统延时                                    */
/* ========================================================================= */

void BSP_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t BSP_GetTick(void)
{
    return HAL_GetTick();
}
