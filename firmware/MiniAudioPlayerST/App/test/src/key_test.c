/**
  ******************************************************************************
  * @file    key_test.c
  * @brief   按键模块上机测试
  * @note    测试方法:
  *          1. 在 main.c 的 while(1) 中调用 KeyTest_Run()
  *          2. 在 KeyScanner_Scan 的释放沿处设置断点 (key_scanner.c 第 67 行)
  *          3. 在 Keil 中 Debug → 按下开发板上的按键 → 断点触发
  *          4. 在 Watch 窗口观察 event.id 和 event.type 的值
  *          5. 确认后即可删除本文件
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "key_test.h"
#include "bsp_key.h"
#include "key_scanner.h"

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  按键测试入口 (由 main.c while(1) 调用)
  * @note   基于 HAL_Delay 的简单轮询, 不依赖 FreeRTOS。
  *         测试完成后删除 main.c 中的调用即可。
  */
void KeyTest_Run(void)
{
    static key_scanner_ctx_t ctx;
    static uint8_t initialized = 0;
    static uint8_t raw_pins = 0;
    static key_event_t event;

    if (!initialized) {
        KeyScanner_Init(&ctx);

        /* 初始化 PA8 为推挽输出, 驱动按键状态指示 LED
           LED 连接: PA8 → 限流电阻(220Ω~1kΩ) → LED(+) → LED(-) → GND */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin   = GPIO_PIN_8;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        initialized = 1;
    }

    /* 读取 4 个按键原始电平 */
    raw_pins = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (BSP_Key_Read((bsp_key_id_t)i) == 0) {
            raw_pins |= (1 << i);
        }
    }

    /* LED 指示: 任意键按下 → PA8 输出高, LED 亮 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,
        (raw_pins != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    KeyScanner_Scan(&ctx, &event);

    if (event.type != KEY_EVENT_NONE) {
        __NOP();  /* 按键事件产生 (释放沿) */
    }

    HAL_Delay(20);
}
