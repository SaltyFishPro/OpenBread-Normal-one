"""NBCI v1 common-glyph offsets bound to one NBFT font; standard library only."""

from __future__ import annotations

from bisect import bisect_left
from pathlib import Path
import struct
import zlib

from font_bin import BitmapFont, Glyph, HEADER, MAX_GLYPHS, RECORD


COMMON_MAGIC = b"NBCI"
COMMON_VERSION = 1
COMMON_PREFIX = struct.Struct("<4sHHHHIII8s")
COMMON_RECORD = struct.Struct("<II")  # Unicode scalar, absolute NBFT record offset.
COMMON_HEADER_SIZE = COMMON_PREFIX.size + HEADER.size


def encode_common_index(font: BitmapFont, codepoints: list[int]) -> bytes:
    points = sorted(set(codepoints))
    if not points or len(points) > MAX_GLYPHS:
        raise ValueError("常用字索引的字符数量无效")
    payload = bytearray()
    for cp in points:
        index = bisect_left(font.codepoints, cp)
        if index == len(font.codepoints) or font.codepoints[index] != cp:
            raise ValueError(f"常用字不在导出字库中：U+{cp:04X}")
        payload.extend(COMMON_RECORD.pack(cp, HEADER.size + index * RECORD.size))
    prefix = COMMON_PREFIX.pack(
        COMMON_MAGIC, COMMON_VERSION, COMMON_HEADER_SIZE, COMMON_RECORD.size,
        font.pixel_size, len(points), COMMON_HEADER_SIZE + len(payload),
        zlib.crc32(payload), bytes(8))
    return prefix + font.data[:HEADER.size] + payload


class CommonGlyphIndex:
    """Validate every entry and demonstrate lookup without a full-font search.

    This desktop reference uses an already validated BitmapFont. Firmware
    separately budgets 7,500 entries and checks target codepoints on lookup;
    this export-time validator checks every target record up front.
    """

    def __init__(self, data: bytes, font: BitmapFont):
        if len(data) < COMMON_HEADER_SIZE:
            raise ValueError("常用字索引文件头不完整")
        (magic, version, header_size, record_size, pixel_size, count,
         file_size, crc, reserved) = COMMON_PREFIX.unpack_from(data)
        if ((magic, version, header_size, record_size) !=
                (COMMON_MAGIC, COMMON_VERSION, COMMON_HEADER_SIZE, COMMON_RECORD.size)
                or reserved != bytes(8) or pixel_size != font.pixel_size
                or not 1 <= count <= MAX_GLYPHS
                or file_size != len(data)
                or file_size != header_size + count * record_size):
            raise ValueError("常用字索引格式、字号或长度无效")
        if data[COMMON_PREFIX.size:COMMON_HEADER_SIZE] != font.data[:HEADER.size]:
            raise ValueError("常用字索引与字库不匹配，请成套重新导出")
        if zlib.crc32(data[header_size:]) != crc:
            raise ValueError("常用字索引 CRC32 校验失败")
        self.font = font
        self.data = data
        self.codepoints: list[int] = []
        self.record_offsets: list[int] = []
        previous = -1
        for cp, offset in COMMON_RECORD.iter_unpack(data[header_size:]):
            if (cp <= previous or cp > 0x10FFFF or 0xD800 <= cp <= 0xDFFF
                    or offset < HEADER.size or offset + RECORD.size > font.bitmap_offset
                    or (offset - HEADER.size) % RECORD.size
                    or RECORD.unpack_from(font.data, offset)[0] != cp):
                raise ValueError("常用字索引码点、顺序或字库位置无效")
            self.codepoints.append(cp)
            self.record_offsets.append(offset)
            previous = cp

    @classmethod
    def load(cls, path: str | Path, font: BitmapFont) -> CommonGlyphIndex:
        return cls(Path(path).read_bytes(), font)

    def record_offset(self, character: str | int) -> int | None:
        cp = ord(character) if isinstance(character, str) else character
        index = bisect_left(self.codepoints, cp)
        if index == len(self.codepoints) or self.codepoints[index] != cp:
            return None
        return self.record_offsets[index]

    def glyph(self, character: str | int) -> Glyph | None:
        offset = self.record_offset(character)
        if offset is None:
            return None
        cp, bitmap_offset, width, height, x, y, advance, _, length = RECORD.unpack_from(
            self.font.data, offset)
        start = self.font.bitmap_offset + bitmap_offset
        return Glyph(cp, width, height, x, y, advance, self.font.data[start:start + length])
