/**
  ******************************************************************************
  * @file    bsp_vs1003.c
  * @brief   VS1003 音频解码器 BSP 驱动实现
  * @note    SCI 使用阻塞式 SPI2，SDI 使用 DREQ + DMA 双缓冲异步发送。
  ******************************************************************************
  */

#include "bsp_vs1003.h"
#include "bsp_spi.h"
#include "spi.h"

#define VS1003_SPI_TIMEOUT_MS        100U
#define VS1003_DREQ_TIMEOUT_MS       500U
#define VS1003_RESET_LOW_DELAY_MS    100U
#define VS1003_RESET_RECOVERY_MS     100U
#define VS1003_SPI_PRESCALER_INIT    SPI_BAUDRATEPRESCALER_128
#define VS1003_SPI_PRESCALER_SCI     SPI_BAUDRATEPRESCALER_128
#define VS1003_SPI_PRESCALER_SDI     SPI_BAUDRATEPRESCALER_8
#define VS1003_CLOCKF_DEFAULT        0x9800U
#define VS1003_VOLUME_DEFAULT        0x2020U
#define VS1003_FLUSH_BYTES           2048U
#define VS1003_STATUS_VERSION_MASK   0x0070U
#define VS1003_STATUS_VERSION_VALUE  0x0030U
#define VS1003_VERIFY_RETRY_COUNT    10U
#define VS1003_VERIFY_RETRY_DELAY_MS 2U
#define VS1003_STREAM_BUFFER_COUNT   2U

typedef enum
{
    VS1003_BUFFER_EMPTY = 0,
    VS1003_BUFFER_WRITING,
    VS1003_BUFFER_READY,
    VS1003_BUFFER_ACTIVE
} vs1003_buffer_state_t;

typedef struct
{
    uint8_t                        data[BSP_VS1003_STREAM_BUFFER_SIZE];
    volatile uint16_t              size;
    volatile uint16_t              offset;
    volatile vs1003_buffer_state_t state;
} vs1003_stream_buffer_t;

typedef struct
{
    vs1003_stream_buffer_t buffers[VS1003_STREAM_BUFFER_COUNT];
    volatile int8_t        producer_index;
    volatile int8_t        active_index;
    volatile uint8_t       write_index;
    volatile uint8_t       read_index;
    volatile uint8_t       tx_busy;
    volatile uint8_t       paused;
    volatile uint16_t      last_chunk_size;
    volatile uint32_t      transferred_bytes;
} vs1003_stream_context_t;

static const bsp_spi_context_t     vs1003_spi = {.hspi = &hspi2};

static volatile bsp_vs1003_state_t vs1003_state = BSP_VS1003_STATE_UNINIT;
static bsp_vs1003_verify_diag_t    vs1003_verify_diag;
static vs1003_stream_context_t     vs1003_stream = {.producer_index = -1, .active_index = -1};

static inline void                 VS1003_XCS_High(void)
{
    HAL_GPIO_WritePin(VS1003_XCS_GPIO_Port, VS1003_XCS_Pin, GPIO_PIN_SET);
}

static inline void VS1003_XCS_Low(void)
{
    HAL_GPIO_WritePin(VS1003_XCS_GPIO_Port, VS1003_XCS_Pin, GPIO_PIN_RESET);
}

static inline void VS1003_XDCS_High(void)
{
    HAL_GPIO_WritePin(VS1003_XDCS_GPIO_Port, VS1003_XDCS_Pin, GPIO_PIN_SET);
}

static inline void VS1003_XDCS_Low(void)
{
    HAL_GPIO_WritePin(VS1003_XDCS_GPIO_Port, VS1003_XDCS_Pin, GPIO_PIN_RESET);
}

static inline void VS1003_XRESET_High(void)
{
    HAL_GPIO_WritePin(VS1003_XRESET_GPIO_Port, VS1003_XRESET_Pin, GPIO_PIN_SET);
}

static inline void VS1003_XRESET_Low(void)
{
    HAL_GPIO_WritePin(VS1003_XRESET_GPIO_Port, VS1003_XRESET_Pin, GPIO_PIN_RESET);
}

static bsp_vs1003_status_t VS1003_FromHALStatus(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
    {
        return BSP_VS1003_OK;
    }
    if (status == HAL_TIMEOUT)
    {
        return BSP_VS1003_ERR_TIMEOUT;
    }
    return BSP_VS1003_ERR_SPI;
}

static bsp_vs1003_status_t VS1003_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    do
    {
        if (BSP_VS1003_IsReady())
        {
            return BSP_VS1003_OK;
        }
    } while ((HAL_GetTick() - start) < timeout_ms);

    return BSP_VS1003_ERR_TIMEOUT;
}

static void VS1003_DeselectAll(void)
{
    VS1003_XCS_High();
    VS1003_XDCS_High();
}

static bsp_vs1003_status_t VS1003_SetPrescaler(uint32_t prescaler)
{
    return VS1003_FromHALStatus(BSP_SPI_SetPrescaler(&vs1003_spi, prescaler));
}

static bsp_vs1003_status_t VS1003_ReadExpected(uint8_t address, uint16_t expected, uint16_t mask);
static bsp_vs1003_status_t VS1003_WriteVerified(uint8_t address, uint16_t value);
static bsp_vs1003_status_t VS1003_ConfigureAfterReset(void);
static void                VS1003_TryFeed(void);

static uint32_t            VS1003_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void VS1003_ExitCritical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void VS1003_ResetStreamContext(void)
{
    uint8_t index;

    for (index = 0U; index < VS1003_STREAM_BUFFER_COUNT; index++)
    {
        vs1003_stream.buffers[index].size   = 0U;
        vs1003_stream.buffers[index].offset = 0U;
        vs1003_stream.buffers[index].state  = VS1003_BUFFER_EMPTY;
    }

    vs1003_stream.producer_index    = -1;
    vs1003_stream.active_index      = -1;
    vs1003_stream.write_index       = 0U;
    vs1003_stream.read_index        = 0U;
    vs1003_stream.tx_busy           = 0U;
    vs1003_stream.paused            = 0U;
    vs1003_stream.last_chunk_size   = 0U;
    vs1003_stream.transferred_bytes = 0U;
}

static void VS1003_ClearVerifyDiag(void)
{
    vs1003_verify_diag.valid = 0U;
}

static void VS1003_RecordVerifyDiag(uint8_t address, uint16_t expected, uint16_t actual,
                                    uint16_t mask)
{
    vs1003_verify_diag.valid    = 1U;
    vs1003_verify_diag.address  = address;
    vs1003_verify_diag.expected = expected;
    vs1003_verify_diag.actual   = actual;
    vs1003_verify_diag.mask     = mask;
}

bsp_vs1003_status_t BSP_VS1003_WriteRegister(uint8_t address, uint16_t value)
{
    uint8_t             frame[4];
    uint8_t             response[4];
    bsp_vs1003_status_t status;

    if (address > BSP_VS1003_REG_AICTRL3)
    {
        return BSP_VS1003_ERR_PARAM;
    }

    /*
     * SCI 读时钟上限低于 SDI 写时钟上限。统一将 SCI 事务切到
     * 750 kHz，为复位后的 1 倍内部时钟和外部接线保留充足裕量。
     */
    status = VS1003_SetPrescaler(VS1003_SPI_PRESCALER_SCI);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    status = VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    frame[0] = BSP_VS1003_SCI_WRITE;
    frame[1] = address;
    frame[2] = (uint8_t)(value >> 8);
    frame[3] = (uint8_t)value;

    VS1003_XDCS_High();
    VS1003_XCS_Low();
    status = VS1003_FromHALStatus(
        BSP_SPI_TxRx(&vs1003_spi, frame, response, sizeof(frame), VS1003_SPI_TIMEOUT_MS));
    VS1003_XCS_High();

    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    return VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
}

bsp_vs1003_status_t BSP_VS1003_ReadRegister(uint8_t address, uint16_t *value)
{
    uint8_t             frame[4];
    uint8_t             response[4];
    bsp_vs1003_status_t status;

    if ((address > BSP_VS1003_REG_AICTRL3) || (value == NULL))
    {
        return BSP_VS1003_ERR_PARAM;
    }

    status = VS1003_SetPrescaler(VS1003_SPI_PRESCALER_SCI);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    status = VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    frame[0] = BSP_VS1003_SCI_READ;
    frame[1] = address;
    frame[2] = 0xFFU;
    frame[3] = 0xFFU;

    VS1003_XDCS_High();
    VS1003_XCS_Low();

    status = VS1003_FromHALStatus(
        BSP_SPI_TxRx(&vs1003_spi, frame, response, sizeof(frame), VS1003_SPI_TIMEOUT_MS));

    VS1003_XCS_High();

    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    *value = ((uint16_t)response[2] << 8) | response[3];
    return VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
}

bsp_vs1003_status_t BSP_VS1003_HardwareReset(void)
{
    bsp_vs1003_status_t status;

    BSP_VS1003_AbortStream();
    vs1003_state = BSP_VS1003_STATE_UNINIT;
    VS1003_DeselectAll();
    VS1003_XRESET_Low();

    status = VS1003_SetPrescaler(VS1003_SPI_PRESCALER_INIT);
    if (status != BSP_VS1003_OK)
    {
        vs1003_state = BSP_VS1003_STATE_ERROR;
        return status;
    }

    HAL_Delay(VS1003_RESET_LOW_DELAY_MS);
    VS1003_XRESET_High();
    HAL_Delay(VS1003_RESET_RECOVERY_MS);

    status = VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
    if (status != BSP_VS1003_OK)
    {
        vs1003_state = BSP_VS1003_STATE_ERROR;
    }
    return status;
}

bsp_vs1003_status_t BSP_VS1003_SoftReset(void)
{
    bsp_vs1003_status_t status;

    BSP_VS1003_AbortStream();
    status = BSP_VS1003_WriteRegister(BSP_VS1003_REG_MODE,
                                      BSP_VS1003_SM_SDINEW | BSP_VS1003_SM_RESET);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    HAL_Delay(VS1003_RESET_RECOVERY_MS);
    status = VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    status = VS1003_WriteVerified(BSP_VS1003_REG_MODE, BSP_VS1003_SM_SDINEW);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    status = VS1003_WriteVerified(BSP_VS1003_REG_CLOCKF, VS1003_CLOCKF_DEFAULT);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    return VS1003_SetPrescaler(VS1003_SPI_PRESCALER_SDI);
}

static bsp_vs1003_status_t VS1003_ReadExpected(uint8_t address, uint16_t expected, uint16_t mask)
{
    bsp_vs1003_status_t status = BSP_VS1003_ERR_VERIFY;
    uint16_t            value  = 0U;

    for (uint8_t retry = 0U; retry < VS1003_VERIFY_RETRY_COUNT; ++retry)
    {
        status = BSP_VS1003_ReadRegister(address, &value);
        if ((status == BSP_VS1003_OK) && ((value & mask) == (expected & mask)))
        {
            VS1003_ClearVerifyDiag();
            return BSP_VS1003_OK;
        }
        if (status == BSP_VS1003_OK)
        {
            VS1003_RecordVerifyDiag(address, expected, value, mask);
        }
        HAL_Delay(VS1003_VERIFY_RETRY_DELAY_MS);
    }

    return (status == BSP_VS1003_OK) ? BSP_VS1003_ERR_VERIFY : status;
}

static bsp_vs1003_status_t VS1003_WriteVerified(uint8_t address, uint16_t value)
{
    bsp_vs1003_status_t status   = BSP_VS1003_ERR_VERIFY;
    uint16_t            readback = 0U;

    for (uint8_t retry = 0U; retry < VS1003_VERIFY_RETRY_COUNT; ++retry)
    {
        status = BSP_VS1003_WriteRegister(address, value);
        if (status == BSP_VS1003_OK)
        {
            status = BSP_VS1003_ReadRegister(address, &readback);
        }
        if ((status == BSP_VS1003_OK) && (readback == value))
        {
            VS1003_ClearVerifyDiag();
            return BSP_VS1003_OK;
        }
        if (status == BSP_VS1003_OK)
        {
            VS1003_RecordVerifyDiag(address, value, readback, 0xFFFFU);
        }
        HAL_Delay(VS1003_VERIFY_RETRY_DELAY_MS);
    }

    return (status == BSP_VS1003_OK) ? BSP_VS1003_ERR_VERIFY : status;
}

static bsp_vs1003_status_t VS1003_ConfigureAfterReset(void)
{
    bsp_vs1003_status_t status = VS1003_WaitReady(VS1003_DREQ_TIMEOUT_MS);

    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_MODE, BSP_VS1003_SM_SDINEW);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_CLOCKF, VS1003_CLOCKF_DEFAULT);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_VOL, VS1003_VOLUME_DEFAULT);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_ReadExpected(BSP_VS1003_REG_STATUS, VS1003_STATUS_VERSION_VALUE,
                                     VS1003_STATUS_VERSION_MASK);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_ReadExpected(BSP_VS1003_REG_CLOCKF, VS1003_CLOCKF_DEFAULT, 0xFFFFU);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_ReadExpected(BSP_VS1003_REG_MODE, BSP_VS1003_SM_SDINEW, 0xFFFFU);
    }

    vs1003_state = (status == BSP_VS1003_OK) ? BSP_VS1003_STATE_READY : BSP_VS1003_STATE_ERROR;
    return status;
}

bsp_vs1003_status_t BSP_VS1003_InitAfterHardwareReset(void)
{
    VS1003_ClearVerifyDiag();
    return VS1003_ConfigureAfterReset();
}

bsp_vs1003_status_t BSP_VS1003_Init(void)
{
    bsp_vs1003_status_t status;

    VS1003_ClearVerifyDiag();
    status = BSP_VS1003_HardwareReset();
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_ConfigureAfterReset();
    }
    return status;
}

void BSP_VS1003_DeInit(void)
{
    BSP_VS1003_AbortStream();
    VS1003_DeselectAll();
    VS1003_XRESET_Low();
    vs1003_state = BSP_VS1003_STATE_UNINIT;
}

static bsp_vs1003_status_t VS1003_SendDataInternal(const uint8_t *data, uint16_t length,
                                                   uint32_t timeout_ms)
{
    bsp_vs1003_status_t status;

    if ((data == NULL) || (length == 0U))
    {
        return BSP_VS1003_ERR_PARAM;
    }

    status = VS1003_SetPrescaler(VS1003_SPI_PRESCALER_SDI);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    VS1003_XCS_High();

    while (length > 0U)
    {
        uint16_t chunk = (length > BSP_VS1003_SDI_CHUNK_SIZE) ? BSP_VS1003_SDI_CHUNK_SIZE : length;

        status = VS1003_WaitReady(timeout_ms);
        if (status != BSP_VS1003_OK)
        {
            VS1003_XDCS_High();
            return status;
        }

        VS1003_XDCS_Low();
        status = VS1003_FromHALStatus(BSP_SPI_Tx(&vs1003_spi, data, chunk, VS1003_SPI_TIMEOUT_MS));
        VS1003_XDCS_High();

        if (status != BSP_VS1003_OK)
        {
            return status;
        }

        data += chunk;
        length -= chunk;
    }

    return BSP_VS1003_OK;
}

bsp_vs1003_status_t BSP_VS1003_SendData(const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    if (vs1003_state != BSP_VS1003_STATE_READY)
    {
        return BSP_VS1003_ERR_NOT_READY;
    }
    if (!BSP_VS1003_IsStreamIdle())
    {
        return BSP_VS1003_ERR_BUSY;
    }

    return VS1003_SendDataInternal(data, length, timeout_ms);
}

static void VS1003_TryFeed(void)
{
    vs1003_stream_buffer_t *active;
    HAL_StatusTypeDef       hal_status;
    uint32_t                primask;
    uint16_t                remaining;
    uint16_t                chunk;
    uint8_t                 scan;
    uint8_t                 index;

    primask = VS1003_EnterCritical();

    if ((vs1003_state != BSP_VS1003_STATE_READY) || (vs1003_stream.paused != 0U) ||
        (vs1003_stream.tx_busy != 0U) || !BSP_VS1003_IsReady())
    {
        VS1003_ExitCritical(primask);
        return;
    }

    if (vs1003_stream.active_index < 0)
    {
        for (scan = 0U; scan < VS1003_STREAM_BUFFER_COUNT; scan++)
        {
            index = (uint8_t)((vs1003_stream.read_index + scan) % VS1003_STREAM_BUFFER_COUNT);
            if (vs1003_stream.buffers[index].state == VS1003_BUFFER_READY)
            {
                vs1003_stream.active_index = (int8_t)index;
                vs1003_stream.read_index   = (uint8_t)((index + 1U) % VS1003_STREAM_BUFFER_COUNT);
                vs1003_stream.buffers[index].state = VS1003_BUFFER_ACTIVE;
                break;
            }
        }
    }

    if (vs1003_stream.active_index < 0)
    {
        VS1003_ExitCritical(primask);
        return;
    }

    active    = &vs1003_stream.buffers[vs1003_stream.active_index];
    remaining = active->size - active->offset;
    chunk     = (remaining > BSP_VS1003_SDI_CHUNK_SIZE) ? BSP_VS1003_SDI_CHUNK_SIZE : remaining;

    vs1003_stream.last_chunk_size = chunk;
    vs1003_stream.tx_busy         = 1U;
    VS1003_XCS_High();
    VS1003_XDCS_Low();
    hal_status = BSP_SPI_Tx_DMA(&vs1003_spi, &active->data[active->offset], chunk);
    if (hal_status != HAL_OK)
    {
        VS1003_XDCS_High();
        vs1003_stream.tx_busy = 0U;
        vs1003_stream.paused  = 1U;
        vs1003_state          = BSP_VS1003_STATE_ERROR;
    }

    VS1003_ExitCritical(primask);
}

uint8_t BSP_VS1003_GetWriteBuffer(uint8_t **buffer, uint16_t *capacity)
{
    uint32_t primask;
    uint8_t  scan;
    uint8_t  index;

    if ((buffer == NULL) || (capacity == NULL) || (vs1003_state != BSP_VS1003_STATE_READY))
    {
        return 0U;
    }

    primask = VS1003_EnterCritical();
    if (vs1003_stream.producer_index >= 0)
    {
        VS1003_ExitCritical(primask);
        return 0U;
    }

    for (scan = 0U; scan < VS1003_STREAM_BUFFER_COUNT; scan++)
    {
        index = (uint8_t)((vs1003_stream.write_index + scan) % VS1003_STREAM_BUFFER_COUNT);
        if (vs1003_stream.buffers[index].state == VS1003_BUFFER_EMPTY)
        {
            vs1003_stream.buffers[index].state  = VS1003_BUFFER_WRITING;
            vs1003_stream.buffers[index].size   = 0U;
            vs1003_stream.buffers[index].offset = 0U;
            vs1003_stream.producer_index        = (int8_t)index;
            vs1003_stream.write_index = (uint8_t)((index + 1U) % VS1003_STREAM_BUFFER_COUNT);
            *buffer                   = vs1003_stream.buffers[index].data;
            *capacity                 = BSP_VS1003_STREAM_BUFFER_SIZE;
            VS1003_ExitCritical(primask);
            return 1U;
        }
    }

    VS1003_ExitCritical(primask);
    return 0U;
}

bsp_vs1003_status_t BSP_VS1003_CommitBuffer(uint16_t size)
{
    vs1003_stream_buffer_t *buffer;
    uint32_t                primask;
    int8_t                  index;

    if ((size == 0U) || (size > BSP_VS1003_STREAM_BUFFER_SIZE))
    {
        return BSP_VS1003_ERR_PARAM;
    }
    if (vs1003_state != BSP_VS1003_STATE_READY)
    {
        return BSP_VS1003_ERR_NOT_READY;
    }

    primask = VS1003_EnterCritical();
    index   = vs1003_stream.producer_index;
    if ((index < 0) || (vs1003_stream.buffers[index].state != VS1003_BUFFER_WRITING))
    {
        VS1003_ExitCritical(primask);
        return BSP_VS1003_ERR_BUSY;
    }

    buffer                       = &vs1003_stream.buffers[index];
    buffer->size                 = size;
    buffer->offset               = 0U;
    buffer->state                = VS1003_BUFFER_READY;
    vs1003_stream.producer_index = -1;
    VS1003_ExitCritical(primask);

    VS1003_TryFeed();
    return (vs1003_state == BSP_VS1003_STATE_READY) ? BSP_VS1003_OK : BSP_VS1003_ERR_SPI;
}

void BSP_VS1003_CancelWriteBuffer(void)
{
    uint32_t primask = VS1003_EnterCritical();
    int8_t   index   = vs1003_stream.producer_index;

    if ((index >= 0) && (vs1003_stream.buffers[index].state == VS1003_BUFFER_WRITING))
    {
        vs1003_stream.buffers[index].size   = 0U;
        vs1003_stream.buffers[index].offset = 0U;
        vs1003_stream.buffers[index].state  = VS1003_BUFFER_EMPTY;
    }
    vs1003_stream.producer_index = -1;
    VS1003_ExitCritical(primask);
}

void BSP_VS1003_AbortStream(void)
{
    uint32_t primask = VS1003_EnterCritical();

    vs1003_stream.paused = 1U;
    if (vs1003_stream.tx_busy != 0U)
    {
        (void)BSP_SPI_DMAStop(&vs1003_spi);
    }
    VS1003_XDCS_High();
    VS1003_ResetStreamContext();
    VS1003_ExitCritical(primask);
}

bsp_vs1003_status_t BSP_VS1003_PauseStream(uint32_t timeout_ms)
{
    uint32_t primask;
    uint32_t start;

    primask              = VS1003_EnterCritical();
    vs1003_stream.paused = 1U;
    VS1003_ExitCritical(primask);

    start = HAL_GetTick();
    while (vs1003_stream.tx_busy != 0U)
    {
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return BSP_VS1003_ERR_TIMEOUT;
        }
    }
    return BSP_VS1003_OK;
}

void BSP_VS1003_ResumeStream(void)
{
    uint32_t primask;

    if ((vs1003_state == BSP_VS1003_STATE_READY) &&
        (VS1003_SetPrescaler(VS1003_SPI_PRESCALER_SDI) != BSP_VS1003_OK))
    {
        vs1003_state = BSP_VS1003_STATE_ERROR;
        return;
    }

    primask              = VS1003_EnterCritical();
    vs1003_stream.paused = 0U;
    VS1003_ExitCritical(primask);
    VS1003_TryFeed();
}

uint8_t BSP_VS1003_IsStreamIdle(void)
{
    uint32_t primask;
    uint8_t  index;
    uint8_t  idle = 1U;

    primask = VS1003_EnterCritical();
    if ((vs1003_stream.tx_busy != 0U) || (vs1003_stream.producer_index >= 0) ||
        (vs1003_stream.active_index >= 0))
    {
        idle = 0U;
    }
    for (index = 0U; (index < VS1003_STREAM_BUFFER_COUNT) && (idle != 0U); index++)
    {
        if (vs1003_stream.buffers[index].state != VS1003_BUFFER_EMPTY)
        {
            idle = 0U;
        }
    }
    VS1003_ExitCritical(primask);
    return idle;
}

uint32_t BSP_VS1003_GetStreamTransferredBytes(void)
{
    uint32_t primask     = VS1003_EnterCritical();
    uint32_t transferred = vs1003_stream.transferred_bytes;

    VS1003_ExitCritical(primask);
    return transferred;
}

bsp_vs1003_status_t BSP_VS1003_Flush(uint32_t timeout_ms)
{
    static const uint8_t zeros[BSP_VS1003_SDI_CHUNK_SIZE] = {0};
    uint16_t             remaining                        = VS1003_FLUSH_BYTES;
    bsp_vs1003_status_t  status;

    while (remaining > 0U)
    {
        status = BSP_VS1003_SendData(zeros, sizeof(zeros), timeout_ms);
        if (status != BSP_VS1003_OK)
        {
            return status;
        }
        remaining -= sizeof(zeros);
    }

    return BSP_VS1003_OK;
}

bsp_vs1003_status_t BSP_VS1003_SetVolume(uint8_t left, uint8_t right)
{
    return BSP_VS1003_WriteRegister(BSP_VS1003_REG_VOL, ((uint16_t)left << 8) | right);
}

bsp_vs1003_status_t BSP_VS1003_SetBass(uint8_t bass, uint8_t bass_freq, int8_t treble,
                                       uint8_t treble_freq)
{
    uint16_t value;

    if ((bass > 15U) || (bass_freq > 15U) || ((bass != 0U) && (bass_freq < 2U)) || (treble < -8) ||
        (treble > 7) || (treble_freq > 15U))
    {
        return BSP_VS1003_ERR_PARAM;
    }

    value = ((uint16_t)((uint8_t)treble & 0x0FU) << 12) | ((uint16_t)treble_freq << 8) |
            ((uint16_t)bass << 4) | bass_freq;
    return BSP_VS1003_WriteRegister(BSP_VS1003_REG_BASS, value);
}

bsp_vs1003_status_t BSP_VS1003_SetSurround(uint8_t enable)
{
    bsp_vs1003_status_t status;
    uint16_t            mode;
    uint16_t            verify;

    status = BSP_VS1003_ReadRegister(BSP_VS1003_REG_MODE, &mode);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    if (enable != 0U)
    {
        mode |= BSP_VS1003_SM_DIFF;
    }
    else
    {
        mode &= (uint16_t)~BSP_VS1003_SM_DIFF;
    }

    status = BSP_VS1003_WriteRegister(BSP_VS1003_REG_MODE, mode);
    if (status != BSP_VS1003_OK)
    {
        return status;
    }

    status = BSP_VS1003_ReadRegister(BSP_VS1003_REG_MODE, &verify);
    if ((status == BSP_VS1003_OK) && (verify != mode))
    {
        return BSP_VS1003_ERR_VERIFY;
    }
    return status;
}

// todo
bsp_vs1003_status_t BSP_VS1003_SineTest(uint8_t tone, uint32_t duration_ms)
{
    static const uint8_t start_command[8] = {0x53U, 0xEFU, 0x6EU, 0x00U,
                                             0x00U, 0x00U, 0x00U, 0x00U};
    static const uint8_t stop_command[8] = {0x45U, 0x78U, 0x69U, 0x74U, 0x00U, 0x00U, 0x00U, 0x00U};
    uint8_t              command[sizeof(start_command)];
    bsp_vs1003_status_t  status;
    bsp_vs1003_status_t  restore_status;
    bsp_vs1003_verify_diag_t operation_diag;
    uint8_t                  operation_diag_valid = 0U;

    if (vs1003_state != BSP_VS1003_STATE_READY)
    {
        return BSP_VS1003_ERR_NOT_READY;
    }

    for (uint8_t i = 0U; i < sizeof(command); ++i)
    {
        command[i] = start_command[i];
    }
    command[3] = tone;

    status = BSP_VS1003_HardwareReset();
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_MODE,
                                      BSP_VS1003_SM_SDINEW | BSP_VS1003_SM_TESTS);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_CLOCKF, VS1003_CLOCKF_DEFAULT);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_WriteVerified(BSP_VS1003_REG_VOL, VS1003_VOLUME_DEFAULT);
    }
    if (status == BSP_VS1003_OK)
    {
        status = VS1003_SendDataInternal(command, sizeof(command), VS1003_DREQ_TIMEOUT_MS);
    }
    if (status == BSP_VS1003_OK)
    {
        HAL_Delay(duration_ms);
        status = VS1003_SendDataInternal(stop_command, sizeof(stop_command), VS1003_DREQ_TIMEOUT_MS);
    }

    if ((status == BSP_VS1003_ERR_VERIFY) && (BSP_VS1003_GetLastVerifyDiag(&operation_diag) != 0U))
    {
        operation_diag_valid = 1U;
    }

    restore_status = BSP_VS1003_Init();
    if (status == BSP_VS1003_OK)
    {
        status = restore_status;
    }
    else if (operation_diag_valid != 0U)
    {
        vs1003_verify_diag = operation_diag;
    }
    return status;
}

uint8_t BSP_VS1003_IsReady(void)
{
    return HAL_GPIO_ReadPin(VS1003_DREQ_GPIO_Port, VS1003_DREQ_Pin) == GPIO_PIN_SET;
}

bsp_vs1003_state_t BSP_VS1003_GetState(void)
{
    return vs1003_state;
}

uint8_t BSP_VS1003_GetLastVerifyDiag(bsp_vs1003_verify_diag_t *diag)
{
    if ((diag == NULL) || (vs1003_verify_diag.valid == 0U))
    {
        return 0U;
    }

    *diag = vs1003_verify_diag;
    return 1U;
}
void BSP_VS1003_SPI_TxCpltCallback(void)
{
    vs1003_stream_buffer_t *active;
    uint32_t                primask = VS1003_EnterCritical();

    if ((vs1003_stream.tx_busy == 0U) || (vs1003_stream.active_index < 0))
    {
        VS1003_XDCS_High();
        VS1003_ExitCritical(primask);
        return;
    }

    VS1003_XDCS_High();
    active = &vs1003_stream.buffers[vs1003_stream.active_index];
    active->offset += vs1003_stream.last_chunk_size;
    vs1003_stream.transferred_bytes += vs1003_stream.last_chunk_size;
    vs1003_stream.last_chunk_size = 0U;
    vs1003_stream.tx_busy         = 0U;

    if (active->offset >= active->size)
    {
        active->size               = 0U;
        active->offset             = 0U;
        active->state              = VS1003_BUFFER_EMPTY;
        vs1003_stream.active_index = -1;
    }
    VS1003_ExitCritical(primask);

    VS1003_TryFeed();
}

void BSP_VS1003_SPI_ErrorCallback(void)
{
    uint32_t primask = VS1003_EnterCritical();

    VS1003_XDCS_High();
    vs1003_stream.tx_busy = 0U;
    vs1003_stream.paused  = 1U;
    vs1003_state          = BSP_VS1003_STATE_ERROR;
    VS1003_ExitCritical(primask);
}

void BSP_VS1003_DREQCallback(void)
{
    VS1003_TryFeed();
}
