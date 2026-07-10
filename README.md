# MiniAudioPlayerST

基于 STM32F072C8T6 的 MP3 音频播放器，SD 卡存储 + VS1003 解码 + OLED 显示。

## 硬件平台

| 模块 | 型号 | 接口 |
|------|------|------|
| MCU | STM32F072C8T6 (Cortex-M0, 48MHz, 64KB Flash, 16KB RAM) | - |
| 音频解码 | VS1003 | SPI2 |
| 存储 | microSD (CH376S 模块, SPI 模式, FAT32) | SPI1 |
| 显示 | MAP2606 OLED (128x64) | I2C1 |
| 按键 | 4 键 (Menu/OK/L/R) | GPIO |
| 调试串口 | USART2 (PA2-TX / PA3-RX), 115200-8N1 | 外接 USB 转串口模块 |

> **引脚预留**: PA2 和 PA3 由调试串口占用, 请勿分配给其他外设。
> CubeMX 中无需配置 USART2, BSP 层自包含 HAL 初始化代码。

## 目录结构

```
MiniAudioPlayerST/
├── docs/                    # 项目文档
│   ├── PRD.md               # 产品需求文档
│   └── Dev.md               # 开发文档（CubeMX 配置、开发步骤）
├── firmware/
│   └── MiniAudioPlayerST/   # STM32CubeMX 生成的 MDK-ARM 工程
│       ├── Core/             # CubeMX 生成代码
│       ├── BSP/              # 板级支持包 (OLED/按键/串口/I2C)
│       ├── App/              # 应用层
│       ├── Drivers/          # HAL 库 + CMSIS
│       └── MDK-ARM/          # Keil 工程文件
├── tools/
│   └── WebSerial/            # Web 串口调试工具
├── generate_compile_commands.ps1  # clangd 编译数据库生成脚本
├── compile_commands.json    # clangd 编译数据库（自动生成，不提交 git）
└── .clangd                  # clangd 配置文件
```

## 调试串口

### 硬件接线

STM32 PA2 (USART2_TX) / PA3 (USART2_RX) ←→ USB 转串口模块 (CH340 / FT232 / CP2102)。

### 固件控制

`BSP/include/bsp_config.h` 中的 `BSP_DEBUG_UART`:
- **1**: PA2/PA3 初始化为 USART2, `printf` 重定向到串口
- **0**: 所有调试代码编译期裁剪, PA2/PA3 释放

CubeMX 无需配置 USART2 — BSP 层 `bsp_uart.c` 自包含 GPIO 和 USART 初始化。

### 上位机

浏览器打开 `tools/WebSerial/index.html`, 波特率 115200。

## 快速开始

### 前置条件

- STM32CubeMX
- Keil MDK-ARM (uVision 5)
- VS Code + clangd 扩展 (LLVM)

### 生成工程

1. 打开 `firmware/MiniAudioPlayerST/MiniAudioPlayerST.ioc` -> GENERATE CODE
2. 在 VS Code 中打开仓库根目录，clangd 自动激活

### 生成 clangd 编译数据库

CubeMX 每次重新生成代码后：

```powershell
.\generate_compile_commands.ps1
```

### 构建与烧录

- TODO: 补充编译与烧录命令 / 步骤

## 测试

### 运行测试

在 `main.c` 中 `#include` 对应的测试头文件，并在 `main()` 的 `while(1)` 之前调用测试入口函数：

```c
/* USER CODE BEGIN Includes */
#include "ssd1315_test.h"
/* USER CODE END Includes */

int main(void)
{
    // ... HAL_Init / SystemClock_Config / MX_*_Init ...
    /* USER CODE BEGIN 2 */
    SSD1315_Test_RunAll();
    /* USER CODE END 2 */

    while (1) { }
}
```

测试完成后注释掉调用即可，不影响后续开发。

### 测试清单

#### SSD1315 OLED 驱动测试 (`App/test/ssd1315_test.c`)

调用 `SSD1315_Test_RunAll()`，每步停顿 2 秒供肉眼观察，串口同步输出 PASS/FAIL，总耗时约 30~40 秒。

| 编号 | 测试项 | 验证内容 | 预期画面 |
|------|--------|---------|---------|
| 0 | Init | 初始化序列 + 电荷泵使能 | 屏幕从随机噪点变为全黑 |
| 1 | DrawPoint ×5 | 四角 + 中心像素点亮 | 5 个白点可见 |
| 2 | ClearPoint | 擦除中心像素 | 中心点消失，四角保留 |
| 3 | Clear | 全屏清除 | 全黑 |
| 4 | DrawLine ×6 | 水平/垂直/对角线 | 矩形边框 + X 形 |
| 5 | DrawCircle ×3 | 画圆 | 左右大圆 + 中心小圆 |
| 6 | ColorTurn | 反色/正常切换 | 画面翻转后恢复 |
| 7 | Display On/Off | 显示开关 | 屏幕熄灭后恢复 |
| 8 | Full-screen Fill | 全屏填充 + 清除 | 全白 → 全黑 |
| 9 | Vertical Stripes | 竖条纹 (每 8 列) | 16 条等间距竖线 |
| 10 | Horizontal Stripes | 横条纹 (每 8 行) | 8 条等间距横线 |
| 11 | Boundary | 边界坐标 + 越界不宕机 | 两个角点可见，无 HardFault |

#### I2C Scanner 测试 (`App/test/iic_scan_test.c`)

调用 `BSP_I2C_Scanner_Test(&hi2c1, 10)`，扫描 I2C1 总线上 0x00~0x7F 地址，串口打印响应设备的地址。

> 后续新增测试模块可追加到本节。

## 开发阶段

| 阶段 | 目标 | 状态 |
|------|------|------|
| 1. 硬件驱动验证 | OLED 点亮、SD 卡读写、VS1003 发单音 | TODO |
| 2. 基础播放链路 | SD 卡 -> FatFS -> VS1003 播放 MP3 | TODO |
| 3. UI 框架 | 屏幕布局渲染 + 按键响应 | TODO |
| 4. 完整功能集成 | 播放控制 + 菜单 + 模式切换 | TODO |
| 5. 异常处理与打磨 | 所有异常路径 + 体验优化 | TODO |

## 功能清单

- 播放/暂停、上一曲/下一曲、音量调节
- 四种播放模式：顺序、列表循环、单曲循环、随机
- SD 卡热插拔检测
- 异常处理：无卡、无文件、解码器错误等

## 文档

- [产品需求文档 (PRD)](docs/PRD.md)
- [开发文档](docs/Dev.md)

## 参考资源

- [STM32F072C8T6 数据手册](https://www.st.com/resource/en/datasheet/stm32f072c8.pdf)
- [VS1003 数据手册](https://www.vlsi.fi/fileadmin/datasheets/vs1003.pdf)
- [FatFS](http://elm-chan.org/fsw/ff/)
- [SSD1306 数据手册](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
