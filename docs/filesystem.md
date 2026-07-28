# OLED 文件列表视图设计

## 设计目标

MCU 端实现类似 QT ListView 的“窗口滚动”列表显示，只做 UI 层（显示 + 高亮），不涉及选中后的业务逻辑。核心约束：极致压缩内存，后台只维护一屏（4行）的数据窗口，而非整份列表。

## 状态设计

Model 层不是"数组 + index"两个量，而是三个：

```
struct ListView {
    int cursor;         // 全局选中项的绝对索引 (0 ~ total-1)
    int window_start;   // 当前窗口第一行对应的绝对索引
    int total;          // 列表总项数
    char buf[4][MAX_FNAME_LEN];  // 当前窗口显示内容（数据副本，非指针）
}
```

**为什么不用 `const char* list[4]`：** 若文件系统读取接口（如 `f_readdir`）复用同一静态缓冲区返回文件名，4 个指针可能全部指向同一块内存，导致 4 行显示内容相同。改用固定长度字符数组，reload 时逐行拷贝，多花几十字节内存换正确性。

## 核心逻辑：移动与窗口判断

```
function listview_move(lv, delta):
    new_cursor = lv.cursor + delta
    if new_cursor < 0 or new_cursor >= lv.total:
        return NO_CHANGE          // 到达边界，忽略

    lv.cursor = new_cursor
    need_reload = false

    if lv.cursor < lv.window_start:
        lv.window_start = lv.cursor
        need_reload = true
    else if lv.cursor >= lv.window_start + 4:
        lv.window_start = lv.cursor - 3
        need_reload = true

    if need_reload:
        return WINDOW_CHANGED      // 需要重新拉取4行数据 + 整屏重绘
    else:
        return HIGHLIGHT_ONLY      // 只是高亮行变化，局部重绘
```

**边界情况：**
- `total < 4`：窗口不满4行，多余行留空不绘制
- `total == 0`：空列表单独处理，避免 window_start 出现异常
- 按键防抖在按键驱动层处理，不进 listview 逻辑

## 数据填充（仅在 WINDOW_CHANGED 时触发）

```
function listview_reload(lv):
    for i in 0..3:
        abs_index = lv.window_start + i
        if abs_index < lv.total:
            lv.buf[i] = sd_get_filename_at(abs_index)   // 拷贝，非存指针
        else:
            lv.buf[i] = ""   // 空行
```

## 渲染层：与 Model 状态解耦

渲染函数只接收"相对高亮行号"，不感知 cursor / window_start 的关系：

```
function oled_render_list(buf[4], highlight_row, full_redraw):
    if full_redraw:
        for row in 0..3:
            draw_text(row, buf[row])
            if row == highlight_row:
                invert(row)
    else:
        // 只重绘旧高亮行与新高亮行，其余不动
        invert_toggle(prev_highlight_row)
        invert_toggle(highlight_row)
```

调用方计算 `highlight_row = cursor - window_start` 后传入。

## 调用流程总结

```
on_key_up/down(delta):
    result = listview_move(lv, delta)
    if result == WINDOW_CHANGED:
        listview_reload(lv)
        oled_render_list(lv.buf, lv.cursor - lv.window_start, full_redraw=true)
    else if result == HIGHLIGHT_ONLY:
        oled_render_list(lv.buf, lv.cursor - lv.window_start, full_redraw=false)
    // NO_CHANGE: 不做任何操作
```

## 内存开销

- `ListView` 结构体本身：3 个 int，约 12 字节
- `buf[4][MAX_FNAME_LEN]`：唯一的数据副本，按 8.3 文件名格式约 4×13 = 52 字节
- 无需保存整份文件列表，SD 端只需支持"按绝对索引取第 N 个文件名"的接口

## 扩展性

后续若要支持选中后的操作（进入目录、播放等），只需在 `cursor` 基础上加一个"确认选中"事件，不影响现有 Model/View 结构；渲染层完全不用改动。