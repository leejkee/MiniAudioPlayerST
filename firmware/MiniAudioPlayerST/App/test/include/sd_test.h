/**
  ******************************************************************************
  * @file    sd_test.h
  * @brief   SD 卡模块上机测试头文件
  ******************************************************************************
  */

#ifndef __SD_TEST_H__
#define __SD_TEST_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

/**
  * @brief  SD 卡测试入口 (由 main.c 一次性调用)
  * @note   测试流程:
  *          1. 挂载文件系统
  *          2. 列出根目录
  *          3. 卸载文件系统
  *         全程通过串口 (BSP_DEBUG_PRINTF) 输出结果
  */
void SD_Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_TEST_H__ */
