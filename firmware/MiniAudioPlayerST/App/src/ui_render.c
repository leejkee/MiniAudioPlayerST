#include "ui_render.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"


static PageHandler_t *current_page = NULL;

void UI_Render_SwitchPage(PageHandler_t *page)
{
    current_page = page;
    BSP_SSD1315_Clear();
    current_page->on_enter();
    BSP_SSD1315_Refresh();
}

/**
  * @brief  主循环每帧调用: 轮询所有按键, 将事件分发给当前页面
  * @note   按键事件由 TIM1 中断中的 BSP_Key_Poll() 产生 (边沿触发, 消费即清零)。
  *         无事件时快速返回, 不阻塞。
  */
void UI_Render_Tick(void)
{
    if (current_page == NULL || current_page->on_key == NULL) {
        return;
    }

    for (bsp_key_id_t id = 0; id < KEY_COUNT; id++) {
        if (BSP_Key_GetEvent(id) == KEY_EDGE_RELEASE) {
            current_page->on_key(id);
        }
    }
}
