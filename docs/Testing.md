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

#### SSD1315 OLED 驱动测试 (`App/test/src/ssd1315_test.c`)

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

#### I2C Scanner 测试 (`App/test/src/iic_scan_test.c`)

调用 `BSP_I2C_Scanner_Test(&hi2c1, 10)`，扫描 I2C1 总线上 0x00~0x7F 地址，串口打印响应设备的地址。

#### SSD1315 字体显示测试 (`App/test/src/ssd1315_font_test.c`)

调用 `SSD1315_Font_Test_RunAll()`，每步停顿 1 秒供肉眼观察，串口同步输出 PASS/FAIL + 每个汉字的查找结果，总耗时约 10~15 秒。

| 编号 | 测试项 | 验证内容 | 预期画面 |
|------|--------|---------|---------|
| 0 | Init | 初始化 SSD1315 | 屏幕全黑 |
| 1 | ASCII (size 16) | 显示字符 `A B C D ~ !` 和字符串 `Hello, SSD1315!` | 第 1 行 6 个 ASCII 字符，第 2 行完整英文句子 |
| 2 | 系统字库汉字 (FONT_TYPE_SYSTEM) | `font_cn` 字库查找并显示 9 个 UI 用字 | 第 3 行 `歌曲播放器`，第 4 行 `正在播放` |
| 3 | 文件字库汉字 (FONT_TYPE_FILE) | `font_file_cn` 字库查找并显示 4 首歌名 | 第 1 行 `十年`，第 2 行 `月光`，第 3 行 `我的梦`，第 4 行 `时光` |

> **字库说明**: `font_cn` (FONT_TYPE_SYSTEM) 内置 59 个 UI 标签用字 (如 歌曲播放器正在上下左右确定返回菜单)；`font_file_cn` (FONT_TYPE_FILE) 内置 85 个字覆盖 10 首示例歌名 (十年、月光、我的梦、时光 等)。两个独立字库分别对应播放器 UI 渲染和 SD 卡歌曲名显示两种场景。

> 后续新增测试模块可追加到本节。