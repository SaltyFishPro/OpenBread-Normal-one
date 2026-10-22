"""Focused format checks, using a locally supplied font (not redistributed)."""

import argparse
import json
from pathlib import Path
import tempfile
import time
import zlib

from PIL import Image, ImageChops, ImageDraw

from font_bin import BitmapFont, FileBitmapFont, HEADER, MAGIC, RECORD, ROW_MSB, VERSION
from font_builder import (FontSource, build_fonts, collect_codepoints, encode_font,
                          is_drawable, make_preview, parse_sizes)
from font_common_index import (COMMON_HEADER_SIZE, COMMON_PREFIX, COMMON_RECORD,
                               CommonGlyphIndex, encode_common_index)


def rejects(action, reason):
    try:
        action()
    except ValueError:
        return
    raise AssertionError(f"Expected rejection: {reason}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--font-index", type=int, default=0)
    parser.add_argument("--gui", action="store_true", help="Also exercise the Tk preview/export buttons")
    parser.add_argument("--all-glyphs", action="store_true", help="Also export all characters of the supplied font")
    args = parser.parse_args()
    source = FontSource.load(args.font, args.font_index)
    assert HEADER.size == 64
    assert collect_codepoints("AA 中\n\t\ufeff") == [32, 65, ord("中")]
    assert collect_codepoints("", True) == list(range(32, 127))
    rejects(lambda: collect_codepoints("\n\t"), "empty charset")
    for value in ("16,16,24", "0,20,24", "16,20,24,32", "a"):
        rejects(lambda: parse_sizes(value), "invalid sizes")
    points = [cp for cp in collect_codepoints("Agijé中，%_() ") if cp in source.supported]
    supplementary = sorted(cp for cp in source.supported if cp > 0xFFFF)
    if supplementary:
        points = sorted(points + supplementary[:1])
    assert len(points) >= 8, "Use a font with Latin glyphs for this verification"
    for size in (16, 20, 24):
        data = encode_font(source, points, size, 128)
        assert data == encode_font(source, points, size, 128), "Output must be deterministic"
        decoded = BitmapFont(data)
        original = source.rasterizer(size)
        for cp in points:
            # Independent expected rendering, with generous borders for negative bearings.
            expected = Image.new("L", (512, 256), 0)
            ImageDraw.Draw(expected).text((128, 128), chr(cp), font=original, fill=255, anchor="ls")
            expected = expected.point([0 if value < 128 else 255 for value in range(256)])
            actual = Image.new("L", expected.size, 0)
            glyph = decoded.glyph(cp)
            assert glyph is not None
            if glyph.width:
                bitmap = Image.frombytes("L", (glyph.width, glyph.height), glyph.pixels())
                actual.paste(bitmap, (128 + glyph.x_offset, 128 + glyph.y_offset))
            assert ImageChops.difference(expected, actual).getbbox() is None, (size, cp)
            assert decoded.ascent >= -glyph.y_offset
            assert decoded.descent >= glyph.y_offset + glyph.height
        space = decoded.glyph(" ")
        assert space and not space.bitmap and space.advance > 0
        assert decoded.glyph(0x10FFFF) is None
        corrupted = bytearray(data)
        corrupted[-1] ^= 1
        rejects(lambda: BitmapFont(bytes(corrupted)), "payload CRC")
        rejects(lambda: BitmapFont(data[:-1]), "truncated file")
        invalid_index = bytearray(data)
        # Duplicate codepoint in second record, but recalculate CRC to exercise index checks.
        invalid_index[88:92] = invalid_index[64:68]
        invalid_index[36:40] = zlib.crc32(invalid_index[64:]).to_bytes(4, "little")
        rejects(lambda: BitmapFont(bytes(invalid_index)), "duplicate sorted index")
    with tempfile.TemporaryDirectory(prefix="font-builder-check-") as directory:
        root = Path(directory)
        rejects(lambda: build_fonts(args.font, "A\U0010ffff", [16], root,
                                    font_index=args.font_index), "unsupported glyph")
        assert not (root / "manifest.json").exists()
        result = build_fonts(args.font, "A \U0010ffff", [16, 20, 24], root,
                             font_index=args.font_index, skip_missing=True, preview_text="A")
        assert result["missing_codepoints"] == ["U+10FFFF"]
        assert len(result["fonts"]) == 3
        for entry in result["fonts"]:
            BitmapFont.load(root / entry["file"])
            with Image.open(root / entry["preview"]) as preview:
                assert preview.size == (384, 168)
                assert set(preview.tobytes()) <= {0, 255}
        assert "U+10FFFF" in (root / "missing.txt").read_text(encoding="utf-8")
        verify_large_index(root)
        verify_common_indexes(args, source, root)
        if args.all_glyphs:
            result = build_fonts(args.font, "", [16, 20, 24], root / "all",
                                 font_index=args.font_index, all_glyphs=True, preview_text="Ag中𠮷")
            expected_points = sorted(cp for cp in source.supported if is_drawable(cp))
            assert result["charset_mode"] == "all-font-glyphs"
            assert result["glyph_count"] == len(expected_points)
            assert not result["missing_codepoints"]
            assert result["coverage"]["supplementary_count"] == sum(cp > 0xFFFF for cp in expected_points)
            for entry in result["fonts"]:
                path = root / "all" / entry["file"]
                decoded = BitmapFont.load(path)
                assert decoded.codepoints == expected_points
                with FileBitmapFont(path) as streaming:
                    samples = expected_points[::max(1, len(expected_points) // 128)] + expected_points[-1:]
                    for cp in samples:
                        assert streaming.glyph(cp) == decoded.glyph(cp)
                        assert streaming.lookup_steps <= len(expected_points).bit_length()
                    original, _ = make_preview(decoded, "Ag中𠮷")
                    from_file, _ = make_preview(streaming, "Ag中𠮷")
                    assert original.tobytes() == from_file.tobytes()
            print(f"PASS: all-font export {len(expected_points):,} Unicode characters at 3 sizes; "
                  "seek readback matches in-memory decoding")
        if args.gui:
            verify_gui_layout()
            verify_gui(args, root)
    print("PASS: pixel round-trip at 3 sizes, baselines, space, Unicode, deterministic output,")
    print("      CRC/truncation/index rejection, missing glyph handling, and exported previews")


def verify_common_indexes(args, source, output):
    """Bind sidecars to real fonts, verify direct glyphs and exercise complete SD exports."""
    root = output / "common-export"
    requested = "A中é" + next((chr(cp) for cp in sorted(source.supported) if cp > 0xFFFF), "")
    requested = "".join(char for char in requested if ord(char) in source.supported)
    common_file = output / "常用字清单.txt"
    common_file.write_text("\ufeff" + requested * 2 + "\n\t\U0010ffff", encoding="utf-8")
    result = build_fonts(args.font, requested + "B", [14, 20, 24], root,
                         font_index=args.font_index, common_charset_file=common_file)
    points = collect_codepoints(requested)
    assert result["common_charset"]["requested_count"] == len(points) + 1
    assert result["common_charset"]["missing_codepoints"] == ["U+10FFFF"]
    for entry in result["fonts"]:
        font = BitmapFont.load(root / entry["file"])
        info = entry["common_index"]
        data = (root / info["file"]).read_bytes()
        index = CommonGlyphIndex(data, font)
        assert index.codepoints == points
        assert len(data) == COMMON_HEADER_SIZE + len(points) * 8
        assert data == encode_common_index(font, points), "Common index must be deterministic"
        for cp in points:
            assert index.glyph(cp) == font.glyph(cp)
        assert index.glyph("B") is None and font.glyph("B") is not None
        # The reference fast path must not call full-font binary lookup.
        font.glyph = lambda *_: (_ for _ in ()).throw(AssertionError("Full-font search used"))
        assert index.glyph(points[0]) is not None
        rejects(lambda: CommonGlyphIndex(data[:-1], font), "common truncation")
        damaged = bytearray(data)
        damaged[-1] ^= 1
        rejects(lambda: CommonGlyphIndex(bytes(damaged), font), "common CRC")
        damaged = bytearray(data)
        damaged[COMMON_PREFIX.size + 40] = 1
        rejects(lambda: CommonGlyphIndex(bytes(damaged), font), "font-header binding")
        # Correct the CRC to test structure validation, not just the checksum.
        def invalid_record(cp, offset):
            damaged = bytearray(data)
            COMMON_RECORD.pack_into(damaged, COMMON_HEADER_SIZE, cp, offset)
            damaged[20:24] = zlib.crc32(damaged[COMMON_HEADER_SIZE:]).to_bytes(4, "little")
            rejects(lambda: CommonGlyphIndex(bytes(damaged), font), "invalid common record")
        invalid_record(points[0], HEADER.size + 1)
        invalid_record(points[0], font.bitmap_offset)
        invalid_record(0xD800, HEADER.size)
        invalid_record(points[0], index.record_offsets[-1])
        if entry["pixel_size"] != 14:
            rejects(lambda: CommonGlyphIndex.load(root / result["fonts"][0]["common_index"]["file"], font),
                    "wrong font size")
        for name in (entry["file"], info["file"]):
            assert (root / "Font" / name).read_bytes() == (root / name).read_bytes()
    # Same-size font changes also invalidate a sidecar (different threshold/payload).
    other = BitmapFont(encode_font(source, collect_codepoints(requested + "BC"), 14, 115))
    rejects(lambda: CommonGlyphIndex.load(root / "font-14.common.bin", other), "changed font")
    package = json.loads((root / "Font" / "manifest.json").read_text(encoding="utf-8"))
    assert "preview" not in package["fonts"][0]
    assert len(package["fonts"]) == 3
    assert "U+10FFFF" in (root / "Font" / "common-missing.txt").read_text(encoding="utf-8")
    for cp in points:
        assert f"U+{cp:04X}" in (root / "Font" / "common-codepoints.txt").read_text(encoding="utf-8")
    # Invalid/empty TXT must leave the previous export intact.
    previous = (root / "Font" / "manifest.json").read_bytes()
    for text in ("\n\t\ufeff", "\U0010ffff"):
        common_file.write_text(text, encoding="utf-8")
        rejects(lambda: build_fonts(args.font, "AB", [14], root,
                                    font_index=args.font_index, common_charset_file=common_file), "empty/common missing")
        assert (root / "Font" / "manifest.json").read_bytes() == previous
    # Disable common indexing and change sizes: no stale sidecars/fonts in SD package.
    (root / "Font" / "用户笔记.txt").write_text("保留", encoding="utf-8")
    result = build_fonts(args.font, "AB", [14], root, font_index=args.font_index)
    assert not result["common_charset"]["enabled"]
    assert sorted(path.name for path in (root / "Font").glob("*.bin")) == ["font-14.bin"]
    assert not (root / "Font" / "common-codepoints.txt").exists()
    assert not (root / "font-14.common.bin").exists()
    assert (root / "Font" / "用户笔记.txt").read_text(encoding="utf-8") == "保留"
    # Exercise a full 7,500-codepoint hot index independently of source font coverage.
    count = 7500
    records = b"".join(RECORD.pack(cp, 0, 0, 0, 0, 0, 16, 0, 0)
                       for cp in range(0x4E00, 0x4E00 + count))
    size = HEADER.size + len(records)
    font = BitmapFont(HEADER.pack(MAGIC, VERSION, HEADER.size, RECORD.size, 14,
                                  14, 0, 1, ROW_MSB, count, HEADER.size, size,
                                  size, zlib.crc32(records), bytes(24)) + records)
    data = encode_common_index(font, font.codepoints)
    assert len(data) == 60096
    index = CommonGlyphIndex(data, font)
    assert all(index.glyph(cp) == font.glyph(cp) for cp in font.codepoints)
    print("PASS: common TXT/BOM/dedup/Unicode, 3-size direct glyphs, font binding, corrupt offsets/CRC,")
    print("      7,500 entries = 60,096 bytes; SD folder, missing report, re-export cleanup and unrelated-file preservation")


def verify_large_index(output):
    # Many codepoints may share a glyph. Prove the old 65536 entry limit is gone
    # independently of how many glyphs the local font happens to contain.
    count = 65537
    records = bytearray()
    for cp in range(0x10000, 0x10000 + count):
        records.extend(RECORD.pack(cp, 0, 0, 0, 0, 0, 16, 0, 0))
    file_size = HEADER.size + len(records)
    header = HEADER.pack(MAGIC, VERSION, HEADER.size, RECORD.size, 16,
                         16, 0, 2, ROW_MSB, count, HEADER.size, file_size,
                         file_size, zlib.crc32(records), bytes(24))
    path = output / "large-index.bin"
    path.write_bytes(header + records)
    decoded = BitmapFont.load(path)
    assert len(decoded.codepoints) == count
    with FileBitmapFont(path) as streaming:
        for cp in (0x10000, 0x1FFFF, 0x20000, 0xFFFF, 0x20001):
            assert streaming.glyph(cp) == decoded.glyph(cp)
            assert streaming.lookup_steps <= 17
        assert streaming.glyph(0xD800) is None
    data = bytearray(path.read_bytes())
    data[-1] ^= 1
    path.write_bytes(data)
    rejects(lambda: FileBitmapFont(path), "streamed CRC")
    print("PASS: 65,537-entry supplementary index; at most 17 record reads per lookup")


def assert_visible(widget, root, horizontal_scroll_canvas=None):
    assert widget.winfo_viewable(), f"Not mapped: {widget}"
    x, y = widget.winfo_rootx(), widget.winfo_rooty()
    right, bottom = x + widget.winfo_width(), y + widget.winfo_height()
    parent = widget.master
    while parent is not None:
        px, py = parent.winfo_rootx(), parent.winfo_rooty()
        horizontal_ok = (horizontal_scroll_canvas is not None
                         or (px <= x and right <= px + parent.winfo_width()))
        assert horizontal_ok and py <= y and bottom <= py + parent.winfo_height(), (
                    f"Clipped {widget}: {(x, y, right, bottom)} by {parent}: "
                    f"{(px, py, parent.winfo_width(), parent.winfo_height())}")
        if parent is root:
            break
        parent = parent.master


def verify_gui_layout():
    import subprocess
    import sys

    # Isolate DPI/font caches and native theme state for each scaling factor.
    for scaling in (96 / 72, 120 / 72, 144 / 72, 192 / 72):
        subprocess.run([sys.executable, "-c", "from verify import verify_gui_layout_scale; "
                        f"verify_gui_layout_scale({scaling!r})"],
                       cwd=Path(__file__).parent, check=True, timeout=20)
    print("PASS: action buttons stay visible and lower settings remain scroll-accessible "
          "at 100/125/150/200% DPI, minimum/default size, both modes and long progress text")


def verify_gui_layout_scale(scaling):
    import tkinter as tk
    from font_builder_gui import FontBuilderApp

    window = tk.Tk()
    window.withdraw()
    window.attributes("-alpha", 0.0)
    window.tk.call("tk", "scaling", scaling)
    app = FontBuilderApp(window)
    try:
        window.deiconify()
        for geometry in ("1050x740", "1240x850"):
            window.geometry(geometry)
            window.after(50, window.quit)
            window.mainloop()
            for custom in (True, False):
                app.all_glyphs.set(not custom)
                app.update_mode()
                for status in ("准备导出", "正在生成 " + "字库进度与文件路径 " * 100):
                    app.status.set(status)
                    window.update_idletasks()
                    for widget in (app.preview_button, app.export_button, app.status_label):
                        assert_visible(widget, window)
                    app.settings_canvas.yview_moveto(1)
                    window.update_idletasks()
                    assert_visible(app.output_frame, window, app.settings_canvas)
                    # Each setting remains reachable even when the new common section
                    # increases content height; it need not all fit at the same scroll.
                    for widget in (app.ascii_check, app.missing_check, app.common_entry, app.common_button):
                        relative_y = widget.winfo_rooty() - app.settings_frame.winfo_rooty()
                        extent = app.settings_frame.winfo_height()
                        app.settings_canvas.yview_moveto(max(0, relative_y - 12) / extent)
                        window.update_idletasks()
                        assert_visible(widget, window, app.settings_canvas)
    finally:
        for timer in window.tk.call("after", "info"):
            window.after_cancel(timer)
        window.destroy()


def verify_gui(args, output):
    import tkinter as tk
    from tkinter import messagebox
    from font_builder_gui import FontBuilderApp

    window = tk.Tk()
    window.withdraw()
    app = FontBuilderApp(window)
    assert app.all_glyphs.get()
    assert app.charset.cget("state") == "disabled"
    app.all_glyphs.set(False)
    app.update_mode()
    assert app.charset.cget("state") == "normal"
    app.font_path.set(str(args.font))
    app.font_index.set(str(args.font_index))
    app.charset.delete("1.0", "end")
    app.charset.insert("1.0", "ABC123 ")
    app.sample.delete("1.0", "end")
    app.sample.insert("1.0", "ABC123")
    app.output.set(str(output / "gui"))
    common_file = output / "gui-common.txt"
    common_file.write_text("AA1\n\t", encoding="utf-8-sig")
    app.common_path.set(str(common_file))
    assert "2 字" in app.common_summary.get()
    failures = []
    old_error = messagebox.showerror
    messagebox.showerror = lambda title, message: failures.append((title, message))
    try:
        for button in (app.preview_button, app.export_button):
            button.invoke()
            assert app.busy
            deadline = time.monotonic() + 60
            while app.busy and time.monotonic() < deadline:
                window.update()
                time.sleep(0.01)
            assert not app.busy, "GUI operation timed out"
            assert not failures, failures
            assert len(app.tabs.tabs()) == 3
        assert (output / "gui" / "manifest.json").is_file()
        assert (output / "gui" / "Font" / "font-14.common.bin").is_file()
        app.all_glyphs.set(True)
        app.update_mode()
        app.output.set(str(output / "gui-all"))
        app.sizes.set("16")
        app.export_button.invoke()
        assert app.busy
        deadline = time.monotonic() + 60
        while app.busy and time.monotonic() < deadline:
            window.update()
            time.sleep(0.01)
        assert not app.busy and not failures, failures
        import json
        result = json.loads((output / "gui-all" / "manifest.json").read_text(encoding="utf-8"))
        assert result["charset_mode"] == "all-font-glyphs"
        assert result["common_charset"]["glyph_count"] == 2
        assert (output / "gui-all" / "Font" / "font-16.common.bin").is_file()
        print("PASS: Tk window construction, async preview/export buttons, 3 result tabs")
    finally:
        messagebox.showerror = old_error
        window.destroy()


if __name__ == "__main__":
    main()
