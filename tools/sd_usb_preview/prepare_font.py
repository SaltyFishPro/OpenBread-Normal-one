from pathlib import Path

project = Path(__file__).resolve().parents[2]
source = (project / "lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c").read_text(encoding="utf-8")
declarations = []
for name in ("u8g2_font_7x14_tf", "u8g2_font_6x12_mf"):
    start = source.index(f"const uint8_t {name}[")
    end = source.index(";\n", start) + 1
    declarations.append(source[start:end])
output = project / "tmp/sd-usb-render/fonts.c"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "u8g2_fonts.h"\n' + "\n".join(declarations) + "\n", encoding="utf-8")
print(output)
