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