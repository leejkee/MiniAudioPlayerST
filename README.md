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

> **引脚预留**: PA2 和 PA3 由 CubeMX 配置的 USART2 调试串口占用，请勿分配给其他外设。

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
│   ├── WebSerial/            # Web 串口调试工具
│   └── FontTool/             # 字库 C 代码生成脚本
├── cmake/
│   ├── arm-none-eabi-gcc.cmake    # Arm GNU 工具链配置
│   ├── STM32F072C8Tx_FLASH.ld     # GCC 链接脚本
│   └── openocd.cmake              # OpenOCD/ST-Link 烧录目标
├── CMakeLists.txt            # GCC 构建与模块定义
├── CMakePresets.json         # Debug/Release 跨平台预设
└── .clangd                   # clangd 编译数据库位置
```

## 调试串口

### 硬件接线

STM32 PA2 (USART2_TX) / PA3 (USART2_RX) ←→ USB 转串口模块 (CH340 / FT232 / CP2102)。

### 固件控制

`BSP/include/bsp_config.h` 中的 `BSP_DEBUG_UART`:
- **1**: 启用 `printf` 串口重定向和 `BSP_DEBUG_PRINTF`
- **0**: 调试打印代码编译期裁剪，但 USART2 仍由 CubeMX 初始化

USART2 的引脚、波特率和硬件初始化由 CubeMX 工程配置管理。

### 上位机

使用 Chrome/Chromium 打开 WebSerial 上位机，波特率为 115200。建议从仓库根目录启动本地服务器：

```bash
python -m http.server 8000 -d tools/WebSerial
```

然后访问 `http://localhost:8000`。

### 字库生成工具

`tools/FontTool/` 包含 4 个 Python 脚本，生成 SSD1315 兼容的字库 C 代码 (column-major GRAM 格式)：

| 脚本 | 用途 | 依赖 |
|------|------|------|
| `generate_ascii_font.py` | 内置 VGA 8x16 控制台位图，无需外部字体文件，生成 ASCII 0x20–0x7E (95 字) | 无 |
| `generate_cn_font.py` | 扫描 App/ 源码提取 CJK 字符 + 内置 UI 必选字，调用 SimSun 渲染 16x16 点阵 | Pillow |
| `scan_dir_font.py` | 扫描目录文件名提取 CJK 字符，调用 SimSun 渲染 16x16 点阵 | Pillow |
| `generate_emoji_font.py` | 将单码点 Unicode 图标渲染为多组固定尺寸的单色点阵，支持非 BMP 码点 | Pillow |

ASCII 和中文脚本默认输出至 `App/include/font_*.h` 与 `App/src/font_*.c`。Unicode 图标脚本默认输出至临时目录 `tmp/FontTool/include/font_icon.h` 与 `tmp/FontTool/src/font_icon.c`。

```powershell
# ASCII 字库 (零依赖，确定性输出)
python tools/FontTool/generate_ascii_font.py

# 中文字库 — 扫描源码 (UI 标签：歌曲播放器、正在播放 等)
python tools/FontTool/generate_cn_font.py

# 中文字库 — 扫描 SD 卡音乐目录 (歌名显示)
python tools/FontTool/scan_dir_font.py E:\music
python tools/FontTool/scan_dir_font.py E:\music --preview    # + ASCII 预览

# 生成 12x12、16x16 两种视觉尺寸，字模单元统一为 16x16
python tools/FontTool/generate_emoji_font.py --preview

# 指定图标和尺寸；U+FE0F 变体选择符可省略
python tools/FontTool/generate_emoji_font.py `
    --codepoints U+25B6 U+23F8 U+23EE U+23ED `
    --small-size 12 `
    --large-size 16

# 确认预览后，显式生成到固件 App 目录
python tools/FontTool/generate_emoji_font.py `
    --small-size 12 `
    --large-size 16 `
    --out-dir firmware/MiniAudioPlayerST/App
```

> **注意**: 中文字库生成需要 Windows SimSun 字体 (`C:\Windows\Fonts\simsun.ttc`)，首次使用需 `pip install Pillow`。新增或删除生成的 `.c` 文件后，需要同步更新 `CMakeLists.txt` 和 Keil 工程文件。

Unicode 图标脚本默认优先使用 `Segoe UI Symbol`，其次使用 `Segoe UI Emoji`，也可通过 `--font` 指定包含目标图标的 TTF/OTF 字体。输出的 `font_icon.h/.c` 包含 `font_icon_lookup()`、`font_icon_small_16x16` 和 `font_icon_large_16x16`。两套数组使用完全相同的 Unicode 索引顺序、16x16 单元、2 个 OLED page 和 32 字节数据；默认将 12x12 小图形居中放置在单元内，大图形使用 16x16 视觉范围。脚本按单个 Unicode 码点查表，不支持 ZWJ、旗帜、肤色修饰等组合 emoji。

固件侧先按码点取索引，再按按键状态直接选择二维数组：

```c
uint16_t icon_index = font_icon_lookup(0x25B6U);

if (icon_index != FONT_ICON_NOT_FOUND) {
    const uint8_t *bitmap = key_pressed
                            ? font_icon_large_16x16[icon_index]
                            : font_icon_small_16x16[icon_index];
    /* 将 bitmap 按 2 pages x 16 columns 写入 OLED 显存。 */
}
```

## 快速开始

项目支持两套相互独立的固件构建后端：

| 开发模式 | 编辑与代码导航 | 编译、链接 | 烧录与调试 |
|----------|----------------|------------|------------|
| GCC 全流程 | VS Code + clangd | CMake + `arm-none-eabi-gcc` | OpenOCD + ST-Link |
| VS Code + Keil | VS Code + clangd（使用 GCC 编译数据库） | Keil ARMCC | Keil IDE |
| Keil IDE | Keil 编辑器 | Keil ARMCC | Keil IDE |

CMake/GCC 和 Keil 共用同一份固件源码，但构建配置彼此独立：

- `CMakeLists.txt` 只管理 GCC 构建，不调用 ARMCC，也不替代 Keil 工程。
- `firmware/MiniAudioPlayerST/MDK-ARM/MiniAudioPlayerST.uvprojx` 继续由 Keil 管理。
- 使用 Keil 作为最终编译器时，仍可在 Windows 或 Linux 上运行一次 CMake 配置，生成 `compile_commands.json`，供 clangd 提供准确的补全、诊断和跳转。
- 新增或删除源文件时，需要同时更新 CMake 源文件列表和 Keil 工程。

### GCC/CMake 开发环境

Linux 和 Windows 均需要：

- CMake 3.22 或更高版本
- Ninja
- Arm GNU Toolchain（`arm-none-eabi-gcc`）
- VS Code + clangd + CMake Tools

需要通过 ST-Link 烧录或调试时，还需要：

- OpenOCD
- `arm-none-eabi-gdb`
- VS Code Cortex-Debug（可选）

STM32CubeMX 和 Keil MDK-ARM 只在重新生成 CubeMX 代码或使用 Keil/ARMCC 构建时需要。

### VS Code 补全与跳转

在仓库根目录执行：

```bash
cmake --preset debug
```

CMake 会生成：

```text
build/debug/compile_commands.json
```

clangd 通过仓库中的 `.clangd` 自动使用该数据库。仅使用 Keil 构建固件的开发者也需要执行这一步，但不要求使用 GCC 生成的 ELF 作为最终固件。

当编译宏、头文件目录或源文件列表变化时，重新执行 `cmake --preset debug`。普通 `.c/.h` 内容修改不需要重新配置，clangd 会自动更新索引。

### GCC 编译

Debug：

```bash
cmake --preset debug
cmake --build --preset debug
```

Release：

```bash
cmake --preset release
cmake --build --preset release
```

产物位于 `build/debug/` 或 `build/release/`：

```text
MiniAudioPlayerST.elf
MiniAudioPlayerST.hex
MiniAudioPlayerST.bin
MiniAudioPlayerST.map
```

### GCC + OpenOCD 烧录

连接一个 ST-Link 时：

```bash
cmake --build --preset debug --target flash
```

该目标会先构建 ELF，再通过 OpenOCD 写入、校验并复位 STM32。连接多个 ST-Link 时可指定探针序列号：

```bash
cmake --preset debug -DSTLINK_SERIAL=<serial-number>
cmake --build --preset debug --target flash
```

### Keil/ARMCC 编译与烧录

使用 Keil 打开：

```text
firmware/MiniAudioPlayerST/MDK-ARM/MiniAudioPlayerST.uvprojx
```

在 Keil IDE 中执行 Build、Download 或 Debug。Keil 不读取 CMake 构建规则；CMake 生成的 GCC 编译数据库只为 VS Code/clangd 提供开发信息。

### 两套编译器的串口输出

`BSP_DEBUG_PRINTF` 在两套构建中均输出至 USART2：

- GCC/newlib 由 CMake 定义 `BSP_UART_USE_NEWLIB`，底层使用 `_write()`。
- Keil ARMCC 不定义该宏，底层使用 `fputc()`。

两种方式均使用 CubeMX 生成的 `huart2`，串口参数为 115200-8N1。

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

#### SSD1315 字体显示测试 (`App/test/ssd1315_font_test.c`)

调用 `SSD1315_Font_Test_RunAll()`，每步停顿 1 秒供肉眼观察，串口同步输出 PASS/FAIL + 每个汉字的查找结果，总耗时约 10~15 秒。

| 编号 | 测试项 | 验证内容 | 预期画面 |
|------|--------|---------|---------|
| 0 | Init | 初始化 SSD1315 | 屏幕全黑 |
| 1 | ASCII (size 16) | 显示字符 `A B C D ~ !` 和字符串 `Hello, SSD1315!` | 第 1 行 6 个 ASCII 字符，第 2 行完整英文句子 |
| 2 | 系统字库汉字 (FONT_TYPE_SYSTEM) | `font_cn` 字库查找并显示 9 个 UI 用字 | 第 3 行 `歌曲播放器`，第 4 行 `正在播放` |
| 3 | 文件字库汉字 (FONT_TYPE_FILE) | `font_file_cn` 字库查找并显示 4 首歌名 | 第 1 行 `十年`，第 2 行 `月光`，第 3 行 `我的梦`，第 4 行 `时光` |

> **字库说明**: `font_cn` (FONT_TYPE_SYSTEM) 内置 59 个 UI 标签用字 (如 歌曲播放器正在上下左右确定返回菜单)；`font_file_cn` (FONT_TYPE_FILE) 内置 85 个字覆盖 10 首示例歌名 (十年、月光、我的梦、时光 等)。两个独立字库分别对应播放器 UI 渲染和 SD 卡歌曲名显示两种场景。

> 后续新增测试模块可追加到本节。

## 开发阶段

| 阶段 | 目标 | 状态 |
|------|------|------|
| 1. 硬件驱动验证 | OLED 点亮、SD 卡读写、VS1003 发单音 | OK |
| 2. 基础播放链路 | SD 卡 -> FatFS -> VS1003 播放 MP3 | OK |
| 3. UI 框架 | 屏幕布局渲染 + 按键响应 | OK |
| 4. 完整功能集成 | 播放控制 + 菜单 + 模式切换 | IN PROCESS |
| 5. 异常处理与打磨 | 所有异常路径 + 体验优化 | TODO |

## 功能清单

- 播放/暂停、上一曲/下一曲、音量调节
- 两种播放模式：列表循环、单曲循环
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
