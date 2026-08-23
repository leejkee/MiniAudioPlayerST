/**
  ******************************************************************************
  * @file    bsp_vs1003.h
  * @brief   VS1003 音频解码器 BSP 驱动
  * @note    SCI 使用阻塞式 SPI，SDI 音频流使用 DREQ + DMA 双缓冲。
  ******************************************************************************
  */

#ifndef __BSP_VS1003_H
#define __BSP_VS1003_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

/* SCI 操作码 ----------------------------------------------------------------*/
#define BSP_VS1003_SCI_WRITE 0x02U
#define BSP_VS1003_SCI_READ  0x03U

/* SCI 寄存器 ----------------------------------------------------------------*/
#define BSP_VS1003_REG_MODE        0x00U
#define BSP_VS1003_REG_STATUS      0x01U
#define BSP_VS1003_REG_BASS        0x02U
#define BSP_VS1003_REG_CLOCKF      0x03U
#define BSP_VS1003_REG_DECODE_TIME 0x04U
#define BSP_VS1003_REG_AUDATA      0x05U
#define BSP_VS1003_REG_WRAM        0x06U
#define BSP_VS1003_REG_WRAMADDR    0x07U
#define BSP_VS1003_REG_HDAT0       0x08U
#define BSP_VS1003_REG_HDAT1       0x09U
#define BSP_VS1003_REG_AIADDR      0x0AU
#define BSP_VS1003_REG_VOL         0x0BU
#define BSP_VS1003_REG_AICTRL0     0x0CU
#define BSP_VS1003_REG_AICTRL1     0x0DU
#define BSP_VS1003_REG_AICTRL2     0x0EU
#define BSP_VS1003_REG_AICTRL3     0x0FU

/* SCI_MODE 位 ---------------------------------------------------------------*/
#define BSP_VS1003_SM_DIFF   0x0001U
#define BSP_VS1003_SM_RESET  0x0004U
#define BSP_VS1003_SM_TESTS  0x0020U
#define BSP_VS1003_SM_STREAM 0x0040U
#define BSP_VS1003_SM_SDINEW 0x0800U

/* SDI 每次 DREQ 有效后保证可发送的最大字节数 -------------------------------*/
#define BSP_VS1003_SDI_CHUNK_SIZE     32U
#define BSP_VS1003_STREAM_BUFFER_SIZE 512U

    typedef enum
    {
        BSP_VS1003_OK = 0,
        BSP_VS1003_ERR_PARAM,
        BSP_VS1003_ERR_SPI,
        BSP_VS1003_ERR_TIMEOUT,
        BSP_VS1003_ERR_VERIFY,
        BSP_VS1003_ERR_NOT_READY,
        BSP_VS1003_ERR_BUSY
    } bsp_vs1003_status_t;

    typedef enum
    {
        BSP_VS1003_STATE_UNINIT = 0,
        BSP_VS1003_STATE_READY,
        BSP_VS1003_STATE_ERROR
    } bsp_vs1003_state_t;

    typedef struct
    {
        uint8_t  valid;
        uint8_t  address;
        uint16_t expected;
        uint16_t actual;
        uint16_t mask;
    } bsp_vs1003_verify_diag_t;

    /**
  * @brief  初始化 VS1003
  * @note   执行硬件复位、软件复位、时钟配置和默认音量设置。
  */
    bsp_vs1003_status_t BSP_VS1003_Init(void);

    /**
  * @brief  在已完成硬件复位后配置并校验 SCI 寄存器
  * @note   调用前必须保证 XRESET 已释放且 DREQ 为高。
  */
    bsp_vs1003_status_t BSP_VS1003_InitAfterHardwareReset(void);

    /**
  * @brief  关闭驱动并将 VS1003 保持在硬件复位状态
  */
    void                BSP_VS1003_DeInit(void);

    /**
  * @brief  执行硬件复位并等待 DREQ 变高
  */
    bsp_vs1003_status_t BSP_VS1003_HardwareReset(void);

    /**
  * @brief  执行软件复位并配置 VS1003 工作时钟
  */
    bsp_vs1003_status_t BSP_VS1003_SoftReset(void);

    /**
  * @brief  写 SCI 寄存器
  */
    bsp_vs1003_status_t BSP_VS1003_WriteRegister(uint8_t address, uint16_t value);

    /**
  * @brief  读 SCI 寄存器
  */
    bsp_vs1003_status_t BSP_VS1003_ReadRegister(uint8_t address, uint16_t *value);

    /**
  * @brief  阻塞发送音频数据
  * @param  timeout_ms 每次等待 DREQ 的最大时间
  * @note   数据按不超过 32 字节分块发送，每块发送前等待 DREQ；
  *         仅用于测试或兼容路径，异步流非空时返回 BUSY。
  */
    bsp_vs1003_status_t BSP_VS1003_SendData(const uint8_t *data, uint16_t length,
                                            uint32_t timeout_ms);

    /**
  * @brief  获取一个空闲的流写缓冲区
  * @note   成功后调用方独占该缓冲区，必须 Commit 或 Cancel。
  * @retval 1=成功，0=当前两个缓冲区都不可写
  */
    uint8_t             BSP_VS1003_GetWriteBuffer(uint8_t **buffer, uint16_t *capacity);

    /**
  * @brief  提交最近一次获取的写缓冲区
  * @note   BSP 随后按 DREQ 流控将数据切成不超过 32 字节的 DMA 事务。
  */
    bsp_vs1003_status_t BSP_VS1003_CommitBuffer(uint16_t size);

    /**
  * @brief  放弃最近一次获取但尚未提交的写缓冲区
  */
    void                BSP_VS1003_CancelWriteBuffer(void);

    /**
  * @brief  中止当前流并释放双缓冲区
  */
    void                BSP_VS1003_AbortStream(void);

    /**
  * @brief  暂停启动新的 DMA，并等待当前 DMA 事务完成
  */
    bsp_vs1003_status_t BSP_VS1003_PauseStream(uint32_t timeout_ms);

    /**
  * @brief  恢复异步流发送
  */
    void                BSP_VS1003_ResumeStream(void);

    /**
  * @brief  查询所有已提交数据是否均已完成发送
  */
    uint8_t             BSP_VS1003_IsStreamIdle(void);

    /**
  * @brief  获取当前流已经由 DMA 完成发送的字节数
  */
    uint32_t            BSP_VS1003_GetStreamTransferredBytes(void);

    /**
  * @brief  发送零字节以清理简单播放流水线
  * @note   这是初版通用清理方法，后续可按音频格式实现取消播放流程。
  */
    bsp_vs1003_status_t BSP_VS1003_Flush(uint32_t timeout_ms);

    /**
  * @brief  设置左右声道衰减值
  * @note   0 表示最大音量，0xFE 接近静音。
  */
    bsp_vs1003_status_t BSP_VS1003_SetVolume(uint8_t left, uint8_t right);

    /**
  * @brief  设置低音和高音增强
  * @param  bass     低音幅度，0~15
  * @param  bass_freq 低音下限，2~15，单位 10 Hz；bass=0 时可为 0
  * @param  treble   高音幅度，-8~7，单位 1.5 dB
  * @param  treble_freq 高音下限，0~15，单位 1 kHz
  */
    bsp_vs1003_status_t BSP_VS1003_SetBass(uint8_t bass, uint8_t bass_freq, int8_t treble,
                                           uint8_t treble_freq);

    /**
  * @brief  设置差分输出模式，开启后，VS1003反转左声道输出的相位，对立体声音频：左右相位差变大，产生虚拟环绕和空间扩展效果，对于单声音频：作呕声道相反香味，形成差分输出信号
  */
    bsp_vs1003_status_t BSP_VS1003_SetSurround(uint8_t enable);

    /**
  * @brief  执行 VS1003 正弦测试
  * @param  tone 频率参数，常用值 0x24
  * @param  duration_ms 测试持续时间
  */
    bsp_vs1003_status_t BSP_VS1003_SineTest(uint8_t tone, uint32_t duration_ms);

    /**
  * @brief  查询 DREQ 当前电平
  */
    uint8_t             BSP_VS1003_IsReady(void);

    /**
  * @brief  获取驱动状态
  */
    bsp_vs1003_state_t  BSP_VS1003_GetState(void);

    /**
  * @brief  获取最近一次寄存器校验失败的详情
  * @retval 1=存在有效详情，0=最近一次校验未发生读回不匹配
  */
    uint8_t             BSP_VS1003_GetLastVerifyDiag(bsp_vs1003_verify_diag_t *diag);

    /* HAL 中断回调分发入口，仅供 BSP 内部事件分发文件调用。 */
    void                BSP_VS1003_SPI_TxCpltCallback(void);
    void                BSP_VS1003_SPI_ErrorCallback(void);
    void                BSP_VS1003_DREQCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_VS1003_H */
