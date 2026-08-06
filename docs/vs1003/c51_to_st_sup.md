# VS1003 驱动移植指南: C51 → STM32F0

## 概览

本文档基于 `tmp/vs1003_refer/` 中的 STC12LE5A60S2 (8051 内核) 参考驱动, 逐功能分析移植到
STM32F072C8T6 (Cortex-M0) + HAL 库的方法。目标 BSP 文件:

- `firmware/MiniAudioPlayerST/BSP/include/bsp_vs1003.h`  (公开接口)
- `firmware/MiniAudioPlayerST/BSP/src/bsp_vs1003.c`      (实现)

---

## 1. 工程基础设施对照

| 项目 | C51 参考代码 | STM32F072 本工程 |
|------|-------------|------------------|
| 平台头文件 | `<Reg52.H>` | `stm32f0xx_hal.h` (via `main.h`) |
| SPI 通信 | GPIO 软件模拟 (`SPIPutChar`/`SPI_RecByte`) | 硬件 **SPI2** (`hspi2`) — 独立总线 |
| SPI 封装层 | 无 | `bsp_spi.c` 接收总线 context，可由 SPI1/SD 和 SPI2/VS1003 复用 |
| 数据发送 | 逐字节 DREQ 轮询 | **DMA1 CH2** (Memory→SPI2_TX, Normal 模式, 32 字节/次) |
| DREQ 检测 | `while(MP3_DREQ==0)` 忙等 | **EXTI10 上升沿中断** + FreeRTOS 计数信号量 |
| 引脚控制 | `sbit` 位变量 + 直接赋值 | `HAL_GPIO_WritePin()` / `HAL_GPIO_ReadPin()` |
| 延时 | `_nop_()` 指令循环 | `HAL_Delay()` |
| 数据类型 | `uchar`/`uint`/`bool`/`bit` 宏 | `<stdint.h>` 标准类型 (`uint8_t`/`uint16_t`/`uint32_t`) |
| 多任务 | 无 (裸机大循环) | FreeRTOS (`audioTask`, 优先级 High) |

### 关键差异 1: SPI 方案

C51 用 **GPIO 软件翻转** 实现 SPI 时序。本工程使用独立的 **SPI2 外设** (PB13/PB14/PB15),
不与 SD 卡的 SPI1 共享总线。因此:

- VS1003 的命令 (SCI) 和数据 (SDI) 共享同一 SPI2, 通过 **XCS** 和 **XDCS** 区分
- **XCS** 和 **XDCS** 是独立的 GPIO, 在 `bsp_vs1003.c` 内部通过 `HAL_GPIO_WritePin()` 控制
- `bsp_spi.c` 只负责通用 SPI 收发；SD 的 CS、VS1003 的 XCS/XDCS 分别由各自设备驱动管理

### 关键差异 2: DREQ 流控 — 从中断忙等到 DMA + RTOS

C51 参考代码在发送每批数据前用 `while(MP3_DREQ==0);` 忙等。本工程 Dev.md 设计了
更高效的方案:

```
VS1003 DREQ 上升沿 (PB10)
  → EXTI10 中断
    → xSemaphoreGiveFromISR() 释放计数信号量
      → audioTask (FreeRTOS, 优先级 High) 被唤醒
        → 从双缓冲取 32 字节
          → DMA1 CH2 自动搬运到 SPI2 TX
            → DMA 完成中断标记当前缓冲块可用
```

这意味着 BSP 层需要配合这个架构:
- **寄存器操作 (SCI)** 用阻塞式 SPI 收发 (快速, 不频繁)
- **音频数据发送 (SDI)** 提供 DMA 启动接口 (`BSP_VS1003_StartDMA(const uint8_t *buf)`)

### 关键差异 3: 双缓冲 Ping-Pong

参考代码一次性把所有数据发完。本工程使用 **双缓冲** (buf_A[32] / buf_B[32]):
DMA 发 buf_A 时, audioTask 可以填 buf_B, 反之亦然。这是应用层的逻辑, 但 BSP 层
需要提供可被反复调用的单次 DMA 启动接口。

---

## 2. 引脚映射规划 (已由 Dev.md 确定)

C51 参考使用的引脚 (vs1003.c:34-50):

```
sbit MP3_XRESET = P3^2;   // 硬件复位 (低有效)
sbit MP3_XCS    = P3^3;   // SCI 命令片选 (低有效)
sbit MP3_XDCS   = P3^4;   // SDI 数据片选 (低有效)
sbit MP3_DREQ   = P3^5;   // 数据就绪 (输入, 高有效)
sbit c_SPI_SI   = P1^5;   // SPI MOSI → 硬件 SPI2 替代
sbit c_SPI_SO   = P1^6;   // SPI MISO → 硬件 SPI2 替代
sbit c_SPI_CLK  = P1^7;   // SPI SCLK → 硬件 SPI2 替代
```

移植到 STM32F072C8T6 的实际引脚 (来自 Dev.md 第 1.2.4 节):

| 信号 | C51 引脚 | STM32F072 引脚 | 模式 | CubeMX Label |
|------|---------|---------------|------|-------------|
| SPI2_SCK | P1.7 | **PB13** | AF Push-Pull | (SPI2 外设) |
| SPI2_MISO | P1.6 | **PB14** | AF Push-Pull | (SPI2 外设) |
| SPI2_MOSI | P1.5 | **PB15** | AF Push-Pull | (SPI2 外设) |
| XCS | P3.3 | **PB11** | GPIO OUT | VS1003_XCS |
| XDCS | P3.4 | **PB12** | GPIO OUT | VS1003_XDCS |
| XRESET | P3.2 | **(待定)** | GPIO OUT | VS1003_XRESET |
| DREQ | P3.5 | **PB10** | GPIO_Input + EXTI10 | VS1003_DREQ |

> **注意**: Dev.md 中未列出 XRESET 引脚。需要在 CubeMX 中分配一个空闲 GPIO。
> 候选: PA8 (与按键不冲突)、PB2 等。

SPI2 参数配置 (来自 Dev.md):

| 参数 | 值 | 说明 |
|------|-----|------|
| Mode | Full-Duplex Master | |
| Data Size | 8 Bits | |
| CPOL | Low | VS1003 要求 SPI 模式 0 |
| CPHA | 1 Edge | |
| NSS | Software | 片选用 GPIO 管理 |
| Baud Rate | ≤ 6 Mbps | VS1003 最大 SPI 时钟约 6-8 MHz |

---

## 3. 逐函数移植对照

### 3.1 基础宏定义 (vs1003.h:1-58)

**参考代码** (`vs1003.h`):
```c
#define VS_WRITE_COMMAND     0x02
#define VS_READ_COMMAND      0x03
#define SPI_MODE             0x00
#define SPI_STATUS           0x01
#define SPI_BASS             0x02
#define SPI_CLOCKF           0x03
#define SPI_DECODE_TIME      0x04
#define SPI_AUDATA           0x05
#define SPI_WRAM             0x06
#define SPI_WRAMADDR         0x07
#define SPI_HDAT0            0x08
#define SPI_HDAT1            0x09
#define SPI_AIADDR           0x0A
#define SPI_VOL              0x0B
#define SPI_AICTRL0          0x0C
#define SPI_AICTRL1          0x0D
#define SPI_AICTRL2          0x0E
#define SPI_AICTRL3          0x0F
#define SM_DIFF              0x0001
#define SM_RESET             0x0004
#define SM_PDOWN             0x0010
// ... 其他 SM_ 位定义
```

**移植**: 直接保留到 `bsp_vs1003.h`。这些是 VS1003 数据手册定义的寄存器地址和位掩码,
与平台无关。`Mp3SetVolume` 宏也保留, 改为内联函数风格。

---

### 3.2 引脚操作宏 (vs1003.c:34-65)

**参考代码**:
```c
sbit MP3_XRESET = P3^2;
sbit MP3_XCS    = P3^3;
sbit MP3_XDCS   = P3^4;
sbit MP3_DREQ   = P3^5;
sbit c_SPI_SI   = P1^5;
sbit c_SPI_SO   = P1^6;
sbit c_SPI_CLK  = P1^7;

#define Mp3PutInReset()       { MP3_XRESET = 0; }
#define Mp3ReleaseFromReset() { MP3_XRESET = 1; }
#define Mp3SelectControl()    { MP3_XCS = 0; }
#define Mp3DeselectControl()  { MP3_XCS = 1; }
#define Mp3SelectData()       { MP3_XDCS = 0; }
#define Mp3DeselectData()     { MP3_XDCS = 1; }
```

**移植**: 宏改为 `static inline` 函数, 使用 `HAL_GPIO_WritePin()`。
DREQ 检测使用 `HAL_GPIO_ReadPin()`。在 `bsp_vs1003.c` 内部实现:

```c
/* 引脚定义 — 来自 Dev.md 第 1.2.4 节 SPI2 配置 */
/* XRESET 引脚 Dev.md 未指定, 以下为建议值 */
#define VS1003_XCS_PORT       GPIOB
#define VS1003_XCS_PIN        GPIO_PIN_11       /* PB11 — XCS (命令片选)    */
#define VS1003_XDCS_PORT      GPIOB
#define VS1003_XDCS_PIN       GPIO_PIN_12       /* PB12 — XDCS (数据片选)   */
#define VS1003_DREQ_PORT      GPIOB
#define VS1003_DREQ_PIN       GPIO_PIN_10       /* PB10 — DREQ (EXTI10)     */
#define VS1003_XRESET_PORT    GPIOx              /* TBD — 待 CubeMX 分配      */
#define VS1003_XRESET_PIN     GPIO_PIN_x

static inline void VS1003_XCS_Low(void)     { HAL_GPIO_WritePin(VS1003_XCS_PORT,  VS1003_XCS_PIN,  GPIO_PIN_RESET); }
static inline void VS1003_XCS_High(void)    { HAL_GPIO_WritePin(VS1003_XCS_PORT,  VS1003_XCS_PIN,  GPIO_PIN_SET); }
static inline void VS1003_XDCS_Low(void)    { HAL_GPIO_WritePin(VS1003_XDCS_PORT, VS1003_XDCS_PIN, GPIO_PIN_RESET); }
static inline void VS1003_XDCS_High(void)   { HAL_GPIO_WritePin(VS1003_XDCS_PORT, VS1003_XDCS_PIN, GPIO_PIN_SET); }
static inline void VS1003_XRESET_Low(void)  { HAL_GPIO_WritePin(VS1003_XRESET_PORT, VS1003_XRESET_PIN, GPIO_PIN_RESET); }
static inline void VS1003_XRESET_High(void) { HAL_GPIO_WritePin(VS1003_XRESET_PORT, VS1003_XRESET_PIN, GPIO_PIN_SET); }
static inline uint8_t VS1003_DREQ_Read(void){ return HAL_GPIO_ReadPin(VS1003_DREQ_PORT, VS1003_DREQ_PIN); }
```

**优势**: 函数形式有类型检查, 调试时可设断点, 同时保持零开销 (编译器会内联)。

---

### 3.3 SPI 初始化: `MSPI_Init()` (vs1003.c:99-119)

**参考代码**:
```c
void MSPI_Init(void)
{
    // LPC2103: 配置 SPI 引脚功能
    // PINSEL0 = (PINSEL0 & 0xFFFF00FF) | 0x00005500;
    // S0SPCCR = 0x08;
    // S0SPCR  = (0<<3) | (0<<4) | (1<<5) | (0<<6) | (0<<7); // CPHA=0, CPOL=0, MSTR=1
    c_SPI_SO = 1;     // MISO 配置为输入
    MP3_DREQ = 1;     // DREQ 配置为输入
}
```

**移植**: STM32 的 SPI2 初始化由 CubeMX 生成的 `MX_SPI2_Init()` 完成 (在 `main.c` 中调用)。
本函数只需:

1. 初始化 VS1003 专用 GPIO (XRESET, XCS, XDCS, DREQ)
2. 将所有控制引脚设为默认高电平 (空闲/未选中状态)

```c
static void VS1003_GPIO_Init(void)
{
    /* 控制引脚: 默认高电平 (均低有效) */
    VS1003_XCS_High();
    VS1003_XDCS_High();
    VS1003_XRESET_High();
    /* DREQ 为输入, 由 CubeMX 初始化, 无需额外配置 */
}
```

---

### 3.4 接口初始化: `InitPortVS1003()` (vs1003.c:126-137)

**参考代码**:
```c
void InitPortVS1003(void)
{
    MSPI_Init();
    MP3_DREQ = 1;
    MP3_XRESET = 1;
    MP3_XCS = 1;
    MP3_XDCS = 1;
}
```

**移植**: 合并到 `VS1003_GPIO_Init()` 中, 对外暴露为 `BSP_VS1003_Init()` 的第一部分。
不需要单独暴露。

---

### 3.5 软件 SPI 发送: `SPIPutChar()` (vs1003.c:147-173)

**参考代码**:
```c
void SPIPutChar(unsigned char ucSendData)
{
    uchar ucCount;
    uchar ucMaskCode;
    ucMaskCode = 0x80;
    for(ucCount=0; ucCount<8; ucCount++)
    {
        Macro_Set_CLK_Low();
        if(ucMaskCode & ucSendData)
            Macro_Set_SI_High();
        else
            Macro_Set_SI_Low();
        Macro_Set_CLK_High();
        ucMaskCode >>= 1;
    }
}
```

**移植**: 完全废弃。替换为 `HAL_SPI_TransmitReceive(&hspi2, ...)` 调用硬件 SPI2。
由于 VS1003 使用独立 SPI2 (SD 卡用 SPI1), 而 `bsp_spi.c` 目前仅封装了 SPI1,
VS1003 的 SCI 寄存器通信**直接使用 HAL API** 或新建一个轻量 SPI2 封装函数:

```c
/* bsp_vs1003.c 内部 SPI2 收发辅助 */
static uint8_t VS1003_SPI_RW(uint8_t tx_data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi2, &tx_data, &rx_data, 1, HAL_MAX_DELAY);
    return rx_data;
}
```

**要点**: VS1003 的 SCI 寄存器读写是半双工的 (发完命令后要收数据), 所以使用
全双工的 `HAL_SPI_TransmitReceive()` 是最通用的选择。纯发送场景也可用它。

---

### 3.6 软件 SPI 接收: `SPI_RecByte()` (vs1003.c:178-199)

**参考代码**:
```c
static uchar SPI_RecByte(void)
{
    uchar ucReadData = 0;
    Macro_Set_SI_High();
    for(ucCount=0; ucCount<8; ucCount++)
    {
        ucReadData <<= 1;
        Macro_Set_CLK_Low();
        if(c_SPI_SO) ucReadData |= 0x01;
        Macro_Set_CLK_High();
    }
    return ucReadData;
}
```

**移植**: 完全废弃。替换为 `VS1003_SPI_RW(0xFF)` — 发送 dummy 字节以产生时钟, 返回值即为接收数据。

---

### 3.7 写 VS1003 寄存器: `Mp3WriteRegister()` (vs1003.c:207-216)

**参考代码**:
```c
void Mp3WriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte)
{
    Mp3DeselectData();          // XDCS=1 (退出数据模式)
    Mp3SelectControl();         // XCS=0  (进入命令模式)
    SPIPutChar(VS_WRITE_COMMAND); // 0x02
    SPIPutChar(addressbyte);
    SPIPutChar(highbyte);
    SPIPutChar(lowbyte);
    Mp3DeselectControl();       // XCS=1
}
```

**移植**: 逻辑不变, 替换 SPI 调用和片选宏:

```c
void VS1003_WriteReg(uint8_t addr, uint16_t data)
{
    VS1003_XDCS_High();            /* 必须先退出数据模式       */
    VS1003_XCS_Low();              /* 选通命令接口             */
    VS1003_SPI_RW(VS_WRITE_COMMAND);  /* 0x02 — 写操作码      */
    VS1003_SPI_RW(addr);              /* 寄存器地址            */
    VS1003_SPI_RW((uint8_t)(data >> 8));   /* 高字节         */
    VS1003_SPI_RW((uint8_t)(data & 0xFF)); /* 低字节         */
    VS1003_XCS_High();             /* 释放命令接口             */
}
```

**接口改进**: 将 `highbyte` / `lowbyte` 合并为单个 `uint16_t data`, 调用方不再需要手动拆字节。

---

### 3.8 读 VS1003 寄存器: `Mp3ReadRegister()` (vs1003.c:224-244)

**参考代码**:
```c
unsigned int Mp3ReadRegister(unsigned char addressbyte)
{
    unsigned int resultvalue = 0;
    uchar ucReadValue;
    Mp3DeselectData();
    Mp3SelectControl();
    SPIPutChar(VS_READ_COMMAND);  // 0x03
    SPIPutChar(addressbyte);
    ucReadValue = SPI_RecByte();
    resultvalue = ucReadValue << 8;
    ucReadValue = SPI_RecByte();
    resultvalue |= ucReadValue;
    Mp3DeselectControl();
    return resultvalue;
}
```

**移植**:

```c
uint16_t VS1003_ReadReg(uint8_t addr)
{
    uint16_t result;
    VS1003_XDCS_High();
    VS1003_XCS_Low();
    VS1003_SPI_RW(VS_READ_COMMAND);   /* 0x03 — 读操作码       */
    VS1003_SPI_RW(addr);              /* 寄存器地址            */
    result  = (uint16_t)VS1003_SPI_RW(0xFF) << 8;  /* 高字节  */
    result |= (uint16_t)VS1003_SPI_RW(0xFF);       /* 低字节  */
    VS1003_XCS_High();
    return result;
}
```

---

### 3.9 软件复位: `Mp3SoftReset()` (vs1003.c:251-267)

**参考代码**:
```c
void Mp3SoftReset(void)
{
    Mp3WriteRegister(SPI_MODE, 0x08, 0x04); // SM_RESET
    wait(1);
    while (MP3_DREQ == 0);                  // 等待 DREQ 变高
    Mp3WriteRegister(SPI_CLOCKF, 0x98, 0x00); // 3 倍频
    Mp3WriteRegister(SPI_AUDATA, 0xBB, 0x81); // 48kHz, 立体声
    Mp3WriteRegister(SPI_BASS,   0x00, 0x55); // 重低音增强
    Mp3SetVolume(10, 10);                     // 初始音量
    wait(1);
    // 发送 4 字节无效数据, 启动 SPI 传输
    Mp3SelectData();
    SPIPutChar(0); SPIPutChar(0);
    SPIPutChar(0); SPIPutChar(0);
    Mp3DeselectData();
}
```

**移植**: 调用 `VS1003_WriteReg()`, 延时用 `HAL_Delay()`:

```c
void VS1003_SoftReset(void)
{
    VS1003_WriteReg(SPI_MODE,   0x0804);  /* SM_RESET bit 2    */
    HAL_Delay(2);                          /* 至少 1.35ms        */
    while (VS1003_DREQ_Read() == 0);       /* 等待 DREQ 恢复     */

    VS1003_WriteReg(SPI_CLOCKF, 0x9800);  /* CLKI = XTALI*3    */
    VS1003_WriteReg(SPI_AUDATA, 0xBB81);  /* 48kHz, 立体声      */
    VS1003_WriteReg(SPI_BASS,   0x0055);  /* 重低音             */
    VS1003_SetVolume(10, 10);             /* 初始音量           */
    HAL_Delay(2);

    /* 发送 4 字节无效数据, 启动 SPI 发送通道 */
    VS1003_XDCS_Low();
    VS1003_SPI_RW(0); VS1003_SPI_RW(0);
    VS1003_SPI_RW(0); VS1003_SPI_RW(0);
    VS1003_XDCS_High();
}
```

---

### 3.10 硬件复位: `Mp3Reset()` (vs1003.c:274-288)

**参考代码**:
```c
void Mp3Reset(void)
{
    Mp3PutInReset();           // XRESET = 0
    wait(200);                 // 延时 100ms (原注释与实际参数不符)
    SPIPutChar(0xFF);          // 发送 dummy 字节
    Mp3DeselectControl();      // XCS = 1
    Mp3DeselectData();         // XDCS = 1
    Mp3ReleaseFromReset();     // XRESET = 1
    wait(200);                 // 延时 100ms
    while (MP3_DREQ == 0);     // 等待 DREQ 变高
    wait(200);
    Mp3SetVolume(50, 50);
    Mp3SoftReset();
}
```

**移植**:
```c
void VS1003_HardwareReset(void)
{
    VS1003_XRESET_Low();                /* 进入复位状态             */
    HAL_Delay(100);                     /* 保持 100ms               */
    VS1003_SPI_RW(0xFF);                /* 发送 dummy 字节          */
    VS1003_XCS_High();                  /* 释放命令接口             */
    VS1003_XDCS_High();                 /* 释放数据接口             */
    VS1003_XRESET_High();               /* 退出复位                 */
    HAL_Delay(100);
    while (VS1003_DREQ_Read() == 0);    /* 等待芯片就绪             */
    HAL_Delay(100);
    VS1003_SetVolume(50, 50);
    VS1003_SoftReset();
}
```

**注意**: 原参考代码注释 "延时100ms" 但 `wait(200)` 的实际延时取决于 C51 的 `_nop_()`
频率。STM32 使用 `HAL_Delay()` 的毫秒精度, 100ms 足够。

---

### 3.11 音量设置: `Mp3SetVolume` 宏 (vs1003.h:56)

**参考代码**:
```c
#define Mp3SetVolume(leftchannel,rightchannel){Mp3WriteRegister(11,(leftchannel),(rightchannel));}
```

**移植**: 改为函数, 添加参数注释:

```c
/**
 * @brief  设置音量
 * @param  left:  左声道衰减值 (0=最大, 254=静音, 255=不更新)
 * @param  right: 右声道衰减值
 * @note   每个声道独立。0x0000 是最大音量, 0xFEFE 接近静音。
 *         典型值: 0x0000 (max), 0x2020 (中), 0x5050 (低)
 */
void VS1003_SetVolume(uint8_t left, uint8_t right)
{
    VS1003_WriteReg(SPI_VOL, ((uint16_t)left << 8) | right);
}
```

---

### 3.12 正弦测试: `VsSineTest()` (vs1003.c:303-376)

**参考代码**:
```c
void VsSineTest(void)
{
    Mp3PutInReset();
    wait(200);
    SPIPutChar(0xFF);
    Mp3DeselectControl();
    Mp3DeselectData();
    Mp3ReleaseFromReset();
    wait(200);
    Mp3SetVolume(50, 50);
    Mp3WriteRegister(SPI_MODE, 0x08, 0x20);  // 进入测试模式
    while (MP3_DREQ == 0);
    Mp3SelectData();
    // 发送 8 字节正弦测试命令: 0x53 0xEF 0x6E n 0x00 0x00 0x00 0x00
    SPIPutChar(0x53); SPIPutChar(0xEF);
    SPIPutChar(0x6E); SPIPutChar(0x24);  // n=0x24, 频率 ~1kHz
    SPIPutChar(0x00); SPIPutChar(0x00);
    SPIPutChar(0x00); SPIPutChar(0x00);
    wait(500);
    Mp3DeselectData();
    // 退出测试 → 换频率 0x44 → 再测 → 退出
    // ...
}
```

**移植**: 保持流程不变, 替换底层调用:

```c
void VS1003_SineTest(void)
{
    static const uint8_t sine_cmd[] = {0x53, 0xEF, 0x6E, 0x00,
                                        0x00, 0x00, 0x00, 0x00};
    static const uint8_t exit_cmd[] = {0x45, 0x78, 0x69, 0x74,
                                        0x00, 0x00, 0x00, 0x00};

    VS1003_HardwareReset();

    /* 进入测试模式 */
    VS1003_WriteReg(SPI_MODE, 0x0820);          /* SM_TESTS bit 5 */
    while (VS1003_DREQ_Read() == 0);

    /* 频率 1: n=0x24 */
    VS1003_XDCS_Low();
    VS1003_SPI_RW(0x53); VS1003_SPI_RW(0xEF);
    VS1003_SPI_RW(0x6E); VS1003_SPI_RW(0x24);
    VS1003_SPI_RW(0x00); VS1003_SPI_RW(0x00);
    VS1003_SPI_RW(0x00); VS1003_SPI_RW(0x00);
    HAL_Delay(500);
    VS1003_XDCS_High();

    /* 退出测试模式 */
    VS1003_XDCS_Low();
    for (int i = 0; i < 8; i++) VS1003_SPI_RW(exit_cmd[i]);
    HAL_Delay(500);
    VS1003_XDCS_High();

    /* 频率 2: n=0x44 */
    VS1003_XDCS_Low();
    VS1003_SPI_RW(0x53); VS1003_SPI_RW(0xEF);
    VS1003_SPI_RW(0x6E); VS1003_SPI_RW(0x44);
    VS1003_SPI_RW(0x00); VS1003_SPI_RW(0x00);
    VS1003_SPI_RW(0x00); VS1003_SPI_RW(0x00);
    HAL_Delay(500);
    VS1003_XDCS_High();

    /* 再次退出 */
    VS1003_XDCS_Low();
    for (int i = 0; i < 8; i++) VS1003_SPI_RW(exit_cmd[i]);
    HAL_Delay(500);
    VS1003_XDCS_High();
}
```

---

### 3.13 音频数据发送: `VS1003B_WriteDAT()` (vs1003.c:423-435)

**参考代码**:
```c
void VS1003B_WriteDAT(unsigned char dat)
{
    Mp3SelectData();       // XDCS = 0
    SPIPutChar(dat);
    Mp3DeselectData();     // XDCS = 1
    Mp3DeselectControl();  // XCS = 1 (冗余保险)
}
```

**移植**: 每发一个字节就切换一次 XDCS 效率极低。移植为三种模式, 对应不同使用场景:

```c
/* 模式 1: 单字节发送 (非性能关键: 寄存器测试、手动调试) */
void VS1003_SendByte(uint8_t data)
{
    VS1003_XDCS_Low();
    VS1003_SPI_RW(data);
    VS1003_XDCS_High();
    VS1003_XCS_High();  /* 确保命令接口释放 */
}

/* 模式 2: 阻塞批量发送 (简单场景, 不依赖 DMA/中断) */
void VS1003_SendData(const uint8_t *buf, uint16_t len)
{
    VS1003_XDCS_Low();
    while (len--)
    {
        while (VS1003_DREQ_Read() == 0);  /* 等待 FIFO 就绪 */
        VS1003_SPI_RW(*buf++);
    }
    VS1003_XDCS_High();
}

/* 模式 3: DMA 单次触发 (配合 FreeRTOS 双缓冲 + DREQ 中断)
 * ──────────────────────────────────────────────
 * 这是 Dev.md 设计的主播放路径:
 *   DREQ(上升沿) → EXTI10 ISR → xSemaphoreGiveFromISR()
 *   → audioTask 醒来 → 切双缓冲指针 → 调本函数启动 DMA
 *   → DMA 完成中断 → 标记当前缓冲块可用
 *
 * 每次 DMA 固定传输 32 字节 (VS1003 FIFO 大小)。
 * DMA 配置: Memory→Peripheral, Normal 模式, Byte×Byte
 */
void VS1003_StartDMA(const uint8_t *buf)
{
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)buf, 32);
}
```

**架构说明**:

| 场景 | 使用函数 | DREQ 检测方式 | 性能 |
|------|---------|-------------|------|
| SCI 寄存器读写 | `VS1003_SPI_RW()` | 无 | 低 (偶发, 不重要) |
| 正弦测试 | `VS1003_SendByte()` | 无 | 低 (诊断用) |
| 简单阻塞播放 | `VS1003_SendData()` | 轮询 | 中 (CPU 浪费在等待) |
| **正式播放** | `VS1003_StartDMA()` | **EXTI10 中断** | **高 (CPU 零等待)** |

> **注意**: 参考代码 `test_1003_PlayMP3File` 每 32 字节查一次 DREQ 然后循环发送。
> Dev.md 方案更进一步: DREQ 触发中断后, audioTask 只负责切换缓冲指针和启动 DMA,
> 完全不参与逐字节搬运。DMA 发送期间 audioTask 可以并行做其他事。

---

### 3.14 环绕声控制 (vs1003.c:440-474)

**参考代码**:
```c
void VS1003B_SetVirtualSurroundOn(void)
{
    // 读 MODE 寄存器 → 设置 bit 0 (SM_DIFF) → 写回
    // 带重试: 最多 10 次, 写后回读校验
}

void VS1003B_SetVirtualSurroundOff(void)
{
    // 与 On 完全相同的代码 (BUG!)
    // 应清除 bit 0 (SM_DIFF) 而非置位
}
```

**移植**: 修正 bug, 合并为一个带参数函数:

```c
/**
 * @brief  设置环绕声效果开关
 * @param  enable: 1=开启, 0=关闭
 */
void VS1003_SetSurround(uint8_t enable)
{
    uint16_t mode;
    int retry = 0;

    do {
        mode = VS1003_ReadReg(SPI_MODE);
        if (enable)
            mode |= SM_DIFF;       /* 置位: 差分输出 (环绕声) */
        else
            mode &= ~SM_DIFF;      /* 清除: 正常输出           */
        VS1003_WriteReg(SPI_MODE, mode);
        if (VS1003_ReadReg(SPI_MODE) == mode) break;  /* 写后校验 */
    } while (++retry < 10);
}
```

---

### 3.15 重低音增强 (vs1003.c:478-505)

**参考代码**:
```c
void VS1003B_SetBassEnhance(uchar ucValue, uchar ucFrequencyID)
{
    // 读 BASS 寄存器 → 保留高字节 → | value<<4 → & freq_id
    // BUG: 位操作顺序错误, & 和 | 混用导致结果不正确
}
```

**移植**: 修正位操作:

```c
/**
 * @brief  设置重低音增强
 * @param  bass:  低音增强幅度 (0~15, 0=关闭, 15=最大)
 * @param  freq:  截止频率下限 (2~15, 单位 10Hz, 如 10=100Hz)
 * @param  treble: 高音增强幅度 (0~15, 通常也设 0~8)
 * @param  t_freq: 截止频率上限 (0~15, 单位 1kHz)
 *
 * 寄存器位布局 (SPI_BASS = 0x02):
 *   高字节: [7:4]=treble_amp, [3:0]=treble_freq_limit
 *   低字节: [7:4]=bass_amp,   [3:0]=bass_freq_limit
 */
void VS1003_SetBass(uint8_t bass, uint8_t freq, uint8_t treble, uint8_t t_freq)
{
    uint16_t bass_val;
    int retry = 0;

    bass_val  = ((uint16_t)(treble & 0x0F) << 12);
    bass_val |= ((uint16_t)(t_freq  & 0x0F) <<  8);
    bass_val |= ((uint16_t)(bass    & 0x0F) <<  4);
    bass_val |=  (uint16_t)(freq    & 0x0F);

    do {
        VS1003_WriteReg(SPI_BASS, bass_val);
        if (VS1003_ReadReg(SPI_BASS) == bass_val) break;
    } while (++retry < 10);
}
```

---

### 3.16 初始化: `VS1003B_Init()` (vs1003.c:510-550)

**参考代码**:
```c
unsigned char VS1003B_Init()
{
    Mp3Reset();
    // 写 MODE   = 0x0800 (默认模式)
    // 写 CLOCKF = 0x9800 (3 倍频)
    // 写 VOL    = uiVolumeCount
    // 全部带重试
}
```

**移植**: 作为对外的 `BSP_VS1003_Init()` 主入口, 整合 GPIO 初始化 + 硬件复位 + 软件复位:

```c
/**
 * @brief  VS1003 初始化 (BSP 层统一入口)
 * @retval 0=成功, 1=失败
 */
int BSP_VS1003_Init(void)
{
    int retry;

    /* 1. GPIO 初始化 */
    VS1003_GPIO_Init();

    /* 2. 硬件复位 */
    VS1003_HardwareReset();

    /* 3. 校验写 MODE 寄存器 */
    retry = 0;
    while (VS1003_ReadReg(SPI_MODE) != 0x0800)
    {
        VS1003_WriteReg(SPI_MODE, 0x0800);
        if (++retry > 10) return 1;
    }

    /* 4. 校验写 CLOCKF 寄存器 (3倍频) */
    retry = 0;
    while (VS1003_ReadReg(SPI_CLOCKF) != 0x9800)
    {
        VS1003_WriteReg(SPI_CLOCKF, 0x9800);
        if (++retry > 10) return 1;
    }

    /* 5. 设置初始音量 */
    retry = 0;
    while (VS1003_ReadReg(SPI_VOL) != 0x1010)  /* -20dB 作为默认 */
    {
        VS1003_WriteReg(SPI_VOL, 0x1010);
        if (++retry > 10) return 1;
    }

    return 0;
}
```

---

### 3.17 播放 MP3 文件 (vs1003.c:568-588)

**参考代码**:
```c
void test_1003_PlayMP3File()
{
    unsigned int data_pointer = 0;
    unsigned int uiCount = sizeof(MusicData);  // 编译时嵌入的数组
    VS1003B_SoftReset();
    while (uiCount > 0)
    {
        if (CheckVS1003B_DRQ())   // DREQ 为高
        {
            for (i = 0; i < 32; i++)  // 每批 32 字节
            {
                VS1003B_WriteDAT(MusicData[data_pointer++]);
            }
            uiCount -= 32;
        }
    }
    VS1003B_Fill2048Zero();  // 播放完毕填 2048 字节零
}
```

**移植**: 此为测试/演示代码, 不是 BSP 层接口。播放逻辑应放在 App 层的 `player.c` 中:

```c
// App 层 (player.c) — 伪代码示意
void PLAYER_PlayFile(const char *path)
{
    FIL file;
    UINT bytes_read;
    uint8_t buf[512];  /* SD 卡扇区大小的整数倍效率最好 */

    VS1003_SoftReset();

    if (f_open(&file, path, FA_READ) != FR_OK) return;

    /* ── 简单阻塞模式 (不使用 DMA) ── */
    while (f_read(&file, buf, sizeof(buf), &bytes_read) == FR_OK && bytes_read > 0)
    {
        uint16_t pos = 0;
        while (pos < bytes_read)
        {
            if (VS1003_DREQ_Read())  /* DREQ=1 → 可发数据 */
            {
                /* 每批至多 32 字节 */
                uint16_t chunk = (bytes_read - pos > 32) ? 32 : (bytes_read - pos);
                VS1003_XDCS_Low();
                for (uint16_t i = 0; i < chunk; i++)
                    VS1003_SPI_RW(buf[pos + i]);
                VS1003_XDCS_High();
                pos += chunk;
            }
        }
    }
    f_close(&file);

    /* 播放结束, BSP 层接口清空流水线 */
    BSP_VS1003_Flush();
}
```

**注意**: 上面是简化版示范。Dev.md 设计的正式播放路径使用 **DREQ 中断 + DMA**:
```
audioTask (FreeRTOS, 优先级 High):
  loop:
    xSemaphoreTake(DREQ_sem, portMAX_DELAY)  ← 由 EXTI10 ISR 释放
    从双缓冲取当前块 (buf_A 或 buf_B, 32 字节)
    BSP_VS1003_StartDMA(buf_A_current)
    切换缓冲指针 (让 SD 读取线程填另一个缓冲)
```
该逻辑属于 App 层 (`player.c`), BSP 层只提供 `BSP_VS1003_StartDMA()` 硬件接口。

**BSP 层只提供基础设施, 不包含文件系统依赖。**

---

### 3.18 辅助函数: `CheckVS1003B_DRQ()` / `VS1003B_Fill2048Zero()`

**参考代码**:
```c
bool CheckVS1003B_DRQ(void)
{
    return MP3_DREQ;
}

void VS1003B_Fill2048Zero()
{
    // 向 VS1003 发送 2048 字节零
}
```

**移植**:
- `CheckVS1003B_DRQ` → `VS1003_DREQ_Read()` (已作为内联函数在 GPIO 部分实现)
- `VS1003B_Fill2048Zero` → 在 App 层 `player.c` 中实现, 或作为 BSP 工具函数:

```c
/**
 * @brief 发送零数据清空 VS1003 内部解码流水线
 * @note  播放结束后调用, 防止最后一帧音频残留
 */
void BSP_VS1003_Flush(void)
{
    VS1003_XDCS_Low();
    for (int i = 0; i < 2048; i++)
    {
        while (VS1003_DREQ_Read() == 0);
        VS1003_SPI_RW(0x00);
    }
    VS1003_XDCS_High();
}
```

---

## 4. 推荐的 bsp_vs1003.h 公开接口

总结以上分析, 最终对外接口清单:

```c
/* ---- 初始化 & 复位 ---- */
int      BSP_VS1003_Init(void);           /* 总入口: GPIO + HW复位 + 寄存器校验  */
void     BSP_VS1003_SoftReset(void);      /* 软件复位 + 配置默认寄存器            */
void     BSP_VS1003_HardwareReset(void);  /* 硬件复位 (GPIO 序列)                 */

/* ---- 寄存器操作 (SCI, 使用 SPI2) ---- */
void     BSP_VS1003_WriteReg(uint8_t addr, uint16_t data);
uint16_t BSP_VS1003_ReadReg(uint8_t addr);

/* ---- 音频数据 (SDI, 使用 SPI2) ---- */
void     BSP_VS1003_SendByte(uint8_t data);                          /* 单字节发送        */
void     BSP_VS1003_SendData(const uint8_t *buf, uint16_t len);     /* 阻塞批量, 带 DREQ */
void     BSP_VS1003_StartDMA(const uint8_t *buf_32bytes);           /* DMA 单次 32 字节  */

/* ---- 音效控制 ---- */
void     BSP_VS1003_SetVolume(uint8_t left, uint8_t right);          /* 0=最大 ~254=静音  */
void     BSP_VS1003_SetBass(uint8_t bass, uint8_t f_bass,
                            uint8_t treble, uint8_t f_treble);
void     BSP_VS1003_SetSurround(uint8_t enable);                     /* 环绕声开关        */

/* ---- 状态 ---- */
uint8_t  BSP_VS1003_IsReady(void);       /* 检查 DREQ, 同 VS1003_DREQ_Read()   */

/* ---- 工具 ---- */
void     BSP_VS1003_Flush(void);         /* 发送 2048 零字节, 播放结束清流水线  */
void     BSP_VS1003_SineTest(void);      /* 正弦测试 (硬件诊断)                  */
```

---

## 5. 移植顺序建议

| 阶段 | 内容 | 可测试性 |
|------|------|---------|
| **P1** | 引脚宏 + GPIO 初始化 + CubeMX 确认 PB11/PB12/PB10 | 万用表量引脚电平 |
| **P2** | `WriteReg` + `ReadReg` (SCI 层 via SPI2) | 读 MODE 寄存器默认值 = `0x0800` |
| **P3** | 硬件复位 + 软件复位 | 复位后 DREQ 变高 |
| **P4** | `SetVolume` | 耳朵听底噪变化 |
| **P5** | `SineTest` 正弦测试 | 独立验证 — 能听到交替音调则硬件全通 |
| **P6** | `SendData` + `Flush` (阻塞模式) | 播放短 MP3 文件, 验证声音正常 |
| **P7** | CubeMX 配置 DMA1 CH2 (SPI2_TX) + EXTI10 | DMA 完成中断可被触发 |
| **P8** | `StartDMA` + DREQ 中断 + FreeRTOS 双缓冲 | 正式播放路径, 无卡顿 |

---

## 6. 参考代码已知缺陷 (移植时修正)

| 缺陷 | 位置 | 修正方案 |
|------|------|---------|
| `VS1003B_SetVirtualSurroundOff` 与 `On` 代码完全相同 | vs1003.c:440-474 | 清除 SM_DIFF 位而非置位, 合并为一个函数 |
| `VS1003B_SetBassEnhance` 位操作错误 (`&=` 后用 `|=`, 再 `&=`) | vs1003.c:497-500 | 正确构建 16-bit 值后一次写入 |
| `wait(200)` 实际延时不确定 (依赖 `_nop_()` + 编译器优化) | 全局 | 替换为 `HAL_Delay(ms)` 精确毫秒延时 |
| `VS1003B_WriteDAT` 每次单字节切换 XDCS, 效率极低 | vs1003.c:423-435 | 批量发送时保持 XDCS 常低, 只在开始/结束时切换 |
| `SPIPutChar` 每次发送都逐 bit 翻转 GPIO | vs1003.c:147-173 | 使用硬件 SPI 外设替代 |
| `Mp3Reset` 中 `wait(200)` 注释说 100ms, 参数说 200 | vs1003.c:277 | 统一使用 `HAL_Delay(100)` |
