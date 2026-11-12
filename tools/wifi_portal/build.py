from pathlib import Path

"""Regenerate src/services/WifiPortalPage.cpp from the single-page source.

The provisioning page is served from flash via WebServer::send_P. Keep the
source as a standalone HTML file so it stays reviewable and diffable, then run:

    python tools/wifi_portal/build.py

to embed it into the raw string that the firmware compiles.
"""

root = Path(__file__).resolve().parents[2]
source = root / 'tools' / 'wifi_portal' / 'index.html'
target = root / 'src' / 'services' / 'WifiPortalPage.cpp'

html = source.read_text(encoding='utf-8')
if '\x00' in html:
    raise SystemExit('page source contains a NUL byte; not embeddable')
delimiter = 'WIFIPORTAL'
if f'){delimiter}"' in html:
    raise SystemExit('raw string delimiter appears in page source')

body = f'''#include "WifiPortalPage.h"

#include <Arduino.h>

// Generated from tools/wifi_portal/index.html by tools/wifi_portal/build.py.
// The page is served from flash and streamed with WebServer::send_P.
const char kWifiPortalPage[] PROGMEM = R"{delimiter}({html}){delimiter}";
'''
target.write_text(body, encoding='utf-8', newline='\n')
print(f'wrote {target} ({len(body)} bytes)')
