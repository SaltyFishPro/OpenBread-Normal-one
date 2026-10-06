from pathlib import Path


project_root = Path(__file__).resolve().parents[2]
source = project_root / "lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c"
output = project_root / "tmp/answers-book-render/fonts.c"

font_source = source.read_text(encoding="utf-8")
start = font_source.index("const uint8_t chinese_font_all[")
end = font_source.index(";\n", start) + 1
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "u8g2_fonts.h"\n' + font_source[start:end] + "\n",
                  encoding="utf-8")
print(output)
