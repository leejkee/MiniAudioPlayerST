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

## 目录结构

```
MiniAudioPlayerST/
├── docs/                    # 项目文档
│   ├── PRD.md               # 产品需求文档
│   └── Dev.md               # 开发文档（CubeMX 配置、开发步骤）
├── firmware/
│   └── MiniAudioPlayerST/   # STM32CubeMX 生成的 MDK-ARM 工程
│       ├── Core/             # 用户代码
│       ├── Drivers/          # HAL 库 + CMSIS
│       └── MDK-ARM/          # Keil 工程文件
├── generate_compile_commands.ps1  # clangd 编译数据库生成脚本
├── compile_commands.json    # clangd 编译数据库（自动生成，不提交 git）
└── .clangd                  # clangd 配置文件
```

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
