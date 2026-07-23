/**
  ******************************************************************************
  * @file    bsp_SD.c
  * @brief   BSP SD 卡驱动 — 基于 SPI 模式的 SD 协议实现
  * @note    硬件引脚 (STM32F072 + CubeMX 配置):
  *            SPI1: PA5(SCK), PA6(MISO), PA7(MOSI)
  *            CS:   PA4 (GPIO 软件控制)
  *
  *          依赖 bsp_spi 层, 不直接调用 HAL。CS 由本模块管理。
  *          CS 逻辑: Low=选中 / High=释放 (标准逻辑, 非反相)。
  *
  *          参考: docs/sd_example.md (SD 协议逻辑)
  ******************************************************************************
  */

#include "bsp_SD.h"
#include "bsp_spi.h"

/* ========================================================================= */
/*                           SD 协议常量                                      */
/* ========================================================================= */

/* 数据令牌 */
#define TOKEN_START_BLOCK    0xFE  /* 单块读写起始令牌 */
#define TOKEN_START_MULTI    0xFC  /* 多块写起始令牌   */
#define TOKEN_STOP_TRANS     0xFD  /* 停止传输令牌     */

/* SD 命令 */
#define CMD0     0    /* GO_IDLE_STATE: 复位到空闲态            */
#define CMD1     1    /* SEND_OP_COND: MMC 初始化               */
#define CMD8     8    /* SEND_IF_COND: 接口条件检测             */
#define CMD9     9    /* SEND_CSD: 读取 CSD 寄存器              */
#define CMD10   10    /* SEND_CID: 读取 CID 寄存器              */
#define CMD12   12    /* STOP_TRANSMISSION: 停止多块传输         */
#define CMD16   16    /* SET_BLOCKLEN: 设置块大小               */
#define CMD17   17    /* READ_SINGLE_BLOCK: 读单块              */
#define CMD18   18    /* READ_MULTIPLE_BLOCK: 读多块            */
#define CMD23   23    /* SET_BLOCK_COUNT: 预置写前擦除块数       */
#define CMD24   24    /* WRITE_BLOCK: 写单块                    */
#define CMD25   25    /* WRITE_MULTIPLE_BLOCK: 写多块           */
#define CMD41   41    /* SD_SEND_OP_COND: ACMD41 (需 CMD55 前缀) */
#define CMD55   55    /* APP_CMD: 下一条为 ACMD                 */
#define CMD58   58    /* READ_OCR: 读取操作条件寄存器            */
#define CMD59   59    /* CRC_ON_OFF: 开关 CRC 校验              */

/* R1 响应位掩码 */
#define R1_IDLE              0x01  /* 空闲态        */
#define R1_ERASE_RESET       0x02  /* 擦除复位      */
#define R1_ILLEGAL_CMD       0x04  /* 非法命令      */
#define R1_CRC_ERR           0x08  /* CRC 错误      */
#define R1_ERASE_SEQ_ERR     0x10  /* 擦除序列错误  */
#define R1_ADDR_ERR          0x20  /* 地址错误      */
#define R1_PARAM_ERR         0x40  /* 参数错误      */
#define R1_READY             0x00  /* 就绪 (非空闲) */

/* 数据响应标记 */
#define DATA_RESP_ACCEPTED   0x05  /* 数据被接受     */
#define DATA_RESP_CRC_ERR    0x0B  /* CRC 错误       */
#define DATA_RESP_WRITE_ERR  0x0D  /* 写入错误       */

/* ACMD41 参数 */
#define ACMD41_HCS           0x40000000U  /* HCS 位 (Host Capacity Support) */

/* OCR 位 (CMD58 返回) */
#define OCR_CCS_BIT          0x40  /* CCS 位于 OCR[31:24] 的 bit30 → 第 1 字节 bit6 */

/* 超时 (ms) */
#define SD_INIT_TIMEOUT      1000   /* ACMD41 循环超时 */
#define SD_CMD_TIMEOUT       200    /* 命令响应超时    */
#define SD_DATA_TIMEOUT      200    /* 数据令牌超时    */
#define SD_WRITE_TIMEOUT     500    /* 写操作忙等待    */

/* 哑字节 (SPI 总线填充) */
#define SD_DUMMY             0xFF

/* ========================================================================= */
/*                            内部驱动状态                                     */
/* ========================================================================= */

static struct {
    bsp_sd_state_t      state;         /* 驱动状态              */
    bsp_sd_card_type_t  card_type;     /* 卡类型                */
    uint32_t            sector_count;  /* 总扇区数              */
    uint16_t            sector_size;   /* 扇区大小 (固定 512)   */
} sd;

/*
 * 释放 CS + 发 1 个哑字节 (8 SCK)。
 * Dev.md 规范: CS 拉高后必须产生额外时钟让 SD 卡完成总线释放。
 */
static inline void CS_Release(void)
{
    BSP_SPI_CS_High();
    BSP_SPI_RW(SD_DUMMY);
}

/*
 * 等待卡就绪: MISO 读到 0xFF 表示卡不忙。
 * 返回 0=就绪, 1=超时
 */
static uint8_t SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = BSP_GetTick();
    do {
        if (BSP_SPI_RW(SD_DUMMY) == 0xFF) {
            return 0;
        }
    } while ((BSP_GetTick() - start) < timeout_ms);
    return 1;
}

/* 速率切换 */
static void SD_SetSpeed(uint8_t low_speed)
{
    BSP_SPI_SetSpeed(
        low_speed ? BSP_SPI_SPEED_LOW : BSP_SPI_SPEED_HIGH);
}

/* ========================================================================= */
/*                      SD 命令发送                                           */
/* ========================================================================= */

/*
 * 发送 6 字节 SD 命令帧, 等待 R1 响应。
 *
 * 帧格式: {cmd|0x40, arg[31:24], arg[23:16], arg[15:8], arg[7:0], crc}
 *
 * CS 保持低电平, 调用方决定是否释放。
 * CMD12 后额外发一个哑字节 (SD 规范要求)。
 *
 * 返回 R1 字节。
 */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t  r1;
    uint32_t start;

    BSP_SPI_CS_Low();

    /* 等待卡退出忙态 */
    SD_WaitReady(SD_CMD_TIMEOUT);

    /* 6 字节命令帧 */
    BSP_SPI_RW(cmd | 0x40);
    BSP_SPI_RW((uint8_t)(arg >> 24));
    BSP_SPI_RW((uint8_t)(arg >> 16));
    BSP_SPI_RW((uint8_t)(arg >> 8));
    BSP_SPI_RW((uint8_t)(arg));
    BSP_SPI_RW(crc);

    /* CMD12: 额外哑字节 */
    if (cmd == CMD12) {
        BSP_SPI_RW(SD_DUMMY);
    }

    /* 等待 R1 (MSB=0 表示有效) */
    start = BSP_GetTick();
    do {
        r1 = BSP_SPI_RW(SD_DUMMY);
        if (!(r1 & 0x80)) {
            return r1;
        }
    } while ((BSP_GetTick() - start) < SD_CMD_TIMEOUT);

    return r1;  /* 超时返回 0xFF */
}

/* ========================================================================= */
/*                      数据接收                                              */
/* ========================================================================= */

/*
 * 从 SD 卡接收一个数据块。
 *
 * CS 管理: 入口拉低 CS / 出口释放 CS (参考 sd_example.md 模式)。
 * 流程: CS Low → 等 0xFE 令牌 → 读 len 字节 → 丢弃 2B CRC → CS High + dummy
 *
 * 返回 0=成功, 1=超时
 */
static uint8_t SD_ReceiveData(uint8_t *data, uint16_t len)
{
    uint32_t start;

    /* CS 选中 (对于多块读的首块,
     * SD_SendCmd 已经保活 CS, 再次拉低无副作用) */
    BSP_SPI_CS_Low();

    /* 等待起始令牌 0xFE */
    start = BSP_GetTick();
    while ((BSP_GetTick() - start) < SD_DATA_TIMEOUT) {
        if (BSP_SPI_RW(SD_DUMMY) == TOKEN_START_BLOCK) {
            goto token_found;
        }
    }
    /* 超时 */
    CS_Release();
    return 1;

token_found:
    /* 接收数据 */
    BSP_SPI_Rx(data, len);

    /* 丢弃 2 字节 CRC */
    BSP_SPI_RW(SD_DUMMY);
    BSP_SPI_RW(SD_DUMMY);

    /* 释放 CS */
    CS_Release();
    return 0;
}

/* ========================================================================= */
/*                      数据发送                                              */
/* ========================================================================= */

/*
 * 向 SD 卡发送一个数据块 (512 字节)。
 *
 * 前提: CS 已被调用方拉低 (SD_SendCmd 保活)。
 * 此函数不管理 CS, 调用方负责最终释放。
 *
 * token: TOKEN_START_BLOCK (单块) / TOKEN_START_MULTI (多块) / TOKEN_STOP_TRANS
 *
 * 返回 0=成功, 1=失败
 */
static uint8_t SD_SendBlock(const uint8_t *buf, uint8_t token)
{
    uint8_t resp;

    /* 发送写令牌 */
    BSP_SPI_RW(token);

    /* 停止令牌: 不发数据 */
    if (token == TOKEN_STOP_TRANS) {
        SD_WaitReady(SD_WRITE_TIMEOUT);
        return 0;
    }

    /* 发送 512 字节数据 */
    BSP_SPI_Tx(buf, 512);

    /* 2 字节假 CRC (SPI 模式通常忽略 CRC) */
    BSP_SPI_RW(SD_DUMMY);
    BSP_SPI_RW(SD_DUMMY);

    /* 读取数据响应 */
    resp = BSP_SPI_RW(SD_DUMMY);
    if ((resp & 0x1F) != DATA_RESP_ACCEPTED) {
        return 1;
    }

    /* 等待卡写入完成 (退出忙态) */
    SD_WaitReady(SD_WRITE_TIMEOUT);
    return 0;
}

/* ========================================================================= */
/*                      CSD 寄存器读取                                        */
/* ========================================================================= */

static uint8_t SD_ReadCSD(uint8_t *csd)
{
    if (SD_SendCmd(CMD9, 0, 0x01) != R1_READY) {
        CS_Release();
        return 1;
    }
    /* CS 由 SD_SendCmd 保活, SD_ReceiveData 接管并释放 */
    return SD_ReceiveData(csd, 16);
}

static uint8_t SD_ReadCID(uint8_t *cid)
{
    if (SD_SendCmd(CMD10, 0, 0x01) != R1_READY) {
        CS_Release();
        return 1;
    }
    return SD_ReceiveData(cid, 16);
}

/* ========================================================================= */
/*                      CSD 解析 & 扇区数计算                                  */
/* ========================================================================= */

/*
 * 读取 CSD 并计算总扇区数。
 *
 * CSD 版本: csd[0] bit[7:6]
 *   0x00 → V1.0 (SDSC)
 *   0x40 → V2.0 (SDHC/SDXC)
 *
 * V2: C_SIZE = csd[7][5:0]<<16 | csd[8]<<8 | csd[9]
 *     sector_count = (C_SIZE + 1) * 1024
 *
 * V1: 组合 C_SIZE / C_SIZE_MULT / READ_BL_LEN
 */
static uint8_t SD_ParseCSD(void)
{
    uint8_t  csd[16];
    uint32_t c_size;

    if (SD_ReadCSD(csd) != 0) {
        return 1;
    }

    if ((csd[0] & 0xC0) == 0x40) {
        /* ---- CSD V2.0 (SDHC/SDXC) ---- */
        c_size  = ((uint32_t)(csd[7] & 0x3F) << 16);
        c_size |= ((uint32_t)csd[8] << 8);
        c_size |=  (uint32_t)csd[9];
        sd.sector_count = (c_size + 1) * 1024U;
    } else {
        /* ---- CSD V1.0 (SDSC) ---- */
        uint8_t  read_bl_len  = csd[5] & 0x0F;
        uint16_t c_size_v1    = ((uint16_t)(csd[6] & 0x03) << 10)
                              | ((uint16_t)csd[7] << 2)
                              | ((uint16_t)(csd[8] & 0xC0) >> 6);
        uint8_t  c_size_mult  = ((csd[9] & 0x03) << 1)
                              | ((csd[10] & 0x80) >> 7);

        sd.sector_count = ((uint32_t)c_size_v1 + 1)
                        * (1UL << (c_size_mult + 2))
                        * (1UL << (read_bl_len - 9));
    }

    return 0;
}

/* ========================================================================= */
/*                      地址转换                                              */
/* ========================================================================= */

/*
 * 将逻辑扇区号转为 SD 命令的地址参数。
 * SDHC/SDXC: 块地址 (扇区号) / SDSC: 字节地址 (扇区号 * 512)
 */
static uint32_t SD_SectorToAddr(uint32_t sector)
{
    if (sd.card_type == BSP_SD_CARD_SDHC || sd.card_type == BSP_SD_CARD_SDXC) {
        return sector;
    }
    return sector << 9;
}

/* ========================================================================= */
/*                        公共 API                                             */
/* ========================================================================= */

/* ---- 初始化与生命周期 ---- */

/**
  * @brief  SD 卡初始化 (软件上下文 + 协议初始化)
  * @retval BSP_SD_OK: 成功, state=READY
  * @note   序列: 低速 → 80+脉冲 → CMD0 → CMD8 → ACMD41 → CMD58 → CMD16 → CSD → 高速
  */
bsp_sd_status_t BSP_SD_Init(void)
{
    uint8_t  r1;
    uint8_t  buf[4];
    uint32_t start;
    uint8_t  is_v2 = 0;

    /* ---- 软件上下文初始化 ---- */
    sd.state        = BSP_SD_STATE_UNINIT;
    sd.card_type    = BSP_SD_CARD_UNKNOWN;
    sd.sector_count = 0;
    sd.sector_size  = 512;

    /* ---- 切低速 ---- */
    SD_SetSpeed(1);

    /* ---- 80+ 时钟脉冲, 引导 SD 卡进入 SPI 模式 ---- */
    CS_Release();
    for (int i = 0; i < 10; i++) {
        BSP_SPI_RW(SD_DUMMY);
    }

    /* ---- CMD0: 复位 (重试直到进入 IDLE 态) ---- */
    do {
        r1 = SD_SendCmd(CMD0, 0, 0x95);
        CS_Release();
    } while (r1 != R1_IDLE);

    /* ---- CMD8: 接口条件检测 (区分 V2/V1) ---- */
    r1 = SD_SendCmd(CMD8, 0x1AA, 0x87);
    if (r1 == R1_IDLE) {
        for (int i = 0; i < 4; i++) {
            buf[i] = BSP_SPI_RW(SD_DUMMY);
        }
        CS_Release();
        if (buf[2] == 0x01 && buf[3] == 0xAA) {
            is_v2 = 1;
        }
    } else {
        CS_Release();
    }

    /* ---- ACMD41 初始化 ---- */
    if (is_v2) {
        /* V2: 带 HCS 位 */
        start = BSP_GetTick();
        do {
            SD_SendCmd(CMD55, 0, 0x01);
            CS_Release();
            r1 = SD_SendCmd(CMD41, ACMD41_HCS, 0x01);
            CS_Release();
            if (r1 == R1_READY) break;
            BSP_DelayMs(10);
        } while ((BSP_GetTick() - start) < SD_INIT_TIMEOUT);

        if (r1 != R1_READY) {
            sd.state = BSP_SD_STATE_ERROR;
            return BSP_SD_ERR_INIT;
        }

        /* CMD58 读取 OCR, 检测 CCS 位 */
        SD_SendCmd(CMD58, 0, 0x01);
        for (int i = 0; i < 4; i++) {
            buf[i] = BSP_SPI_RW(SD_DUMMY);
        }
        CS_Release();

        sd.card_type = (buf[0] & OCR_CCS_BIT) ? BSP_SD_CARD_SDHC
                                               : BSP_SD_CARD_SDSC;
    } else {
        /*
         * 非 V2: 用一次 ACMD41 探测区分 V1 和 MMC。
         * V1 卡响应 R1 <= 1 (IDLE 或 READY); MMC 卡不支持 ACMD41, 返回 >1。
         */
        SD_SendCmd(CMD55, 0, 0x01);
        CS_Release();
        r1 = SD_SendCmd(CMD41, 0, 0x01);
        CS_Release();

        if (r1 <= 1) {
            /* V1 卡 */
            start = BSP_GetTick();
            do {
                SD_SendCmd(CMD55, 0, 0x01);
                CS_Release();
                r1 = SD_SendCmd(CMD41, 0, 0x01);
                CS_Release();
                if (r1 == R1_READY) break;
                BSP_DelayMs(10);
            } while ((BSP_GetTick() - start) < SD_INIT_TIMEOUT);

            if (r1 != R1_READY) {
                sd.state = BSP_SD_STATE_ERROR;
                return BSP_SD_ERR_INIT;
            }
        } else {
            /* MMC 卡 */
            start = BSP_GetTick();
            do {
                r1 = SD_SendCmd(CMD1, 0, 0x01);
                CS_Release();
                if (r1 == R1_READY) break;
                BSP_DelayMs(10);
            } while ((BSP_GetTick() - start) < SD_INIT_TIMEOUT);

            if (r1 != R1_READY) {
                sd.state = BSP_SD_STATE_ERROR;
                return BSP_SD_ERR_INIT;
            }
        }
        sd.card_type = BSP_SD_CARD_SDSC;
    }

    /* ---- CMD16: 设置块大小 = 512 ---- */
    r1 = SD_SendCmd(CMD16, 512, 0x01);
    CS_Release();
    if (r1 != R1_READY) {
        sd.state = BSP_SD_STATE_ERROR;
        return BSP_SD_ERR_INIT;
    }

    /* ---- CMD59: 关闭 CRC (SPI 模式默认) ---- */
    SD_SendCmd(CMD59, 0, 0x01);
    CS_Release();

    /* ---- 读取 CSD, 计算扇区数 ---- */
    if (SD_ParseCSD() != 0) {
        sd.state = BSP_SD_STATE_ERROR;
        return BSP_SD_ERR_INIT;
    }

    /* ---- 切高速 ---- */
    SD_SetSpeed(0);

    sd.state = BSP_SD_STATE_READY;
    return BSP_SD_OK;
}

/**
  * @brief  释放 SD 卡
  */
void BSP_SD_DeInit(void)
{
    CS_Release();
    sd.state = BSP_SD_STATE_UNINIT;
}

/**
  * @brief  获取驱动状态
  */
bsp_sd_state_t BSP_SD_GetState(void)
{
    return sd.state;
}

/**
  * @brief  软件检测 SD 卡是否在位 (无硬件 CD 引脚)
  * @retval 0:不在位, 1:在位
  * @note   发 CMD0 后检测 IDLE 态 (R1=0x01)
  */
uint8_t BSP_SD_IsPresent(void)
{
    uint8_t r1;

    SD_SetSpeed(1);
    r1 = SD_SendCmd(CMD0, 0, 0x95);
    CS_Release();

    if (sd.card_type != BSP_SD_CARD_UNKNOWN) {
        SD_SetSpeed(0);  /* 恢复高速 */
    }

    return (r1 == R1_IDLE) ? 1 : 0;
}

/**
  * @brief  获取 SD 卡信息
  */
bsp_sd_status_t BSP_SD_GetInfo(bsp_sd_info_t *info)
{
    if (info == NULL)                   return BSP_SD_ERR_PARAM;
    if (sd.state != BSP_SD_STATE_READY) return BSP_SD_ERR_NOT_READY;

    info->type         = sd.card_type;
    info->sector_count = sd.sector_count;
    info->sector_size  = sd.sector_size;
    return BSP_SD_OK;
}

/* ---- 扇区读写 ---- */

/**
  * @brief  读取 count 个 512B 扇区
  * @param  sector: 起始逻辑扇区号
  * @param  buffer: 缓冲区 (≥ count*512)
  * @param  count:  扇区数
  */
bsp_sd_status_t BSP_SD_ReadBlocks(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    uint8_t  r1;
    uint32_t addr;

    if (buffer == NULL || count == 0)   return BSP_SD_ERR_PARAM;
    if (sd.state != BSP_SD_STATE_READY) return BSP_SD_ERR_NOT_READY;

    addr = SD_SectorToAddr(sector);

    if (count == 1) {
        /* ---- 单块读 CMD17 ---- */
        r1 = SD_SendCmd(CMD17, addr, 0x01);
        if (r1 != R1_READY) {
            CS_Release();
            return BSP_SD_ERR_RW;
        }
        return SD_ReceiveData(buffer, 512) ? BSP_SD_ERR_TIMEOUT : BSP_SD_OK;
    } else {
        /* ---- 多块读 CMD18 ---- */
        r1 = SD_SendCmd(CMD18, addr, 0x01);
        if (r1 != R1_READY) {
            CS_Release();
            return BSP_SD_ERR_RW;
        }

        for (uint32_t i = 0; i < count; i++) {
            /*
             * SD_ReceiveData 内部: CS_Low → 等 0xFE → 读 512B → CS_Release
             * 多块读间 CS 的 toggling 与参考实现一致。
             */
            if (SD_ReceiveData(buffer + (i * 512), 512) != 0) {
                SD_SendCmd(CMD12, 0, 0x01);
                CS_Release();
                return BSP_SD_ERR_TIMEOUT;
            }
        }

        /* CMD12 停止多块传输 */
        SD_SendCmd(CMD12, 0, 0x01);
        CS_Release();
        return BSP_SD_OK;
    }
}

/**
  * @brief  写入 count 个 512B 扇区
  * @param  sector: 起始逻辑扇区号
  * @param  buffer: 数据源 (≥ count*512)
  * @param  count:  扇区数
  */
bsp_sd_status_t BSP_SD_WriteBlocks(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    uint8_t  r1;
    uint32_t addr;

    if (buffer == NULL || count == 0)   return BSP_SD_ERR_PARAM;
    if (sd.state != BSP_SD_STATE_READY) return BSP_SD_ERR_NOT_READY;

    addr = SD_SectorToAddr(sector);

    if (count == 1) {
        /* ---- 单块写 CMD24 ---- */
        r1 = SD_SendCmd(CMD24, addr, 0x01);
        if (r1 != R1_READY) {
            CS_Release();
            return BSP_SD_ERR_RW;
        }
        /* SD_SendCmd 保持 CS 低 → SD_SendBlock 写数据 → 最终释放 CS */
        if (SD_SendBlock(buffer, TOKEN_START_BLOCK) != 0) {
            CS_Release();
            return BSP_SD_ERR_RW;
        }
        CS_Release();
        return BSP_SD_OK;
    } else {
        /* ---- 多块写 CMD25 ---- */
        /* ACMD23: 预置擦除块数 */
        SD_SendCmd(CMD55, 0, 0x01);
        CS_Release();
        SD_SendCmd(CMD23, count, 0x01);
        CS_Release();

        /* 启动多块写 */
        r1 = SD_SendCmd(CMD25, addr, 0x01);
        if (r1 != R1_READY) {
            CS_Release();
            return BSP_SD_ERR_RW;
        }

        /*
         * CS 由 SD_SendCmd 保活。
         * 多块写期间 CS 维持低电平, 逐块写入, 最后发停止令牌。
         */
        for (uint32_t i = 0; i < count; i++) {
            BSP_SPI_CS_Low();
            SD_WaitReady(SD_CMD_TIMEOUT);

            if (SD_SendBlock(buffer + (i * 512), TOKEN_START_MULTI) != 0) {
                /* 异常中止: 发停止令牌 */
                BSP_SPI_RW(TOKEN_STOP_TRANS);
                SD_WaitReady(SD_WRITE_TIMEOUT);
                CS_Release();
                return BSP_SD_ERR_RW;
            }
        }

        /* 发送停止传输令牌 */
        BSP_SPI_CS_Low();
        SD_WaitReady(SD_CMD_TIMEOUT);
        BSP_SPI_RW(TOKEN_STOP_TRANS);
        SD_WaitReady(SD_WRITE_TIMEOUT);
        CS_Release();
        return BSP_SD_OK;
    }
}

/**
  * @brief  刷新写入、确保总线空闲 (对应 FatFS CTRL_SYNC)
  */
bsp_sd_status_t BSP_SD_Sync(void)
{
    CS_Release();
    BSP_SPI_RW(SD_DUMMY);
    BSP_SPI_RW(SD_DUMMY);
    return BSP_SD_OK;
}
