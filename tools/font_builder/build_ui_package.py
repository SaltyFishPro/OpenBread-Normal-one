"""Refresh UI subsets, SD package metadata and the boot-download manifest."""
from pathlib import Path
import hashlib
import json
import struct

from font_ui_subset import build_subset

root = Path(__file__).resolve().parents[2]
output = root / "tools/font_builder/output"
package = output / "Font"
manifest_path = package / "manifest.json"
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
for size in (14, 20, 24):
    data, missing = build_subset(output / f"font-{size}.bin", root / "src/services/UiFontCodepoints.h")
    name = f"font-{size}.ui.bin"
    (output / name).write_bytes(data)
    (package / name).write_bytes(data)
    info = next(item for item in manifest["fonts"] if item["pixel_size"] == size)
    info["ui_subset"] = {"file": name, "format": "NBFT", "version": 1,
        "source_codepoints_header": "UiFontCodepoints.h",
        "glyph_count": struct.unpack_from("<I", data, 20)[0], "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "missing_codepoints": [f"U+{cp:04X}" for cp in missing]}
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
bundle = root / "src/services/FontBundle.h"
source = bundle.read_text(encoding="utf-8")
start = source.index("constexpr File kFiles[] = {")
entries = []
total = 0
for size in (14, 20, 24):
    for suffix in ("", ".ui"):
        name = f"font-{size}{suffix}.bin"
        data = (package / name).read_bytes()
        total += len(data)
        count = struct.unpack_from("<I", data, 20)[0]
        crc = struct.unpack_from("<I", data, 36)[0]
        entries.append(f'    {{"{name}", {len(data)}U,\n     "{hashlib.sha256(data).hexdigest()}",\n     {size}, {count}U, 0x{crc:08X}U}},')
source = source[:start] + "constexpr File kFiles[] = {\n" + "\n".join(entries) + "\n};\n" + \
    "constexpr uint8_t kFileCount = sizeof(kFiles) / sizeof(kFiles[0]);\n" + \
    f"constexpr uint32_t kTotalBytes = {total}U;\n" + "}  // namespace FontBundle\n"
bundle.write_text(source, encoding="utf-8")
print(f"Updated SD package and boot manifest: 6 files, {total} bytes")
