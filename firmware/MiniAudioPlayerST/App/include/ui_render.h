#ifndef __UI_RENDER_H__
#define __UI_RENDER_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "bsp_key.h"

    typedef struct
    {
        void (*on_enter)(void);            // 切入渲染
        void (*on_key)(bsp_key_id_t key);  // 按键事件处理
        void (*on_tick)(void);             // 循环体
    } PageHandler_t;

    /* 切换根页面并清空后退历史，适用于 UI 初始化或流程重置。 */
    void    UI_Render_SwitchPage(PageHandler_t *page);

    /* 进入子页面并保存当前页面；返回 0 表示参数无效或历史栈已满。 */
    uint8_t UI_Render_PushPage(PageHandler_t *page);

    /* 返回最近的来源页面；当前已经是根页面时返回 0。 */
    uint8_t UI_Render_PopPage(void);
    uint8_t UI_Render_CanGoBack(void);
    void    UI_Render_Tick(void); /* 主循环每帧调用页面更新并分发按键事件 */

#ifdef __cplusplus
}
#endif

#endif  // __UI_RENDER_H__
