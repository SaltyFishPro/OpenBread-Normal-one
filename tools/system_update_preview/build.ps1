$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$previewPython = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
Push-Location $projectRoot
try {
    & $previewPython tools/system_update_preview/prepare_fonts.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src -c tmp/system-update-render/fonts.c `
        -o tmp/system-update-render/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'src/bsp/SdFontFile.cpp', 'src/bsp/SdFontCacheBuffer.cpp',
        'src/services/SdBitmapFont.cpp', 'src/ui/SdBitmapText.cpp', 'src/ui/SdUiText.cpp', 'src/services/UiFontCache.cpp',
        'tools/system_update_preview/preview.cpp', 'tmp/system-update-render/fonts.o',
        'src/ui/pages/SystemUpdatePage.cpp', 'src/services/OtaReleaseNotes.cpp',
        'src/ui/DetailHeader.cpp', 'src/ui/PopupView.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -DOB_SD_FONT_LOG_ENABLED=0 -DOB_LOG_LEVEL=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/system_update_preview/stubs -I tools/reader_font_preview/stubs -I tools/sd_font_tests/stubs -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src @sources -o tmp/system-update-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/system-update-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with pillow python tools/system_update_preview/compose.py
    exit $LASTEXITCODE
}
finally { Pop-Location }
