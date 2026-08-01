/**
  ******************************************************************************
  * @file    bsp_spi.c
  * @brief   BSP SPI 通用抽象层实现
  ******************************************************************************
  */

#include "bsp_spi.h"

static uint8_t BSP_SPI_IsValid(const bsp_spi_context_t *context)
{
    return (context != NULL) && (context->hspi != NULL);
}

HAL_StatusTypeDef BSP_SPI_RW(const bsp_spi_context_t *context,
                             uint8_t tx_data,
                             uint8_t *rx_data,
                             uint32_t timeout)
{
    if (!BSP_SPI_IsValid(context) || (rx_data == NULL)) {
        return HAL_ERROR;
    }

    return HAL_SPI_TransmitReceive(context->hspi,
                                   &tx_data,
                                   rx_data,
                                   1,
                                   timeout);
}

HAL_StatusTypeDef BSP_SPI_TxRx(const bsp_spi_context_t *context,
                               const uint8_t *tx_buf,
                               uint8_t *rx_buf,
                               uint16_t len,
                               uint32_t timeout)
{
    if (!BSP_SPI_IsValid(context)
        || (tx_buf == NULL)
        || (rx_buf == NULL)
        || (len == 0U)) {
        return HAL_ERROR;
    }

    return HAL_SPI_TransmitReceive(context->hspi,
                                   (uint8_t *)tx_buf,
                                   rx_buf,
                                   len,
                                   timeout);
}

HAL_StatusTypeDef BSP_SPI_Tx(const bsp_spi_context_t *context,
                             const uint8_t *buf,
                             uint16_t len,
                             uint32_t timeout)
{
    if (!BSP_SPI_IsValid(context) || (buf == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    return HAL_SPI_Transmit(context->hspi,
                            (uint8_t *)buf,
                            len,
                            timeout);
}

HAL_StatusTypeDef BSP_SPI_Rx(const bsp_spi_context_t *context,
                             uint8_t *buf,
                             uint16_t len,
                             uint8_t dummy_data,
                             uint32_t timeout)
{
    if (!BSP_SPI_IsValid(context) || (buf == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    /*
     * STM32 HAL 在全双工主机模式下会将接收缓冲区同时作为发送缓冲区，
     * 因此需要先写入调用方指定的 dummy 字节。
     */
    for (uint16_t i = 0U; i < len; ++i) {
        buf[i] = dummy_data;
    }

    return HAL_SPI_Receive(context->hspi, buf, len, timeout);
}

HAL_StatusTypeDef BSP_SPI_SetPrescaler(const bsp_spi_context_t *context,
                                       uint32_t prescaler)
{
    if (!BSP_SPI_IsValid(context)) {
        return HAL_ERROR;
    }

    if (context->hspi->Init.BaudRatePrescaler == prescaler) {
        return HAL_OK;
    }

    context->hspi->Init.BaudRatePrescaler = prescaler;
    return HAL_SPI_Init(context->hspi);
}
