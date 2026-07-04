/**
  ******************************************************************************
  * @file    key_scanner.c
  * @brief   按键扫描模块 — 应用层实现
  * @note    依赖 BSP 层 BSP_Key_Read() 获取原始电平。
  *          当前仅支持短按检测: 按下沿记录状态, 释放沿产生 KEY_EVENT_SHORT。
  *          长按/连发判定将在后续阶段加入。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "key_scanner.h"

/* 公开函数 ------------------------------------------------------------------*/

/**
  * @brief  初始化按键扫描上下文
  */
void KeyScanner_Init(key_scanner_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->state = 0x00;
}

/**
  * @brief  按键扫描核心逻辑
  * @note   每 20ms 调用一次。
  *         通过 BSP_Key_Read() 读取原始电平 → 边沿检测 → 释放沿产生短按事件。
  *         一次调用最多产生一个事件。
  */
void KeyScanner_Scan(key_scanner_ctx_t *ctx, key_event_t *event)
{
    if (ctx == NULL || event == NULL) {
        return;
    }

    /* 输出先清零 */
    event->id   = KEY_MENU;
    event->type = KEY_EVENT_NONE;

    /* 1. 读取当前所有按键原始电平 (bitmap, bit=1 表示按下) */
    uint8_t current_raw = 0x00;
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (BSP_Key_Read((bsp_key_id_t)i) == 0) {  /* 0 = 按下, 读到哪个位被按下了就把对应的位改成1 */
            current_raw |= (1 << i);
        }
    }

    /* 2. 边沿检测: 对比上一轮状态, 找到变化的位, 使用异或运算即可对比出不同的位, 若已经修改则对应位会被置1, 反之状态不改变置0 */
    uint8_t changed = current_raw ^ ctx->state;

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        // 用来判定该位对应的按键是否发生状态变化
        uint8_t bit_mask = (1 << i);

        // 按下和松开，都会发生状态转换，进入该分支
        if (changed & bit_mask) {

            if (current_raw & bit_mask) {
                /* 按下沿, do nothing → 记录状态 */
                ctx->state |= bit_mask;
            } else {
                /* 释放沿, trigger → 记录状态 */
                if (event->type == KEY_EVENT_NONE) {
                    event->id   = (bsp_key_id_t)i;
                    event->type = KEY_EVENT_SHORT;
                }

                // 因为状态中的按键已经释放了，所以要把状态中的对应位清零
                ctx->state &= ~bit_mask;
            }
        }
        /* 持续按下或持续释放: 不处理 */
    }
}
