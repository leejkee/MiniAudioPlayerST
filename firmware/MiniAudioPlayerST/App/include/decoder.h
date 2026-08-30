/**
  ******************************************************************************
  * @file    decoder.h
  * @brief   文件音频流与 VS1003 解码器协调层
  ******************************************************************************
  */

#ifndef __DECODER_H
#define __DECODER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "fatfs.h"

typedef enum
{
    DECODER_STATE_UNINIT = 0,
    DECODER_STATE_IDLE,
    DECODER_STATE_RUNNING,
    DECODER_STATE_PAUSED,
    DECODER_STATE_DRAINING,
    DECODER_STATE_ERROR
} decoder_state_t;

typedef enum
{
    DECODER_OK = 0,
    DECODER_ERR_PARAM,
    DECODER_ERR_NOT_INITIALIZED,
    DECODER_ERR_FILE,
    DECODER_ERR_DEVICE
} decoder_status_t;

typedef enum
{
    DECODER_EVENT_NONE = 0,
    DECODER_EVENT_FINISHED,
    DECODER_EVENT_FILE_ERROR,
    DECODER_EVENT_DEVICE_ERROR
} decoder_event_t;

decoder_status_t Decoder_Init(void);

/**
  * @brief  开始解码指定文件中的音频区间
  * @note   file 由调用方持有，Decoder_Stop()/Decoder_Abort() 前不得关闭。
  */
decoder_status_t Decoder_Start(FIL *file, uint32_t audio_offset, uint32_t audio_size);

/**
  * @brief  主循环轮询入口，每次最多读取并提交一个流缓冲区
  */
decoder_event_t  Decoder_Tick(void);

/** 立即中止当前流，不软复位解码器。 */
void             Decoder_Abort(void);

/** 停止当前流并软复位解码器。 */
decoder_status_t Decoder_Stop(void);
decoder_status_t Decoder_Pause(void);
decoder_status_t Decoder_Resume(void);
decoder_status_t Decoder_SetVolume(uint8_t percent);

decoder_state_t  Decoder_GetState(void);
uint32_t         Decoder_GetTransferredBytes(void);
uint32_t         Decoder_GetElapsedSeconds(void);

#ifdef __cplusplus
}
#endif

#endif /* __DECODER_H */
