"""Verify immutable common-pack export and its independent firmware requirements."""
from pathlib import Path
import tempfile
import struct
from font_ui_subset import build_subset
from font_bin import BitmapFont

root = Path(__file__).resolve().parents[2]
charset = Path(__file__).with_name('charsets')
han = (charset / 'common3500.txt').read_text(encoding='utf-8').strip()
assert len(han) == len(set(han)) == 3500
fixed = set(han) | set((charset / 'ui-common-v1-extra.txt').read_text(encoding='utf-8').rstrip('\n'))
assert len(fixed) == 3611
with tempfile.TemporaryDirectory() as temporary:
    # A hypothetical future firmware adds a rare character. Pack bytes must
    # remain unchanged, with full-font fallback serving that new requirement.
    header = Path(temporary) / 'UiFontCodepoints.h'
    header.write_text('0x9F98', encoding='utf-8')
    for size in (14, 20, 24):
        source = root / f'tools/font_builder/output/font-{size}.bin'
        data, missing = build_subset(source, header)
        assert missing == [0xFFFD]
        assert data == (source.parent / f'font-{size}.ui.bin').read_bytes()
        assert struct.unpack_from('<I', data, 40)[0] == 3611
        decoded = BitmapFont(data[:40] + bytes(24) + data[64:])
        assert len(decoded.codepoints) == 3610
        assert all(decoded.glyph(c) for c in han)
        assert BitmapFont.load(source).glyph(0x9F98) is not None
print('PASS: 3500 unique Han, 3611 immutable codepoints, all 3 sizes, future UI change leaves pack unchanged')
