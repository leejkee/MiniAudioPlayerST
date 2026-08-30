/**
  ******************************************************************************
  * @file    decoder.c
  * @brief   文件音频流与 VS1003 解码器协调层实现
  ******************************************************************************
  */

#include "decoder.h"

#include "bsp_vs1003.h"

#include <string.h>

#define DECODER_DEFAULT_VOLUME          70U
#define DECODER_VOLUME_ATTENUATION_MAX  80U
#define DECODER_END_FLUSH_BYTES         2048U
#define DECODER_STREAM_PAUSE_TIMEOUT_MS 10U
#define DECODER_ELAPSED_RETRY_DELAY_MS  1U
#define DECODER_ELAPSED_INVALID_MIN     0xFFF0U

typedef struct
{
    decoder_state_t state;
    decoder_state_t state_before_pause;
    FIL            *file;
    uint32_t        audio_size;
    uint32_t        submitted_bytes;
    uint16_t        flush_remaining;
    uint16_t        elapsed_seconds;
    uint8_t         initialized;
    uint8_t         volume;
} decoder_context_t;

static decoder_context_t decoder;

static uint8_t           Decoder_VolumeToAttenuation(uint8_t percent)
{
    if (percent == 0U)
    {
        return 0xFEU;
    }

    return (uint8_t)(((uint32_t)(100U - percent) * DECODER_VOLUME_ATTENUATION_MAX) / 100U);
}

static decoder_status_t Decoder_ApplyVolume(uint8_t percent)
{
    uint8_t             attenuation = Decoder_VolumeToAttenuation(percent);
    uint8_t             keep_paused = (decoder.state == DECODER_STATE_PAUSED);
    bsp_vs1003_status_t status;

    status = BSP_VS1003_PauseStream(DECODER_STREAM_PAUSE_TIMEOUT_MS);
    if (status == BSP_VS1003_OK)
    {
        status = BSP_VS1003_SetVolume(attenuation, attenuation);
    }
    if (keep_paused == 0U)
    {
        BSP_VS1003_ResumeStream();
    }

    return (status == BSP_VS1003_OK) ? DECODER_OK : DECODER_ERR_DEVICE;
}

static void Decoder_ClearStream(void)
{
    decoder.file               = NULL;
    decoder.audio_size         = 0U;
    decoder.submitted_bytes    = 0U;
    decoder.flush_remaining    = 0U;
    decoder.elapsed_seconds    = 0U;
    decoder.state_before_pause = DECODER_STATE_IDLE;
}

static decoder_event_t Decoder_SetTickError(decoder_event_t event)
{
    decoder.state = DECODER_STATE_ERROR;
    return event;
}

static decoder_event_t Decoder_PumpAudio(void)
{
    uint8_t            *buffer;
    uint16_t            capacity;
    UINT                bytes_read;
    FRESULT             file_status;
    uint32_t            stream_remaining;
    uint16_t            read_size;
    bsp_vs1003_status_t device_status;

    if (decoder.file == NULL)
    {
        return Decoder_SetTickError(DECODER_EVENT_FILE_ERROR);
    }
    if (BSP_VS1003_GetState() != BSP_VS1003_STATE_READY)
    {
        return Decoder_SetTickError(DECODER_EVENT_DEVICE_ERROR);
    }
    if (!BSP_VS1003_GetWriteBuffer(&buffer, &capacity))
    {
        return DECODER_EVENT_NONE;
    }

    stream_remaining = (decoder.audio_size > decoder.submitted_bytes)
                           ? decoder.audio_size - decoder.submitted_bytes
                           : 0U;
    if ((decoder.audio_size != 0U) && (stream_remaining == 0U))
    {
        BSP_VS1003_CancelWriteBuffer();
        decoder.flush_remaining = DECODER_END_FLUSH_BYTES;
        decoder.state           = DECODER_STATE_DRAINING;
        return DECODER_EVENT_NONE;
    }

    read_size = capacity;
    if ((decoder.audio_size != 0U) && (stream_remaining < read_size))
    {
        read_size = (uint16_t)stream_remaining;
    }

    file_status = f_read(decoder.file, buffer, read_size, &bytes_read);
    if (file_status != FR_OK)
    {
        BSP_VS1003_CancelWriteBuffer();
        return Decoder_SetTickError(DECODER_EVENT_FILE_ERROR);
    }
    if (bytes_read == 0U)
    {
        BSP_VS1003_CancelWriteBuffer();
        decoder.flush_remaining = DECODER_END_FLUSH_BYTES;
        decoder.state           = DECODER_STATE_DRAINING;
        return DECODER_EVENT_NONE;
    }

    device_status = BSP_VS1003_CommitBuffer((uint16_t)bytes_read);
    if (device_status != BSP_VS1003_OK)
    {
        BSP_VS1003_CancelWriteBuffer();
        return Decoder_SetTickError(DECODER_EVENT_DEVICE_ERROR);
    }

    decoder.submitted_bytes += bytes_read;
    return DECODER_EVENT_NONE;
}

static decoder_event_t Decoder_PumpEndOfStream(void)
{
    uint8_t            *buffer;
    uint16_t            capacity;
    uint16_t            chunk;
    bsp_vs1003_status_t device_status;

    if (BSP_VS1003_GetState() != BSP_VS1003_STATE_READY)
    {
        return Decoder_SetTickError(DECODER_EVENT_DEVICE_ERROR);
    }

    if (decoder.flush_remaining == 0U)
    {
        if (BSP_VS1003_IsStreamIdle())
        {
            Decoder_ClearStream();
            decoder.state = DECODER_STATE_IDLE;
            return DECODER_EVENT_FINISHED;
        }
        return DECODER_EVENT_NONE;
    }

    if (!BSP_VS1003_GetWriteBuffer(&buffer, &capacity))
    {
        return DECODER_EVENT_NONE;
    }

    chunk = (decoder.flush_remaining > capacity) ? capacity : decoder.flush_remaining;
    memset(buffer, 0, chunk);

    device_status = BSP_VS1003_CommitBuffer(chunk);
    if (device_status != BSP_VS1003_OK)
    {
        BSP_VS1003_CancelWriteBuffer();
        return Decoder_SetTickError(DECODER_EVENT_DEVICE_ERROR);
    }

    decoder.flush_remaining -= chunk;
    return DECODER_EVENT_NONE;
}

decoder_status_t Decoder_Init(void)
{
    bsp_vs1003_status_t status;

    if (decoder.initialized != 0U)
    {
        BSP_VS1003_AbortStream();
    }
    memset(&decoder, 0, sizeof(decoder));
    decoder.state  = DECODER_STATE_UNINIT;
    decoder.volume = DECODER_DEFAULT_VOLUME;

    status = BSP_VS1003_Init();
    if (status != BSP_VS1003_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }

    decoder.initialized = 1U;
    decoder.state       = DECODER_STATE_IDLE;
    return DECODER_OK;
}

decoder_status_t Decoder_Start(FIL *file, uint32_t audio_offset, uint32_t audio_size)
{
    if (decoder.initialized == 0U)
    {
        return DECODER_ERR_NOT_INITIALIZED;
    }
    if (file == NULL)
    {
        return DECODER_ERR_PARAM;
    }

    BSP_VS1003_AbortStream();
    Decoder_ClearStream();
    decoder.state = DECODER_STATE_IDLE;

    if (f_lseek(file, audio_offset) != FR_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_FILE;
    }
    if (BSP_VS1003_SoftReset() != BSP_VS1003_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }
    if (Decoder_ApplyVolume(decoder.volume) != DECODER_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }

    decoder.file            = file;
    decoder.audio_size      = audio_size;
    decoder.submitted_bytes = 0U;
    decoder.state           = DECODER_STATE_RUNNING;
    return DECODER_OK;
}

decoder_event_t Decoder_Tick(void)
{
    switch (decoder.state)
    {
        case DECODER_STATE_RUNNING:
            return Decoder_PumpAudio();
        case DECODER_STATE_DRAINING:
            return Decoder_PumpEndOfStream();
        case DECODER_STATE_UNINIT:
        case DECODER_STATE_IDLE:
        case DECODER_STATE_PAUSED:
        case DECODER_STATE_ERROR:
        default:
            return DECODER_EVENT_NONE;
    }
}

void Decoder_Abort(void)
{
    if (decoder.initialized != 0U)
    {
        BSP_VS1003_AbortStream();
    }
    Decoder_ClearStream();
    decoder.state = (decoder.initialized != 0U) ? DECODER_STATE_IDLE : DECODER_STATE_UNINIT;
}

decoder_status_t Decoder_Stop(void)
{
    if (decoder.initialized == 0U)
    {
        return DECODER_ERR_NOT_INITIALIZED;
    }

    Decoder_Abort();
    if (BSP_VS1003_SoftReset() != BSP_VS1003_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }
    return DECODER_OK;
}

decoder_status_t Decoder_Pause(void)
{
    if ((decoder.state != DECODER_STATE_RUNNING) && (decoder.state != DECODER_STATE_DRAINING))
    {
        return DECODER_ERR_PARAM;
    }
    if (BSP_VS1003_PauseStream(DECODER_STREAM_PAUSE_TIMEOUT_MS) != BSP_VS1003_OK)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }

    decoder.state_before_pause = decoder.state;
    decoder.state              = DECODER_STATE_PAUSED;
    return DECODER_OK;
}

decoder_status_t Decoder_Resume(void)
{
    decoder_state_t resume_state;

    if (decoder.state != DECODER_STATE_PAUSED)
    {
        return DECODER_ERR_PARAM;
    }

    resume_state  = decoder.state_before_pause;
    decoder.state = resume_state;
    BSP_VS1003_ResumeStream();
    if (BSP_VS1003_GetState() != BSP_VS1003_STATE_READY)
    {
        decoder.state = DECODER_STATE_ERROR;
        return DECODER_ERR_DEVICE;
    }
    return DECODER_OK;
}

decoder_status_t Decoder_SetVolume(uint8_t percent)
{
    decoder_status_t status;

    if (percent > 100U)
    {
        return DECODER_ERR_PARAM;
    }
    if (decoder.initialized == 0U)
    {
        return DECODER_ERR_NOT_INITIALIZED;
    }

    status = Decoder_ApplyVolume(percent);
    if (status == DECODER_OK)
    {
        decoder.volume = percent;
    }
    return status;
}

decoder_state_t Decoder_GetState(void)
{
    return decoder.state;
}

uint32_t Decoder_GetTransferredBytes(void)
{
    uint32_t transferred = BSP_VS1003_GetStreamTransferredBytes();

    if ((decoder.audio_size != 0U) && (transferred > decoder.audio_size))
    {
        transferred = decoder.audio_size;
    }
    return transferred;
}

uint32_t Decoder_GetElapsedSeconds(void)
{
    uint16_t seconds;
    uint8_t  retry;
    uint8_t  resume_stream;

    if ((decoder.state != DECODER_STATE_RUNNING) && (decoder.state != DECODER_STATE_PAUSED) &&
        (decoder.state != DECODER_STATE_DRAINING))
    {
        return decoder.elapsed_seconds;
    }

    resume_stream = (decoder.state != DECODER_STATE_PAUSED);
    if (BSP_VS1003_PauseStream(DECODER_STREAM_PAUSE_TIMEOUT_MS) != BSP_VS1003_OK)
    {
        if (resume_stream != 0U)
        {
            BSP_VS1003_ResumeStream();
        }
        return decoder.elapsed_seconds;
    }

    for (retry = 0U; retry < 2U; retry++)
    {
        if (BSP_VS1003_ReadRegister(BSP_VS1003_REG_DECODE_TIME, &seconds) != BSP_VS1003_OK)
        {
            break;
        }
        if (seconds < DECODER_ELAPSED_INVALID_MIN)
        {
            decoder.elapsed_seconds = seconds;
            break;
        }
        if (retry == 0U)
        {
            HAL_Delay(DECODER_ELAPSED_RETRY_DELAY_MS);
        }
    }

    if (resume_stream != 0U)
    {
        BSP_VS1003_ResumeStream();
    }
    return decoder.elapsed_seconds;
}
