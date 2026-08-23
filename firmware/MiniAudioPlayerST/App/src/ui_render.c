#include "ui_render.h"
#include "bsp_ssd1315.h"
#include "bsp_key.h"

#define UI_NAV_HISTORY_DEPTH 4U

static PageHandler_t *current_page = NULL;
static PageHandler_t *page_history[UI_NAV_HISTORY_DEPTH];
static uint8_t        page_history_count = 0U;

static void           _ActivatePage(PageHandler_t *page)
{
    current_page = page;
    BSP_SSD1315_ClearBuffer();
    if (current_page->on_enter != NULL)
    {
        current_page->on_enter();
    }
    BSP_SSD1315_Refresh();
}

void UI_Render_SwitchPage(PageHandler_t *page)
{
    if (page == NULL)
    {
        return;
    }

    page_history_count = 0U;
    _ActivatePage(page);
}

uint8_t UI_Render_PushPage(PageHandler_t *page)
{
    if ((page == NULL) || (page == current_page))
    {
        return 0U;
    }

    if (current_page == NULL)
    {
        UI_Render_SwitchPage(page);
        return 1U;
    }

    if (page_history_count >= UI_NAV_HISTORY_DEPTH)
    {
        return 0U;
    }

    page_history[page_history_count] = current_page;
    page_history_count++;
    _ActivatePage(page);
    return 1U;
}

uint8_t UI_Render_PopPage(void)
{
    if (page_history_count == 0U)
    {
        return 0U;
    }

    page_history_count--;
    _ActivatePage(page_history[page_history_count]);
    page_history[page_history_count] = NULL;
    return 1U;
}

uint8_t UI_Render_CanGoBack(void)
{
    return (page_history_count > 0U) ? 1U : 0U;
}

/**
  * @brief  主循环每帧调用页面更新，并分发按键事件
  * @note   按键事件由 TIM1 中断中的 BSP_Key_Poll() 产生 (边沿触发, 消费即清零)。
  *         页面 on_tick 和按键处理均不得阻塞。
  */
void UI_Render_Tick(void)
{
    if (current_page == NULL)
    {
        return;
    }

    if (current_page->on_tick != NULL)
    {
        current_page->on_tick();
    }

    if (current_page->on_key == NULL)
    {
        return;
    }

    for (bsp_key_id_t id = 0; id < KEY_COUNT; id++)
    {
        if (BSP_Key_GetEvent(id) == KEY_EDGE_RELEASE)
        {
            if ((id == KEY_MENU) && (UI_Render_CanGoBack() != 0U))
            {
                (void)UI_Render_PopPage();
            }
            else
            {
                current_page->on_key(id);
            }
        }
    }
}
