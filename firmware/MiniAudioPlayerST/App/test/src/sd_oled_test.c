/**
  ******************************************************************************
  * @file    sd_oled_test.c
  * @brief   SD 卡 + OLED 文件列表页面联合测试 — 实现
  * @note    组合测试 SD_Card / FileManager / UI_Render / BSP_SSD1315 / BSP_Key
  *          通过 KEY_L / KEY_R 上下移动光标浏览 /music 目录文件。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "sd_oled_test.h"
#include "sd_card.h"
#include "bsp_ssd1315.h"
#include "bsp_config.h"
#include "ui_file_page.h"

/* -------------------------------------------------------------------------- */
/*  一次性初始化: OLED → SD 挂载 → 切换到文件列表页面                         */
/* -------------------------------------------------------------------------- */
void SD_OLED_Test_Init(void)
{
    static const WCHAR music_path[] = {'/', 'm', 'u', 's', 'i', 'c', 0};

    BSP_DEBUG_PRINTF("\r\n");
    BSP_DEBUG_PRINTF("========== SD + OLED File Page Test ==========\r\n");

    /* 1. 初始化 OLED 显示 */
    BSP_SSD1315_Init();
    BSP_DEBUG_PRINTF("[INIT] OLED initialized.\r\n");

    /* 2. 挂载 SD 卡 (FATFS f_mount) */
    SD_Status_t sd_stat = SD_Mount();
    if (sd_stat != SD_OK) {
        BSP_DEBUG_PRINTF("[ERROR] SD mount failed, code=%d\r\n", sd_stat);
        BSP_SSD1315_Clear();
        BSP_SSD1315_ShowString(0, 0, "SD Mount Err!", 1);
        BSP_SSD1315_Refresh();
        return;
    }
    BSP_DEBUG_PRINTF("[INIT] SD card mounted OK.\r\n");

    /* 3. 输出 /music 原始目录项和文件计数, 用于核对 OLED 列表内容 */
    BSP_DEBUG_PRINTF("[DIAG] Listing /music directory...\r\n");
    sd_stat = SD_Debug_ListDir("/music");
    BSP_DEBUG_PRINTF("[DIAG] SD_Debug_ListDir status=%d\r\n", sd_stat);
    BSP_DEBUG_PRINTF("[DIAG] SD_CountFiles(/music)=%u\r\n",
                     SD_CountFiles(music_path));

    /* 4. 切换到文件列表页面 (Page_File.on_enter → FileManager_Init → _Render) */
    // UI_Render_SwitchPage(&Page_File);
    Page_File.on_enter();
    BSP_DEBUG_PRINTF("[INIT] Switched to File Page.\r\n");
}

/* -------------------------------------------------------------------------- */
/*  主循环: 轮询按键 → 分发给当前页面                                         */
/* -------------------------------------------------------------------------- */
void SD_OLED_Test_Run(void)
{
    for (bsp_key_id_t id = 0; id < KEY_COUNT; id++) {
        if (BSP_Key_GetEvent(id) == KEY_EDGE_RELEASE) {
           Page_File.on_key(id);
        }
    }
}
