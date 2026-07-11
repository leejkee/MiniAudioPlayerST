# STM32F072C8T6 音频播放器 — 开发文档

---

## 目录

1. [STM32CubeMX 工程生成与配置](#1-stm32cubemx-工程生成与配置)
2. [功能开发步骤](#2-功能开发步骤)
3. [工程化注意事项](#3-工程化注意事项)
4. [附录](#4-附录)

---

## 1. STM32CubeMX 工程生成与配置

### 1.1 创建新工程

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | 打开 STM32CubeMX，点击 **New Project** → **Access to MCU Selector** | |
| 2 | 在 Commercial Part Number 中输入 `STM32F072C8Tx`，双击搜索结果 | 选用 LQFP48 封装 |
| 3 | 保存工程：`File → Save Project As...` → 命名为 `MiniAudioPlayerST.ioc`，路径选择 `firmware/` | 路径不要含中文或空格 |

### 1.2 Pinout 配置（Pinout & Configuration 页面）

#### 1.2.1 系统核心

| 配置项 | 标签页 | 参数 | 值 |
|--------|--------|------|-----|
| SYS | Debug | Serial Wire | **Serial Wire**（必须开启，否则无法二次烧录） |
| RCC | High Speed Clock (HSE) | **Crystal/Ceramic Resonator** | 外部 8MHz 无源晶振（常见两脚石英晶振 + 负载电容），启用 PF0/OSC_IN、PF1/OSC_OUT |
| RCC | Low Speed Clock (LSE) | | Disable（本工程无需 RTC） |

> ⚠️ **Crystal vs Bypass 区别**：
> - **Crystal/Ceramic Resonator**：无源晶振（两脚 8MHz 石英晶振 + 两个负载电容），OSC_IN 和 OSC_OUT 各接晶振一脚，利用 MCU 内部 Pierce 振荡电路起振。适合绝大多数 DIY/开发板。
> - **Bypass Clock Source**：有源晶振（外部时钟模块输出方波信号），仅接 OSC_IN，OSC_OUT 悬空，内部振荡电路被旁路。适合需要高精度时钟的场景。

#### 1.2.2 GPIO 输入 — 按键

| 标签名 | 引脚 | 模式 | GPIO 配置 |
|--------|------|------|-----------|
| KEY_MENU | **PA9**  | GPIO_Input | Pull-up |
| KEY_OK   | **PA10**  | GPIO_Input | Pull-up |
| KEY_L    | **PA11** | GPIO_Input | Pull-up |
| KEY_R    | **PA12** | GPIO_Input | Pull-up |

> **按键方案说明**：PRD 要求硬件消抖（由板级 RC 电路实现），默认上拉，按下接 GND。软件侧由 FreeRTOS 按键扫描任务 20ms 轮询去抖，不依赖外部中断。

#### 1.2.3 SPI1 — SD 卡（CH376S 模块）

| 标签名 | 引脚 | 模式 |
|--------|------|------|
| SPI1_SCK  | **PA5** | Alternate Function Push-Pull |
| SPI1_MISO | **PA6** | Alternate Function Push-Pull |
| SPI1_MOSI | **PA7** | Alternate Function Push-Pull |
| SPI1_CS   | **PA4** | GPIO_Output（软件控制 CS） |

**SPI1 参数配置：**

| 参数 | 值 | 说明 |
|------|-----|------|
| Mode | Full-Duplex Master | |
| Hardware NSS | **Disable** | 软件控制 CS |
| Frame Format | Motorola | |
| Data Size | 8 Bits | |
| Prescaler | **128 (375 Kbps)** | 初始化低速 < 400kHz，初始化完成后动态切高速 |
| CPOL | Low | |
| CPHA | 1 Edge | |
| CRC Calculation | Disable | |

> **注意**：SD 卡 SPI 初始化阶段必须低速（< 400kHz），初始化成功后可在代码中动态调整预分频至 4 或 2（最高 ~12Mbps）。Cortex-M0 的 SPI1 挂在 APB2（配合 F072 实际总线是 APB1@48MHz 仍有余量），最大时钟 24MHz。

#### 1.2.4 SPI2 — VS1003 音频解码模块

| 标签名 | 引脚 | 模式 |
|--------|------|------|
| SPI2_SCK  | **PB13** | Alternate Function Push-Pull |
| SPI2_MISO | **PB14** | Alternate Function Push-Pull |
| SPI2_MOSI | **PB15** | Alternate Function Push-Pull |
| XCS       | **PB11** | GPIO_Output（命令片选） |
| XDCS      | **PB12** | GPIO_Output（数据片选） |
| DREQ      | **PB10** | GPIO_Input，上拉，外部中断 **上升沿触发** |

**SPI2 参数配置：**

| 参数 | 值 | 说明 |
|------|-----|------|
| Mode | Full-Duplex Master | |
| Hardware NSS | Disable | 两个软件 CS（XCS / XDCS），软件中分别控制 |
| Frame Format | Motorola | |
| Data Size | 8 Bits | |
| Prescaler | **64 (750 Kbps)** | 初始化 1MHz 以下，运行中可动态切换到 ~6MHz |
| CPOL | Low | VS1003 要求 SPI 模式 0（CPOL=0, CPHA=0） |
| CPHA | 1 Edge | |
| CRC Calculation | Disable | |

> **重点**：VS1003 有两根片选 — **XCS（命令）** 与 **XDCS（数据）**，务必在软件中分别管理。DREQ 配置为 GPIO 上升沿外部中断，对应 PRD 第 3.2 节中的流控方案。

#### 1.2.5 I2C1 — OLED（MAP2606 / SSD1306）

| 标签名 | 引脚 | 模式 |
|--------|------|------|
| I2C1_SCL | **PB6** | Alternate Function Open-Drain |
| I2C1_SDA | **PB7** | Alternate Function Open-Drain |

**I2C1 参数配置：**

> I2C Mode配置：可选Disable(禁用)、I2C(通用双线模式)或SMBus(带超时机制/硬件报警的系统管理总线模式)。

| 参数 | 值 |
|------|-----|
| I2C Speed Mode | **Fast Mode** |
| Clock Speed | 400 000 Hz |
| I2C Address | `0x3C << 1 = 0x78`（SSD1306 7 位地址 0x3C） |

> **注意**：部分 SSD1306 模块 SA0 引脚拉高时地址为 `0x3D`，需根据实际模块焊盘确认。

### 1.3 Middleware 配置

#### 1.3.1 FreeRTOS

在 **Middleware → FREERTOS** 中选择 **CMSIS_V1**（或 CMSIS_V2，视 CubeMX 版本）：

| 参数 | 值 | 说明 |
|------|-----|------|
| USE_PREEMPTION | Enabled | 抢占式调度 |
| TICK_RATE_HZ | 1000 | 1ms tick |
| MAX_PRIORITIES | 8 | |
| MINIMAL_STACK_SIZE | 128 words | |
| TOTAL_HEAP_SIZE | **4096**（4KB） | PRD RAM 预算中 FreeRTOS 开销对应 4KB |
| USE_MUTEXES | Enabled | FATFS 重入需要 |
| USE_COUNTING_SEMAPHORES | Enabled | DREQ 流控 |
| USE_TASK_NOTIFICATIONS | Enabled | 低开销任务间通信 |

> ⚠️ Cortex-M0 没有 `CLZ` 等指令，FreeRTOS 的 port 层会自动适配。CubeMX 生成后确认 PendSV / SysTick 中断配置正确。

#### 1.3.2 FATFS

在 **Middleware → FATFS** 中选择 **User-defined** 模式：

| 参数 | 值 | 说明 |
|------|-----|------|
| Mode | **User-defined** | 手动适配 SPI SD |
| _VOLUMES | 1 | 仅一个卷（SD 卡） |
| _MAX_SS | 512 | 扇区大小 |
| _MIN_SS | 512 | |
| _USE_LFN | **1（启用 LFN 静态缓冲区）** | 支持长文件名 |
| _MAX_LFN | 64 | 最长文件名（按 PRD 约束） |
| _CODE_PAGE | **936（简体中文 GBK）** | 中文文件名支持 |
| _USE_ERASE | 0 | 不需要 |
| _FS_LOCK | 1 | 文件锁定 |
| _FS_REENTRANT | **1（启用）** | FreeRTOS 下需要重入保护 |
| _FS_TIMEOUT | 1000 | 信号量超时 ms |
| _SYNC_t | osSemaphoreId | FreeRTOS 互斥信号量句柄 |

> CubeMX 生成 `user_diskio.c`，在其中实现 SPI 读写 SD 卡的实际函数：`disk_initialize()`、`disk_read()`、`disk_write()`、`disk_status()`、`disk_ioctl()`。

### 1.4 FreeRTOS 任务

在 **Tasks and Queues** 选项卡中创建以下任务：

| Task Name | Priority (osPriority) | Stack Size (words) | Entry Function | 说明 |
|-----------|----------------------|---------------------|----------------|------|
| audioTask | osPriority**High** | 256 (1024B) | StartAudioTask | DREQ 信号量等待 + SD 读取 + 填充缓冲 + 启动 DMA |
| uiTask    | osPriority**Normal** | 128 (512B) | StartUITask | 仅在状态变化时刷新 OLED |
| keyTask   | osPriority**Normal** | 64 (256B) | StartKeyTask | 20ms 轮询 GPIO，按下沿发事件 |
| mainTask  | osPriority**Normal** | 128 (512B) | StartMainTask | 主状态机，协调各模块 |

> CMSIS V1 中优先级数值 **越大优先级越高**。audioTask 为最高，防止音频 underrun。

### 1.5 DMA 配置

| DMA 通道 | 外设 | 方向 | 模式 | 数据宽度 |
|----------|------|------|------|---------|
| DMA1 Channel 2 | SPI2_TX | Memory to Peripheral | **Normal**（单次，每次手动启动） | Byte × Byte |
| DMA1 Channel 3 | SPI1_RX | Peripheral to Memory | Normal | Byte × Byte |

> **关键**：SPI2 TX 的 DMA 配置为 **Normal 模式**，不是 Circular。每次 DREQ 外部中断触发后，由任务手动启动一次 32 字节 DMA 传输，与 PRD 9.5 节方案一致。

### 1.6 NVIC 中断优先级

Cortex-M0 仅支持 2 位抢占优先级（即 0~3）。STM32F0 默认使用 NVIC_PRIORITYGROUP_2（2 抢占 + 2 子）。

| 中断 | 抢占优先级 | 子优先级 | 说明 |
|------|-----------|---------|------|
| EXTI10 (DREQ) | **0** | 0 | 最高优先级 — 音频流控 |
| DMA1 Channel 2 (SPI2 TX 完成) | 1 | 0 | 音频数据 DMA 完成 |
| SPI1 / SPI2 IRQ | 2 | 0 | SPI 通信中断 |
| EXTI8..11 (按键) | 3 | 0 | 按键中断 |
| I2C1 Event | 4 | 0 | I2C 事件 |
| SysTick | 5 | 0 | FreeRTOS 心跳 |

### 1.7 时钟树配置

```
HSE (外部 8MHz 晶振)
  │
  └── /1 ──→ PLL (×6) ──→ PLLCLK = 48MHz
                              │
                              └── SYSCLK = 48MHz
                                    ├── HCLK = 48MHz (AHB)
                                    ├── APB1 (PCLK1) = 48MHz
                                    │     ├── SPI2
                                    │     ├── I2C1
                                    │     └── TIM2/3 等
                                    └── APB2 (PCLK2) = 48MHz (若有)
```

**配置步骤**：

1. Clock Configuration 页 → HSE 栏输入 **8MHz**
2. PLL Source Mux 选择 **HSE**
3. PREDIV 设为 **/1**，PLLMUL 设为 **×6**（8 × 6 = 48MHz）
4. System Clock Mux 选择 **PLLCLK**
5. 确认 HCLK / APB1 / APB2 均不分频（即 = 48MHz）
6. 点击 **Resolve Clock Issues** 确认无报警

> **说明**：选用外部 8MHz 晶振精度比内部 RC 高，音频场景下更稳定。若板上只有一个 8MHz 无源晶振且未接负载电容，需确认起振正常后再锁定此时钟方案。

### 1.8 项目生成设置

**Project Manager → Project**：

| 配置项 | 值 |
|--------|-----|
| Project Name | `MiniAudioPlayerST` |
| Project Location | `firmware/`（仓库根目录下的 firmware 文件夹） |
| Application Structure | **Advanced**（分开 Core 和 Drivers 目录） |
| Toolchain / IDE | **MDK-ARM**（Keil），或 **STM32CubeIDE** |
| Minimum Heap Size | `0x200` (512B) |
| Minimum Stack Size | `0x400` (1KB) |

**Code Generator**：

| 配置项 | 值 |
|--------|-----|
| Copy all used libraries into the project folder | ✅ 勾选 |
| Generate peripheral initialization as a pair of `.c/.h` files | ✅ 勾选 |
| **Keep User Code when re-generating** | ✅ 勾选（**最关键**） |
| Delete previously generated files when not re-generated | ❌ 不勾选 |
| Enable Full Assert | ✅ 开发阶段勾选 |

### 1.9 生成代码

点击 **GENERATE CODE** → 等待完成 → 在 IDE 中打开工程。

---

## 2. 功能开发步骤

按 PRD 第 11 节的五阶段规划展开。

### 阶段 1：硬件驱动验证

**目标**：逐个确认硬件焊接和通信链路正确。

#### 1.1 GPIO 按键验证

- 4 个 GPIO 输入引脚上拉，按下接 GND。
- 在 keyTask 中 20ms 周期轮询 GPIO 电平（硬件 RC 消抖，软件 20ms 间隔天然跳过抖动窗口）。
- 验证方式：按下按键 → OLED 显示对应字符 / 串口 printf。

#### 1.2 OLED 点亮验证

- 移植 SSD1306 I2C 驱动。
- 实现画点/画线/写字符基础函数。
- 显示 "Hello World" 全屏。
- 确认 I2C 地址正确（0x3C 或 0x3D）。
- 验证局部刷新能力（非全屏刷新，减少 I2C 通信量）。

**参考文件**：`firmware/App/ui/ssd1306.h`、`ssd1306.c`

#### 1.3 SD 卡读写验证

- 实现 SPI1 操作 SD 卡的基础函数（`sd_spi_write()` / `sd_spi_read()`）。
- 实现 `disk_initialize()` → 上电发送 CMD0 → CMD8 → ACMD41 初始化序列。
- 实现 `disk_read()` / `disk_write()` → 能读写单个扇区。
- 挂载 FATFS → `f_mount()` 返回 FR_OK。
- 创建/读取一个测试文件。

**参考文件**：`firmware/App/fs/sdcard_spi.c`、`firmware/Middleware/Third_Party/FatFs/src/user_diskio.c`

#### 1.4 VS1003 发声验证

- 实现 VS1003 SPI 命令接口（`vs1003_write_reg()` / `vs1003_read_reg()`）。
- 初始化序列：硬件复位 → 软件复位 → 设置音量/时钟/低音增强等 SCI 寄存器。
- **正弦波测试**：SCI_MODE 中置位 TEST bit，写入频率字 → 应发出单音。
- 再通过 SPI 发送一段 MCU Flash 中预存的简短 MP3 数据 → 确认解码正常。

**参考文件**：`firmware/App/audio/vs1003.c`、`vs1003.h`

### 阶段 2：基础播放链路

**目标**：SD 卡 → FatFS → VS1003 完整播放第一首 MP3。

#### 2.1 文件扫描模块

- 上电后扫描 SD 卡根目录 `/`。
- 筛选后缀 `.mp3`（大小写不敏感）。
- 文件名去后缀存储到数组 `file_list[]`，每项 ≤ 64 字符。
- 限制最大 20 个文件（PRD 第 9.4 节）。

```c
uint8_t fs_scan_mp3_files(const char *path);
// 返回找到的文件数，0 表示无音乐文件
```

#### 2.2 DREQ 流控 + DMA 双缓冲

- 配置 DREQ 外部中断（上升沿触发）。
- 双缓冲结构：
  ```c
  uint8_t audio_buf[2][32];   // 两个 32 字节缓冲区
  volatile uint8_t active_buf; // 当前 DMA 正在发送的缓冲区索引
  volatile uint8_t fill_buf;   // 需要填充的缓冲区索引
  ```

- DREQ ISR 流程：
  1. 清中断标志
  2. `xSemaphoreGiveFromISR(dreq_sem, &xHigherPriorityTaskWoken);`
  3. `portYIELD_FROM_ISR(xHigherPriorityTaskWoken);`

- audioTask 流程（PRD 9.5 节推荐方案）：
  1. 等待 DREQ 信号量（`xSemaphoreTake()`）
  2. 找到空闲 buffer → `f_read()` 读取 32 字节
  3. 启动 DMA 发送 32 字节到 VS1003
  4. 若 `f_read` 返回 0 → 标记播放结束

#### 2.3 打通完整链路

- `f_open()` → 循环 `f_read()` 32 字节 → 等 DREQ → 发 SPI2 DMA → 直到文件结束。
- 验证方式：SD 卡存一首示例 MP3，上电后自动播放 5 秒，音频正常输出。

### 阶段 3：UI 框架

**目标**：屏幕渲染 + 按键交互正确，暂不接入播放控制。

#### 3.1 字库制作

Flash 仅 64KB，按 PRD 5.4 节 **仅提取实际使用的字符点阵**：

- 中文字符集 = 固定 UI 文字（"菜单"、"音量"、"播放模式"、"音量调节"、播放模式名等） + 所有歌曲文件名中的中文。
- 英文字符集 = 常用 ASCII（0x20–0x7E），8×16 像素。
- 制作工具：PCtoLCD2002 或在线点阵生成工具。
- 格式：
  ```c
  // 中文 16×16，每字 32 字节
  typedef struct {
      uint16_t code;         // GBK 编码
      uint8_t  bitmap[32];   // 16×16 点阵
  } font16_t;
  ```
- 声明为 `const` 放入 Flash，不占 RAM。
- 查找函数：`const uint8_t* font_find_16(uint16_t gbk_code)` → 返回 bitmap 指针。

#### 3.2 OLED UI 渲染

**主界面（PRD 5.1 节）**：

```
┌──────────────────────────────────┐
│ 第 1 行：Logo  │ 音量: 70% │ 模式图标 │
│ 第 2 行：歌曲名（超 8 汉字则滚动）     │
│ 第 3 行：上一曲 │ 播放/暂停 │ 下一曲    │
│ 第 4 行：[01:23]/[04:56]            │
└──────────────────────────────────┘
```

**菜单界面（PRD 5.2 节）**：

```
┌─────────────────────┐
│ 第 1 行：Menu 标题    │
│ 第 2 行：> 音量调节   │
│ 第 3 行：  播放模式   │
│ 第 4 行：  文件列表   │
└─────────────────────┘
```

- 歌曲名滚动：超 8 汉字（16 英文字符）时，定时器每 500ms 左移 1 列（像素级滚动），到末尾后停顿 2s 回弹。
- **仅在数据变化时刷新屏**（PRD 8.1 节要求），减少 I2C 通信量。

#### 3.3 按键扫描任务

- 20ms 周期轮询。
- 区分短按（< 1s）与长按（≥ 1s）。
- 长按连发：R/L 键在音量调节界面按下 1s 后，每 100ms 重复发射一次事件。
- 按键事件通过 FreeRTOS 消息队列发给主状态机任务：

  ```c
  typedef enum { KEY_MENU, KEY_OK, KEY_L, KEY_R } key_id_t;

  typedef struct {
      key_id_t id;
      uint8_t  is_long_press;
  } key_event_t;
  ```

### 阶段 4：完整功能集成

**目标**：所有 PRD 功能跑通。

#### 4.1 主状态机

实现 PRD 第 10 节状态机：

```c
typedef enum {
    STATE_INIT,
    STATE_MAIN,
    STATE_MENU,
    STATE_VOLUME_ADJ,
    STATE_PLAY_MODE,
    STATE_ERROR
} system_state_t;

typedef enum { PLAY_STOP, PLAY_PLAYING, PLAY_PAUSED } play_state_t;

typedef enum {
    MODE_SEQUENTIAL,   // 顺序播放
    MODE_LOOP_ALL,     // 列表循环
    MODE_LOOP_ONE,     // 单曲循环
    MODE_SHUFFLE       // 随机播放
} play_mode_t;
```

**导航栈**（PRD 10.3 节）：

```c
#define NAV_STACK_DEPTH_MAX 4
typedef struct {
    system_state_t stack[NAV_STACK_DEPTH_MAX];
    int8_t sp;
} nav_stack_t;

// Menu 短按 → PopNavigationStack()
// Menu 长按 → ClearNavigationStack()
```

#### 4.2 播放控制

| 函数 | 说明 |
|------|------|
| `player_play(uint8_t index)` | 打开指定文件，开启 DREQ 流控循环 |
| `player_pause()` | 停止 DMA，记录当前文件偏移 |
| `player_resume()` | 从当前位置继续 |
| `player_next()` | 根据播放模式计算下一首索引 |
| `player_prev()` | 上一首 |
| `player_stop()` | 停止并关闭文件 |

#### 4.3 音量控制

- VS1003 SCI_VOL 寄存器（地址 `0x0B`）：
  ```c
  // vol_percent ∈ {0, 10, 20, ..., 100}
  // 音量值 = (100 - vol_percent) * 0xFE / 100
  // 范围: 0x00 (最大声) ~ 0xFE (最轻)
  // 静音: 0xFEFE (左右声道均为 0xFE)
  ```
- 仅在变化时写入一次 VS1003。

#### 4.4 播放模式切换

R 键正向循环 / L 键反向循环（PRD 10.4 节）：

```
顺序播放 → 列表循环 → 单曲循环 → 随机播放 → 顺序播放
```

每次切换更新 OLED 第 1 行的模式图标。

#### 4.5 播放时间显示

- 通过 `f_tell()` 计算已读取字节数粗估：
  ```
  elapsed_sec ≈ bytes_played / (bitrate / 8)
  ```
- 或解析 VS1003 的 SCI_HDAT0/SCI_HDAT1 获取帧头信息。
- 1 秒周期定时器更新 OLED 第 4 行 `[mm:ss]/[MM:SS]`。

### 阶段 5：异常处理与打磨

| 异常项 | 检测方式 | 用户可见行为 | 恢复方式 |
|--------|---------|-------------|---------|
| SD 卡未插入 | 初始化时 retry 3 次失败 | "No SD Card" | 5s 定时重试 |
| SD 卡无 .mp3 | `file_count == 0` | "No Music Files" | 等待插拔 SD 卡 |
| VS1003 初始化失败 | `SCI_STATUS` 读回异常 | "Decoder Error" | 自动重试 3 次 |
| 播放中拔卡 | `f_read()` 返回错误 | "SD Removed"，停止播放 | 插卡后重新挂载 |
| FS 挂载失败 | `f_mount()` ≠ FR_OK | "FS Error" | 自动重试 |

---

## 3. 工程化注意事项

### 3.1 内存预算

Flash 64KB / RAM 16KB，严格遵守：

| 约束 | 值 |
|------|-----|
| 文件列表 | 20 首 × 64 字节 = 1280B（约 1.25KB RAM） |
| 双缓冲 | 2 × 32 = 64B（可忽略） |
| OLED 显存 | 128×64/8 = 1024B（1KB RAM） |
| FreeRTOS 堆 | 4KB（CubeMX 中 `TOTAL_HEAP_SIZE = 4096`） |
| 字库 | 全部 `const`，放 Flash，不占 RAM |

- `printf` 会显著增加 Flash 占用（约 5~10KB），开发阶段用于调试，发布版本关闭或替换为轻量实现。
- 在 `startup_stm32f072c8tx.s` 中检查：
  ```asm
  Stack_Size      EQU     0x00000400   ; 1KB
  Heap_Size       EQU     0x00000200   ; 512B
  ```

### 3.2 CubeMX 重新生成纪律

- **所有用户代码**必须写在 `/* USER CODE BEGIN XXX */` … `/* USER CODE END XXX */` 注释块内。
- 不要在 CubeMX 生成的文件中在注释块外添加 `#include` 或变量声明。
- App 层代码放在独立文件夹 `firmware/App/` 中，CubeMX 不会删除此目录。
- 每次 CubeMX 重新生成前建议 `git stash` → 生成后再 `git stash pop`，确保不丢失用户代码。

### 3.3 SPI 片选管理

- SD 卡（SPI1）与 VS1003（SPI2）使用两组独立 SPI，互不冲突。
- VS1003 内部有 **XCS（命令 CS）** 与 **XDCS（数据 CS）**，必须分别控制，两者不可同时为低。
- 切换 SPI 设备前，先拉高当前 CS，再拉低目标 CS。

### 3.4 DREQ 中断安全

**ISR 中只做三件事**：

1. 清中断标志
2. `xSemaphoreGiveFromISR()` 给 audioTask
3. 需要时 `portYIELD_FROM_ISR()`

**ISR 中绝不调用**：`f_read()`、`HAL_SPI_Transmit_DMA()`、任何可能阻塞的函数。这些必须在 audioTask 中执行。

### 3.5 音频缓冲区管理

```
缓冲区状态机:
[EMPTY] --f_read()--> [FULL] --DMA start--> [SENDING] --DMA TC--> [EMPTY]
```

- `audio_buf[0]` 和 `audio_buf[1]` 各自维护状态标记。
- DMA 传输完成中断中：将 buffer 标记为 EMPTY → 通知 audioTask。
- audioTask 伪代码：

  ```c
  while (1) {
      xSemaphoreTake(dreq_sem, portMAX_DELAY);
      buf = get_empty_buffer();
      if (buf) {
          f_read(&file, buf->data, 32, &br);
          if (br == 0) { /* 播完 */ break; }
          HAL_SPI_Transmit_DMA(&hspi2, buf->data, 32);
      } else {
          underrun_count++;
      }
  }
  ```

### 3.6 文件名编码与字库

- SD 卡文件名为 GBK 编码 → FatFS `f_readdir` 返回的 `fname` 直接是 GBK。
- 显示时，用 GBK 编码查字库 → 获得 bitmap → 画到 OLED。
- **字库生成流程**：
  1. 统计所有歌曲文件名 + UI 固定文字中的中文
  2. 去重得到字符集合
  3. 用 PCtoLCD2002 生成 16×16 点阵
  4. 存为 C `const` 数组，放入 Flash
- 未收录字符显示占位符 "□"。

### 3.7 `.gitignore` 建议

```gitignore
# Keil IDE
*.uvguix.*
*.scvd
*.uvoptx
*.dbgconf
Objects/
Listings/
DebugConfig/

# CubeMX
*.mxproject

# 编译产物
*.o
*.d
*.crf

# 临时文件
*.bak
*.tmp
```

- `.ioc` 文件务必提交 git。
- `Drivers/`、`Middleware/` 完整提交。

### 3.8 调试建议

1. **串口日志**：选用一个空闲 USART（如 USART2 PA2/PA3 或 PA14/PA15 的 SWCLK/SWDIO 之外引脚）输出日志，或使用 SWO（ITM）输出。
2. **分段验证**：每写完一个驱动，单独烧录测试，不要一次性整合后调试。
3. **断言**：开发阶段开启 `configASSERT()`，捕获空指针/参数越界。
4. **栈水印**：每个任务运行时调用 `uxTaskGetStackHighWaterMark()`，确定合适栈大小后压至安全值 + 20%。

---

## 4. 附录

### 4.1 外设引脚分配总览

| 外设 | 信号 | 引脚 | 备注 |
|------|------|------|------|
| SPI1 (SD) | SCK | PA5 | |
| SPI1 (SD) | MISO | PA6 | |
| SPI1 (SD) | MOSI | PA7 | |
| SPI1 (SD) | CS | PA4 | 软件控制 |
| SPI2 (VS1003) | SCK | PB13 | |
| SPI2 (VS1003) | MISO | PB14 | |
| SPI2 (VS1003) | MOSI | PB15 | |
| SPI2 (VS1003) | XCS | PB11 | 命令片选 |
| SPI2 (VS1003) | XDCS | PB12 | 数据片选 |
| GPIO (VS1003) | DREQ | PB10 | 上升沿 EXTI |
| I2C1 (OLED) | SCL | PB6 | |
| I2C1 (OLED) | SDA | PB7 | |
| GPIO (KEY) | MENU | PA8 | 上拉 |
| GPIO (KEY) | OK | PA9 | 上拉 |
| GPIO (KEY) | L | PA10 | 上拉 |
| GPIO (KEY) | R | PA11 | 上拉 |

### 4.2 推荐目录结构（CubeMX 生成后手工补充）

```
firmware/
├── MiniAudioPlayerST.ioc          # CubeMX 项目文件（纳入版本控制）
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── stm32f0xx_it.h
│   │   └── stm32f0xx_hal_conf.h
│   ├── Src/
│   │   ├── main.c                 # 在 USER CODE 区域添加初始化调用
│   │   ├── stm32f0xx_it.c         # 中断服务程序
│   │   └── freertos.c             # FreeRTOS 任务入口函数
│   └── Startup/
│       └── startup_stm32f072c8tx.s
├── Drivers/
│   ├── CMSIS/
│   └── STM32F0xx_HAL_Driver/
├── Middleware/
│   ├── Third_Party/
│   │   └── FatFs/                 # FatFS 源码
│   └── FreeRTOS/
└── App/                           # 【手工创建 — 应用层代码】
    ├── audio/
    │   ├── vs1003.c
    │   ├── vs1003.h
    │   ├── audio_player.c
    │   └── audio_player.h
    ├── ui/
    │   ├── ssd1306.c
    │   ├── ssd1306.h
    │   ├── ui_render.c
    │   ├── ui_render.h
    │   ├── font_cn.c              # 中文 16×16 字库（仅所需字符）
    │   ├── font_en.c              # 英文 8×16 字库
    │   └── font.h
    └── fs/
        ├── sdcard_spi.c
        ├── sdcard_spi.h
        ├── file_scanner.c
        └── file_scanner.h
```

### 4.3 关键寄存器速查

| 芯片 | 寄存器 | 地址 | 说明 |
|------|--------|------|------|
| VS1003 | SCI_MODE | 0x00 | 模式控制（软件复位 0x0804） |
| VS1003 | SCI_STATUS | 0x01 | 状态 |
| VS1003 | SCI_BASS | 0x02 | 低音增强 |
| VS1003 | SCI_CLOCKF | 0x03 | 时钟频率 + 倍频 |
| VS1003 | SCI_VOL | 0x0B | 音量（左声道:右声道） |
| SSD1306 | DEV_ADDR | 0x3C | I2C 7 位地址 |
| SSD1306 | CMD_DISPLAY_ON | 0xAF | 显示开 |

### 4.4 参考资料

- STM32F072C8T6 数据手册: https://www.st.com/resource/en/datasheet/stm32f072c8.pdf
- VS1003 数据手册: https://www.vlsi.fi/fileadmin/datasheets/vs1003.pdf
- FatFS: http://elm-chan.org/fsw/ff/
- SSD1306 数据手册: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
- FreeRTOS 官方文档: https://www.freertos.org/Documentation/
- STM32CubeMX 用户手册: https://www.st.com/resource/en/user_manual/um1718.pdf

### 4.5 版本历史

| 版本 | 日期 | 修改内容 | 作者 |
|------|------|---------|------|
| v0.1 | 2026-07-01 | 初稿（对应 PRD v0.1） | leejkee |
