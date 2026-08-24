#ifndef __UI_EXCEPTION_PAGE_H__
#define __UI_EXCEPTION_PAGE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "player.h"
#include "ui_render.h"

extern PageHandler_t Page_Exception;

/**
  * @brief  Set the initialization failure displayed by the exception page.
  */
void                 UI_ExceptionPage_SetError(player_status_t error);

#ifdef __cplusplus
}
#endif

#endif /* __UI_EXCEPTION_PAGE_H__ */
