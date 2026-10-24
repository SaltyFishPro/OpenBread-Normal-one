# System Update Frame Preview

From the repository root in PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/system_update_preview/build.ps1
```

The tool compiles the production `SystemUpdatePage`, the actual ST7305 framebuffer
driver, the retained resident Latin/numeric font, and the optional manifest log parser. It writes
native 384x168 PGM/PNG frames and a nearest-neighbor contact sheet to
`tmp/system-update-render/`.

The executable checks local and remote logs, 4-second automatic scrolling, 8-second
manual pause, bounded paging, popup default-cancel behavior, download progress,
error/success states, exit and re-entry reset, `millis()` wraparound, UTF-8 line
boundaries, and old/escaped/invalid optional manifest notes. These are host-side
logic and framebuffer checks; OTA networking, SPI timing, physical ghosting, and
button feel still require the device.
