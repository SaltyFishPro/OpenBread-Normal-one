# 圆弧转盘 GIF 生成器

Python 3.10+ 桌面工具，使用 Pillow 生成 GIF，Tkinter 提供文字编辑窗口。支持中文字体。此工具用于交互效果设计，不接入设备固件。

默认加载 `openbread.json`：384×168 像素、18px 字号，适配设备屏幕尺寸。英文 `example.json` 仍保留大尺寸参考效果。

## 运行

从仓库根目录执行：

```powershell
python -m pip install -r tools/arc_menu/requirements.txt
python tools/arc_menu/arc_menu.py
```

在窗口中每行输入 `选项名称 | 右侧数值`，例如：

```text
设置 | 系统
音乐 | 12 首
阅读 | 词库
时钟 | 12:30
无线功能 | 关闭
游戏 | 2 项
```

修改左侧标题、尺寸、字号、帧率、停留时间、转动时间或字体路径，然后点击“生成 GIF”。右侧数值可留空。默认输出为 [output/arc_menu.gif](output/arc_menu.gif)（运行后生成）。导出期间保持窗口打开。

## 动态预览

打开窗口后，右侧自动播放当前配置。修改左侧文字或参数后点击“更新预览”，应用修改并从第一项重新播放；“暂停 / 播放”控制动画。预览无需先生成 GIF，使用与导出相同的绘图和帧序列。

可选 1× 原始像素或 2× 最近邻放大查看，空间不足时自动缩小。右侧明确显示导出尺寸和实际预览倍率；预览放大不改变 GIF 的 384×168 尺寸。预览运行速度也受电脑绘图速度影响，导出的 GIF 按配置帧时长播放。

当前为白底黑字、蓝色焦点的桌面效果预览，未模拟设备单色化及实际屏幕刷新。

## 保存配置

使用“保存配置”保存 JSON，下次通过 `--config` 加载。Windows 默认选择微软雅黑；其他平台建议指定支持中文的字体。字体路径可为绝对路径，也可相对于配置文件。

附带 [openbread.json](openbread.json) 中文菜单示例，可直接打开编辑：

```powershell
python tools/arc_menu/arc_menu.py --config tools/arc_menu/openbread.json
```

专注时钟视觉预览使用 [focus_clock.json](focus_clock.json)：

```powershell
python tools/arc_menu/arc_menu.py --config tools/arc_menu/focus_clock.json --output tools/arc_menu/output/focus_clock.gif
```

该配置只用于确认圆弧转盘的菜单层级和视觉效果，`OK`、计时开始、时长选择等暂时没有接入固件。

## 命令行直接生成

```powershell
python tools/arc_menu/arc_menu.py --config tools/arc_menu/example.json --output tools/arc_menu/output/arc_menu.gif
```

命令行模式直接覆盖指定输出文件。可编辑 [example.json](example.json) 中的 `items`：`label` 是选项文字，`value` 是右侧说明。颜色为白底黑字、蓝色焦点。长文字会自动缩小到可用宽度。

## 动画逻辑

蓝点固定在焦点处，选项沿圆弧依次上移，文字角度随位置变化。每次转动一格，使用平滑加减速，到位后更新右侧数值并停留。最后一项转回第一项，GIF 无限循环。此 GIF 是自动播放演示，本身不响应按键或拖动。

`hold_ms` 控制停留时间，`move_ms` 控制转动时长，`fps` 控制转动帧率；GIF 时间按 10ms 精度取整。源代码对选项数、画布尺寸和总帧像素量设有上限，避免导出时占用过多内存。
