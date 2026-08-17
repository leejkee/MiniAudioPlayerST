/**
  ******************************************************************************
  * @file    bsp_spi_event.c
  * @brief   HAL SPI 异步事件到设备 BSP 的统一分发，本项目主要处理SD和VS1003两个SPI设备
  ******************************************************************************
  */

#include "bsp_SD.h"

/**
  * @brief SPI DMA 全双工传输完成回调
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL) {
        return;
    }

    if (hspi->Instance == SPI1) {
        BSP_SD_SPI_TxRxCpltCallback();
    }
}

/**
  * @brief SPI DMA 错误回调
  */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL) {
        return;
    }

    if (hspi->Instance == SPI1) {
        BSP_SD_SPI_ErrorCallback();
    }
}
