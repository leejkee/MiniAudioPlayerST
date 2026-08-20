/**
  ******************************************************************************
  * @file    key_test.c
  * @brief   按键模块上机测试
  * @note    测试方法:
  *          1. 在 main.c 的 while(1) 中调用 KeyTest_Run()
  *          2. PC 串口工具连接 USART2
  *          3. 依次按下并释放开发板上的按键
  *          4. 检查串口输出的逻辑键名和 GPIO 是否符合硬件连接
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "key_test.h"
#include "bsp_config.h"
#define LOG_MODULE "KeyTest"
#include "bsp_key.h"

/* 按键调试信息 --------------------------------------------------------------*/
static const char *const key_names[KEY_COUNT] = {
    [KEY_MENU] = "MENU (PA9)",
    [KEY_OK]   = "OK (PA10)",
    [KEY_L]    = "L (PB0)",
    [KEY_R]    = "R (PB1)",
};

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  按键测试入口 (由 main.c while(1) 调用)
  * @note   仅在消抖后的按键状态发生变化时输出串口消息, 避免持续刷屏。
  */
void KeyTest_Run(void)
{
    static uint8_t initialized;
    static uint8_t previous_raw_pressed;
    static uint8_t previous_pressed;
    uint8_t raw_pressed = 0;
    uint8_t current_pressed = 0;

    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) {
        raw_pressed |= (1U << KEY_MENU);
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_RESET) {
        raw_pressed |= (1U << KEY_OK);
    }
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
        raw_pressed |= (1U << KEY_L);
    }
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
        raw_pressed |= (1U << KEY_R);
    }

    for (bsp_key_id_t id = KEY_MENU; id < KEY_COUNT; id++) {
        if (BSP_Key_Read(id) == 0) {
            current_pressed |= (1U << id);
        }
    }

    if (!initialized) {
        LOG_DEBUG("Banner", "Key Driver UART Test");
        LOG_DEBUG("KEY", "Monitoring debounced key states...");
        LOG_DEBUG("RAW", "Initial GPIOA->IDR: 0x%04X, GPIOB->IDR: 0x%04X, key bitmap: 0x%02X",
                         (unsigned int)GPIOA->IDR,
                         (unsigned int)GPIOB->IDR,
                         raw_pressed);
        LOG_DEBUG("KEY", "Initial pressed bitmap: 0x%02X", current_pressed);
        previous_raw_pressed = raw_pressed;
        previous_pressed = current_pressed;
        initialized = 1;
        return;
    }

    if (raw_pressed != previous_raw_pressed) {
        LOG_DEBUG("RAW", "GPIOB->IDR: 0x%04X, keys: 0x%02X, PB0=%u, PB1=%u",
                         (unsigned int)GPIOB->IDR,
                         raw_pressed,
                         (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0),
                         (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1));
        previous_raw_pressed = raw_pressed;
    }

    uint8_t changed = current_pressed ^ previous_pressed;

    for (bsp_key_id_t id = KEY_MENU; id < KEY_COUNT; id++) {
        uint8_t key_mask = (1U << id);
        if (changed & key_mask) {
            LOG_DEBUG("KEY", "%s: %s",
                             key_names[id],
                             (current_pressed & key_mask) ? "PRESSED" : "RELEASED");
        }
    }

    previous_pressed = current_pressed;
}

void KeyTest_Init(void)
{
    LOG_DEBUG("Banner", "Key Driver UART Test");
    LOG_DEBUG("KEY", "Initializing key driver...");
    BSP_Key_Init();
    LOG_DEBUG("KEY", "Key driver initialized. Press and release keys to see debug output.");
}
