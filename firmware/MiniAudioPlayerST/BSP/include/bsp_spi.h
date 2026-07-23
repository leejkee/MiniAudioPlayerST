/**
  ******************************************************************************
  * @file    bsp_spi.h
  * @brief   BSP SPI 模块 — 平台抽象层 (SD 卡专用)
  * @note    本模块将硬件细节 (HAL、引脚、SPI 实例) 封装在 bsp_spi.c 内部。
  *          调用方 (bsp_SD) 不感知底层平台, 移植时只改 bsp_spi.c。
  ******************************************************************************
  */

#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* SPI 速度等级 ----------------------------------------------------------------*/
#define BSP_SPI_SPEED_LOW       0   /* 低速 (<400kHz), SD 初始化阶段      */
#define BSP_SPI_SPEED_HIGH      1   /* 高速, SD 初始化完成后数据读写阶段  */

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  单字节全双工收发
  * @param  tx_data: 发送字节
  * @retval 接收字节
  */
uint8_t BSP_SPI_RW(uint8_t tx_data);

/**
  * @brief  多字节发送 (忽略接收数据)
  * @param  buf: 发送缓冲区
  * @param  len: 字节数
  */
void BSP_SPI_Tx(const uint8_t *buf, uint16_t len);

/**
  * @brief  多字节接收 (发送 0xFF 作为时钟)
  * @param  buf: 接收缓冲区
  * @param  len: 字节数
  */
void BSP_SPI_Rx(uint8_t *buf, uint16_t len);

/**
  * @brief  动态切换 SPI 时钟速度
  * @param  speed: BSP_SPI_SPEED_LOW 或 BSP_SPI_SPEED_HIGH
  * @note   平台实现负责映射到硬件分频值。
  */
void BSP_SPI_SetSpeed(uint8_t speed);

/* ========================================================================= */
/*                       平台抽象: CS 引脚控制                                 */
/* ========================================================================= */

/**
  * @brief  CS = 低电平, 选中 SD 卡
  */
void BSP_SPI_CS_Low(void);

/**
  * @brief  CS = 高电平, 释放 SD 卡
  */
void BSP_SPI_CS_High(void);

/* ========================================================================= */
/*                       平台抽象: 系统延时                                    */
/* ========================================================================= */

/**
  * @brief  毫秒级阻塞延时
  */
void BSP_DelayMs(uint32_t ms);

/**
  * @brief  获取系统毫秒滴答 (非阻塞超时用)
  * @retval 自启动以来的毫秒数
  */
uint32_t BSP_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H__ */
