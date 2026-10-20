"""NBFT v1 reader. Standard library only; independent of font rasterization."""

from __future__ import annotations

from bisect import bisect_left
from dataclasses import dataclass
from pathlib import Path
import struct
import zlib


MAGIC = b"NBFT"
VERSION = 1
ROW_MSB = 1
MAX_GLYPHS = 0x110000 - 0x800  # All Unicode scalar values; exclude surrogates.
HEADER = struct.Struct("<4sHHHHhhhHIIIII24s")
RECORD = struct.Struct("<IIHHhhhHI")


@dataclass(frozen=True)
class Glyph:
    codepoint: int
    width: int
    height: int
    x_offset: int
    y_offset: int
    advance: int
    bitmap: bytes

    def pixels(self) -> bytes:
        """Unpack into row-major 8-bit pixels: 255 = ink, 0 = background."""
        stride = (self.width + 7) // 8
        return bytes(
            255 if self.bitmap[y * stride + x // 8] & (0x80 >> (x % 8)) else 0
            for y in range(self.height)
            for x in range(self.width)
        )


class BitmapFont:
    def __init__(self, data: bytes):
        if len(data) < HEADER.size:
            raise ValueError("BIN 文件头不完整")
        (magic, version, header_size, record_size, self.pixel_size,
         self.ascent, self.descent, self.line_gap, flags, count,
         index_offset, bitmap_offset, file_size, crc, reserved) = HEADER.unpack_from(data)
        if (magic, version, header_size, record_size, flags) != (
                MAGIC, VERSION, HEADER.size, RECORD.size, ROW_MSB):
            raise ValueError("不支持的 BIN 格式或版本")
        if reserved != bytes(24) or not 6 <= self.pixel_size <= 128:
            raise ValueError("BIN 文件头参数无效")
        if min(self.ascent, self.descent, self.line_gap) < 0:
            raise ValueError("BIN 行高参数无效")
        if (not count or count > MAX_GLYPHS or index_offset != HEADER.size or
                bitmap_offset != index_offset + count * RECORD.size or
                file_size != len(data) or bitmap_offset > len(data)):
            raise ValueError("BIN 长度或索引范围无效")
        if zlib.crc32(data[HEADER.size:]) != crc:
            raise ValueError("BIN 数据 CRC32 校验失败")
        self.data = data
        self.bitmap_offset = bitmap_offset
        self.codepoints: list[int] = []
        self._records: list[tuple[int, ...]] = []
        previous = -1
        expected_offset = 0
        self.max_bitmap_bytes = 0
        for index in range(count):
            record = RECORD.unpack_from(data, index_offset + index * RECORD.size)
            cp, offset, width, height, x, y, advance, glyph_flags, length = record
            if cp <= previous or cp > 0x10FFFF or 0xD800 <= cp <= 0xDFFF:
                raise ValueError("BIN 字符索引无效或未排序")
            if glyph_flags or min(advance, offset) < 0:
                raise ValueError("BIN 字形参数无效")
            if ((width == 0) != (height == 0) or width > 1024 or height > 1024 or
                    length != ((width + 7) // 8) * height or
                    offset != expected_offset or bitmap_offset + offset + length > len(data)):
                raise ValueError("BIN 字形位图范围无效")
            if width % 8:
                padding_mask = (1 << (8 - width % 8)) - 1
                stride = (width + 7) // 8
                if any(data[bitmap_offset + offset + row * stride + stride - 1] & padding_mask
                       for row in range(height)):
                    raise ValueError("BIN 行末补位必须为 0")
            previous = cp
            expected_offset += length
            self.max_bitmap_bytes = max(self.max_bitmap_bytes, length)
            self.codepoints.append(cp)
            self._records.append(record)
        if bitmap_offset + expected_offset != len(data):
            raise ValueError("BIN 存在未索引的数据")

    @classmethod
    def load(cls, path: str | Path) -> BitmapFont:
        return cls(Path(path).read_bytes())

    @property
    def line_height(self) -> int:
        return self.ascent + self.descent + self.line_gap

    def glyph(self, character: str | int) -> Glyph | None:
        cp = ord(character) if isinstance(character, str) else character
        index = bisect_left(self.codepoints, cp)
        if index == len(self.codepoints) or self.codepoints[index] != cp:
            return None
        cp, offset, width, height, x, y, advance, _, length = self._records[index]
        start = self.bitmap_offset + offset
        return Glyph(cp, width, height, x, y, advance, self.data[start:start + length])


class FileBitmapFont:
    """Desktop reference for seek-based SD access; no full font/index in RAM.

    Uses files already structurally validated by BitmapFont during export.
    CRC verification is streamed once on open; individual lookups read only
    O(log glyph_count) index records followed by the requested bitmap.
    """

    def __init__(self, path: str | Path):
        self._file = Path(path).open("rb")
        try:
            self._file.seek(0, 2)
            file_size = self._file.tell()
            self._file.seek(0)
            header = self._file.read(HEADER.size)
            if len(header) != HEADER.size:
                raise ValueError("BIN 文件头不完整")
            (magic, version, header_size, record_size, self.pixel_size,
             self.ascent, self.descent, self.line_gap, flags, self.glyph_count,
             self.index_offset, self.bitmap_offset, declared_size, crc, reserved) = HEADER.unpack(header)
            if ((magic, version, header_size, record_size, flags) !=
                    (MAGIC, VERSION, HEADER.size, RECORD.size, ROW_MSB) or
                    reserved != bytes(24) or not 6 <= self.pixel_size <= 128 or
                    min(self.ascent, self.descent, self.line_gap) < 0 or
                    not 1 <= self.glyph_count <= MAX_GLYPHS or
                    self.index_offset != HEADER.size or
                    self.bitmap_offset != HEADER.size + self.glyph_count * RECORD.size or
                    self.bitmap_offset > file_size or declared_size != file_size):
                raise ValueError("BIN 文件头或索引范围无效")
            self.bitmap_size = file_size - self.bitmap_offset
            checksum = 0
            while chunk := self._file.read(4096):
                checksum = zlib.crc32(chunk, checksum)
            if checksum != crc:
                raise ValueError("BIN 数据 CRC32 校验失败")
            self.lookup_steps = 0
        except Exception:
            self._file.close()
            raise

    @property
    def line_height(self) -> int:
        return self.ascent + self.descent + self.line_gap

    def glyph(self, character: str | int) -> Glyph | None:
        cp = ord(character) if isinstance(character, str) else character
        self.lookup_steps = 0
        if not 0 <= cp <= 0x10FFFF or 0xD800 <= cp <= 0xDFFF:
            return None
        low, high = 0, self.glyph_count
        while low < high:
            middle = (low + high) // 2
            self._file.seek(self.index_offset + middle * RECORD.size)
            record = self._file.read(RECORD.size)
            if len(record) != RECORD.size:
                raise ValueError("BIN 字符索引不完整")
            codepoint, offset, width, height, x, y, advance, flags, length = RECORD.unpack(record)
            self.lookup_steps += 1
            if codepoint < cp:
                low = middle + 1
            elif codepoint > cp:
                high = middle
            else:
                if (flags or advance < 0 or (width == 0) != (height == 0) or
                        max(width, height) > 1024 or
                        length != ((width + 7) // 8) * height or
                        offset + length > self.bitmap_size):
                    raise ValueError("BIN 字形位图范围无效")
                self._file.seek(self.bitmap_offset + offset)
                bitmap = self._file.read(length)
                if len(bitmap) != length:
                    raise ValueError("BIN 字形位图不完整")
                return Glyph(cp, width, height, x, y, advance, bitmap)
        return None

    def close(self):
        self._file.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
