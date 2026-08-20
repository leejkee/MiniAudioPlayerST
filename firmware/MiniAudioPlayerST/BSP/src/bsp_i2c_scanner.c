/**
  ******************************************************************************
  * @file    bsp_i2c_scanner.c
  * @brief   BSP I2C 总线扫描模块 — 实现
  * @note    遍历 7-bit 地址空间, 通过 HAL_I2C_IsDeviceReady 探测应答。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp_i2c_scanner.h"
#include "bsp_config.h"

#define LOG_MODULE "I2CScanner"

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  扫描 I2C1 总线, 打印所有应答的设备地址
  * @param  hi2c: I2C 句柄指针
  * @param  timeout: 单次探测超时时间 (ms)
  * @retval 发现的设备数量
  */
uint8_t BSP_I2C_Scanner_Scan(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t count = 0;

    if (hi2c == NULL) {
        LOG_DEBUG("Scan", "handle is NULL");
        return 0;
    }

    LOG_DEBUG("Scan", "Scanning 7-bit address space");

    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 2, timeout) == HAL_OK) {
            LOG_DEBUG("Found", "address=0x%02X", addr);
            count++;
        }
    }

    if (count == 0) {
        LOG_DEBUG("Result", "No device found");
    } else {
        LOG_DEBUG("Result", "%u device(s) found", count);
    }

    return count;
}
