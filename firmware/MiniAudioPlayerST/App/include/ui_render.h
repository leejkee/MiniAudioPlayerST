#ifndef __UI_RENDER_H__
#define __UI_RENDER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "bsp_key.h"

typedef struct
{
    void (*on_enter)(void);
    void (*on_key)(bsp_key_id_t key);
    void (*on_tick)(void);
}PageHandler_t;


void UI_Render_SwitchPage(PageHandler_t *page);
void UI_Render_Tick(void);        /* 主循环每帧调用: 轮询按键并分发给当前页面 */


#ifdef __cplusplus
}
#endif

#endif // __UI_RENDER_H__
