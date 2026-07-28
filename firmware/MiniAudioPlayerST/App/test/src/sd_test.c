/**
  ******************************************************************************
  * @file    sd_test.c
  * @brief   SD 卡模块上机测试
  * @note    测试方法:
  *          1. 在 main.c 的 USER CODE BEGIN 2 中调用 SD_Test_Run()
  *          2. 插入 FAT32 格式化的 SD 卡
  *          3. 通过串口工具 (USART2, 115200 8N1, UTF-8) 观察输出
  *          4. 确认根目录文件列表正确后即可删除本调用
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "sd_test.h"
#include "sd_card.h"
#include "bsp_config.h"

/* 测试入口 ------------------------------------------------------------------*/

/**
  * @brief  SD 卡功能测试 — 挂载、列目录、卸载
  * @note   一次性测试, 运行完毕即退出。
  *         如果挂载失败, 打印错误并跳过后续步骤。
  */
void SD_Test_Run(void)
{
    BSP_DEBUG_PRINTF("\r\n");
    BSP_DEBUG_PRINTF("========== SD Card Test Start ==========\r\n");
    BSP_DEBUG_PRINTF("\r\n");

    /* ---- 步骤 1: 挂载文件系统 ---- */
    BSP_DEBUG_PRINTF("[TEST 1] Mounting SD card...\r\n");
    if (SD_Mount() != SD_OK) {
        BSP_DEBUG_PRINTF("[TEST 1] Mount FAILED - check SD card\r\n");
        BSP_DEBUG_PRINTF("\r\n");
        BSP_DEBUG_PRINTF("========== SD Card Test Aborted ==========\r\n");
        return;
    }
    BSP_DEBUG_PRINTF("[TEST 1] Mount OK\r\n");
    BSP_DEBUG_PRINTF("\r\n");

    /* ---- 步骤 2: 列出根目录 ---- */
    BSP_DEBUG_PRINTF("[TEST 2] Listing root directory:\r\n");
    BSP_DEBUG_PRINTF("-------------------------------------------\r\n");
    SD_Debug_ListDir("/");
    BSP_DEBUG_PRINTF("-------------------------------------------\r\n");
    BSP_DEBUG_PRINTF("\r\n");

    /* ---- 步骤 3: 列出 /music 目录 ---- */
    BSP_DEBUG_PRINTF("[TEST 3] Listing /music directory:\r\n");
    BSP_DEBUG_PRINTF("-------------------------------------------\r\n");
    SD_Debug_ListDir("/music");
    BSP_DEBUG_PRINTF("-------------------------------------------\r\n");
    BSP_DEBUG_PRINTF("\r\n");

    /* ---- 步骤 4: 卸载文件系统 ---- */
    BSP_DEBUG_PRINTF("[TEST 4] Unmounting SD card...\r\n");
    SD_Unmount();
    BSP_DEBUG_PRINTF("[TEST 4] Unmount OK\r\n");
    BSP_DEBUG_PRINTF("\r\n");

    BSP_DEBUG_PRINTF("========== SD Card Test End ==========\r\n");
}
