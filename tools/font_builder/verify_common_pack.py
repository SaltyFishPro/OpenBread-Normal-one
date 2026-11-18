"""Verify the published common-pack export and its firmware requirement list."""
from pathlib import Path
import hashlib
import tempfile
import struct
from font_ui_subset import build_subset
from font_bin import BitmapFont

root = Path(__file__).resolve().parents[2]
charset = Path(__file__).with_name('charsets')


def codepoints(name: str) -> list[int]:
    text = (charset / name).read_text(encoding='utf-8')
    return [ord(c) for c in text if c not in '\r\n']


def fingerprint(values: list[int]) -> str:
    return hashlib.sha256(','.join(map(str, values)).encode('ascii')).hexdigest()


han = (charset / 'common3500.txt').read_text(encoding='utf-8').strip()
assert len(han) == len(set(han)) == 3500
# v1 stays frozen: released packs may still carry it and the firmware keeps its
# fill/fallback path. v2 is the published pack that covers the current UI text,
# including the TXT reader arrows and full-width space.
v1 = codepoints('ui-common-v1-extra.txt')
v2 = codepoints('ui-common-v2-extra.txt')
assert fingerprint(v1) == 'ceae2fbcf897fa58ad5199ead1594c9154cac7287552bc8438b4e756485b6959'
assert len(v2) == 114 and set(v1) < set(v2)
assert fingerprint(v2) == '879d6e204904cc738ca9553cbb35e7cb2b0fedb331d4fad2b520fa69b6c803ea'
fixed = set(han) | set(v2)
assert len(fixed) == 3614
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
        assert struct.unpack_from('<I', data, 40)[0] == 3614
        decoded = BitmapFont(data[:40] + bytes(24) + data[64:])
        assert len(decoded.codepoints) == 3613
        assert all(decoded.glyph(c) for c in han)
        assert BitmapFont.load(source).glyph(0x9F98) is not None
print('PASS: 3500 unique Han, frozen v1, 3614 v2 codepoints, all 3 sizes, future UI change leaves pack unchanged')
