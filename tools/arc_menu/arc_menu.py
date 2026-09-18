"""Editable arc menu GIF exporter. Requires Pillow; GUI uses standard Tkinter."""
from __future__ import annotations

import argparse
import json
import math
import queue
import threading
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
DEFAULT_CONFIG = HERE / "openbread.json"
DEFAULT_OUTPUT = HERE / "output" / "arc_menu.gif"


def font_path(config: dict, base: Path) -> str:
    requested = config.get("font", "").strip()
    if requested:
        path = Path(requested)
        path = path if path.is_absolute() else base / path
        if not path.is_file():
            raise ValueError(f"字体文件不存在：{path}")
        return str(path)
    for name in (
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ):
        if Path(name).is_file():
            return name
    raise ValueError("未找到字体，请在 font 中指定支持所需文字的 TTF/OTF/TTC 字体。")


def validate(config: dict) -> None:
    limits = {
        "width": (384, 1600), "height": (168, 1000),
        "font_size": (12, 100), "fps": (5, 30),
        "hold_ms": (100, 3000), "move_ms": (100, 2000),
    }
    for key, (low, high) in limits.items():
        value = config.get(key)
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f"{key} 必须为 {low}～{high} 的整数。")
    items = config.get("items")
    if not isinstance(items, list) or not 2 <= len(items) <= 24:
        raise ValueError("请填写 2～24 个选项。")
    for item in items:
        if not isinstance(item, dict) or not isinstance(item.get("label"), str):
            raise ValueError("每个选项需要 label 字符串。")
        if not item["label"].strip() or len(item["label"]) > 80:
            raise ValueError("选项名称不能为空，且不能超过 80 个字符。")
        if not isinstance(item.get("value", ""), str) or len(item.get("value", "")) > 40:
            raise ValueError("右侧数值应为不超过 40 字符的字符串。")
    if not isinstance(config.get("caption", ""), str):
        raise ValueError("caption 必须为字符串。")


def fitted_font(path: str, text: str, size: int, max_width: int):
    font = ImageFont.truetype(path, size)
    while font.getlength(text) > max_width and size > 6:
        size -= 1
        font = ImageFont.truetype(path, size)
    if font.getlength(text) > max_width:
        raise ValueError(f"文字过长，请缩短文字或增大画布：{text}")
    return font


class ArcRenderer:
    def __init__(self, config: dict, base: Path):
        validate(config)
        self.config = config
        self.width, self.height = config["width"], config["height"]
        self.path = font_path(config, base)
        self.font_size = min(config["font_size"], self.height // 9)
        self.radius = self.width * 0.84
        self.pivot_x = -self.width * 0.46
        self.spacing = max(self.font_size * 1.55, self.height * 0.085)
        self.angle_step = self.spacing / self.radius
        self.sprites = [self.label_sprite(i["label"]) for i in config["items"]]
        # Shared fixed palette avoids color flicker across GIF frames.
        self.palette = Image.new("P", (1, 1))
        colors = [v for gray in range(255) for v in (gray, gray, gray)]
        colors += [0, 86, 255]
        self.palette.putpalette(colors)

    def label_sprite(self, label: str) -> Image.Image:
        # Render at 2x resolution before rotation for smooth text edges.
        font = fitted_font(self.path, label, self.font_size * 2, int(self.width * 0.43) * 2)
        bounds = font.getbbox(label)
        width, height = bounds[2] - bounds[0], bounds[3] - bounds[1]
        # Text starts at the centre of a transparent tile, so rotation keeps
        # its left-middle anchor exactly on the circular path.
        tile = Image.new("RGBA", (2 * (width + 8), 2 * (height + 8)))
        draw = ImageDraw.Draw(tile)
        draw.text((tile.width // 2 - bounds[0], tile.height // 2 - height / 2 - bounds[1]),
                  label, font=font, fill="black")
        return tile

    def frame(self, position: float, selected: int) -> Image.Image:
        canvas = Image.new("RGB", (self.width, self.height), "white")
        draw = ImageDraw.Draw(canvas)
        cy = self.height / 2
        dot_x = self.width * 0.285
        dot_r = max(4, self.font_size * 0.36)
        draw.ellipse((dot_x - dot_r, cy - dot_r, dot_x + dot_r, cy + dot_r),
                     fill=(0, 86, 255))
        caption = self.config.get("caption", "")
        cap_font = fitted_font(self.path, caption, self.font_size, int(self.width * 0.23))
        draw.text((self.width * 0.018, cy), caption, font=cap_font, fill="black", anchor="lm")
        value = self.config["items"][selected].get("value", "")
        value_font = fitted_font(self.path, value, self.font_size, int(self.width * 0.15))
        draw.text((self.width * 0.96, cy), value, font=value_font, fill="black", anchor="rm")

        # Draw repeated items above/below the focus, including across the wrap.
        visible = math.ceil(self.height / (2 * self.spacing)) + 3
        center = math.floor(position)
        for index in range(center - visible, center + visible + 1):
            angle = (index - position) * self.angle_step
            if abs(angle) > math.pi / 2:
                continue
            x = self.pivot_x + self.radius * math.cos(angle)
            y = cy + self.radius * math.sin(angle)
            tile = self.sprites[index % len(self.sprites)].rotate(
                -math.degrees(angle), resample=Image.Resampling.BICUBIC, expand=True)
            tile = tile.resize((max(1, tile.width // 2), max(1, tile.height // 2)),
                               Image.Resampling.LANCZOS)
            canvas.paste(tile, (round(x - tile.width / 2), round(y - tile.height / 2)), tile)
        return canvas.quantize(palette=self.palette, dither=Image.Dither.NONE)


def animation_timing(config: dict) -> tuple[int, int, int]:
    # GIF duration resolution is 10 ms.
    frame_ms = max(10, round(100 / config["fps"]) * 10)
    steps = max(2, round(config["move_ms"] / frame_ms))
    hold_ms = max(10, round(config["hold_ms"] / 10) * 10)
    return frame_ms, steps, hold_ms


def frame_sequence(config: dict):
    """One loop shared by the live preview and GIF export."""
    frame_ms, steps, hold_ms = animation_timing(config)
    count = len(config["items"])
    for selected in range(count):
        yield float(selected), selected, hold_ms
        for step in range(1, steps + 1):
            t = step / steps
            eased = t * t * (3 - 2 * t)
            # Update the value when the next item settles at the focus.
            value_index = (selected + 1) % count if step == steps else selected
            yield selected + eased, value_index, frame_ms


class AnimationPreview:
    """Render one frame at a time, using the GUI event loop instead of a GIF file."""

    def __init__(self, scheduler, display):
        self.scheduler = scheduler
        self.display = display
        self.renderer = None
        self.sequence = None
        self.pending = None
        self.playing = False

    def load(self, config: dict, base: Path):
        renderer = ArcRenderer(config, base)
        self.pause()
        self.renderer = renderer
        self.sequence = iter(frame_sequence(config))
        self.playing = True
        self.advance()

    def advance(self):
        self.pending = None
        if not self.playing:
            return
        try:
            position, selected, delay = next(self.sequence)
        except StopIteration:
            self.sequence = iter(frame_sequence(self.renderer.config))
            position, selected, delay = next(self.sequence)
        self.display(self.renderer.frame(position, selected))
        self.pending = self.scheduler.after(delay, self.advance)

    def pause(self):
        self.playing = False
        if self.pending is not None:
            self.scheduler.after_cancel(self.pending)
            self.pending = None

    def resume(self):
        if self.renderer is not None and not self.playing:
            self.playing = True
            self.advance()


def export_gif(config: dict, output: Path, base: Path = HERE, progress=None) -> int:
    renderer = ArcRenderer(config, base)
    _, steps, _ = animation_timing(config)
    count = len(config["items"])
    estimated = count * (steps + 1) * config["width"] * config["height"]
    if estimated > 240_000_000:
        raise ValueError("导出帧数与画布过大，请降低分辨率、帧率、转动时长或选项数。")
    frames, durations = [], []
    for index, (position, selected, delay) in enumerate(frame_sequence(config)):
        frames.append(renderer.frame(position, selected))
        durations.append(delay)
        if (index + 1) % (steps + 1) == 0:
            completed = (index + 1) // (steps + 1)
            if progress:
                progress(completed, count)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(output, save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, disposal=2, optimize=False)
    return len(frames)


def run_gui(config: dict, base: Path, default_output: Path) -> None:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from PIL import ImageTk

    root = tk.Tk()
    root.title("圆弧转盘 GIF 生成器")
    root.geometry("1280x760")
    root.minsize(1000, 680)
    panel = ttk.Frame(root, padding=16)
    panel.pack(side="left", fill="both")
    preview_panel = ttk.Frame(root, padding=16)
    preview_panel.pack(side="right", fill="both", expand=True)
    ttk.Label(preview_panel, text="动态预览 · 修改参数后点击更新预览").pack(anchor="w")
    preview_info = tk.StringVar(value="")
    ttk.Label(preview_panel, textvariable=preview_info).pack(anchor="w", pady=8)
    preview_canvas = tk.Canvas(preview_panel, background="#e5e7eb", highlightthickness=0)
    preview_canvas.pack(fill="both", expand=True)
    preview_image_id = preview_canvas.create_image(0, 0, anchor="center")
    preview_controls = ttk.Frame(preview_panel)
    preview_controls.pack(fill="x", pady=12)
    preview_scale = tk.StringVar(value="2×")
    ttk.Label(preview_controls, text="查看倍率").pack(side="left")
    scale_box = ttk.Combobox(preview_controls, textvariable=preview_scale,
                             values=("1×", "2×"), width=4, state="readonly")
    scale_box.pack(side="left", padx=8)
    preview_frame = None
    preview_photo = None

    def display_preview(frame=None):
        nonlocal preview_frame, preview_photo
        if frame is not None:
            preview_frame = frame
        if preview_frame is None:
            return
        width = max(1, preview_canvas.winfo_width())
        height = max(1, preview_canvas.winfo_height())
        scale = min(int(preview_scale.get()[0]), max(1, width - 24) / preview_frame.width,
                    max(1, height - 24) / preview_frame.height)
        size = (max(1, round(preview_frame.width * scale)),
                max(1, round(preview_frame.height * scale)))
        preview_photo = ImageTk.PhotoImage(preview_frame.resize(size, Image.Resampling.NEAREST))
        preview_canvas.itemconfigure(preview_image_id, image=preview_photo)
        preview_canvas.coords(preview_image_id, width / 2, height / 2)
        preview_info.set(f"导出尺寸 {preview_frame.width}×{preview_frame.height}  ·  "
                         f"预览 {scale:.2f}×（窗口不足时自动缩小）")

    preview = AnimationPreview(root, display_preview)
    preview_canvas.bind("<Configure>", lambda event: display_preview())
    scale_box.bind("<<ComboboxSelected>>", lambda event: display_preview())
    ttk.Label(panel, text="选项名称 | 右侧数值（每行一项，按顺序循环）").pack(anchor="w")
    editor = tk.Text(panel, width=34, height=10, font=("Microsoft YaHei", 12), undo=True)
    editor.pack(fill="both", expand=True, pady=(6, 12))
    editor.insert("1.0", "\n".join(f'{i["label"]} | {i.get("value", "")}' for i in config["items"]))
    fields = ttk.Frame(panel)
    fields.pack(fill="x")
    values = {}
    labels = [("caption", "左侧标题"), ("width", "宽度"), ("height", "高度"),
              ("font_size", "字号"), ("fps", "帧率"), ("hold_ms", "停留毫秒"),
              ("move_ms", "转动毫秒"), ("font", "字体路径（留空自动）")]
    for row, (key, label) in enumerate(labels):
        ttk.Label(fields, text=label).grid(row=row, column=0, sticky="w", pady=3)
        values[key] = tk.StringVar(value=str(config.get(key, "")))
        ttk.Entry(fields, textvariable=values[key]).grid(row=row, column=1, sticky="ew", padx=12)
    fields.columnconfigure(1, weight=1)
    destination = tk.StringVar(value=str(default_output))
    ttk.Label(panel, text="输出 GIF 路径").pack(anchor="w", pady=(12, 3))
    ttk.Entry(panel, textvariable=destination).pack(fill="x")
    status = tk.StringVar(value="修改后可更新右侧预览，或直接生成 GIF。")
    ttk.Label(panel, textvariable=status, wraplength=380).pack(anchor="w", pady=10)
    buttons = ttk.Frame(panel)
    buttons.pack(fill="x")
    events = queue.Queue()
    busy = False

    def collect():
        result = {key: value.get().strip() for key, value in values.items()}
        for key in ("width", "height", "font_size", "fps", "hold_ms", "move_ms"):
            result[key] = int(result[key])
        result["items"] = []
        for line in editor.get("1.0", "end").splitlines():
            if line.strip():
                label, _, value = line.partition("|")
                result["items"].append({"label": label.strip(), "value": value.strip()})
        validate(result)
        result["font"] = font_path(result, base)
        return result

    def save_config():
        try:
            result = collect()
            name = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
            if name:
                Path(name).write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
                status.set(f"配置已保存：{name}")
        except (ValueError, OSError) as exc:
            messagebox.showerror("配置错误", str(exc))

    def update_preview():
        try:
            preview.load(collect(), base)
            pause_button.config(text="暂停")
            status.set("预览已更新。查看倍率不影响导出的像素尺寸。")
        except (ValueError, OSError) as exc:
            messagebox.showerror("预览失败", str(exc))

    def toggle_preview():
        if preview.playing:
            preview.pause()
        else:
            preview.resume()
        pause_button.config(text="暂停" if preview.playing else "播放")

    def generate():
        nonlocal busy
        if busy:
            return
        try:
            result = collect()
            output = Path(destination.get().strip())
            if output.suffix.lower() != ".gif":
                raise ValueError("输出路径必须以 .gif 结尾。")
            if output.exists() and not messagebox.askyesno("覆盖文件", f"覆盖已有文件？\n{output}"):
                return
        except (ValueError, OSError) as exc:
            messagebox.showerror("配置错误", str(exc))
            return
        busy = True
        generate_button.config(state="disabled")
        status.set("正在生成…")

        def worker():
            try:
                export_gif(result, output, base,
                           progress=lambda done, total: events.put(("progress", f"正在生成 {done}/{total}")))
                events.put(("done", f"已生成：{output.resolve()}"))
            except Exception as exc:
                events.put(("error", str(exc)))
        threading.Thread(target=worker, daemon=True).start()

    def poll():
        nonlocal busy
        try:
            while True:
                kind, message = events.get_nowait()
                status.set(message)
                if kind != "progress":
                    busy = False
                    generate_button.config(state="normal")
                    if kind == "error":
                        messagebox.showerror("导出失败", message)
        except queue.Empty:
            pass
        root.after(100, poll)

    def close():
        if not busy or messagebox.askyesno("正在导出", "退出将中断导出，确定退出？"):
            preview.pause()
            root.destroy()

    ttk.Button(preview_controls, text="更新预览", command=update_preview).pack(side="right")
    pause_button = ttk.Button(preview_controls, text="暂停", command=toggle_preview)
    pause_button.pack(side="right", padx=8)
    ttk.Button(buttons, text="保存配置", command=save_config).pack(side="left")
    generate_button = ttk.Button(buttons, text="生成 GIF", command=generate)
    generate_button.pack(side="right")
    root.protocol("WM_DELETE_WINDOW", close)
    root.after(100, poll)
    root.after_idle(update_preview)
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--output", type=Path, help="Export GIF without opening the editor")
    args = parser.parse_args()
    try:
        config = json.loads(args.config.read_text(encoding="utf-8-sig"))
        if not isinstance(config, dict):
            raise ValueError("配置必须为 JSON 对象。")
        validate(config)
        if args.output:
            count = export_gif(config, args.output, args.config.resolve().parent)
            print(f"Saved {args.output.resolve()} ({count} rendered frames)")
        else:
            run_gui(config, args.config.resolve().parent, DEFAULT_OUTPUT)
    except (ValueError, OSError) as exc:
        parser.exit(1, f"Error: {exc}\n")


if __name__ == "__main__":
    main()
