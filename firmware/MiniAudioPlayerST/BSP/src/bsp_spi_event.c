/**
  ******************************************************************************
  * @file    bsp_spi_event.c
  * @brief   HAL SPI 异步事件到设备 BSP 的统一分发，本项目主要处理SD和VS1003两个SPI设备
  ******************************************************************************
  */

#include "bsp_SD.h"
#include "bsp_vs1003.h"

/**
  * @brief SPI DMA 单向发送完成回调
  */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if ((hspi != NULL) && (hspi->Instance == SPI2))
    {
        BSP_VS1003_SPI_TxCpltCallback();
    }
}

/**
  * @brief SPI DMA 全双工传输完成回调
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL)
    {
        return;
    }

    if (hspi->Instance == SPI1)
    {
        BSP_SD_SPI_TxRxCpltCallback();
    }
}

/**
  * @brief SPI DMA 错误回调
  */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL)
    {
        return;
    }

    if (hspi->Instance == SPI1)
    {
        BSP_SD_SPI_ErrorCallback();
    }
    if (hspi->Instance == SPI2)
    {
        BSP_VS1003_SPI_ErrorCallback();
    }
}

/**
  * @brief GPIO 外部中断统一分发
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == VS1003_DREQ_Pin)
    {
        BSP_VS1003_DREQCallback();
    }
}
