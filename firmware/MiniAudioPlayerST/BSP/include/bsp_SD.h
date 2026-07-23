#ifndef __BSP_SD_H
#define __BSP_SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * SD驱动状态
 *
 * 描述BSP层自身状态，
 * 不代表SD卡内部协议状态。
 */
typedef enum
{
    BSP_SD_STATE_UNINIT = 0,  // 驱动未初始化
    BSP_SD_STATE_READY,       // 卡初始化完成，可以访问
    BSP_SD_STATE_ERROR        // 初始化或通信错误

} bsp_sd_state_t;

/*
 * SD卡类型
 */
typedef enum
{
    BSP_SD_CARD_UNKNOWN = 0,

    BSP_SD_CARD_SDSC,  // Standard Capacity
    BSP_SD_CARD_SDHC,  // High Capacity
    BSP_SD_CARD_SDXC

} bsp_sd_card_type_t;

/*
 * SD驱动返回状态
 */
typedef enum
{
    BSP_SD_OK = 0,

    BSP_SD_ERR_PARAM,     // 参数错误
    BSP_SD_ERR_NO_CARD,   // 无卡
    BSP_SD_ERR_INIT,      // 初始化失败
    BSP_SD_ERR_TIMEOUT,   // 超时
    BSP_SD_ERR_CRC,       // CRC错误
    BSP_SD_ERR_RW,        // 读写失败
    BSP_SD_ERR_NOT_READY  // 未准备好

} bsp_sd_status_t;

/*
 * SD卡信息
 */
typedef struct
{
    bsp_sd_card_type_t type;

    uint32_t           sector_count;

    uint32_t           sector_size;  // SD固定512 Bytes

} bsp_sd_info_t;

/*
 * 初始化 SD 卡
 *
 * 完成：
 * - 软件状态初始化
 * - CS 引脚默认高电平
 * - SPI 低速 → CMD0 → CMD8 → ACMD41 → CMD58 → CMD16 → CSD解析 → SPI高速
 * - 识别卡类型 (SDSC/SDHC/SDXC)
 *
 * 成功后 state=READY, 返回 BSP_SD_OK。
 * 失败返回具体错误码。
 */
bsp_sd_status_t BSP_SD_Init(void);

/*
 * 释放 SD 卡资源
 *
 * CS 置高 + dummy, state=UNINIT。
 * 不关闭 SPI 外设。
 */
void            BSP_SD_DeInit(void);

/*
 * 查询卡是否存在
 *
 * 如果没有CD检测引脚，
 * 可以返回软件检测结果。
 */
uint8_t         BSP_SD_IsPresent(void);

/*
 * 获取驱动状态
 */
bsp_sd_state_t  BSP_SD_GetState(void);

/*
 * 获取SD卡信息
 */
bsp_sd_status_t BSP_SD_GetInfo(bsp_sd_info_t *info);

/*
 * 读取多个512 Bytes扇区
 *
 * sector:
 *   逻辑扇区号
 *
 * count:
 *   扇区数量
 *
 * buffer:
 *   数据缓存
 */
bsp_sd_status_t BSP_SD_ReadBlocks(uint32_t sector, uint8_t *buffer, uint32_t count);

/*
 * 写入多个512 Bytes扇区
 */
bsp_sd_status_t BSP_SD_WriteBlocks(uint32_t sector, const uint8_t *buffer, uint32_t count);

/*
 * 等待写操作完成
 *
 * 对应FatFS:
 * CTRL_SYNC
 */
bsp_sd_status_t BSP_SD_Sync(void);

#ifdef __cplusplus
}
#endif

#endif