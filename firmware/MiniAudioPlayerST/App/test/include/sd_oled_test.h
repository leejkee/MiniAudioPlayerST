/**
  ******************************************************************************
  * @file    sd_oled_test.h
  * @brief   SD 卡 + OLED 文件列表页面联合测试
  * @note    测试方法:
  *          1. 在 main.c 的 init 段调用 SD_OLED_Test_Init()
  *          2. 在 while(1) 中调用 SD_OLED_Test_Run()
  *          3. 按 KEY_L / KEY_R 上下移动光标, 观察 OLED 文件列表刷新
  *          4. 依赖: SD 卡已插入并格式化为 FAT32, /music 目录下有文件
  ******************************************************************************
  */

#ifndef __SD_OLED_TEST_H__
#define __SD_OLED_TEST_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

    /**
  * @brief  测试初始化 — 挂载 SD、初始化 OLED、切换到文件列表页面
  * @note   在 main() 外设初始化完成后调用一次。
  *         BSP_Key_Init() 必须已调用 (按键消抖定时器已启动)。
  */
    void SD_OLED_Test_Init(void);

    /**
  * @brief  测试主循环 — 轮询按键并分发给当前页面
  * @note   在 main() 的 while(1) 中持续调用。
  *         内部调用 UI_Render_Tick() 完成按键 → 页面分发。
  */
    void SD_OLED_Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_OLED_TEST_H__ */
