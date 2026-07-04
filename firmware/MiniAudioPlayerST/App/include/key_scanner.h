/**
  ******************************************************************************
  * @file    key_scanner.h
  * @brief   按键扫描模块 — 应用层
  * @note    依赖 BSP 层 BSP_Key_Read() 读取原始电平。
  *          在 FreeRTOS keyTask 中每 20ms 调用 KeyScanner_Scan()。
  *          当前仅支持短按检测，长按/连发将在后续阶段加入。
  ******************************************************************************
  */

#ifndef __KEY_SCANNER_H__
#define __KEY_SCANNER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "bsp_key.h"

/* 按键事件类型 --------------------------------------------------------------*/
typedef enum {
    KEY_EVENT_NONE       = 0,   /* 无事件 */
    KEY_EVENT_SHORT,            /* 短按 (按下后释放) */
} key_event_type_t;

/* 按键事件 ------------------------------------------------------------------*/
typedef struct {
    bsp_key_id_t      id;       /* 哪个按键 */
    key_event_type_t  type;     /* 事件类型 */
} key_event_t;

/* 按键扫描上下文 ------------------------------------------------------------*/
typedef struct {
    uint8_t  state;             /* 上一轮稳定状态 (bitmap, 1=按下) */
} key_scanner_ctx_t;

/* 公开 API ------------------------------------------------------------------*/

/**
  * @brief  初始化按键扫描上下文
  * @param  ctx: 上下文指针
  */
void KeyScanner_Init(key_scanner_ctx_t *ctx);

/**
  * @brief  按键扫描函数 (由 keyTask 每 20ms 调用一次)
  * @param  ctx:   上下文指针
  * @param  event: 输出参数, 本次扫描产生的按键事件 (无事件时 type == KEY_EVENT_NONE)
  * @note   每次调用最多产生一个事件 (多键同时按下时按 ID 顺序取第一个)。
  *         检测逻辑: 按下沿记录, 释放沿产生 SHORT 事件。
  */
void KeyScanner_Scan(key_scanner_ctx_t *ctx, key_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_SCANNER_H__ */
