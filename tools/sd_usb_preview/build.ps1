$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force tmp/sd-usb-render | Out-Null
    python tools/sd_usb_preview/prepare_font.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src -c tmp/sd-usb-render/fonts.c `
        -o tmp/sd-usb-render/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/sd_usb_preview/preview.cpp',
        'tmp/sd-usb-render/fonts.o',
        'src/ui/pages/HomePage.cpp',
        'src/ui/assets/main_menu/icons8-clock.cpp',
        'src/ui/pages/SdUsbPage.cpp',
        'src/ui/DetailHeader.cpp',
        'src/ui/IconBitmap.cpp',
        'src/ui/Segment7Font.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src @sources -o tmp/sd-usb-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/sd-usb-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -DOB_SD_USB_LOG_ENABLED=0 -I tools/sd_usb_preview/stubs -I . `
        tools/sd_usb_preview/switch_test.cpp src/bsp/SdUsbSwitch.cpp `
        -o tmp/sd-usb-render/switch-test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/sd-usb-render/switch-test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with pillow python tools/sd_usb_preview/compose.py
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
