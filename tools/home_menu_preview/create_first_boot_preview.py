"""Keep the single-file first-boot flow aligned with the XMB preview."""

import json
import re
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = HERE / "xmb_menu_mock.html"
OUTPUT = HERE / "first_boot_mock.html"
START = "<!-- XMB_DOCUMENT_START -->"
END = "<!-- XMB_DOCUMENT_END -->"


def main():
    xmb = SOURCE.read_text(encoding="utf-8")
    xmb, focus_count = re.subn(r'\s*<button id="tab-focus"[^>]*>.*?</button>', "", xmb, count=1)
    focus_handler = 'document.getElementById("tab-focus").onclick=()=>{window.location.href="main_menu_mock.html";};'
    restart_handler = 'document.getElementById("tab-first-boot").onclick=()=>{window.location.href="first_boot_mock.html";};'
    if focus_count != 1 or xmb.count(focus_handler) != 1 or xmb.count(restart_handler) != 1:
        raise SystemExit("XMB navigation shape changed; update the inline preview adapter")
    xmb = xmb.replace(focus_handler, "")
    xmb = xmb.replace(restart_handler,
                      'document.getElementById("tab-first-boot").onclick=()=>{window.parent.restartSetupPreview();};')

    html = OUTPUT.read_text(encoding="utf-8")
    start = html.index(START) + len(START)
    end = html.index(END, start)
    encoded = json.dumps(xmb, ensure_ascii=False).replace("</", "<\\/")
    result = html[:start] + f'\n<script id="xmb-document" type="application/json">{encoded}</script>\n' + html[end:]
    OUTPUT.write_text(result, encoding="utf-8", newline="\n")
    print(f"Embedded XMB in {OUTPUT.name}: {len(xmb)} characters")


if __name__ == "__main__":
    main()
