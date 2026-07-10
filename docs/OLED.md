# OLED I2C 驱动开发文档

---

## 目录

1. [需求溯源 -- OLED 要显示什么](#1-需求溯源----oled-要显示什么)
2. [架构设计 -- 如何满足需求](#2-架构设计----如何满足需求)
3. [BSP 层 -- I2C 通信封装](#3-bsp-层----i2c-通信封装)
4. [SSD1315 控制器驱动 -- 显存模型与绘图](#4-ssd1315-控制器驱动----显存模型与绘图)
5. [UI 渲染框架 -- 接口定义与实现规划](#5-ui-渲染框架----接口定义与实现规划)
   - 5.1 [整体数据流](#51-整体数据流)
   - 5.2 [数据结构定义](#52-数据结构定义) (`ui_screen_t`, `ui_event_t`, `player_context_t`)
   - 5.3 [四区域布局接口](#53-四区域布局接口) (Zone 坐标常量 + 全屏/单区域渲染函数)
   - 5.4 [uiTask 事件分发](#54-uitask-事件分发)
   - 5.5 [界面切换映射表](#55-界面切换映射表) (PRD 状态转换 → ui_event_t → Render_*)
   - 5.6 [导航栈接口](#56-导航栈接口)
   - 5.7 [队列契约](#57-uitask-与-maintask-的队列契约)
   - 5.8 [文件创建清单](#58-文件创建清单)
   - 5.9 [开发顺序](#59-开发顺序)
6. [运行时流程 -- 从按键到像素](#6-运行时流程----从按键到像素)
   - 6.1 [场景 A: 切歌 (局部刷新)](#61-场景-a-用户按-r-键切歌-全屏刷新)
   - 6.2 [场景 B: 时间更新 (单区域)](#62-场景-b-每秒时间戳更新-单区域刷新)
   - 6.3 [场景 C: 菜单进入与返回](#63-场景-c-menu-短按--菜单--返回主界面-两次全屏刷新)
   - 6.4 [三种场景性能对比](#64-三种场景的性能对比)
7. [开发与调试](#7-开发与调试)
8. [附录](#8-附录)

---

## 1. 需求溯源 -- OLED 要显示什么

本章从 PRD 中提取与 OLED 直接相关的功能需求, 作为后续架构设计和代码实现的输入。

### 1.1 硬件指标 (PRD 3.3)

| 项目 | 内容 | 对驱动的约束 |
|------|------|-------------|
| 模块型号 | MAP2606 (SSD1315 控制器) | 命令集兼容 SSD1306 |
| 分辨率 | 128 x 64 像素 | 显存 1024 字节 |
| 颜色 | 单色 (白) | 1 bit/像素, 无需灰度 |
| 接口 | I2C, 7-bit 地址 0x3C | 需封装 I2C 命令/数据两种写模式 |
| 可显示行数 | 4 行 (基于 16px 字体) | 每行占用 2 个 PAGE (16 像素 = 2 页) |
| 每行最多字符 | 16 个 (基于 8px 字体) | 128 列 / 8px = 16 字符/行 |

### 1.2 界面布局需求 (PRD 5.1 / 5.2)

本项目有两种界面, 每种界面的布局需求直接决定渲染函数的参数和绘制顺序:

**主界面** -- 播放状态下的默认屏幕:

```
┌──────────────────────────────────┐
│ 第 1 行: Logo  │ 音量: 70% │ 模式图标 │  ← 16px 行高, 混合中文/数字/图标
│ 第 2 行: 歌曲名称 (超过 8 汉字则滚动)    │  ← 16px 行高, 需支持像素级滚动
│ 第 3 行: 上一曲  │  播放/暂停  │  下一曲  │  ← 16px 行高, 按键图标
│ 第 4 行: [01:23]/[04:56]              │  ← 16px 行高, 纯数字+符号
└──────────────────────────────────┘
```

**菜单界面** -- Menu 键短按进入:

```
┌─────────────────────┐
│ 第 1 行: Menu 标题    │
│ 第 2 行: > 音量调节   │  ← ">" 表示当前选中
│ 第 3 行:   播放模式   │
│ 第 4 行:   文件列表   │
└─────────────────────┘
```

**从布局推导出的渲染需求**:

| 需求编号 | 需求描述 | 实现要点 |
|----------|---------|---------|
| UI-01 | 16px 行高对齐到 PAGE 边界 | 每行对应 2 个 PAGE, 行 1 = PAGE0+1, 行 2 = PAGE2+3, ... |
| UI-02 | 12px/16px 混合字号 | 中文 16x16, 英文/数字 8x16, 需两套字库 |
| UI-03 | 超长歌曲名滚动 | 像素级水平移位, 500ms 间隔, 到尾端停顿 2s 回弹 |
| UI-04 | ">" 光标指示 | ASCII 字符 0x3E, 8x16 字库已包含 |
| UI-05 | 音量百分比 0%~100% | 步进 10%, 数字绘制 (0, 10, 20, ..., 100) |
| UI-06 | 播放时间 [mm:ss]/[MM:SS] | 8x16 数字 + ASCII 符号 `[` `]` `:` `/` |

### 1.3 交互与性能需求

| 需求编号 | 需求描述 | 来源 | 对驱动的约束 |
|----------|---------|------|-------------|
| PERF-01 | **仅在数据变化时刷新屏幕** | PRD 8.1 | 必须支持局部刷新, 不能全屏重绘 |
| PERF-02 | 按键响应延迟 < 20ms | PRD 8.1 | 单次刷新耗时需 < 5ms, 为状态机留余量 |
| PERF-03 | 冷启动到界面可交互 < 10s | PRD 8.1 | 初始化序列需在 50ms 内完成 |
| PERF-04 | 屏幕刷新时画面稳定, 无闪烁 | PRD 8.1 | 先绘到镜像显存再批量同步, 不能逐像素发 I2C |
| ERR-01 | SD 卡未插入 → 显示 "No SD Card" | PRD 7 | 需 `Render_Error()` 函数显示错误信息 |
| ERR-02 | 无音频文件 → 显示 "No Music Files" | PRD 7 | 同上 |
| ERR-03 | VS1003 初始化失败 → 显示 "Decoder Error" | PRD 7 | 同上 |
| ERR-04 | 播放中 SD 卡拔出 → 显示 "SD Removed" | PRD 7 | 同上 |
| ERR-05 | 文件系统挂载失败 → 显示 "FS Error" | PRD 7 | 同上 |

### 1.4 字体与字库需求 (PRD 5.4)

| 项目 | 说明 | 存储位置 |
|------|------|---------|
| 中文字库 | 16x16 点阵, 仅提取 UI 固定文字 + 歌曲文件名中的中文 | Flash (const) |
| 英文字库 | 8x16 点阵, ASCII 0x20~0x7E (95 个可打印字符) | Flash (const) |
| OLED 硬字库 | **不使用** | -- |

> Flash 仅 64KB。按 PRD 策略, 仅提取实际使用的中文字符, 通过 `PCtoLCD2002` 生成点阵后存为 `const` 数组。未收录字符显示占位符 "□"。字库制作详见 `docs/Dev.md` 第 3.1 节。

---

## 2. 架构设计 -- 如何满足需求

### 2.1 从需求到分层的推导

上一章的需求可以自然归类为三个不同的关注点:

| 关注点 | 对应需求 | 问题域 |
|--------|---------|--------|
| I2C 物理层通信 | 发送命令/数据到 OLED | 硬件时序、地址、超时 |
| 像素级操作 | 画点、画线、字符点阵展开 | GDDRAM 页式组织、坐标换算 |
| 界面级操作 | 4 行布局、滚动、状态切换 | FreeRTOS 任务调度、事件驱动刷新 |

这三个关注点对应三层架构, 与项目整体分层 (PRD 9.1 / Dev 第 2 节) 一致:

```
┌──────────────────────────────────────────────────────┐
│                    FreeRTOS 任务层                      │
│                                                      │
│  uiTask (Priority: Normal, Stack: 128 words)          │
│    │                                                 │
│    │  等待 UI 事件 (队列阻塞, 不轮询)                     │
│    │  仅在状态变化时触发渲染                              │
│    │                                                 │
│    ▼                                                 │
│  ┌──────────────────────────────────────┐            │
│  │         App 层 — 界面组装              │            │
│  │                                      │            │
│  │  ui_render.c                         │            │
│  │    Render_MainScreen()               │ ← 对应 UI-01~06 │
│  │    Render_MenuScreen()  (规划中)       │            │
│  │    Render_ErrorScreen()  (规划中)      │ ← 对应 ERR-01~05│
│  │                                      │            │
│  │  ─ ─ ─ ─ 关注点分界线 ─ ─ ─ ─        │            │
│  │                                      │            │
│  │  ssd1315.c                           │            │
│  │    GDDRAM 镜像显存 + 脏页标记           │            │
│  │    绘图 API: SetPixel / DrawLine ...  │            │
│  │    Refresh() — 增量同步脏页到 OLED      │ ← 满足 PERF-01/02/04│
│  ├──────────────────────────────────────┤            │
│  │         BSP 层 — 硬件抽象              │            │
│  │                                      │            │
│  │  bsp_oled.c                          │            │
│  │    BSP_OLED_WriteCmd()               │            │
│  │    BSP_OLED_WriteData()              │            │
│  ├──────────────────────────────────────┤            │
│  │         HAL 层 (CubeMX 生成)          │            │
│  │                                      │            │
│  │  HAL_I2C_Master_Transmit()           │            │
│  │  I2C1: PB6(SCL) / PB7(SDA)          │            │
│  │  Fast Mode 400kHz, 地址 0x3C<<1      │            │
│  └──────────────────────────────────────┘            │
│                                                      │
│  消息流:                                               │
│  keyTask ──(key_event_t)──▶ mainTask ──(ui_event_t)──▶ uiTask  │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### 2.2 分层职责与需求映射

| 层 | 文件 | 职责 | 直接满足的需求 |
|----|------|------|--------------|
| **UI 渲染** | `ui_render.h/c` | 根据系统状态组装屏幕内容 (4 行布局、菜单、错误信息) | UI-01~06, ERR-01~05 |
| **控制器驱动** | `ssd1315.h/c` | 管理 GDDRAM 镜像显存、脏页标记、提供像素级绘图 API、增量刷新 | PERF-01, PERF-02, PERF-04 |
| **I2C 封装** | `bsp_oled.h/c` | 封装 HAL I2C 写命令/写数据, 隐藏 HAL 细节 | -- |
| **I2C 扫描** | `bsp_i2c_scanner.h/c` | 调试阶段确认 I2C 总线地址 | -- |
| **CubeMX 配置** | `i2c.c/h` | I2C1 GPIO/时钟/速率配置 (Dev 1.2.5) | PERF-03 |

### 2.3 关键设计决策

每个决策都直接对应一项需求约束:

| 决策 | 解决的问题 | 依据 |
|------|-----------|------|
| **镜像显存 + 脏页增量刷新** | PERF-01 (仅变化时刷新) + PERF-04 (无闪烁) | 全屏刷新 25ms 会闪烁且违反"仅变化时刷新"原则; 脏页机制下典型刷新仅 1~2 页 (3~6ms) |
| **先绘到显存, 再统一 Refresh** | PERF-04 (无闪烁) | 逐像素发 I2C 会导致数百次通信 + 画面撕裂; 批量刷新一次完成 |
| **uiTask 事件驱动, 非定时轮询** | PERF-01 (仅变化时刷新) | uiTask 阻塞在队列上等待事件, 无事件时零 CPU 占用 |
| **BSP 层暴露最小接口** | 分层解耦 | 仅暴露 `WriteCmd` 和 `WriteData`, 上层不感知 I2C 句柄/超时参数 |
| **仅提取使用的中文字符** | Flash 64KB 限制 (PRD 5.4) | 完整 GBK 字库 ~250KB, 远超 Flash; 仅取所需字符约 20KB |

### 2.4 初始化时序 (满足 PERF-03)

```
上电 / 复位
  │
  ├─ HAL_Init() + SystemClock_Config()
  │    └─ 48MHz SYSCLK, I2C1 时钟使能
  │
  ├─ MX_I2C1_Init()                          ← CubeMX 生成 (Dev 1.2.5)
  │    └─ GPIO: PB6(AF-OD) / PB7(AF-OD)
  │    └─ I2C1: Fast Mode, 400kHz
  │    └─ 耗时: ~1ms
  │
  ├─ SSD1315_Init()                          ← App 层控制器初始化
  │    ├─ 发送 17 条初始化命令 (~2ms)
  │    └─ Clear() + 全屏 Refresh (~25ms)
  │    └─ 合计: ~30ms
  │
  ├─ Render_Init()                           ← UI 框架初始化
  │    └─ Render_MainScreen() 绘制默认界面
  │         ├─ Clear() (~25ms)
  │         ├─ 绘制 4 行元素 (SetPixel × N)
  │         └─ Refresh() — 脏页增量 (~6ms)
  │    └─ 合计: ~35ms
  │
  └─ osKernelStart()                         ← FreeRTOS 调度器启动
       └─ uiTask 就绪, 等待第一个 UI 事件
```

总初始化耗时约 **65ms**, 远小于 PERF-03 要求的 10s 冷启动预算。

---

## 3. BSP 层 -- I2C 通信封装

### 3.1 硬件连接

| 信号 | MCU 引脚 | CubeMX 配置 | 说明 |
|------|---------|------------|------|
| I2C1_SCL | PB6 | Alternate Function Open-Drain | 时钟线, 外部 4.7kΩ 上拉 |
| I2C1_SDA | PB7 | Alternate Function Open-Drain | 数据线, 外部 4.7kΩ 上拉 |

I2C 参数 (来自 Dev 1.2.5):

| 参数 | 值 |
|------|-----|
| Speed Mode | Fast Mode |
| Clock Speed | 400 000 Hz |
| 7-bit Address | 0x3C (写地址 = 0x78) |

### 3.2 接口定义

```c
// BSP/include/bsp_oled.h

/* 发送单字节命令到 SSD1315
 * 格式: {0x00, command} → 控制字节 0x00 表示后续是命令 */
void BSP_OLED_WriteCmd(uint8_t command);

/* 发送 GDDRAM 数据块到 SSD1315
 * 格式: 先发控制字节 0x40 → 再发 len 字节的数据 */
void BSP_OLED_WriteData(uint8_t *data, uint16_t len);
```

### 3.3 实现 (`BSP/src/bsp_oled.c`)

```c
#define OLED_I2C_ADDR (0x3C << 1)   // 7-bit 地址左移 1 位得到 8-bit 写地址
#define OLED_TIMEOUT 100             // 命令超时 100ms

void BSP_OLED_WriteCmd(uint8_t command)
{
    uint8_t data[2] = {0x00, command};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, data, 2, OLED_TIMEOUT);
}

void BSP_OLED_WriteData(uint8_t *data, uint16_t len)
{
    // 控制字节 + 数据必须在同一次 I2C 事务中发送
    static uint8_t buf[129];  // 1 控制字节 + 最大 128 数据字节
    buf[0] = 0x40;
    for (uint16_t i = 0; i < len && i < 128; i++) {
        buf[i + 1] = data[i];
    }
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, len + 1, HAL_MAX_DELAY);
}
```

### 3.4 I2C 控制字节说明

SSD1315 通过 I2C 写的第一个字节区分后续内容类型:

| 控制字节 | 含义 | BSP 中的使用场景 |
|----------|------|-----------------|
| `0x00` | 下一字节是命令 (Co=0, D/C#=0) | `BSP_OLED_WriteCmd()` -- 发送单条命令 |
| `0x40` | 下一字节是 GDDRAM 数据 (Co=0, D/C#=1) | `BSP_OLED_WriteData()` -- 刷新显存页 |

本驱动不使用 Co=1 的变体 (`0x80`/`0xC0`), 因为命令之间不需要插入额外的控制字节。

### 3.5 超时策略

| 操作 | 超时值 | 理由 |
|------|--------|------|
| `WriteCmd` | 100ms | 仅 2 字节, 400kHz 下 ~0.05ms 即可完成; 100ms 是异常兜底 |
| `WriteData` (控制字节) | 100ms | 仅 1 字节 |
| `WriteData` (数据负载) | `HAL_MAX_DELAY` | 脏页刷新时 128 字节 ~3ms; 后续字库数据可能更多, 不设硬上限 |

---

## 4. SSD1315 控制器驱动 -- 显存模型与绘图

### 4.1 设计目标

SSD1315 驱动层需满足以下需求:

- **PERF-01**: 支撑上层"仅在数据变化时刷新"的能力 → 脏页标记 + 增量同步
- **PERF-04**: 画面无闪烁 → 镜像显存先绘后刷
- **UI-01~06**: 提供像素级绘图 API 供 UI 渲染层调用
- **PERF-02**: 单次 Refresh 耗时 < 5ms → 仅刷新脏页, 不发送未修改的页

### 4.2 GDDRAM 镜像显存

MCU 侧维护 1024 字节的镜像显存, 与 SSD1315 内部 GDDRAM 保持逻辑一致:

```c
static uint8_t SSD1315_GRAM[8][128];  // [页 0~7][列 0~127], 共 1024 字节
```

**内存布局与屏幕坐标的对应关系**:

```
逻辑视角 (C 数组):
  SSD1315_GRAM[page][col]  其中 page = 0..7, col = 0..127

物理视角 (一维连续 1024 字节):
  byte   0 ~  127: PAGE0 (COM0~COM7  × SEG0~SEG127)
  byte 128 ~  255: PAGE1 (COM8~COM15 × SEG0~SEG127)
  ...
  byte 896 ~ 1023: PAGE7 (COM56~COM63 × SEG0~SEG127)

每个字节 = 同一列的 8 个纵向像素:

  SSD1315_GRAM[page][col]:
    bit 0 (LSB) → COM(page×8+0)  ← 页内最上一行
    bit 1       → COM(page×8+1)
    ...
    bit 7 (MSB) → COM(page×8+7)  ← 页内最下一行

坐标换算:
  page = y / 8       (整数除法, y ∈ [0, 63])
  bit  = y % 8       (余数, bit ∈ [0, 7])
```

| PAGE | 覆盖行 | 对应 UI 行 |
|------|--------|-----------|
| PAGE0 | y = 0~7 | 第 1 行上半 |
| PAGE1 | y = 8~15 | 第 1 行下半 |
| PAGE2 | y = 16~23 | 第 2 行上半 |
| PAGE3 | y = 24~31 | 第 2 行下半 |
| PAGE4 | y = 32~39 | 第 3 行上半 |
| PAGE5 | y = 40~47 | 第 3 行下半 |
| PAGE6 | y = 48~55 | 第 4 行上半 |
| PAGE7 | y = 56~63 | 第 4 行下半 |

> UI 每行 16px 高, 恰好占 2 个 PAGE。因此界面仅修改一行文字时, 最多标记 2 个脏页, Refresh 仅发送 256 字节。

### 4.3 脏页局部刷新 (核心机制)

**问题**: 全屏刷新 1024 字节在 400kHz I2C 下需 ~25ms, 画面会明显闪烁, 且违反 PERF-01 "仅变化时刷新" 原则。

**方案**: 维护 1 字节 `dirty_pages`, 每个 bit 对应一个 PAGE 是否被修改过:

```c
static uint8_t dirty_pages;  // bit 0 = PAGE0, bit 1 = PAGE1, ...
```

**工作流程**:

```
绘制阶段 (多次 SetPixel):
  SSD1315_SetPixel(x, y, color)
    ├─ 修改 SSD1315_GRAM[page][x]  (仅操作 MCU 侧镜像, 不产生 I2C)
    └─ dirty_pages |= (1 << page)  (标记该页为脏)

同步阶段 (一次 Refresh):
  SSD1315_Refresh()
    └─ for p in 0..7:
         if (dirty_pages >> p) & 1:
           ├─ Set Column Addr: 0x21, 0x00, 0x7F  (全宽 128 列)
           ├─ Set Page Addr:  0x22, p, p         (仅当前页)
           ├─ Write Data: SSD1315_GRAM[p], 128   (发送 128 字节)
           └─ dirty_pages 对应 bit 清零 (在循环结束后统一清零)
```

**效果对比**:

| 场景 | 脏页数 | I2C 数据量 | 耗时 | 说明 |
|------|--------|-----------|------|------|
| 仅更新音量数字 (第 1 行, PAGE0+1) | 2 | 256 字节 | ~6ms | 典型场景 |
| 仅更新时间戳 (第 4 行, PAGE6+7) | 2 | 256 字节 | ~6ms | 1s 周期更新 |
| 全屏重绘 (切歌/进菜单) | 8 | 1024 字节 | ~25ms | 仅状态跳变时发生 |
| 无变化 | 0 | 0 | 0 | uiTask 被事件唤醒但无需重绘 |

**实现代码** (`App/src/ssd1315.c`):

```c
void SSD1315_Refresh(void) {
    for (uint8_t p = 0; p < 8; p++) {
        if (dirty_pages & (1 << p)) {
            BSP_OLED_WriteCmd(0x21);          // 设置列地址
            BSP_OLED_WriteCmd(0x00);          //   起始列 = 0
            BSP_OLED_WriteCmd(0x7F);          //   结束列 = 127
            BSP_OLED_WriteCmd(0x22);          // 设置页地址
            BSP_OLED_WriteCmd(p);             //   起始页 = p
            BSP_OLED_WriteCmd(p);             //   结束页 = p
            BSP_OLED_WriteData(SSD1315_GRAM[p], 128);
        }
    }
    dirty_pages = 0;
}
```

> **为何列范围用 0~127 而非精确范围?** 维持列范围的精确性需额外计算脏区域内最小/最大列, 增加代码复杂度。128 列全宽在 400kHz I2C 下仅增加 ~3ms 传输时间, 而脏页机制已经节省了未修改页的全部传输。精确列范围的收益远小于其代码成本, 故取全宽。

### 4.4 初始化序列

`SSD1315_Init()` 严格遵循 SSD1315 规格书规定的上电顺序:

| 步 | 命令 | 参数 | 类别 | 说明 |
|----|------|------|------|------|
| 1 | `0xAE` | -- | 显示控制 | **先关显示**, 配置期间避免异常画面 |
| 2 | `0xD5` | `0x80` | 时钟 | 分频比=1, 振荡频率=1000b (默认值) |
| 3 | `0xA8` | `0x3F` | 分辨率 | MUX=63 → 64 COM 行 |
| 4 | `0xD3` | `0x00` | 显示偏移 | 偏移=0, 从 COM0 开始 |
| 5 | `0x40` | -- | 起始行 | 显示起始行=0 |
| 6 | `0x8D` | `0x14` | **电荷泵** | 使能内部电荷泵, 输出 7.5V |
| 7 | `0x20` | `0x00` | 寻址 | 水平寻址模式 |
| 8 | `0xA1` | -- | 重映射 | SEG 左右镜像 |
| 9 | `0xC8` | -- | 重映射 | COM 上下翻转 |
| 10 | `0xDA` | `0x12` | COM 配置 | 交替引脚, 不禁用左右重映射 |
| 11 | `0x81` | `0x7F` | 对比度 | 中间值 127 |
| 12 | `0xD9` | `0xF1` | 预充电 | Phase1=1, Phase2=15 |
| 13 | `0xDB` | `0x40` | VCOMH | ~0.77 × VCC |
| 14 | `0xA4` | -- | 显示模式 | 恢复 GDDRAM 内容 (非全屏点亮) |
| 15 | `0xA6` | -- | 显示模式 | 正常显示 (非反色) |
| 16 | -- | -- | 清屏 | `SSD1315_Clear()` → 全屏写 0x00 |
| 17 | `0xAF` | -- | 显示控制 | **开显示** |

> **电荷泵顺序是关键**: 规格书要求 `8Dh` (使能) → `14h` (选电压) → `AFh` (开显示)。顺序错误会导致屏幕不亮或亮度异常。

### 4.5 接口总览 -- 上层需要什么, SSD1315 就提供什么

SSD1315 模块的全部接口按"上层调用者"的使用场景分为 5 组。以下每组都标注了实现状态和上层使用方式。

#### 第一组: 生命周期 (上电时调用 1 次)

| 接口 | 状态 | 上层调用示例 |
|------|------|-------------|
| `void SSD1315_Init(void)` | **已实现** | `Render_Init()` 中调用, 发送 17 条初始化命令 + 清屏 |

FreeRTOS 与此无关 -- `SSD1315_Init()` 在 `osKernelStart()` 之前由 `main()` 或 `defaultTask` 调用, 是裸机代码。

#### 第二组: 显存操作 (每帧渲染的开始和结束)

| 接口 | 状态 | 上层调用示例 |
|------|------|-------------|
| `void SSD1315_Clear(void)` | **已实现** | 页面切换时调用 -- 将整个镜像显存写 0 + 全屏 Refresh |
| `void SSD1315_Refresh(void)` | **已实现** | 一帧绘制的最后一步 -- 把脏页同步到 OLED 硬件 |

**这是 SSD1315 模块最核心的两个接口**。上层使用模式:

```c
// 模式 1: 全屏重绘 (页面切换)
SSD1315_Clear();                        // 整屏清零 + 全屏刷新
// ... 所有绘制操作 (只改 MCU 侧 GRAM, 不产生 I2C) ...
SSD1315_Refresh();                      // 脏页增量同步 (~25ms)

// 模式 2: 局部刷新 (数据更新)
// ... 仅修改特定 Zone 的像素 ...
SSD1315_Refresh();                      // 仅脏页同步 (~3~6ms/页)
```

> **关键**: `Clear()` 会产生一次全屏 I2C 通信 (1024 字节)。如果只是修改一行文字, 不要调用 `Clear()`, 而是自己用 `SetPixel(x, y, 0)` 擦除旧内容, 再用 `SetPixel(x, y, 1)` 绘制新内容, 最后 `Refresh()` 仅发送被修改的页。

#### 第三组: 像素级绘图 (已实现, 所有图形的基础)

| 接口 | 状态 | 上层调用示例 |
|------|------|-------------|
| `void SSD1315_SetPixel(uint8_t x, uint8_t y, uint8_t color)` | **已实现** | 所有绘制函数的底层; color=0 擦除, color=1 点亮 |
| `void SSD1315_DrawPoint(uint8_t x, uint8_t y)` | **已实现** | 等价 `SetPixel(x, y, 1)` |
| `void SSD1315_ClearPoint(uint8_t x, uint8_t y)` | **已实现** | 等价 `SetPixel(x, y, 0)` |

核心实现 (`SetPixel`) 已在上文 4.3 节详述: 仅修改 `SSD1315_GRAM[page][x]` 并标记 `dirty_pages`, 不产生 I2C。上层 (例如 `DrawLine`) 可以连续调用数百次, 所有变更在 `Refresh()` 时一次性同步。

#### 第四组: 图形绘制 (已实现, 构建在 SetPixel 之上)

| 接口 | 状态 | 上层调用示例 |
|------|------|-------------|
| `void SSD1315_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)` | **已实现** | 绘制分割线 / 下划线 / 简单图标边框 |
| `void SSD1315_DrawCircle(uint8_t x, uint8_t y, uint8_t r)` | **已实现** | 绘制圆形图标 (如播放按钮) |

实现: `DrawLine` = Bresenham 整数算法; `DrawCircle` = 中点画圆 + 八对称。内部均逐像素调用 `SetPixel`, 自动累积脏页。

> **还缺什么?** 上层可能还需要 `SSD1315_DrawRect(x, y, w, h, fill)` 画矩形 (填充/描边), 用于擦除一整块区域。如果后续发现逐像素擦除太慢, 可以新增此接口 -- 由于知道矩形区域的 PAGE 边界, 可以直接操作 `SSD1315_GRAM` 的连续字节而非逐像素 SetPixel, 性能更好。

#### 第五组: 帧同步 (已实现, 上层必须理解其行为)

| 接口 | 状态 | 说明 |
|------|------|------|
| `void SSD1315_Refresh(void)` | **已实现** | 遍历 8 页, 对每个脏页发送 128 字节到 OLED, 然后清零 `dirty_pages` |

**上层使用规则**: 一次完整的屏幕更新 = N 次绘制调用 + **恰好 1 次** `Refresh()`。如果忘了调用, 画面不会变化; 如果多次调用, 后续调用是空操作 (dirty_pages 已被第一次清零)。

#### 第六组: 文字显示 (接口已声明, 待字库支撑后实现)

这是当前缺口最大的一组。上层 (`ui_render.c`) 需要画文字, SSD1315 需要对每个字符查字库 → 展开点阵 → 逐像素 SetPixel:

| 接口 | 状态 | 要实现什么 |
|------|------|-----------|
| `void SSD1315_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size)` | **待实现** | 根据 ASCII 码查 `font_en` 数组, 取 8x16 点阵, 逐列逐 bit 调用 `SetPixel` |
| `void SSD1315_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)` | **待实现** | 遍历 `str`, 对每个字符调用 `ShowChar`, x 坐标累加 8 像素 |
| `void SSD1315_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)` | **待实现** | 数字 → 字符串 → 调用 `ShowString`; `len` 控制最小显示位数 (不足补空格) |
| `void SSD1315_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size)` | **待实现** | 根据 GBK 编码查 `font_cn` 数组, 取 16x16 点阵, 逐列逐 bit 调用 `SetPixel` |

**`ShowChar` 的参考实现** (等 `font_en.c` 就绪后即可编写):

```c
// font_en.c 需要提供:
extern const uint8_t font_en_8x16[][16];  // 95 个字符 × 16 字节

void SSD1315_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size) {
    uint8_t idx = chr - 0x20;  // ASCII 0x20 (' ') = 数组第 0 个
    if (idx >= 95) return;     // 不可打印字符

    const uint8_t *bitmap = font_en_8x16[idx];  // 16 字节

    for (uint8_t col = 0; col < 8; col++) {     // 8 列
        uint8_t line = bitmap[col];
        for (uint8_t row = 0; row < 16; row++) { // 16 行
            if (line & (1 << row)) {
                SSD1315_SetPixel(x + col, y + row, 1);
            }
        }
    }
    // dirty_pages 由 SetPixel 内部自动标记, 无需额外处理
}
```

**`ShowString` 的参考实现**:

```c
void SSD1315_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size) {
    while (*str) {
        SSD1315_ShowChar(x, y, *str, size);
        x += 8;   // 英文字符宽度 8px
        str++;
    }
}
```

**`ShowChinese` 的参考实现** (等 `font_cn.c` 就绪后):

```c
// font_cn.c 需要提供:
//   typedef struct { uint16_t code; uint8_t bitmap[32]; } font_cn_16x16_t;
//   extern const font_cn_16x16_t font_cn_table[];
//   extern const uint16_t font_cn_count;
//   const uint8_t* font_cn_find(uint16_t gbk_code);

void SSD1315_ShowChinese(uint8_t x, uint8_t y, uint8_t index, uint8_t size) {
    // index 是字库表中的序号 (ui_render 层在编译时已知每个中文字符的索引)
    const uint8_t *bitmap = font_cn_table[index].bitmap;  // 32 字节

    for (uint8_t col = 0; col < 16; col++) {    // 16 列
        uint8_t line_hi = bitmap[col * 2];       // 上半 8 行
        uint8_t line_lo = bitmap[col * 2 + 1];   // 下半 8 行
        for (uint8_t row = 0; row < 8; row++) {
            if (line_hi & (1 << row)) SSD1315_SetPixel(x + col, y + row, 1);
            if (line_lo & (1 << row)) SSD1315_SetPixel(x + col, y + 8 + row, 1);
        }
    }
}
```

#### 第七组: 显示控制 (已实现, 特定场景使用)

| 接口 | 状态 | 上层使用场景 |
|------|------|-------------|
| `void SSD1315_DisplayOn(void)` | **已实现** | 从 sleep 恢复 (本工程暂不需要) |
| `void SSD1315_DisplayOff(void)` | **已实现** | 进入低功耗 sleep 模式 (本工程暂不需要) |
| `void SSD1315_ColorTurn(uint8_t i)` | **已实现** | 反色显示效果 (本工程暂不需要) |
| `void SSD1315_DisplayTurn(uint8_t orientation)` | **已实现** | 旋转 180°/镜像 (本工程暂不需要) |

#### 实现优先级

```
已完成 (可直接使用):
  SSD1315_Init      SSD1315_Clear      SSD1315_Refresh
  SSD1315_SetPixel  SSD1315_DrawPoint  SSD1315_ClearPoint
  SSD1315_DrawLine  SSD1315_DrawCircle
  SSD1315_DisplayOn/Off/ColorTurn/DisplayTurn

第一优先 (阻塞上层文字渲染):
  SSD1315_ShowChar   ← 依赖 font_en.c (8x16 ASCII 点阵)
  SSD1315_ShowString ← 依赖 ShowChar
  SSD1315_ShowNum    ← 依赖 ShowString

第二优先 (阻塞中文显示):
  SSD1315_ShowChinese  ← 依赖 font_cn.c (16x16 GBK 点阵)

第三优先 (增强体验):
  SSD1315_DrawRect     ← 可选, 加速矩形区域擦除
  SSD1315_ScrollDisplay ← 可选, 硬件滚动 vs 软件移位待评估
```

### 4.6 `SetPixel` 实现

```c
void SSD1315_SetPixel(uint8_t x, uint8_t y, uint8_t color) {
    uint8_t page = y / 8;
    uint8_t bit  = y % 8;
    if (color)
        SSD1315_GRAM[page][x] |=  (1 << bit);   // 置位 → 点亮
    else
        SSD1315_GRAM[page][x] &= ~(1 << bit);   // 清零 → 熄灭
    dirty_pages |= (1 << page);                  // 标记脏页
}
```

**说明**: `SetPixel` 不直接操作硬件, 仅修改 MCU 侧镜像显存并标记脏页。实际的 I2C 传输延迟到 `Refresh()` 调用时一次性完成。这使得上层可以连续调用数百次 `SetPixel` (例如 `DrawLine` 或 `DrawCircle`) 而不产生任何 I2C 开销。

### 4.7 `DrawLine` / `DrawCircle` 实现

- **`DrawLine`**: 标准整数 Bresenham 算法, 无浮点, 适合 Cortex-M0
- **`DrawCircle`**: 中点画圆算法, 利用八对称性每次绘制 8 个像素
- 两者均逐像素调用 `SetPixel`, 自动累积脏页, 不直接操作硬件

### 4.8 模块边界 -- SSD1315 不管什么, FreeRTOS 管什么

这是理解整个 OLED 子系统职责划分的关键:

```
SSD1315 模块的边界:
  ✅ 管理 GDDRAM 镜像显存 (SSD1315_GRAM[8][128])
  ✅ 标记脏页 (dirty_pages)
  ✅ 提供像素/图形/文字绘制接口
  ✅ 将脏页通过 I2C 同步到 OLED 硬件 (Refresh)
  ❌ 不知道 FreeRTOS 的存在 — 模块内没有任务、队列、信号量
  ❌ 不知道"何时"该刷新 — 由上层 (uiTask) 决定调用时机
  ❌ 不知道屏幕布局 — 不知道 Zone 0~3, 只提供 (x, y) 坐标接口
  ❌ 不知道播放器状态 — 不引用 g_player_ctx

FreeRTOS uiTask 负责:
  ✅ 阻塞等待 ui_queue (何时响应)
  ✅ 根据事件类型决定调用哪些 SSD1315 接口 (响应什么)
  ✅ 读取 g_player_ctx 获取播放器状态数据
  ✅ 在调用 SSD1315 接口之间组织 Zone 0~3 的布局逻辑
```

这样分层后, SSD1315 是一个纯 C 模块 (无 OS 依赖), 可以在裸机环境中独立测试, 也可以被任何需要操作 OLED 的任务调用。

---

## 5. UI 渲染框架 -- 接口定义与实现规划

第 1~4 章解决了"怎么把像素发到 OLED"的问题。本章解决"什么时候该发什么像素"的问题 -- 即 UI 状态管理与界面切换的完整接口契约。

### 5.1 整体数据流

```
按键输入                     系统状态                       OLED 输出
────────                    ────────                       ────────

keyTask                    mainTask                       uiTask
  │                          │                              │
  │ key_event_t              │                              │
  ├──(key_queue)────────────▶│                              │
  │                          │ 读取 g_player_ctx            │
  │                          │ 更新 g_player_ctx            │
  │                          │                              │
  │                          │ ui_event_t                   │
  │                          ├──(ui_queue)─────────────────▶│
  │                          │                              │ 读取 g_player_ctx
  │                          │                              │ 调用 Render_*()
  │                          │                              ├──▶ SSD1315_*()
  │                          │                              │    BSP_OLED_*()
  │                          │                              │    HAL_I2C_*()
  │                          │                              │    OLED 硬件
```

**核心原则**: mainTask 是状态的**唯一写入者**, uiTask 是 OLED 的**唯一绘制者**。两者通过 `ui_queue` 异步解耦 -- mainTask 不等待绘制完成, uiTask 不关心事件来源。

### 5.2 数据结构定义

以下类型是 UI 渲染框架的接口契约, 应放入新文件 `App/include/ui_types.h`:

#### 5.2.1 屏幕类型枚举

对应 PRD 第 10 节状态机的每个可见页面:

```c
// App/include/ui_types.h

/* 屏幕页面类型 — 每个枚举值对应一种 OLED 全屏布局 */
typedef enum {
    UI_SCREEN_MAIN,         // PRD 5.1 主界面 (四行布局)
    UI_SCREEN_MENU,         // PRD 5.2 菜单界面 (标题 + 列表)
    UI_SCREEN_VOLUME,       // 音量调节子界面
    UI_SCREEN_PLAY_MODE,    // 播放模式切换子界面
    UI_SCREEN_ERROR,        // 异常信息界面 (ERR-01~05)
} ui_screen_t;
```

#### 5.2.2 UI 事件类型

mainTask 通过发送 `ui_event_t` 通知 uiTask 需要切换界面或更新内容:

```c
/* UI 事件类型 — mainTask → uiTask 的消息协议 */
typedef enum {
    UI_EVENT_SCREEN_CHANGED,    // 屏幕页面切换 (伴随 .screen 字段)
    UI_EVENT_DATA_CHANGED,      // 当前页面数据变化 (音量/模式/歌曲名/时间/播放状态)
    UI_EVENT_TICK_1S,           // 每秒定时信号, 用于时间戳更新
} ui_event_type_t;

/* UI 事件 */
typedef struct {
    ui_event_type_t type;

    /* 仅当 type == UI_EVENT_SCREEN_CHANGED 时有效 */
    ui_screen_t     screen;

    /* 仅当 type == UI_EVENT_SCREEN_CHANGED && screen == UI_SCREEN_ERROR 时有效 */
    const char     *error_msg;

    /* 仅当 type == UI_EVENT_DATA_CHANGED 时有效, 指示哪些数据变了 */
    union {
        struct {
            uint8_t volume       : 1;  // 音量变化 → 刷新第 1 行
            uint8_t play_mode    : 1;  // 播放模式变化 → 刷新第 1 行
            uint8_t track        : 1;  // 歌曲名变化 → 刷新第 2 行
            uint8_t play_state   : 1;  // 播放/暂停变化 → 刷新第 3 行图标
            uint8_t time_elapsed : 1;  // 时间变化 → 刷新第 4 行
        } flags;
        uint8_t raw;                   // 原始字节, 方便整体比较
    } changed;
} ui_event_t;
```

**`changed` 字段的设计意图**: 当 `type == UI_EVENT_DATA_CHANGED` 时, `changed.flags` 精确指示哪些数据发生了变化, uiTask 据此决定执行**全屏重绘**还是**单行局部刷新**:

| changed 场景 | uiTask 行为 | 刷新行 | I2C 通信量 |
|-------------|------------|--------|-----------|
| 仅 `time_elapsed` | `Render_UpdateTime()` | 第 4 行 (PAGE6+7) | ~256 字节 / 6ms |
| 仅 `volume` | `Render_UpdateVolume()` | 第 1 行 (PAGE0+1) | ~256 字节 / 6ms |
| 仅 `track` | `Render_UpdateTrackName()` | 第 2 行 (PAGE2+3) | ~256 字节 / 6ms |
| `track` + `time_elapsed` | `Render_UpdateTrackName()` + `Render_UpdateTime()` | 第 2+4 行 | ~512 字节 / 12ms |
| 多标志位同时置位 或 `SCREEN_CHANGED` | `Render_MainScreen()` | 全屏 (8 页) | ~1024 字节 / 25ms |

#### 5.2.3 播放器上下文 (渲染数据源)

uiTask 读取全局状态来获取渲染所需的数据。这些数据由 mainTask 维护, uiTask 只读:

```c
// App/include/player_context.h (或放入 ui_types.h)

/* 播放状态 */
typedef enum {
    PLAY_STOP,
    PLAY_PLAYING,
    PLAY_PAUSED,
} play_state_t;

/* 播放模式 */
typedef enum {
    MODE_SEQUENTIAL,    // 顺序播放
    MODE_LOOP_ALL,      // 列表循环
    MODE_LOOP_ONE,      // 单曲循环
    MODE_SHUFFLE,       // 随机播放
} play_mode_t;

/* 播放器全局上下文 — 由 mainTask 写入, uiTask 和其他任务只读 */
typedef struct {
    play_state_t  play_state;       // 当前播放状态
    play_mode_t   play_mode;        // 当前播放模式
    uint8_t       volume;           // 音量 0~100 (步进 10)
    uint8_t       current_track;    // 当前曲目索引 (0 ~ file_count-1)
    uint8_t       file_count;       // SD 卡中 MP3 文件总数
    char          track_name[65];   // 当前歌曲名 (去后缀, ≤64 字符)
    uint16_t      elapsed_sec;      // 当前曲目已播放秒数
    uint16_t      total_sec;        // 当前曲目总秒数
} player_context_t;

/* 全局单例 — 定义在 main.c 或 player_context.c */
extern player_context_t g_player_ctx;
```

### 5.3 四区域布局接口

屏幕 128x64 像素被划分为 4 个水平区域, 每区域固定 16 像素高。这个划分是 UI 渲染层的**基础坐标契约**:

```
列: 0 ─────────────────────────────── 127
行:
 0 ┬──────────────────────────────────┐
   │ Zone 0: 顶栏 (PAGE0 + PAGE1)      │  Logo / 音量 / 模式图标
   │   y = 0~15, 共 16px              │
16 ┼──────────────────────────────────┤
   │ Zone 1: 曲目名 (PAGE2 + PAGE3)    │  歌曲名称 (超宽时滚动)
   │   y = 16~31, 共 16px            │
32 ┼──────────────────────────────────┤
   │ Zone 2: 控制栏 (PAGE4 + PAGE5)    │  上一曲 / 播放暂停 / 下一曲
   │   y = 32~47, 共 16px            │
48 ┼──────────────────────────────────┤
   │ Zone 3: 时间栏 (PAGE6 + PAGE7)    │  [elapsed]/[total]
   │   y = 48~63, 共 16px            │
64 ┴──────────────────────────────────┘
```

#### 5.3.1 区域坐标常量 (头文件)

```c
// App/include/ui_render.h 中定义

/* 区域 Y 坐标常量 — 每区 16px 高 */
#define UI_ZONE0_Y       0     // 顶栏: Logo + 音量 + 模式
#define UI_ZONE1_Y      16     // 曲目名
#define UI_ZONE2_Y      32     // 控制栏
#define UI_ZONE3_Y      48     // 时间栏
#define UI_ZONE_HEIGHT  16     // 每区高度

/* 区域对应的 PAGE 范围 */
#define UI_ZONE0_PAGE0   0     // Zone 0 = PAGE0 + PAGE1
#define UI_ZONE0_PAGE1   1
#define UI_ZONE1_PAGE0   2     // Zone 1 = PAGE2 + PAGE3
#define UI_ZONE1_PAGE1   3
#define UI_ZONE2_PAGE0   4     // Zone 2 = PAGE4 + PAGE5
#define UI_ZONE2_PAGE1   5
#define UI_ZONE3_PAGE0   6     // Zone 3 = PAGE6 + PAGE7
#define UI_ZONE3_PAGE1   7
```

#### 5.3.2 渲染函数接口

```c
// App/include/ui_render.h

/* ===== 全屏渲染 (页面切换时调用) ===== */

/* 绘制主界面 — 调用四个区域的绘制函数组装完整屏幕 */
void Render_MainScreen(void);

/* 绘制菜单界面 — cursor 指示当前选中行 (0/1/2) */
void Render_MenuScreen(uint8_t cursor);

/* 绘制异常界面 — msg 来自 ERR-01~05 */
void Render_ErrorScreen(const char *msg);

/* ===== 全屏渲染的内部函数 ===== */

/* 清空指定区域 (用 0x00 填充该区域的 128×16 像素) */
static void Render_ClearZone(uint8_t zone_y);

/* ===== 单区域更新 (数据变化但页面不变时调用) ===== */

/* 仅更新 Zone 0: 音量数字 + 模式图标
 * 场景: 音量调节 / 模式切换 */
void Render_UpdateTopBar(void);

/* 仅更新 Zone 1: 歌曲名称
 * 场景: 切歌 */
void Render_UpdateTrackName(void);

/* 仅更新 Zone 2: 播放/暂停图标
 * 场景: 按 OK 键切换播放/暂停 */
void Render_UpdateControlBar(void);

/* 仅更新 Zone 3: 播放时间 [mm:ss]/[MM:SS]
 * 场景: 每秒定时器触发 */
void Render_UpdateTime(void);
```

#### 5.3.3 区域绘制函数的职责边界

每个 `Render_Update*` 函数必须自己负责**清空旧内容 + 绘制新内容**, 调用者不需要先 Clear:

```c
// 实现示例 (Render_UpdateTime 的伪代码)
void Render_UpdateTime(void) {
    // 1. 擦除旧内容: 在 Zone 3 范围内逐像素写 0
    for (uint8_t y = UI_ZONE3_Y; y < UI_ZONE3_Y + UI_ZONE_HEIGHT; y++) {
        for (uint8_t x = 0; x < 128; x++) {
            SSD1315_SetPixel(x, y, 0);
        }
    }

    // 2. 绘制新内容
    char buf[18];
    snprintf(buf, sizeof(buf), "[%02u:%02u]/[%02u:%02u]",
             g_player_ctx.elapsed_sec / 60, g_player_ctx.elapsed_sec % 60,
             g_player_ctx.total_sec   / 60, g_player_ctx.total_sec   % 60);
    SSD1315_ShowString(0, UI_ZONE3_Y, buf, 1);

    // 3. 标记脏页 → Refresh 仅发 PAGE6+7
    // (SetPixel 内部已自动标记)
}
```

### 5.3.4 实战: 精确更新歌曲名一栏

下面以"用户切歌, 仅更新 Zone 1 (曲目名, y=16~31)"为例, 完整展示从代码到 OLED 硬件的每一步。这是理解整个渲染机制最好的入口。

**目标**: 将第 2 行从 `"01-AAA"` 变为 `"02-BBB"`, 其他 3 行不动。

**关键约束**: 不调用 `SSD1315_Clear()` (那会全屏擦除 + 1024 字节 I2C)。只擦 Zone 1 + 画新字 + Refresh。

#### 第一步: 理解屏幕与 GRAM 的对应关系

```
屏幕布局 (128×64):                     GRAM 数组 (8 页 × 128 列):
                                       
行 0  ┌──────── Zone 0 ────────┐      SSD1315_GRAM[0][0..127] ← PAGE0
      │  MAP26    音量:70%      │      SSD1315_GRAM[1][0..127] ← PAGE1
行15  │                        │      
行16  ├──────── Zone 1 ────────┤      SSD1315_GRAM[2][0..127] ← PAGE2  ⬅ 目标区域
      │  02-BBB                │      SSD1315_GRAM[3][0..127] ← PAGE3  ⬅ 目标区域
行31  │                        │      
行32  ├──────── Zone 2 ────────┤      SSD1315_GRAM[4][0..127] ← PAGE4
      │  ◄  ►❚❚  ►            │      SSD1315_GRAM[5][0..127] ← PAGE5
行47  │                        │      
行48  ├──────── Zone 3 ────────┤      SSD1315_GRAM[6][0..127] ← PAGE6
      │  [01:23]/[04:56]       │      SSD1315_GRAM[7][0..127] ← PAGE7
行63  └────────────────────────┘      

Zone 1 = PAGE2 + PAGE3 = GRAM[2][*] + GRAM[3][*]
```

#### 第二步: 擦除旧内容

```c
// Render_UpdateTrackName() — 第一步: 把 Zone 1 区域全部写 0 (灭)
for (uint8_t y = UI_ZONE1_Y; y < UI_ZONE1_Y + UI_ZONE_HEIGHT; y++) {
    for (uint8_t x = 0; x < 128; x++) {
        SSD1315_SetPixel(x, y, 0);  // color=0 = 熄灭
    }
}
```

这 2048 次 (128×16) `SetPixel` 调用的效果:

```
每次 SetPixel(x, y, 0) 执行:
  page = y / 8;
  GRAM[page][x] &= ~(1 << (y % 8));     // 把对应的 bit 清零
  dirty_pages |= (1 << page);           // 标记该页为脏

结果:
  GRAM[2][0..127] → 全部变成 0x00       dirty_pages |= 0b00000100 (bit 2)
  GRAM[3][0..127] → 全部变成 0x00       dirty_pages |= 0b00001000 (bit 3)

dirty_pages 当前值: 0b00001100
  ↑ 只有 bit2 和 bit3 是 1, 其余 6 个 bit 是 0
```

此时 OLED 屏幕**还没有任何变化** — 所有修改都在 MCU 的 RAM 里, I2C 总线上没有传输任何数据。

#### 第三步: 绘制新内容

```c
// Render_UpdateTrackName() — 第二步: 画新的歌曲名
SSD1315_ShowString(0, UI_ZONE1_Y, "02-BBB", 1);
//  内部展开: '0'→SetPixel(0~7,  16~31, 1)
//            '2'→SetPixel(8~15,  16~31, 1)
//            '-'→SetPixel(16~23, 16~31, 1)
//            'B'→SetPixel(24~31, 16~31, 1)
//            'B'→SetPixel(32~39, 16~31, 1)
//            'B'→SetPixel(40~47, 16~31, 1)
```

每个 `SetPixel(x, y, 1)` 把对应 bit 置 1:

```
GRAM[2][x] 的某些 bit 从 0 变 1  (字符点阵的亮像素)
GRAM[3][x] 的某些 bit 从 0 变 1
dirty_pages |= 0b00000100;   // bit 2 已经是 1, 无变化
dirty_pages |= 0b00001000;   // bit 3 已经是 1, 无变化

dirty_pages 当前值: 仍然是 0b00001100
```

此时 OLED 屏幕**仍然没有变化**。Zone 1 的旧内容 "01-AAA" 和新内容 "02-BBB" 都已经在 GRAM 里了, 但 OLED 硬件还保留着旧数据。

#### 第四步: 同步到 OLED

```c
// Render_UpdateTrackName() — 第三步: 一次性同步
SSD1315_Refresh();
```

`Refresh()` 内部遍历 8 页:

```
PAGE0: dirty_pages bit0 = 0 → 跳过! (Zone 0 不动)
PAGE1: dirty_pages bit1 = 0 → 跳过! (Zone 0 不动)
PAGE2: dirty_pages bit2 = 1 → 发送 128 字节! ✦ I2C 活动
  BSP_OLED_WriteCmd(0x21);  BSP_OLED_WriteCmd(0x00);  BSP_OLED_WriteCmd(0x7F);
  BSP_OLED_WriteCmd(0x22);  BSP_OLED_WriteCmd(2);     BSP_OLED_WriteCmd(2);
  BSP_OLED_WriteData(GRAM[2], 128);  // ~3ms @ 400kHz
PAGE3: dirty_pages bit3 = 1 → 发送 128 字节! ✦ I2C 活动
  BSP_OLED_WriteCmd(0x21);  BSP_OLED_WriteCmd(0x00);  BSP_OLED_WriteCmd(0x7F);
  BSP_OLED_WriteCmd(0x22);  BSP_OLED_WriteCmd(3);     BSP_OLED_WriteCmd(3);
  BSP_OLED_WriteData(GRAM[3], 128);  // ~3ms @ 400kHz
PAGE4: dirty_pages bit4 = 0 → 跳过! (Zone 2 不动)
PAGE5: dirty_pages bit5 = 0 → 跳过! (Zone 2 不动)
PAGE6: dirty_pages bit6 = 0 → 跳过! (Zone 3 不动)
PAGE7: dirty_pages bit7 = 0 → 跳过! (Zone 3 不动)

dirty_pages = 0;  // 全部清零, 准备下一帧
```

#### 第五步: 结果

```
I2C 总线上的实际活动:
  START → 0x78(W) → 0x40 → [PAGE2 的 128 字节] → STOP  (~3ms)
  START → 0x78(W) → 0x40 → [PAGE3 的 128 字节] → STOP  (~3ms)
  结束。

  总 I2C 通信量: 256 字节
  总耗时: ~6ms
  其他 6 页: 零 I2C 通信

用户视角:
  OLED 第 2 行从 "01-AAA" 瞬间变为 "02-BBB"
  其余 3 行完全没有闪烁或扰动
```

#### 完整函数

```c
void Render_UpdateTrackName(void) {
    // Step 1: 擦除 Zone 1 (y=16~31)
    for (uint8_t y = UI_ZONE1_Y; y < UI_ZONE1_Y + UI_ZONE_HEIGHT; y++) {
        for (uint8_t x = 0; x < 128; x++) {
            SSD1315_SetPixel(x, y, 0);
        }
    }

    // Step 2: 绘制新歌曲名
    SSD1315_ShowString(0, UI_ZONE1_Y, g_player_ctx.track_name, 1);

    // Step 3: 同步脏页 (仅 PAGE2+PAGE3)
    SSD1315_Refresh();
}
```

#### 同样的模式用于其他三个 Zone

| 函数 | 擦除区域 | 绘制内容 | 脏页 | I2C |
|------|---------|---------|------|-----|
| `Render_UpdateTopBar()` | y=0~15 | Logo + 音量 + 模式 | PAGE0+1 | 256B/6ms |
| `Render_UpdateTrackName()` | y=16~31 | 歌曲名 | PAGE2+3 | 256B/6ms |
| `Render_UpdateControlBar()` | y=32~47 | 按键图标 | PAGE4+5 | 256B/6ms |
| `Render_UpdateTime()` | y=48~63 | [mm:ss]/[MM:SS] | PAGE6+7 | 256B/6ms |

**这就是脏页机制的全部价值**: 每个 Zone 精确对应 2 个 PAGE, 修改任一 Zone 只会触发 2 个 PAGE 的 I2C 同步, 其余 6 个 PAGE 完全不动。没有脏页的话, 即使只改一个字符, 也必须发送全部 1024 字节。

### 5.4 uiTask 事件分发

uiTask 是整个 UI 系统的调度中心。它阻塞等待 `ui_queue`, 根据事件类型将渲染任务分发到对应的函数:

```c
// Core/Src/freertos.c 中的 StartUITask (或放入 App/src/ui_task.c)

void StartUITask(void *argument) {
    Render_Init();

    ui_event_t event;
    while (1) {
        if (xQueueReceive(ui_queue, &event, portMAX_DELAY) != pdPASS) {
            continue;
        }

        switch (event.type) {

            case UI_EVENT_SCREEN_CHANGED:
                switch (event.screen) {
                    case UI_SCREEN_MAIN:      Render_MainScreen();            break;
                    case UI_SCREEN_MENU:      Render_MenuScreen(0);           break;
                    case UI_SCREEN_VOLUME:    Render_MainScreen();            break; // 复用主界面, 高亮音量区
                    case UI_SCREEN_PLAY_MODE: Render_MainScreen();            break; // 复用主界面, 高亮模式区
                    case UI_SCREEN_ERROR:     Render_ErrorScreen(event.error_msg); break;
                }
                break;

            case UI_EVENT_DATA_CHANGED:
                // 按需局部刷新 — 不执行全屏 Clear
                if (event.changed.flags.volume     ) Render_UpdateTopBar();
                if (event.changed.flags.play_mode  ) Render_UpdateTopBar();
                if (event.changed.flags.track      ) Render_UpdateTrackName();
                if (event.changed.flags.play_state ) Render_UpdateControlBar();
                if (event.changed.flags.time_elapsed) Render_UpdateTime();
                SSD1315_Refresh();  // 所有脏页一次性同步
                break;

            case UI_EVENT_TICK_1S:
                if (g_player_ctx.play_state == PLAY_PLAYING) {
                    g_player_ctx.elapsed_sec++;
                    Render_UpdateTime();
                    SSD1315_Refresh();  // 仅 2 页
                }
                break;
        }
    }
}
```

**关键行为**:

| 事件 | 调用 | 清屏? | Refresh 量 | 场景 |
|------|------|-------|-----------|------|
| `SCREEN_CHANGED` | `Render_*Screen()` | 是 (内部 Clear) | 8 页 | 页面跳转 |
| `DATA_CHANGED` (单标志) | `Render_Update*()` | 仅目标 Zone | 2 页 | 微调 |
| `DATA_CHANGED` (多标志) | 多次 `Render_Update*()` | 各目标 Zone | 2~6 页 | 同时变化 |
| `TICK_1S` (播放中) | `Render_UpdateTime()` | 仅 Zone 3 | 2 页 | 每秒 |

### 5.5 界面切换映射表

以下表格定义了 PRD 第 10 节状态机的每条状态转换, 对应的 UI 动作, 以及谁来发送 `ui_event_t`:

| 状态转换 (PRD 10.2) | 触发源 | mainTask 动作 | ui_event_t | uiTask 响应 |
|---------------------|--------|--------------|-----------|------------|
| 初始化 → 主界面 | 自动 | 扫描 SD 卡, 设置 g_player_ctx | `SCREEN_CHANGED` / `UI_SCREEN_MAIN` | `Render_MainScreen()` |
| 初始化 → 错误 | 自动 | 检测到异常 | `SCREEN_CHANGED` / `UI_SCREEN_ERROR` | `Render_ErrorScreen(msg)` |
| 主界面 → 菜单 | Menu 短按 | PushNavigationStack() | `SCREEN_CHANGED` / `UI_SCREEN_MENU` | `Render_MenuScreen(0)` |
| 菜单 → 主界面 | Menu 短按 | PopNavigationStack() | `SCREEN_CHANGED` / `UI_SCREEN_MAIN` | `Render_MainScreen()` |
| 菜单 → 音量调节 | OK 键 | 设置状态 | `SCREEN_CHANGED` / `UI_SCREEN_VOLUME` | `Render_MainScreen()` + 高亮 |
| 菜单 → 播放模式 | OK 键 | 设置状态 | `SCREEN_CHANGED` / `UI_SCREEN_PLAY_MODE` | `Render_MainScreen()` + 高亮 |
| 主界面播放/暂停 | OK 短按 | player_pause() / player_resume() | `DATA_CHANGED` / `play_state=1` | `Render_UpdateControlBar()` |
| 主界面上一曲 | L 短按 | player_prev() | `DATA_CHANGED` / `track=1` | `Render_UpdateTrackName()` |
| 主界面下一曲 | R 短按 | player_next() | `DATA_CHANGED` / `track=1` | `Render_UpdateTrackName()` |
| 音量调节 L/R | L/R 键 | 更新 g_player_ctx.volume | `DATA_CHANGED` / `volume=1` | `Render_UpdateTopBar()` |
| 播放模式切换 L/R | L/R 键 | 更新 g_player_ctx.play_mode | `DATA_CHANGED` / `play_mode=1` | `Render_UpdateTopBar()` |
| 错误 → 初始化 | 自动重试 | 重新扫描 | `SCREEN_CHANGED` / `UI_SCREEN_MAIN` 或 `UI_SCREEN_ERROR` | 对应渲染 |

### 5.6 导航栈接口

对应 PRD 10.3 节的 Menu 键导航规则, 导航栈由 mainTask 维护, 但接口定义应与 UI 类型放在一起:

```c
// App/include/ui_types.h

#define NAV_STACK_DEPTH_MAX 4

typedef struct {
    ui_screen_t stack[NAV_STACK_DEPTH_MAX];
    int8_t      sp;           // 栈顶指针, -1 表示空栈
} nav_stack_t;

/* 由 mainTask 调用 */
void NavStack_Init(nav_stack_t *ns);
void NavStack_Push(nav_stack_t *ns, ui_screen_t screen);
ui_screen_t NavStack_Pop(nav_stack_t *ns);        // 返回弹出后的栈顶 (即上一级)
void NavStack_Clear(nav_stack_t *ns);             // 清空 → 回到主界面
```

**Menu 键行为与导航栈的对应**:

| Menu 按键 | mainTask 操作 | 发送的 ui_event_t | 屏幕结果 |
|-----------|-------------|-------------------|---------|
| 短按 (<1s) | `NavStack_Pop()` → 返回上一级 | `SCREEN_CHANGED` / `上一级screen` | 回到上一级 |
| 长按 (>=1s) | `NavStack_Clear()` → 栈清空 | `SCREEN_CHANGED` / `UI_SCREEN_MAIN` | 直接回主界面 |

### 5.7 uiTask 与 mainTask 的队列契约

```c
// 队列创建 (在 freertos.c 的 MX_FREERTOS_Init 中)
QueueHandle_t ui_queue;
ui_queue = xQueueCreate(8, sizeof(ui_event_t));

// mainTask 发送示例: 通知 uiTask 切歌
void mainTask_notify_track_changed(void) {
    ui_event_t ev = {
        .type = UI_EVENT_DATA_CHANGED,
        .changed = { .flags = { .track = 1, .time_elapsed = 1 } },
    };
    xQueueSend(ui_queue, &ev, 0);
}

// mainTask 发送示例: 通知 uiTask 进入菜单
void mainTask_notify_enter_menu(void) {
    ui_event_t ev = {
        .type = UI_EVENT_SCREEN_CHANGED,
        .screen = UI_SCREEN_MENU,
    };
    xQueueSend(ui_queue, &ev, 0);
}
```

**队列容量选择**: 8 个槽位。最坏情况下 (按键连按 + 1s 定时器堆积) 仍有余量, 且 `xQueueSend` 使用 0 超时 (非阻塞), mainTask 不会因 UI 渲染慢而被阻塞。

### 5.8 文件创建清单

当前 `ui_render.h/c` 仅包含三个空函数。按上述接口定义, 需创建/修改以下文件:

| 文件 | 动作 | 内容 |
|------|------|------|
| `App/include/ui_types.h` | **新建** | `ui_screen_t`, `ui_event_t`, `ui_event_type_t`, `nav_stack_t`, `player_context_t` |
| `App/include/ui_render.h` | **修改** | 添加 Zone 坐标常量、完整渲染函数声明 |
| `App/src/ui_render.c` | **修改** | 实现 `Render_MainScreen`, `Render_MenuScreen`, `Render_ErrorScreen`, 四个 `Render_Update*` 函数 |
| `App/src/ui_task.c` | **新建 (可选)** | `StartUITask()` 的实现, 或放入 `Core/Src/freertos.c` |

### 5.9 开发顺序

```
Phase 1: 类型定义
  ├─ 创建 ui_types.h — ui_screen_t, ui_event_t, player_context_t
  ├─ 创建 player_context.c — g_player_ctx 全局变量定义
  └─ 编译通过, 确认类型无冲突

Phase 2: 字库支撑
  ├─ 创建 font_en.c — 8x16 ASCII 字库 → SSD1315_ShowChar/ShowString/ShowNum 实现
  └─ 硬编码测试: 在 main 中调用 ShowString 显示 "Hello World"

Phase 3: 全屏渲染
  ├─ 实现 Render_MainScreen() — 四区域绘制 (用硬编码测试数据)
  ├─ 实现 Render_ErrorScreen()
  ├─ 实现 Render_MenuScreen()
  └─ 验证: 上电后主界面正确显示四行内容

Phase 4: 局部刷新
  ├─ 实现四个 Render_Update*() 函数
  └─ 验证: 单区域更新耗时 ~6ms, 屏幕无闪烁

Phase 5: 集成
  ├─ uiTask 事件分发循环
  ├─ mainTask → ui_queue 的发送调用
  ├─ 按键 → 状态 → UI 全链路联调
  └─ 验证: PRD 12.1 功能验收清单全部通过
```

---

## 6. 运行时流程 -- 从按键到像素

本章追踪三个完整的运行时场景, 展示数据如何从硬按键开始, 经过 FreeRTOS 消息队列、状态机决策、UI 事件分发, 最终到达 OLED 的每个像素。

### 6.1 场景 A: 用户按 R 键切歌 (全屏刷新)

```
时间轴 (总计 ~50ms, < PERF-02 要求的 100ms)

t=0ms   [硬件] R 键 (PA12) 按下 → GPIO 低电平

t=5ms   [keyTask] KeyScanner_Scan() 检测到释放沿
           → key_event_t ev = {.id = KEY_R, .type = KEY_EVENT_SHORT}
           → xQueueSend(key_queue, &ev, 0)

t=6ms   [mainTask] xQueueReceive(key_queue) 拿到 KEY_R
           → 当前状态 = STATE_MAIN
           → player_next() 切换文件
           → 更新 g_player_ctx.current_track, track_name, total_sec
           → 构建 ui_event_t:
               { .type = UI_EVENT_DATA_CHANGED,
                 .changed.flags = { .track = 1, .time_elapsed = 1 } }
           → xQueueSend(ui_queue, &ev, 0)

t=7ms   [uiTask] xQueueReceive(ui_queue) 拿到事件
           → event.type = DATA_CHANGED
           → event.changed.flags.track = 1 → Render_UpdateTrackName()
           │    ├─ ClearZone(UI_ZONE1_Y)             // 擦除旧歌名区域
           │    │    └─ SetPixel(x, y, 0) * (128×16) // 仅修改 GRAM
           │    │       dirty_pages |= 0b00001100     // PAGE2+3
           │    │
           │    └─ ShowString(0, 16, g_player_ctx.track_name, 1)
           │         └─ 逐字符展开 8x16 点阵 → SetPixel(x, y, 1)
           │            dirty_pages 不变 (PAGE2+3 已在之前标记)
           │
           → event.changed.flags.time_elapsed = 1 → Render_UpdateTime()
           │    ├─ ClearZone(UI_ZONE3_Y)             // 擦除旧时间区域
           │    │    └─ SetPixel(x, y, 0) * (128×16)
           │    │       dirty_pages |= 0b11000000     // PAGE6+7
           │    │
           │    └─ ShowString(0, 48, "[00:00]/[03:45]", 1)
           │
           → SSD1315_Refresh()
                ├─ dirty_pages = 0b11001100 → 4 页脏
                ├─ PAGE2: BSP_OLED_WriteCmd + WriteData(128B) → ~3ms
                ├─ PAGE3: BSP_OLED_WriteCmd + WriteData(128B) → ~3ms
                ├─ PAGE6: BSP_OLED_WriteCmd + WriteData(128B) → ~3ms
                ├─ PAGE7: BSP_OLED_WriteCmd + WriteData(128B) → ~3ms
                └─ dirty_pages = 0

t=19ms  [BSP] 4 × HAL_I2C_Master_Transmit() 完成
           I2C 总线: START → 0x78(W) → 0x40 → D0~D127 → STOP (×4 次)

结果: 屏幕第 2 行显示新歌名, 第 4 行显示 [00:00]/[03:45]
      第 1 行和第 3 行无变化, 零 I2C 通信
      总 I2C 通信量: 512 字节 (vs 全屏刷新 1024 字节, 节省 50%)
```

### 6.2 场景 B: 每秒时间戳更新 (单区域刷新)

```
时间轴 (每秒重复一次, 每次 ~9ms)

[Software Timer 1s] → 回调中:
    ui_event_t ev = { .type = UI_EVENT_TICK_1S };
    xQueueSendFromISR(ui_queue, &ev, ...);

[uiTask] xQueueReceive → TICK_1S
    → if (g_player_ctx.play_state != PLAY_PLAYING) return; // 暂停时跳过
    → g_player_ctx.elapsed_sec++;
    → Render_UpdateTime()
         ├─ ClearZone(UI_ZONE3_Y)
         │    └─ dirty_pages = 0b11000000
         ├─ ShowString(0, 48, "[01:25]/[03:45]", 1)
         └─ SSD1315_Refresh()
              └─ PAGE6 128B (~3ms) + PAGE7 128B (~3ms)

结果: 仅第 4 行更新, 其余 3 行完全不参与 I2C 通信
      总耗时 ~6ms, 对音频播放无影响 (audioTask 优先级更高, 可抢占)
```

### 6.3 场景 C: Menu 短按 → 菜单 → 返回主界面 (两次全屏刷新)

```
t=0     [keyTask] KEY_MENU 短按 → key_queue

t=1     [mainTask] 当前 STATE_MAIN
          → NavStack_Push(UI_SCREEN_MAIN)
          → NavStack_Push(UI_SCREEN_MENU)
          → ui_event_t ev = {SCREEN_CHANGED, .screen = UI_SCREEN_MENU}
          → xQueueSend(ui_queue, &ev, 0)

t=2     [uiTask] SCREEN_CHANGED / MENU
          → Render_MenuScreen(0)
               ├─ SSD1315_Clear()             // 全屏写 0
               ├─ ShowString(0,  0, "Menu", 1)         // Zone 0: 标题
               ├─ ShowString(0, 16, "> 音量调节", 1)    // Zone 1: 选中项
               ├─ ShowString(0, 32, "  播放模式", 1)    // Zone 2
               ├─ ShowString(0, 48, "  文件列表", 1)    // Zone 3
               └─ SSD1315_Refresh()           // 8 页全刷新 ~25ms

用户看到菜单界面, 按 R/L 移动 ">" 光标:

t=100   [keyTask] KEY_R 短按 → key_queue
t=101   [mainTask] 更新 cursor = 1
          → ui_event_t ev = {SCREEN_CHANGED, .screen = UI_SCREEN_MENU}
          // 可优化为 DATA_CHANGED + cursor 字段, 避免全屏刷新

t=102   [uiTask] Render_MenuScreen(1)
          → ">" 移到第 3 行 → 全屏刷新 ~25ms

用户按 Menu 返回:

t=200   [keyTask] KEY_MENU 短按 → key_queue
t=201   [mainTask] NavStack_Pop() → 栈顶回到 UI_SCREEN_MAIN
          → ui_event_t ev = {SCREEN_CHANGED, .screen = UI_SCREEN_MAIN}

t=202   [uiTask] Render_MainScreen() → 全屏刷新 ~25ms
```

### 6.4 三种场景的性能对比

| 场景 | I2C 通信量 | 耗时 | 刷新页数 | 用户感知 |
|------|-----------|------|---------|---------|
| A: 切歌 (局部刷新) | 512 字节 | ~12ms | 4 页 | 无明显闪烁 |
| B: 时间更新 | 256 字节 | ~6ms | 2 页 | 完全无感 |
| C: 页面跳转 (全屏) | 1032 字节 | ~25ms | 8 页 | 轻微闪烁, 可接受 |
| 全屏刷新 (无脏页) | 1032 字节 | ~25ms | 8 页 | 对比基线 |

---

## 7. 开发与调试

### 7.1 开发阶段 (对应 Dev 第 2 节阶段规划)

| 阶段 | 目标 | 验证方式 | 当前状态 |
|------|------|---------|---------|
| 阶段 1.2 | OLED 点亮, 显示 "Hello World" | `SSD1315_Init()` + 画点/线 + 全屏刷新 | **已完成** |
| 阶段 3.1 | 字库制作 | `font_en.c` / `font_cn.c` 编译通过, Flash 占用可控 | **待实现** |
| 阶段 3.2 | 主界面渲染 | 不接播放控制, 硬编码测试数据显示四行布局 | **待实现** |
| 阶段 4 | 完整集成 | 播放状态变化 → 屏幕实时更新 | **待实现** |

### 7.2 常见问题排查

| 现象 | 可能原因 | 排查步骤 |
|------|---------|---------|
| 屏幕完全不亮 | I2C 地址错误 | 运行 `BSP_I2C_Scanner_Scan(&hi2c1, 10)` 确认实际地址 |
| 屏幕完全不亮 | 电荷泵未使能或顺序错误 | 确认 Init 顺序: `0x8D → 0x14 → ... → 0xAF` |
| 屏幕全亮 (无内容) | `0xA4` vs `0xA5` 配置错误 | `0xA5` = Force Entire Display ON, 正常显示需 `0xA4` |
| 画面闪烁 | 全屏刷新过于频繁 | 确认调用的是 `Refresh()` (脏页) 而非 `Clear()` + `Refresh()` |
| 内容上下颠倒 | COM 扫描方向 | 当前使用 `0xC8` (翻转); 若颠倒则改为 `0xC0` |
| 内容左右镜像 | SEG 重映射 | 当前使用 `0xA1` (镜像); 若镜像则改为 `0xA0` |
| I2C 通信超时 | 接线或上拉电阻缺失 | 检查 PB6/PB7 是否有 4.7kΩ 上拉到 3.3V |
| I2C 通信超时 | 速率过高 | 临时降为 Standard Mode (100kHz) 测试 |
| 图像有条纹/错位 | 页地址设置错误 | 检查 `Refresh()` 中 `0x22` 的起止页是否一致 |

### 7.3 性能基准

| 操作 | I2C 数据量 | 耗时 (400kHz) | 发生场景 |
|------|-----------|---------------|---------|
| 单条命令 | ~4 字节 | ~0.1ms | 初始化 |
| 刷新 1 脏页 | ~132 字节 | ~3ms | 仅修改 1 行中的 1 页 |
| 刷新 2 脏页 | ~264 字节 | ~6ms | 修改 1 行 (16px = 2 页)、时间更新 |
| 全屏刷新 8 页 | ~1033 字节 | ~25ms | Clear 后首次绘制、切歌 |
| 完整初始化 | ~1100 字节 | ~30ms | 仅上电一次 |
| uiTask 空闲 | 0 | 0 | 无事件时无通信 |

### 7.4 内存占用

| 数据结构 | 大小 | 位置 | 备注 |
|----------|------|------|------|
| `SSD1315_GRAM[8][128]` | 1024 字节 | RAM (static) | 镜像显存, PRD RAM 预算已分配 1KB |
| `dirty_pages` | 1 字节 | RAM (static) | |
| 英文字库 `font_en` | ~1.5KB | Flash (const) | 95 字符 × 16 字节, 不占 RAM |
| 中文字库 `font_cn` | ~20KB | Flash (const) | 取决于提取的字符数, 不占 RAM |

---

## 8. 附录

### 8.1 文件清单

| 文件 | 层 | 说明 |
|------|-----|------|
| `BSP/include/bsp_oled.h` | BSP | I2C 写命令/写数据接口 |
| `BSP/src/bsp_oled.c` | BSP | `HAL_I2C_Master_Transmit` 封装实现 |
| `BSP/include/bsp_i2c_scanner.h` | BSP (调试) | I2C 总线扫描接口 |
| `BSP/src/bsp_i2c_scanner.c` | BSP (调试) | 遍历 7-bit 地址探测 ACK |
| `App/include/ssd1315.h` | App (驱动) | SSD1315 控制器 API 声明 |
| `App/src/ssd1315.c` | App (驱动) | GDDRAM 模型 + 脏页刷新 + 绘图实现 |
| `App/include/ui_render.h` | App (UI) | UI 渲染接口声明 |
| `App/src/ui_render.c` | App (UI) | 界面组装 (当前为 stub) |
| `App/test/iic_scan_test.c` | App (测试) | I2C 扫描测试用例 |
| `docs/SSD1315/SSD1315_Command_Table.md` | 参考 | SSD1315 完整命令表 |

### 8.2 SSD1315 vs SSD1306

SSD1315 是 SSD1306 的后继型号, 命令集几乎完全兼容:

- SSD1315 新增内部 IREF 设置命令 `0xAD`, 可配置恒流源基准
- SSD1315 对比度范围更广
- 本工程命令序列同时兼容两种控制器

### 8.3 项目文档交叉引用

| 主题 | 参考文档 |
|------|---------|
| UI 布局定义 (主界面/菜单/子界面) | `docs/PRD.md` 第 5 节 |
| 按键交互定义 | `docs/PRD.md` 第 6 节 |
| 异常处理与错误信息 | `docs/PRD.md` 第 7 节 |
| 状态机设计 | `docs/PRD.md` 第 10 节 |
| I2C1 CubeMX 配置 | `docs/Dev.md` 第 1.2.5 节 |
| FreeRTOS 任务配置 | `docs/Dev.md` 第 1.4 节 |
| 字库制作指南 | `docs/Dev.md` 第 3.1 节 |
| 内存预算 | `docs/Dev.md` 第 3.1 节 |
| SSD1315 命令表 | `docs/SSD1315/SSD1315_Command_Table.md` |

### 8.4 外部参考资料

- SSD1306 数据手册: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
- STM32F072C8T6 参考手册 (I2C 章节)
- FatFS: http://elm-chan.org/fsw/ff/

### 8.5 版本历史

| 版本 | 日期 | 修改内容 | 作者 |
|------|------|---------|------|
| v0.1 | 2026-07-09 | 初稿 | leejkee |
| v0.2 | 2026-07-09 | 重构为需求驱动的结构: 需求功能 → 架构设计 → 代码实现 | leejkee |
