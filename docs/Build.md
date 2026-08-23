## 编译和运行
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

### 代码格式化

本项目固定使用 clang-format 22.1.8，本地与 CI 必须使用相同版本。格式化工具由跨平台 CMake 脚本提供，不需要配置 ARM 工具链或生成构建目录。

在仓库根目录格式化代码：

```bash
cmake -DCLANG_FORMAT_MODE=FORMAT -P cmake/clang-format.cmake
```

只检查格式而不修改文件：

```bash
cmake -DCLANG_FORMAT_MODE=CHECK -P cmake/clang-format.cmake
```

`CHECK` 是默认模式，因此也可简写为：

```bash
cmake -P cmake/clang-format.cmake
```

脚本会依次查找 `clang-format-22` 和 `clang-format`，并校验版本必须为 22.1.8。如果可执行文件未加入 `PATH`，可显式指定路径：

```bash
cmake -DCLANG_FORMAT_EXECUTABLE=/path/to/clang-format -DCLANG_FORMAT_MODE=FORMAT -P cmake/clang-format.cmake
```

格式化范围仅包含 `firmware/MiniAudioPlayerST/App/` 和 `firmware/MiniAudioPlayerST/BSP/` 下的 C/H 文件，并排除自动生成的字库文件。`Core/` 目录（包括 `Core/Src/main.c`）不会被格式化。

提交代码前应先在本地执行 `FORMAT`，检查差异后再提交。GitHub Actions 会执行 `CHECK` 作为合入前的格式校验，不会在 CI 中自动修改或提交源码。

### VS Code 补全与跳转

> 无论您是否选择使用gcc工具链，我们推荐您安装arm-gcc编译器到您的机器上，这样您可以直接使用我们提供的CMake来生成 `compile_command.json`，这样您可以直接通过 `vscode` + `clangd插件` 获得本项目代码的lsp 补全、跳转支持

在仓库根目录执行：

```bash
cmake --preset debug
```

CMake 会生成：

```text
build/debug/compile_commands.json
```

clangd 通过仓库中的 `.clangd` 自动使用该数据库。

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

`LOG_DEBUG` 在两套构建中均输出至 USART2：

- GCC/newlib 由 CMake 定义 `BSP_UART_USE_NEWLIB`，底层使用 `_write()`。
- Keil ARMCC 不定义该宏，底层使用 `fputc()`。

两种方式均使用 CubeMX 生成的 `huart2`，串口参数为 115200-8N1。

## 调试串口

### 硬件接线

STM32 PA2 (USART2_TX) / PA3 (USART2_RX) ←→ USB 转串口模块 (CH340 / FT232 / CP2102)。

### 固件控制

`BSP/include/bsp_config.h` 中的 `LOG_DEBUG`:
- **1**: 启用 `printf` 串口重定向和 `LOG_DEBUG`
- **0**: 调试打印代码编译期裁剪，但 USART2 仍由 CubeMX 初始化

USART2 的引脚、波特率和硬件初始化由 CubeMX 工程配置管理。

### 上位机

使用 Chrome/Chromium 打开 WebSerial 上位机，波特率为 115200。建议从仓库根目录启动本地服务器：

```bash
python -m http.server 8000 -d tools/WebSerial
```

然后访问 `http://localhost:8000`。
