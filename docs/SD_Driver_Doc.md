# SD Driver 接口文档

> 本文档基于 `MDK-ARM\SDdriver.h` 和 `MDK-ARM\SDdriver.c` 整理，描述 STM32 SPI 方式驱动 SD 卡的底层接口。

---

## 1. 宏定义与数据结构

### 1.1 SD 卡类型定义

| 宏名称 | 值 | 说明 |
|--------|------|------|
| `ERR` | `0x00` | 错误/未识别 |
| `MMC` | `0x01` | MMC 卡 |
| `V1` | `0x02` | SD V1.x 标准卡 |
| `V2` | `0x04` | SD V2.0 标准卡 |
| `V2HC` | `0x06` | SD V2.0 高容量卡 (SDHC) |

### 1.2 SD 命令宏

| 宏名称 | 值 | 说明 |
|--------|------|------|
| `CMD0` | 0 | 复位/空闲 |
| `CMD1` | 1 | 激活初始化 (MMC) |
| `CMD8` | 8 | 发送接口条件 (SEND_IF_COND) |
| `CMD9` | 9 | 读取 CSD 寄存器 |
| `CMD10` | 10 | 读取 CID 寄存器 |
| `CMD12` | 12 | 停止数据传输 |
| `CMD16` | 16 | 设置块大小 |
| `CMD17` | 17 | 读单块 |
| `CMD18` | 18 | 读多块 |
| `CMD23` | 23 | 设置写前预擦除块数 |
| `CMD24` | 24 | 写单块 |
| `CMD25` | 25 | 写多块 |
| `CMD41` | 41 | ACMD41 (需先发 CMD55) |
| `CMD55` | 55 | 应用命令前缀 |
| `CMD58` | 58 | 读取 OCR 寄存器 |
| `CMD59` | 59 | 使能/禁止 CRC |

### 1.3 数据响应标志

| 宏名称 | 值 | 说明 |
|--------|------|------|
| `MSD_DATA_OK` | `0x05` | 数据写入成功 |
| `MSD_DATA_CRC_ERROR` | `0x0B` | 数据 CRC 错误 |
| `MSD_DATA_WRITE_ERROR` | `0x0D` | 数据写入错误 |
| `MSD_DATA_OTHER_ERROR` | `0xFF` | 其他数据错误 |

### 1.4 命令响应标志 (R1)

| 宏名称 | 值 | 说明 |
|--------|------|------|
| `MSD_RESPONSE_NO_ERROR` | `0x00` | 无错误 |
| `MSD_IN_IDLE_STATE` | `0x01` | 处于空闲状态 |
| `MSD_ERASE_RESET` | `0x02` | 擦除复位 |
| `MSD_ILLEGAL_COMMAND` | `0x04` | 非法命令 |
| `MSD_COM_CRC_ERROR` | `0x08` | 命令 CRC 错误 |
| `MSD_ERASE_SEQUENCE_ERROR` | `0x10` | 擦除序列错误 |
| `MSD_ADDRESS_ERROR` | `0x20` | 地址错误 |
| `MSD_PARAMETER_ERROR` | `0x40` | 参数错误 |
| `MSD_RESPONSE_FAILURE` | `0xFF` | 响应失败 |

### 1.5 关键结构体

| 结构体 | 用途 | 关键字段 |
|--------|------|----------|
| `MSD_CSD` | 卡特定数据 (Card Specific Data) | `CSDStruct`, `DeviceSize`, `RdBlockLen`, `MaxBusClkFrec`, `CSD_CRC` 等 |
| `MSD_CID` | 卡识别数据 (Card Identification Data) | `ManufacturerID`, `OEM_AppliID`, `ProdName1/2`, `ProdSN`, `ManufactDate` 等 |
| `MSD_CARDINFO` | 卡完整信息 | 包含 `CSD`, `CID`, `Capacity`, `BlockSize`, `RCA`, `CardType`, `SpaceTotal`, `SpaceFree` |

---

## 2. 接口函数表

### 2.1 公共接口

| # | 函数签名 | 实现思路 | 依赖 |
|---|----------|----------|------|
| 1 | `uint8_t SD_init(void)` | **SD 卡初始化**：低速 SPI 模式下发送 80+ 个时钟脉冲使卡进入 SPI 模式；发送 CMD0 复位进入 IDLE 态；发送 CMD8 检测 SD 版本；若响应合法则循环发送 CMD55+CMD41 (ACMD41) 启动初始化并判断 SDHC/标准容量；若 CMD8 无响应则尝试 CMD55+CMD41 识别 V1 卡，失败则走 CMD1 识别 MMC；最后发送 CMD16 设置块大小为 512 字节。成功后切换为高速 SPI。返回 0 成功，1 失败。 | `SPI_setspeed`, `SD_CS`, `spi_readwrite`, `SD_sendcmd` (内部), `HAL_Delay` |
| 2 | `void SD_CS(uint8_t p)` | **SD 卡片选控制**：`p==0` 时通过 HAL 库拉高 CS 引脚（失能/释放），`p!=0` 时拉低 CS 引脚（使能/选中）。注意逻辑与常见约定相反。 | `HAL_GPIO_WritePin`, `SD_CS_GPIO_Port`, `SD_CS_Pin` (定义于 `main.h`) |
| 3 | `uint32_t SD_GetSectorCount(void)` | **获取 SD 卡扇区总数**：先通过 `SD_GETCSD` 读取 16 字节 CSD 数据；判断 `csd[0]&0xC0` 是否为 `0x40` 区分 V2 和 V1；V2 卡取 `csd[8..9]` 计算 C_SIZE，`(csize+1) << 10` 得到总扇区数；V1 卡从 `csd[5..10]` 提取 C_SIZE 和 C_SIZE_MULT 按标准公式计算容量。 | `SD_GETCSD` |
| 4 | `uint8_t SD_GETCID(uint8_t *cid_data)` | **读取 CID 寄存器**：发送 CMD10 读取 CID；收到 0x00 响应后调用 `SD_ReceiveData` 接收 16 字节 CID 数据。返回 0 成功。 | `SD_sendcmd`, `SD_ReceiveData`, `SD_CS` |
| 5 | `uint8_t SD_GETCSD(uint8_t *csd_data)` | **读取 CSD 寄存器**：发送 CMD9 读取 CSD；收到 0x00 响应后调用 `SD_ReceiveData` 接收 16 字节 CSD 数据。返回 0 成功。 | `SD_sendcmd`, `SD_ReceiveData`, `SD_CS` |
| 6 | `int MSD0_GetCardInfo(PMSD_CARDINFO SD0_CardInfo)` | **获取卡完整信息**：分别读取 CSD (CMD9) 和 CID (CMD10) 寄存器原始数据；逐字节解析 CSD 各位段填充 `MSD_CSD` 结构体（含 CSD 版本、设备大小、访问时间、读写电流等）；逐字节解析 CID 各位段填充 `MSD_CID` 结构体（含厂商 ID、产品名、序列号、生产日期等）；对 V2HC 卡修正 `DeviceSize` 计算；最终计算 `Capacity = DeviceSize × 512 × 1024`。 | `SD_sendcmd`, `SD_ReceiveData`, `MSD_BLOCKSIZE` |
| 7 | `uint8_t SD_ReceiveData(uint8_t *data, uint16_t len)` | **接收 SD 卡数据块**：选中 SD 卡后在 SPI 总线上循环读取字节，直到收到数据起始令牌 `0xFE`；然后连续读取 `len` 个字节存入 `data` 缓冲区；最后读取 2 字节 CRC（不存储，直接丢弃）。含 `HAL_Delay(100)` 等待令牌，效率一般。 | `SD_CS`, `spi_readwrite`, `HAL_Delay` |
| 8 | `uint8_t SD_SendBlock(uint8_t *buf, uint8_t cmd)` | **向 SD 卡发送一个数据块**：先等待 SD 卡就绪（读 `0xFF`）；发送写命令令牌 `cmd`；若 `cmd != 0xFD`（非停止传输令牌），依次发送 512 字节数据 + 2 字节假 CRC；读取响应字节，检查低 5 位是否为 `0x05`（数据接受）。`cmd` 取值：`0xFC`=多块写开始, `0xFD`=停止传输, `0xFE`=单块写开始。 | `spi_readwrite` |
| 9 | `uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint8_t cnt)` | **读磁盘扇区**：非 V2HC 卡将扇区号左移 9 位转为字节地址；单块 (`cnt==1`) 发送 CMD17 读单块；多块发送 CMD18 启动连续读，循环 `SD_ReceiveData` 接收数据，最后发送 CMD12 停止。返回 0 成功。 | `SD_sendcmd`, `SD_ReceiveData`, `SD_CS`, `SD_TYPE` |
| 10 | `uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint8_t cnt)` | **写磁盘扇区**：非 V2HC 卡将扇区号乘以 512 转为字节地址；单块发送 CMD24 + `SD_SendBlock`(0xFE)；多块先发 CMD55+CMD23 预置擦除块数（非 MMC），再发 CMD25 启动多块写，循环 `SD_SendBlock`(0xFC) 写入每块，最后发送 `0xFD` 停止令牌结束传输。返回 0 成功。 | `SD_sendcmd`, `SD_SendBlock`, `SD_CS`, `SD_TYPE` |

### 2.2 SPI 底层接口

| # | 函数签名 | 实现思路 | 依赖 |
|---|----------|----------|------|
| 11 | `uint8_t spi_readwrite(uint8_t Txdata)` | **SPI 单字节收发**：调用 HAL 库 `HAL_SPI_TransmitReceive` 在 SPI1 上全双工传输 1 字节，超时 100ms。发送 `Txdata` 同时接收 `Rxdata` 返回。 | `HAL_SPI_TransmitReceive`, `hspi1` |
| 12 | `void SPI_setspeed(uint8_t speed)` | **SPI 速率切换**：直接修改 `hspi1.Init.BaudRatePrescaler` 分频系数。初始化时设为 `SPI_BAUDRATEPRESCALER_256`（低速），完成初始化后设为 `SPI_BAUDRATEPRESCALER_2`（高速）。 | `hspi1` |

### 2.3 内部静态函数（.c 文件内部使用）

| # | 函数签名 | 实现思路 | 依赖 |
|---|----------|----------|------|
| 13 | `int SD_sendcmd(uint8_t cmd, uint32_t arg, uint8_t crc)` | **发送 SD 命令**：先拉低 CS 并等待卡就绪（读到 `0xFF`）；依次 SPI 发送命令字节 `cmd|0x40`（传输位=01）、4 字节参数 `arg` (MSB 优先)、1 字节 CRC；CMD12 时多发一个哑字节；循环读取 R1 响应直到最高位为 0（卡不忙），返回 R1。 | `SD_CS`, `spi_readwrite`, `HAL_Delay` |

---

## 3. 全局变量

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `SD_TYPE` | `uint8_t` | 当前 SD 卡类型，取值 `ERR` / `MMC` / `V1` / `V2` / `V2HC` |
| `SD0_CardInfo` | `MSD_CARDINFO` | 卡 0 的完整信息（CSD + CID + 容量等） |
| `DFF` | `uint8_t` | 哑字节常量 `0xFF`，用于 SPI 时钟填充 |

---

## 4. 外部依赖关系

```
main.h (引脚宏: SD_CS_Pin, SD_CS_GPIO_Port; hspi1 句柄声明)
  │
ff.h (FATFS 头文件，仅 include)
  │
  ▼
SDdriver.h / SDdriver.c
  │
  ├── STM32 HAL 库
  │     ├── HAL_GPIO_WritePin()     → CS 引脚控制
  │     ├── HAL_SPI_TransmitReceive() → SPI 全双工通信 (SPI1)
  │     └── HAL_Delay()             → 延时等待
  │
  └── 硬件抽象
        ├── hspi1 (SPI_HandleTypeDef)  → SPI1 外设句柄
        ├── GPIOA, PIN_4               → SD 卡片选引脚
        └── SPI_BAUDRATEPRESCALER_x    → 分频系数 (256 / 2)
```

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| `main.h` | 用户项目 | 硬件引脚宏定义 (`SD_CS_Pin=GPIO_PIN_4`, `SD_CS_GPIO_Port=GPIOA`) 和 SPI 句柄 `hspi1` 声明 |
| `ff.h` | FATFS 库 | 仅被 include，该模块自身不直接使用 FATFS 接口 |
| `hspi1` | `Src/main.c` | SPI1 外设句柄，初始化为 Master/双线/8bit/MSB 模式 |
| `HAL_GPIO_WritePin` | STM32 HAL | 控制 SD 卡片选引脚电平 |
| `HAL_SPI_TransmitReceive` | STM32 HAL | SPI 全双工单字节收发 |
| `HAL_Delay` | STM32 HAL | 毫秒级延时（初始化等待、数据令牌等待） |

---

## 5. 上层调用关系

`SDdriver` 模块被 `user_diskio.c` (FATFS 磁盘 I/O 适配层) 调用，映射关系：

| FATFS diskio 函数 | 调用的 SDdriver 接口 |
|-------------------|-----------------------|
| `USER_initialize()` | `SD_init()`, `SPI_setspeed()`, `spi_readwrite()` |
| `USER_read()` | `SD_ReadDisk()` |
| `USER_write()` | `SD_WriteDisk()` |
| `USER_ioctl(GET_SECTOR_COUNT)` | `SD_GetSectorCount()` |
| `USER_ioctl(CTRL_SYNC)` | `SD_CS()`, `spi_readwrite()` |

---

## 6. 注意事项

1. **CS 逻辑反直觉**：`SD_CS(1)` 拉低 CS 使能芯片，`SD_CS(0)` 拉高 CS 释放芯片，与常见低有效约定相反。
2. **地址模式差异**：V2HC 卡使用块地址（扇区号），其他类型需转换为字节地址（左移 9 位 / 乘 512）。
3. **SPI 速率切换**：初始化时必须使用低速 `_256` 分频（≤400kHz），初始化完成后切换到 `_2` 分频高速模式。
4. **SD_ReceiveData 性能**：令牌等待循环内包含 `HAL_Delay(100)`，在高频读取场景下可能成为瓶颈。
5. **CRC 处理**：SPI 模式下 CRC 校验默认关闭（CMD59），因此发送的 CRC 值（除 CMD0 的 0x95 和 CMD8 的 0x87 外）一般为 `0x01` 占位即可。
