#!/usr/bin/env python3
"""GIF 转 C 数组工具 GUI。"""

from __future__ import annotations

import sys
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

# Ensure sibling import works when launching from project root.
THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

import gif_to_c as core

try:
    from PIL import Image, ImageOps, ImageTk  # type: ignore
except Exception:
    Image = None  # type: ignore
    ImageOps = None  # type: ignore
    ImageTk = None  # type: ignore

APP_TITLE = "GIF/Image to C Array Tool"
PREVIEW_BOX_WIDTH = 320
PREVIEW_BOX_HEIGHT = 220
FRAME_THUMB_WIDTH = 180
FRAME_THUMB_HEIGHT = 120


def require_pillow() -> None:
    if Image is None or ImageOps is None or ImageTk is None:
        raise RuntimeError("缺少 Pillow，请先执行: pip install -r tools/gif_to_c/requirements.txt")


def parse_int(value: str, label: str, min_value: int | None = None, max_value: int | None = None) -> int:
    try:
        number = int(value)
    except Exception as exc:
        raise RuntimeError(f"{label} 不是有效整数: {value}") from exc

    if min_value is not None and number < min_value:
        raise RuntimeError(f"{label} 不能小于 {min_value}")
    if max_value is not None and number > max_value:
        raise RuntimeError(f"{label} 不能大于 {max_value}")
    return number


def to_binary_image(image: "Image.Image", threshold: int = 128, invert: bool = False) -> "Image.Image":
    require_pillow()
    gray = image.convert("L")
    if invert:
        gray = ImageOps.invert(gray)
    return gray.point(lambda p: 0 if p < threshold else 255, mode="1")


def unpack_frame_to_image(packed: list[int], width: int, height: int) -> "Image.Image":
    require_pillow()
    img = Image.new("1", (width, height), 1)
    px = img.load()
    bytes_per_row = (width + 7) // 8

    for y in range(height):
        for x in range(width):
            idx = y * bytes_per_row + (x // 8)
            bit = (packed[idx] >> (7 - (x % 8))) & 1
            px[x, y] = 0 if bit else 1

    return img


def sanitize_symbol(name: str) -> str:
    return core.sanitize_symbol(name.strip() or "icon")


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("980x760")
        self.resizable(True, True)

        self.gif_preview_photo = None
        self.gif_preview_frames: list[Image.Image] = []
        self.gif_anim_images: list[ImageTk.PhotoImage] = []
        self.gif_anim_job: str | None = None
        self.gif_anim_index = 0
        self.gif_frame_photos: list[ImageTk.PhotoImage] = []

        self.gif_input_var = tk.StringVar()
        self.gif_output_var = tk.StringVar()
        self.gif_name_var = tk.StringVar(value="icon")
        self.gif_w_var = tk.StringVar(value="32")
        self.gif_h_var = tk.StringVar(value="32")
        self.gif_threshold_var = tk.StringVar(value="128")
        self.gif_bg_var = tk.StringVar(value="255")
        self.gif_resize_var = tk.StringVar(value="nearest")
        self.gif_invert_var = tk.BooleanVar(value=False)
        self.gif_dedupe_var = tk.BooleanVar(value=True)
        self.gif_status_var = tk.StringVar(value="就绪")

        self._build_ui()

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.pack(fill="both", expand=True)

        row = 0
        ttk.Label(root, text="输入 GIF").grid(row=row, column=0, sticky="w")
        ttk.Entry(root, textvariable=self.gif_input_var, width=78).grid(row=row, column=1, sticky="we")
        ttk.Button(root, text="浏览", command=self.pick_gif_input).grid(row=row, column=2, padx=6)
        row += 1

        ttk.Label(root, text="输出文件").grid(row=row, column=0, sticky="w")
        ttk.Entry(root, textvariable=self.gif_output_var, width=78).grid(row=row, column=1, sticky="we")
        ttk.Button(root, text="浏览", command=self.pick_gif_output).grid(row=row, column=2, padx=6)
        row += 1

        ttk.Label(root, text="符号名").grid(row=row, column=0, sticky="w")
        ttk.Entry(root, textvariable=self.gif_name_var, width=22).grid(row=row, column=1, sticky="w")
        row += 1

        form = ttk.Frame(root)
        form.grid(row=row, column=0, columnspan=3, sticky="we", pady=(8, 0))
        ttk.Label(form, text="宽").grid(row=0, column=0, sticky="w")
        ttk.Entry(form, textvariable=self.gif_w_var, width=6).grid(row=0, column=1, padx=(4, 14))
        ttk.Label(form, text="高").grid(row=0, column=2, sticky="w")
        ttk.Entry(form, textvariable=self.gif_h_var, width=6).grid(row=0, column=3, padx=(4, 14))
        ttk.Label(form, text="阈值").grid(row=0, column=4, sticky="w")
        ttk.Entry(form, textvariable=self.gif_threshold_var, width=6).grid(row=0, column=5, padx=(4, 14))
        ttk.Label(form, text="透明背景灰度").grid(row=0, column=6, sticky="w")
        ttk.Entry(form, textvariable=self.gif_bg_var, width=6).grid(row=0, column=7, padx=(4, 14))
        ttk.Label(form, text="缩放").grid(row=0, column=8, sticky="w")
        ttk.Combobox(
            form,
            textvariable=self.gif_resize_var,
            values=["nearest", "bilinear", "bicubic", "lanczos"],
            state="readonly",
            width=10,
        ).grid(row=0, column=9, padx=(4, 14))
        row += 1

        opts = ttk.Frame(root)
        opts.grid(row=row, column=0, columnspan=3, sticky="w", pady=(8, 2))
        ttk.Checkbutton(opts, text="反色", variable=self.gif_invert_var).pack(side="left", padx=(0, 14))
        ttk.Checkbutton(opts, text="去重连续重复帧", variable=self.gif_dedupe_var).pack(side="left")
        row += 1

        actions = ttk.Frame(root)
        actions.grid(row=row, column=0, columnspan=3, sticky="w", pady=(4, 8))
        ttk.Button(actions, text="预览首帧", command=self.preview_gif).pack(side="left")
        ttk.Button(actions, text="动态预览", command=self.animate_gif_preview).pack(side="left", padx=8)
        ttk.Button(actions, text="停止预览", command=self._stop_gif_animation).pack(side="left")
        ttk.Button(actions, text="开始转换", command=self.convert_gif).pack(side="left", padx=8)
        ttk.Button(actions, text="Image to C", command=self.convert_image).pack(side="left", padx=8)
        row += 1

        self.gif_preview_label = ttk.Label(root, text="暂无预览")
        self.gif_preview_label.grid(row=row, column=0, columnspan=3, sticky="w")
        row += 1

        preview_wrap = ttk.Frame(root, width=PREVIEW_BOX_WIDTH + 4, height=PREVIEW_BOX_HEIGHT + 4)
        preview_wrap.grid(row=row, column=0, columnspan=3, sticky="w", pady=(4, 8))
        preview_wrap.grid_propagate(False)

        self.gif_preview_canvas = tk.Label(preview_wrap, relief="solid", anchor="center")
        self.gif_preview_canvas.pack(fill="both", expand=True)
        row += 1

        ttk.Label(root, text="逐帧预览（可滚动）").grid(row=row, column=0, columnspan=3, sticky="w")
        row += 1

        frames_wrap = ttk.Frame(root)
        frames_wrap.grid(row=row, column=0, columnspan=3, sticky="nsew", pady=(2, 8))
        frames_wrap.columnconfigure(0, weight=1)
        frames_wrap.rowconfigure(0, weight=1)

        self.gif_frames_canvas = tk.Canvas(frames_wrap, height=220, highlightthickness=1)
        self.gif_frames_canvas.grid(row=0, column=0, sticky="nsew")
        self.gif_frames_scrollbar = ttk.Scrollbar(
            frames_wrap,
            orient="vertical",
            command=self.gif_frames_canvas.yview,
        )
        self.gif_frames_scrollbar.grid(row=0, column=1, sticky="ns")
        self.gif_frames_canvas.configure(yscrollcommand=self.gif_frames_scrollbar.set)

        self.gif_frames_inner = ttk.Frame(self.gif_frames_canvas)
        self.gif_frames_window = self.gif_frames_canvas.create_window(
            (0, 0),
            window=self.gif_frames_inner,
            anchor="nw",
        )
        self.gif_frames_inner.bind("<Configure>", self._on_gif_frames_inner_configure)
        self.gif_frames_canvas.bind("<Configure>", self._on_gif_frames_canvas_configure)
        self.gif_frames_canvas.bind("<MouseWheel>", self._on_gif_frame_mousewheel)
        self.gif_frames_canvas.bind("<Button-4>", self._on_gif_frame_mousewheel)
        self.gif_frames_canvas.bind("<Button-5>", self._on_gif_frame_mousewheel)
        root.rowconfigure(row, weight=1)
        row += 1

        ttk.Label(root, textvariable=self.gif_status_var).grid(row=row, column=0, columnspan=3, sticky="w")
        root.columnconfigure(1, weight=1)

    def _render_preview_image(
        self,
        image: "Image.Image",
        threshold: int,
        target_width: int,
        target_height: int,
    ) -> "Image.Image":
        require_pillow()
        src = to_binary_image(image, threshold=threshold).convert("L")
        src_w, src_h = src.size
        if src_w <= 0 or src_h <= 0:
            return Image.new("L", (target_width, target_height), 255)

        scale = min(target_width / src_w, target_height / src_h)
        resized_w = max(1, int(round(src_w * scale)))
        resized_h = max(1, int(round(src_h * scale)))
        resized = src.resize((resized_w, resized_h), Image.Resampling.NEAREST)

        canvas = Image.new("L", (target_width, target_height), 255)
        paste_x = (target_width - resized_w) // 2
        paste_y = (target_height - resized_h) // 2
        canvas.paste(resized, (paste_x, paste_y))
        return canvas

    def _set_preview(self, target: tk.Label, image: "Image.Image", attr_name: str, threshold: int = 128) -> None:
        preview = self._render_preview_image(
            image,
            threshold=threshold,
            target_width=PREVIEW_BOX_WIDTH,
            target_height=PREVIEW_BOX_HEIGHT,
        )
        photo = ImageTk.PhotoImage(preview)
        setattr(self, attr_name, photo)
        target.configure(image=photo, text="")

    def _clear_gif_frame_previews(self) -> None:
        self.gif_frame_photos = []
        for child in self.gif_frames_inner.winfo_children():
            child.destroy()
        self.gif_frames_canvas.configure(scrollregion=(0, 0, 0, 0))
        self.gif_frames_canvas.yview_moveto(0.0)

    def _populate_gif_frame_previews(self, frames: list["Image.Image"], threshold: int) -> None:
        self._clear_gif_frame_previews()

        for idx, frame in enumerate(frames, start=1):
            item = ttk.Frame(self.gif_frames_inner, padding=(2, 2))
            item.grid(row=idx - 1, column=0, sticky="we", pady=(0, 4))

            index_label = ttk.Label(item, text=f"第 {idx:03d} 帧", width=11)
            index_label.pack(side="left", padx=(0, 6))

            thumb = self._render_preview_image(
                frame,
                threshold=threshold,
                target_width=FRAME_THUMB_WIDTH,
                target_height=FRAME_THUMB_HEIGHT,
            )
            photo = ImageTk.PhotoImage(thumb)
            self.gif_frame_photos.append(photo)

            image_label = tk.Label(item, image=photo, relief="solid", borderwidth=1, anchor="center")
            image_label.pack(side="left")

            for widget in (item, index_label, image_label):
                widget.bind("<MouseWheel>", self._on_gif_frame_mousewheel)
                widget.bind("<Button-4>", self._on_gif_frame_mousewheel)
                widget.bind("<Button-5>", self._on_gif_frame_mousewheel)

        self.gif_frames_inner.update_idletasks()
        self.gif_frames_canvas.configure(scrollregion=self.gif_frames_canvas.bbox("all"))

    def _on_gif_frames_inner_configure(self, _event: tk.Event) -> None:
        self.gif_frames_canvas.configure(scrollregion=self.gif_frames_canvas.bbox("all"))

    def _on_gif_frames_canvas_configure(self, event: tk.Event) -> None:
        self.gif_frames_canvas.itemconfigure(self.gif_frames_window, width=event.width)

    def _on_gif_frame_mousewheel(self, event: tk.Event) -> None:
        if getattr(event, "num", None) == 4:
            delta = -1
        elif getattr(event, "num", None) == 5:
            delta = 1
        else:
            raw_delta = int(getattr(event, "delta", 0))
            if raw_delta == 0:
                return
            delta = -int(raw_delta / 120) if raw_delta % 120 == 0 else (-1 if raw_delta > 0 else 1)
        self.gif_frames_canvas.yview_scroll(delta, "units")

    def _stop_gif_animation(self) -> None:
        if self.gif_anim_job is not None:
            self.after_cancel(self.gif_anim_job)
            self.gif_anim_job = None

    def _start_gif_animation(self, threshold: int) -> None:
        self._stop_gif_animation()
        if not self.gif_preview_frames:
            return

        self.gif_anim_images = []
        for frame in self.gif_preview_frames:
            preview = self._render_preview_image(
                frame,
                threshold=threshold,
                target_width=PREVIEW_BOX_WIDTH,
                target_height=PREVIEW_BOX_HEIGHT,
            )
            self.gif_anim_images.append(ImageTk.PhotoImage(preview))

        self.gif_anim_index = 0
        self._tick_gif_animation()

    def _tick_gif_animation(self) -> None:
        if not self.gif_anim_images:
            self.gif_anim_job = None
            return

        image = self.gif_anim_images[self.gif_anim_index]
        self.gif_preview_canvas.configure(image=image, text="")
        self.gif_preview_photo = image
        self.gif_anim_index = (self.gif_anim_index + 1) % len(self.gif_anim_images)
        self.gif_anim_job = self.after(80, self._tick_gif_animation)

    def pick_gif_input(self) -> None:
        p = filedialog.askopenfilename(
            title="选择 GIF 文件",
            filetypes=[("GIF files", "*.gif"), ("Image files", "*.png;*.jpg;*.jpeg;*.bmp;*.webp;*.tif;*.tiff"), ("All files", "*.*")],
        )
        if not p:
            return

        self.gif_input_var.set(p)
        if not self.gif_output_var.get().strip():
            self.gif_output_var.set(str(Path(p).with_suffix(".h")))
        if self.gif_name_var.get().strip() in {"", "icon", "wifi"}:
            self.gif_name_var.set(sanitize_symbol(Path(p).stem))

    def pick_gif_output(self) -> None:
        p = filedialog.asksaveasfilename(
            title="选择输出文件",
            defaultextension=".h",
            filetypes=[("Header files", "*.h"), ("C files", "*.c"), ("All files", "*.*")],
        )
        if p:
            self.gif_output_var.set(p)

    def _gif_options(self) -> core.ConvertOptions:
        return core.ConvertOptions(
            name=self.gif_name_var.get().strip() or "icon",
            width=parse_int(self.gif_w_var.get(), "宽", 1, 4096),
            height=parse_int(self.gif_h_var.get(), "高", 1, 4096),
            threshold=parse_int(self.gif_threshold_var.get(), "阈值", 0, 255),
            invert=self.gif_invert_var.get(),
            resize_filter=self.gif_resize_var.get().strip() or "nearest",
            dedupe=self.gif_dedupe_var.get(),
            bg=parse_int(self.gif_bg_var.get(), "透明背景灰度", 0, 255),
        )

    def preview_gif(self) -> None:
        try:
            require_pillow()
            input_path = Path(self.gif_input_var.get().strip())
            if not input_path.exists():
                raise RuntimeError("未找到输入 GIF 文件")

            opts = self._gif_options()
            if input_path.suffix.lower() == ".gif":
                frames = core.collect_frames_from_gif(input_path, opts)
            else:
                frames = core.collect_frames_from_image(input_path, opts)
            if not frames:
                raise RuntimeError("没有可预览的帧")

            self.gif_preview_frames = [
                unpack_frame_to_image(frame, opts.width, opts.height) for frame in frames
            ]
            self._stop_gif_animation()
            self._set_preview(
                self.gif_preview_canvas,
                self.gif_preview_frames[0],
                "gif_preview_photo",
                threshold=opts.threshold,
            )
            self._populate_gif_frame_previews(self.gif_preview_frames, threshold=opts.threshold)
            self.gif_preview_label.configure(
                text=f"预览: 帧数={len(frames)} | 尺寸={opts.width}x{opts.height}"
            )
            self.gif_status_var.set("预览完成")
        except Exception as exc:
            self._clear_gif_frame_previews()
            self.gif_status_var.set("预览失败")
            messagebox.showerror("GIF 预览失败", str(exc))

    def animate_gif_preview(self) -> None:
        try:
            if not self.gif_preview_frames:
                self.preview_gif()
            if not self.gif_preview_frames:
                raise RuntimeError("没有可预览的帧")

            threshold = parse_int(self.gif_threshold_var.get(), "阈值", 0, 255)
            self._start_gif_animation(threshold)
            self.gif_status_var.set("动态预览中")
        except Exception as exc:
            self.gif_status_var.set("动态预览失败")
            messagebox.showerror("GIF 动态预览失败", str(exc))

    def convert_gif(self) -> None:
        try:
            self._stop_gif_animation()
            input_path = Path(self.gif_input_var.get().strip())
            output_path = Path(self.gif_output_var.get().strip())

            if not input_path.exists():
                raise RuntimeError("未找到输入 GIF 文件")
            if not output_path.as_posix().strip():
                raise RuntimeError("输出路径为空")

            opts = self._gif_options()
            frame_count, symbol = core.convert_gif_to_c(input_path, output_path, opts)
            self.gif_status_var.set(f"完成: {output_path} | 帧数={frame_count} | 符号={symbol}_frames")
            messagebox.showinfo(
                "GIF 转换成功",
                f"已生成: {output_path}\n\n帧数: {frame_count}\n符号: {symbol}_frames",
            )
        except Exception as exc:
            self.gif_status_var.set("转换失败")
            messagebox.showerror("GIF 转换失败", str(exc))

    def convert_image(self) -> None:
        try:
            self._stop_gif_animation()
            input_path = Path(self.gif_input_var.get().strip())
            output_path = Path(self.gif_output_var.get().strip())

            if not input_path.exists():
                raise RuntimeError("Input image file not found")
            if not output_path.as_posix().strip():
                raise RuntimeError("Output path is empty")

            opts = self._gif_options()
            frame_count, symbol = core.convert_image_to_c(input_path, output_path, opts)
            self.gif_status_var.set(f"Done: {output_path} | frames={frame_count} | symbol={symbol}_frames")
            messagebox.showinfo(
                "Image conversion done",
                f"Generated: {output_path}\n\nFrames: {frame_count}\nSymbol: {symbol}_frames",
            )
        except Exception as exc:
            self.gif_status_var.set("Failed")
            messagebox.showerror("Image conversion failed", str(exc))



def main() -> None:
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
