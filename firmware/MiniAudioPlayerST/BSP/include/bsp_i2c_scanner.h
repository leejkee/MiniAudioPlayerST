/**
  ******************************************************************************
  * @file    bsp_i2c_scanner.h
  * @brief   BSP I2C 总线扫描模块 — 探测 I2C1 上挂载的设备地址
  * @note    仅封装 HAL I2C 扫描逻辑, 供测试/调试阶段使用。
  *          I2C1 引脚: PB6 (SCL) / PB7 (SDA)
  ******************************************************************************
  */

#ifndef __BSP_I2C_SCANNER_H__
#define __BSP_I2C_SCANNER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  扫描 I2C1 总线, 打印所有应答的设备地址
  * @param  hi2c: I2C 句柄指针 (CubeMX 生成, 例如 &hi2c1)
  * @param  timeout: 单次探测超时时间 (毫秒), 建议 10ms
  * @retval 发现的设备数量 (0 ~ 127)
  * @note   地址范围 0x01 ~ 0x7F (7-bit), 会跳过保留地址段。
  *         探测结果通过 BSP_DEBUG_PRINTF 宏输出, 需 BSP_DEBUG_UART=1。
  */
uint8_t BSP_I2C_Scanner_Scan(I2C_HandleTypeDef *hi2c, uint32_t timeout);


#ifdef __cplusplus
}
#endif

#endif /* __BSP_I2C_SCANNER_H__ */
