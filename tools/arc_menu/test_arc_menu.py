"""Run with: python -m unittest discover -s tools/arc_menu -p test_*.py"""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image, ImageChops

import arc_menu as menu


class Scheduler:
    def __init__(self):
        self.callbacks = {}
        self.serial = 0

    def after(self, delay, callback):
        self.serial += 1
        self.callbacks[self.serial] = (delay, callback)
        return self.serial

    def after_cancel(self, token):
        del self.callbacks[token]

    def tick(self):
        token = next(iter(self.callbacks))
        _, callback = self.callbacks.pop(token)
        callback()


class MenuTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(menu.DEFAULT_CONFIG.read_text(encoding="utf-8"))

    def test_screen_gif_and_loop(self):
        sequence = list(menu.frame_sequence(self.config))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "screen.gif"
            menu.export_gif(self.config, path)
            with Image.open(path) as gif:
                self.assertEqual(gif.size, (384, 168))
                self.assertEqual(gif.info["loop"], 0)
                first = gif.convert("RGB")
                duration = 0
                for frame in range(gif.n_frames):
                    gif.seek(frame)
                    duration += gif.info["duration"]
                self.assertIsNone(ImageChops.difference(first, gif.convert("RGB")).getbbox())
                self.assertEqual(duration, sum(frame[2] for frame in sequence))

    def test_preview_pause_reload_and_wrap(self):
        scheduler = Scheduler()
        displayed = []
        preview = menu.AnimationPreview(scheduler, lambda image: displayed.append(image))
        preview.load(self.config, menu.HERE)
        first = displayed[-1]
        preview.pause()
        self.assertFalse(scheduler.callbacks)
        preview.resume()
        for _ in range(len(list(menu.frame_sequence(self.config))) - 1):
            scheduler.tick()
        self.assertIsNone(ImageChops.difference(first.convert("RGB"), displayed[-1].convert("RGB")).getbbox())
        updated = dict(self.config, caption="新标题", width=480)
        preview.load(updated, menu.HERE)
        self.assertEqual(displayed[-1].size, (480, 168))
        self.assertEqual(len(scheduler.callbacks), 1)
        with self.assertRaises(ValueError):
            preview.load(dict(updated, height=0), menu.HERE)
        self.assertEqual(preview.renderer.width, 480)
        self.assertEqual(len(scheduler.callbacks), 1)
        preview.pause()

    def test_preview_uses_export_sequence(self):
        scheduler = Scheduler()
        preview = menu.AnimationPreview(scheduler, lambda image: None)
        with patch.object(menu.ArcRenderer, "frame", return_value=None) as render:
            preview.load(self.config, menu.HERE)
            sequence = list(menu.frame_sequence(self.config))
            for index, (position, selected, delay) in enumerate(sequence):
                if index:
                    scheduler.tick()
                render.assert_called_with(position, selected)
                self.assertEqual(next(iter(scheduler.callbacks.values()))[0], delay)
            preview.pause()

    def test_gui_preview_controls(self):
        import tkinter as tk
        from tkinter import ttk

        try:
            root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest(str(exc))
        root.withdraw()

        def descendants(widget):
            for child in widget.winfo_children():
                yield child
                yield from descendants(child)

        def exercise():
            root.update_idletasks()
            widgets = list(descendants(root))
            canvas = next(w for w in widgets if isinstance(w, tk.Canvas))
            self.assertTrue(canvas.itemcget(1, "image"))
            buttons = {w.cget("text"): w for w in widgets if isinstance(w, ttk.Button)}
            buttons["暂停"].invoke()
            self.assertEqual(buttons["暂停"].cget("text"), "播放")
            buttons["暂停"].invoke()
            self.assertEqual(buttons["暂停"].cget("text"), "暂停")
            old_image = canvas.itemcget(1, "image")
            editor = next(w for w in widgets if isinstance(w, tk.Text))
            editor.delete("1.0", "end")
            editor.insert("1.0", "新选项 | 说明\n另一个 | 状态")
            buttons["更新预览"].invoke()
            self.assertNotEqual(canvas.itemcget(1, "image"), old_image)
            buttons["暂停"].invoke()

        try:
            with patch.object(tk, "Tk", return_value=root), \
                    patch.object(root, "mainloop", side_effect=exercise), \
                    patch("tkinter.messagebox.showerror") as error:
                menu.run_gui(self.config, menu.HERE, menu.DEFAULT_OUTPUT)
                error.assert_not_called()
        finally:
            root.destroy()


if __name__ == "__main__":
    unittest.main()
