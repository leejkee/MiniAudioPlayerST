/**
  ******************************************************************************
  * @file    bsp_spi.h
  * @brief   BSP SPI 通用抽象层
  * @note    本模块只封装 SPI 总线操作，不管理具体设备的 CS 和协议时序。
  ******************************************************************************
  */

#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief SPI 总线上下文
  *
  * 每个 SPI 外设创建一个上下文，由 SD、VS1003 等设备驱动持有引用。
  * CS 引脚属于设备驱动，不属于 SPI 总线。
  */
typedef struct
{
    SPI_HandleTypeDef *hspi;
} bsp_spi_context_t;

/**
  * @brief  单字节全双工收发
  * @param  context SPI 总线上下文
  * @param  tx_data 发送字节
  * @param  rx_data 接收字节输出地址
  * @param  timeout HAL 超时时间，单位 ms
  */
HAL_StatusTypeDef BSP_SPI_RW(const bsp_spi_context_t *context, uint8_t tx_data, uint8_t *rx_data,
                             uint32_t timeout);

/**
  * @brief  多字节全双工收发
  * @note   适用于要求命令和响应处于同一个 SPI 事务中的设备协议。
  */
HAL_StatusTypeDef BSP_SPI_TxRx(const bsp_spi_context_t *context, const uint8_t *tx_buf,
                               uint8_t *rx_buf, uint16_t len, uint32_t timeout);

/**
  * @brief  多字节发送
  */
HAL_StatusTypeDef BSP_SPI_Tx(const bsp_spi_context_t *context, const uint8_t *buf, uint16_t len,
                             uint32_t timeout);

/**
  * @brief  多字节接收
  * @param  dummy_data 主机产生接收时钟时发送的填充字节
  * @note   主机模式下由 HAL 产生接收所需的 SPI 时钟。
  */
HAL_StatusTypeDef BSP_SPI_Rx(const bsp_spi_context_t *context, uint8_t *buf, uint16_t len,
                             uint8_t dummy_data, uint32_t timeout);

/**
  * @brief  设置 SPI 硬件预分频
  * @param  prescaler SPI_BAUDRATEPRESCALER_x
  * @note   调用前必须确保总线空闲且没有 DMA/中断传输正在进行。
  */
HAL_StatusTypeDef BSP_SPI_SetPrescaler(const bsp_spi_context_t *context, uint32_t prescaler);

/**
    * @brief  使用 DMA 启动多字节发送
    * @note   本函数为异步接口。返回 HAL_OK 只表示 DMA 已成功启动，
    *         传输缓冲区必须保持有效，直到发送完成回调触发。
    */
HAL_StatusTypeDef BSP_SPI_Tx_DMA(const bsp_spi_context_t *context, const uint8_t *buf, uint16_t len);

/**
    * @brief  使用 DMA 启动全双工收发
    * @note   TX 和 RX DMA 必须同时配置。
    *         tx_buf 和 rx_buf 必须保持有效，直到收发完成回调触发。
    */
HAL_StatusTypeDef    BSP_SPI_TxRx_DMA(const bsp_spi_context_t *context, const uint8_t *tx_buf,
                                      uint8_t *rx_buf, uint16_t len);

/**
    * @brief  停止当前 SPI DMA 传输
    */
HAL_StatusTypeDef    BSP_SPI_DMAStop(const bsp_spi_context_t *context);

/**
    * @brief  获取当前 SPI HAL 状态
    */
HAL_SPI_StateTypeDef BSP_SPI_GetState(const bsp_spi_context_t *context);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H__ */
