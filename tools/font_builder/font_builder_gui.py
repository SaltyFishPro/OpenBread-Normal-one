"""Standalone Tk GUI. Font generation never touches the firmware source tree."""

from __future__ import annotations

import os
from pathlib import Path
import queue
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

from PIL import Image, ImageTk

from font_builder import DEFAULT_TEXT, EXAMPLE_CHARSET, build_fonts, collect_codepoints, parse_sizes


class FontBuilderApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.busy = False
        self.messages: queue.Queue = queue.Queue()
        root.title("Normal One · SD 点阵字库工具")
        root.geometry("1240x850")
        root.minsize(1050, 740)
        root.protocol("WM_DELETE_WINDOW", self.close)
        style = ttk.Style(root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 18, "bold"))
        style.configure("Hint.TLabel", foreground="#586572")

        self.font_path = tk.StringVar()
        self.font_index = tk.StringVar(value="0")
        self.sizes = tk.StringVar(value="14,20,24")
        self.threshold = tk.StringVar(value="128")
        self.output = tk.StringVar(value=str(Path(__file__).parent / "output"))
        self.ascii = tk.BooleanVar(value=True)
        self.all_glyphs = tk.BooleanVar(value=True)
        self.skip_missing = tk.BooleanVar(value=False)
        self.common_path = tk.StringVar()
        self.common_summary = tk.StringVar(value="可选；未选择时仅导出完整字库。")
        self.status = tk.StringVar(value="默认生成字体全部字符；选择字体后即可预览或导出。")
        self.result = tk.StringVar(value="预览将从生成的 BIN 解码；每页画布为 384×168。")

        page = ttk.Frame(root, padding=20)
        page.pack(fill="both", expand=True)
        page.columnconfigure(0, weight=1)
        page.rowconfigure(1, weight=1)
        heading = ttk.Frame(page)
        heading.grid(row=0, column=0, sticky="ew", pady=(0, 16))
        ttk.Label(heading, text="SD 点阵字库", style="Title.TLabel").pack(anchor="w")
        ttk.Label(heading, text="导入字体 → 选择字符与字号 → 预览 BIN → 导出到 SD 卡",
                  style="Hint.TLabel").pack(anchor="w", pady=(4, 0))
        body = ttk.Frame(page)
        body.grid(row=1, column=0, sticky="nsew")
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        settings = ttk.Frame(body, padding=(0, 0, 18, 0))
        settings.grid(row=0, column=0, sticky="nsew")
        settings.columnconfigure(0, weight=1)
        settings.rowconfigure(0, weight=1)
        self.settings_canvas = tk.Canvas(settings, width=480, height=1, highlightthickness=0)
        self.settings_canvas.grid(row=0, column=0, sticky="nsew")
        yscroll = ttk.Scrollbar(settings, orient="vertical", command=self.settings_canvas.yview)
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll = ttk.Scrollbar(settings, orient="horizontal", command=self.settings_canvas.xview)
        xscroll.grid(row=1, column=0, sticky="ew")
        self.settings_canvas.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)
        left = self.settings_frame = ttk.Frame(self.settings_canvas)
        self.settings_window = self.settings_canvas.create_window(0, 0, window=left, anchor="nw")
        left.columnconfigure(0, weight=1)
        left.rowconfigure(1, weight=1)
        right = ttk.Frame(body)
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(2, weight=1)

        source = ttk.LabelFrame(left, text="1  字体与参数", padding=12)
        source.grid(row=0, column=0, sticky="ew")
        source.columnconfigure(1, weight=1)
        ttk.Label(source, text="字体文件").grid(row=0, column=0, sticky="w")
        ttk.Entry(source, textvariable=self.font_path).grid(row=0, column=1, sticky="ew", padx=8)
        ttk.Button(source, text="浏览…", command=self.choose_font).grid(row=0, column=2)
        ttk.Label(source, text="集合索引").grid(row=1, column=0, sticky="w", pady=(10, 0))
        ttk.Spinbox(source, from_=0, to=99, textvariable=self.font_index, width=7).grid(
            row=1, column=1, sticky="w", padx=8, pady=(10, 0))
        ttk.Label(source, text="TTF / OTF 通常为 0", style="Hint.TLabel").grid(
            row=1, column=2, pady=(10, 0))
        ttk.Label(source, text="像素字号").grid(row=2, column=0, sticky="w", pady=(10, 0))
        ttk.Entry(source, textvariable=self.sizes, width=20).grid(
            row=2, column=1, sticky="w", padx=8, pady=(10, 0))
        ttk.Label(source, text="最多三个，以逗号分隔", style="Hint.TLabel").grid(
            row=2, column=2, pady=(10, 0))
        ttk.Label(source, text="二值阈值").grid(row=3, column=0, sticky="w", pady=(10, 0))
        ttk.Spinbox(source, from_=1, to=255, textvariable=self.threshold, width=7).grid(
            row=3, column=1, sticky="w", padx=8, pady=(10, 0))
        ttk.Label(source, text="低：笔画粗；高：笔画细", style="Hint.TLabel").grid(
            row=3, column=2, pady=(10, 0))

        charset_box = ttk.LabelFrame(left, text="2  字符范围", padding=12)
        charset_box.grid(row=1, column=0, sticky="nsew", pady=12)
        charset_box.columnconfigure(0, weight=1)
        charset_box.rowconfigure(3, weight=1)
        ttk.Radiobutton(charset_box, text="字体全部字符（含扩展区、生僻字）",
                        variable=self.all_glyphs, value=True, command=self.update_mode).grid(
                            row=0, column=0, sticky="w")
        ttk.Radiobutton(charset_box, text="自定义字符集", variable=self.all_glyphs, value=False,
                        command=self.update_mode).grid(row=1, column=0, sticky="w", pady=(4, 8))
        buttons = ttk.Frame(charset_box)
        buttons.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        self.import_button = ttk.Button(buttons, text="导入 UTF-8 TXT…", command=self.load_charset)
        self.import_button.pack(side="left")
        self.example_button = ttk.Button(buttons, text="示例", command=self.load_example)
        self.example_button.pack(side="left", padx=8)
        self.charset = scrolledtext.ScrolledText(charset_box, height=10, width=30, wrap="word",
                                                 font=("Microsoft YaHei UI", 11), undo=True)
        self.charset.grid(row=3, column=0, sticky="nsew")
        self.load_example()
        self.ascii_check = ttk.Checkbutton(charset_box, text="加入可打印 ASCII（英文、数字与常用符号）",
                                           variable=self.ascii)
        self.ascii_check.grid(row=4, column=0, sticky="w", pady=(8, 0))
        self.missing_check = ttk.Checkbutton(charset_box, text="跳过字体缺少的字符，并输出 missing.txt",
                                             variable=self.skip_missing)
        self.missing_check.grid(row=5, column=0, sticky="w", pady=(4, 0))
        ttk.Label(charset_box, text="全部模式按字体实际覆盖生成，忽略控制符与格式字符。",
                  style="Hint.TLabel").grid(row=6, column=0, sticky="w", pady=(6, 0))
        self.update_mode()

        self.common_frame = ttk.LabelFrame(left, text="3  常用字加速（可选）", padding=12)
        self.common_frame.grid(row=2, column=0, sticky="ew", pady=(0, 12))
        self.common_frame.columnconfigure(0, weight=1)
        common_row = ttk.Frame(self.common_frame)
        common_row.grid(row=0, column=0, sticky="ew")
        self.common_entry = ttk.Entry(common_row, textvariable=self.common_path)
        self.common_entry.pack(side="left", fill="x", expand=True)
        self.common_button = ttk.Button(common_row, text="常用字 TXT…", command=self.choose_common_charset)
        self.common_button.pack(side="left", padx=(8, 0))
        ttk.Button(common_row, text="清除", command=lambda: self.common_path.set("")).pack(side="left", padx=(6, 0))
        ttk.Label(self.common_frame, textvariable=self.common_summary, width=1,
                  style="Hint.TLabel").grid(row=1, column=0, sticky="ew", pady=(6, 0))
        ttk.Label(self.common_frame, text="独立于字符范围，不裁剪字库；未收录的字会列入缺字清单。",
                  style="Hint.TLabel").grid(row=2, column=0, sticky="w", pady=(4, 0))
        self.common_path.trace_add("write", self.update_common_summary)

        self.output_frame = ttk.LabelFrame(left, text="4  输出目录", padding=12)
        self.output_frame.grid(row=3, column=0, sticky="ew")
        export = self.output_frame
        entry_row = ttk.Frame(export)
        entry_row.pack(fill="x")
        ttk.Entry(entry_row, textvariable=self.output).pack(side="left", fill="x", expand=True)
        ttk.Button(entry_row, text="浏览…", command=self.choose_output).pack(side="left", padx=(8, 0))
        ttk.Label(export, text="导出后，将输出目录中的整个 Font 文件夹复制到 SD 卡根目录。",
                  style="Hint.TLabel").pack(anchor="w", pady=(6, 0))

        ttk.Label(right, text="预览文字（独立于字符集；缺字显示空框）").grid(row=0, column=0, sticky="w")
        self.sample = scrolledtext.ScrolledText(right, height=5, width=42, wrap="word", font=("Microsoft YaHei UI", 11))
        self.sample.grid(row=1, column=0, sticky="ew", pady=(8, 12))
        self.sample.insert("1.0", DEFAULT_TEXT)
        self.tabs = ttk.Notebook(right)
        self.tabs.grid(row=2, column=0, sticky="nsew")
        placeholder = ttk.Frame(self.tabs, padding=20)
        self.tabs.add(placeholder, text="BIN 预览")
        ttk.Label(placeholder, text="选择字体后点击「生成预览」。\n支持原始像素与 2 倍像素预览。",
                  style="Hint.TLabel").pack(anchor="w")
        ttk.Label(right, textvariable=self.result, wraplength=520, style="Hint.TLabel").grid(
            row=3, column=0, sticky="w", pady=(10, 0))
        ttk.Label(right, text="SD字库测试支持常用字索引；其他页面尚未迁移。",
                  style="Hint.TLabel").grid(row=4, column=0, sticky="w", pady=(6, 0))

        footer = ttk.Frame(page)
        footer.grid(row=2, column=0, sticky="ew", pady=(12, 0))
        actions = ttk.Frame(footer)
        actions.pack(fill="x")
        self.preview_button = ttk.Button(actions, text="生成预览", command=lambda: self.start(False))
        self.preview_button.pack(side="left")
        self.export_button = ttk.Button(actions, text="导出字库与 SD 文件夹", command=lambda: self.start(True))
        self.export_button.pack(side="left", padx=10)
        ttk.Button(actions, text="打开输出目录", command=self.open_output).pack(side="left")
        self.progress = ttk.Progressbar(actions, mode="indeterminate", length=120)
        self.progress.pack(side="right")
        # Reserve one line: progress text must not push the action buttons out of view.
        self.status_label = ttk.Label(footer, textvariable=self.status, anchor="w", width=1)
        self.status_label.pack(fill="x", pady=(8, 0))
        self.settings_canvas.bind("<Configure>", self.resize_settings)
        self.settings_frame.bind("<Configure>", self.resize_settings)
        self.root.after(100, self.poll)

    def resize_settings(self, _event=None):
        # Shrink the text editor first; retain scroll access when controls no longer fit.
        minimum_height = (self.settings_frame.winfo_reqheight()
                          - self.charset.frame.winfo_reqheight() + 60)
        self.settings_canvas.itemconfigure(
            self.settings_window,
            width=max(self.settings_canvas.winfo_width(), self.settings_frame.winfo_reqwidth()),
            height=max(self.settings_canvas.winfo_height(), minimum_height))
        self.settings_canvas.configure(scrollregion=self.settings_canvas.bbox("all"))

    def choose_font(self):
        path = filedialog.askopenfilename(title="选择字体", filetypes=[
            ("字体文件", "*.ttf *.otf *.ttc *.otc"), ("所有文件", "*.*")])
        if path:
            self.font_path.set(path)

    def update_mode(self):
        custom = not self.all_glyphs.get()
        self.charset.configure(state="normal" if custom else "disabled")
        for widget in (self.import_button, self.example_button, self.ascii_check, self.missing_check):
            widget.state(["!disabled"] if custom else ["disabled"])

    def choose_output(self):
        path = filedialog.askdirectory(title="选择本地输出目录")
        if path:
            self.output.set(path)

    def choose_common_charset(self):
        path = filedialog.askopenfilename(title="选择常用字清单（UTF-8 TXT）",
                                          filetypes=[("UTF-8 文本", "*.txt")])
        if path:
            self.common_path.set(path)

    def update_common_summary(self, *_args):
        path = self.common_path.get().strip()
        if not path:
            self.common_summary.set("可选；未选择时仅导出完整字库。")
            return
        try:
            points = collect_codepoints(Path(path).expanduser().read_text(encoding="utf-8-sig"))
            self.common_summary.set(
                f"去重后 {len(points):,} 字；索引正文约 {len(points) * 8 / 1024:.1f} KiB / 字号。")
        except (OSError, ValueError, UnicodeError):
            self.common_summary.set("请指定包含有效字符的 UTF-8 TXT 文件。")

    def load_charset(self):
        path = filedialog.askopenfilename(title="导入字符集", filetypes=[("UTF-8 文本", "*.txt")])
        if path:
            try:
                text = Path(path).read_text(encoding="utf-8-sig")
                self.charset.delete("1.0", "end")
                self.charset.insert("1.0", text)
            except (OSError, UnicodeError) as exc:
                messagebox.showerror("导入失败", str(exc))

    def load_example(self):
        self.charset.delete("1.0", "end")
        self.charset.insert("1.0", EXAMPLE_CHARSET.read_text(encoding="utf-8"))

    def open_output(self):
        path = Path(self.output.get()).expanduser()
        if not path.is_dir():
            messagebox.showinfo("输出目录", "目录尚不存在，请先导出字库。")
            return
        if os.name == "nt":
            os.startfile(path)
        else:
            messagebox.showinfo("输出目录", str(path.resolve()))

    def start(self, export: bool):
        if self.busy:
            return
        try:
            if not self.font_path.get().strip():
                raise ValueError("请先选择字体文件")
            if export and not self.output.get().strip():
                raise ValueError("请选择输出目录")
            sizes = parse_sizes(self.sizes.get())
            options = dict(font_path=self.font_path.get(), font_index=int(self.font_index.get()),
                           charset="" if self.all_glyphs.get() else self.charset.get("1.0", "end-1c"), sizes=sizes,
                           threshold=int(self.threshold.get()), include_ascii=self.ascii.get(),
                           skip_missing=self.skip_missing.get(), preview_text=self.sample.get("1.0", "end-1c"),
                           all_glyphs=self.all_glyphs.get(),
                           common_charset_file=self.common_path.get().strip() or None)
            output = Path(self.output.get()).expanduser()
            names = [f"font-{size}.bin" for size in sizes] + ["manifest.json", "preview.png", "charset.txt", "missing.txt"]
            names += [f"preview-{size}.png" for size in sizes]
            names += ["Font/manifest.json"]
            names += [f"Font/font-{size}.ui.bin" for size in sizes]
            if export and any((output / name).exists() for name in names):
                if not messagebox.askyesno("覆盖导出", "输出目录已有同名文件，是否替换这些字库和预览？"):
                    return
        except ValueError as exc:
            messagebox.showerror("参数有误", str(exc))
            return
        self.busy = True
        self.preview_button.state(["disabled"])
        self.export_button.state(["disabled"])
        self.progress.start()
        self.status.set("正在生成，请稍候…")

        def work():
            try:
                with tempfile.TemporaryDirectory(prefix="normal-one-font-") as temp:
                    target = output if export else Path(temp)
                    result = build_fonts(output=target, progress=lambda text: self.messages.put(("progress", text)), **options)
                    images = []
                    for entry in result["fonts"]:
                        with Image.open(target / entry["preview"]) as image:
                            images.append(image.copy())
                    self.messages.put(("done", (result, images, str(target) if export else None)))
            except Exception as exc:
                self.messages.put(("error", str(exc)))

        threading.Thread(target=work, daemon=True).start()

    def poll(self):
        try:
            while True:
                kind, value = self.messages.get_nowait()
                if kind == "progress":
                    self.status.set(value)
                    continue
                self.busy = False
                self.preview_button.state(["!disabled"])
                self.export_button.state(["!disabled"])
                self.progress.stop()
                if kind == "error":
                    self.status.set("生成失败；请检查字体、字符集和参数。")
                    messagebox.showerror("生成失败", value)
                else:
                    result, images, output = value
                    self.show_previews(result, images)
                    self.status.set(f"导出完成；复制到 SD 卡：{Path(output) / 'Font'}" if output
                                    else "BIN 已生成并解码预览，尚未保存到输出目录。")
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def show_previews(self, result: dict, images: list[Image.Image]):
        for tab_id in self.tabs.tabs():
            self.tabs.nametowidget(tab_id).destroy()
        for entry, image in zip(result["fonts"], images):
            frame = ttk.Frame(self.tabs, padding=8)
            self.tabs.add(frame, text=f"{entry['pixel_size']} px")
            text = f"{entry['glyph_count']} 字符 · {entry['bytes']:,} bytes · 行高 {entry['line_height']} px"
            if "ui_subset" in entry:
                text += (f" · UI 子集 {entry['ui_subset']['glyph_count']} 字形 / "
                         f"{entry['ui_subset']['bytes']:,} bytes")
            ttk.Label(frame, text=text).pack(anchor="w", pady=(0, 8))
            controls = ttk.Frame(frame)
            controls.pack(fill="x", pady=(0, 8))
            ttk.Label(controls, text="384×168 画布", style="Hint.TLabel").pack(side="left")
            scale_var = tk.IntVar(value=1)
            viewport = ttk.Frame(frame)
            viewport.pack(fill="both", expand=True)
            viewport.rowconfigure(0, weight=1)
            viewport.columnconfigure(0, weight=1)
            canvas = tk.Canvas(viewport, width=400, height=336,
                               background="#e9edf1", highlightthickness=0)
            canvas.grid(row=0, column=0, sticky="nsew")
            xscroll = ttk.Scrollbar(viewport, orient="horizontal", command=canvas.xview)
            xscroll.grid(row=1, column=0, sticky="ew")
            yscroll = ttk.Scrollbar(viewport, orient="vertical", command=canvas.yview)
            yscroll.grid(row=0, column=1, sticky="ns")
            canvas.configure(xscrollcommand=xscroll.set, yscrollcommand=yscroll.set)

            def refresh(target=canvas, source=image, variable=scale_var):
                scale = variable.get()
                target.photo = ImageTk.PhotoImage(source.resize(
                    (384 * scale, 168 * scale), Image.Resampling.NEAREST))
                target.delete("all")
                target.create_image(0, 0, image=target.photo, anchor="nw")
                target.configure(scrollregion=(0, 0, 384 * scale, 168 * scale))

            ttk.Radiobutton(controls, text="1× 原始像素", variable=scale_var, value=1,
                            command=refresh).pack(side="left", padx=(10, 0))
            ttk.Radiobutton(controls, text="2×", variable=scale_var, value=2,
                            command=refresh).pack(side="left", padx=(8, 0))
            refresh()
        total = sum(entry["bytes"] for entry in result["fonts"])
        preview_missing = set(cp for entry in result["fonts"] for cp in entry["preview_missing_codepoints"])
        mode = "字体全部字符" if result["charset_mode"] == "all-font-glyphs" else "自定义字符集"
        self.result.set(f"{result['source']['family']} · {mode} · {result['glyph_count']:,} 字符"
                        f"（扩展区 {result['coverage']['supplementary_count']:,}）· 共 {total:,} bytes；"
                        f"跳过缺字 {len(result['missing_codepoints'])} 个；预览缺字 {len(preview_missing)} 个。")
        common = result["common_charset"]
        if common["enabled"]:
            self.result.set(self.result.get() +
                            f" 常用字收录 {common['glyph_count']:,} / {common['requested_count']:,}；"
                            f"未收录 {len(common['missing_codepoints'])} 个（见 common-missing.txt）。")
        ui_subsets = [entry for entry in result["fonts"] if "ui_subset" in entry]
        if ui_subsets:
            missing_total = sum(len(entry["ui_subset"]["missing_codepoints"]) for entry in ui_subsets)
            note = f" 已生成固定 UI 子集 {len(ui_subsets)} 份（font-*.ui.bin）"
            if missing_total:
                note += f"，源字体缺 {missing_total} 个 UI 字形（占位框）"
            self.result.set(self.result.get() + note + "。")

    def close(self):
        if self.busy:
            messagebox.showinfo("正在生成", "请等待本次生成结束后关闭窗口。")
        else:
            self.root.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    FontBuilderApp(window)
    window.mainloop()
