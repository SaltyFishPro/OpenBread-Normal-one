$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    python tools/ui_sd_font_preview/prepare_codepoints.py --check
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    foreach ($size in @(14, 20, 24)) {
        python tools/font_builder/font_ui_subset.py --size $size --check
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    python tools/ui_sd_font_preview/prepare.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 -I lib/ST7305_MonoTFT_Library/src `
        -c tmp/ui-sd-font/fonts.c -o tmp/ui-sd-font/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/ui_sd_font_preview/test.cpp', 'tmp/ui-sd-font/fonts.o',
        'src/services/UiFontCache.cpp', 'src/services/SdBitmapFont.cpp',
        'src/bsp/SdFontFile.cpp', 'src/bsp/SdFontCacheBuffer.cpp',
        'src/ui/SdBitmapText.cpp', 'src/ui/SdUiText.cpp', 'src/ui/DetailHeader.cpp',
        'src/ui/PopupView.cpp', 'src/ui/IconBitmap.cpp', 'src/ui/Segment7Font.cpp',
        'src/ui/pages/HomePage.cpp', 'src/ui/assets/main_menu/icons8-clock.cpp',
        'src/ui/pages/SdUsbPage.cpp', 'src/ui/pages/ComputerStatusPage.cpp',
        'src/ui/pages/TimeCalibrationPage.cpp', 'src/ui/pages/FocusClockPage.cpp',
        'src/ui/pages/RemotePage.cpp', 'src/ui/pages/SettingsPage.cpp',
        'src/ui/pages/AnswersBookPage.cpp', 'src/ui/pages/ReaderPage.cpp',
        'src/services/ReaderService.cpp', 'src/ui/pages/SystemUpdatePage.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -DOB_SD_FONT_LOG_ENABLED=0 -DOB_LOG_LEVEL=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/ui_sd_font_preview/stubs -I tools/reader_font_preview/stubs `
        -I tools/sd_font_tests/stubs -I tools/system_update_preview/stubs `
        -I tools/answers_book_preview/stubs -I . -I lib/ST7305_MonoTFT_Library/src `
        @sources -o tmp/ui-sd-font/test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/ui-sd-font/test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with pillow python tools/ui_sd_font_preview/compare.py
    exit $LASTEXITCODE
}
finally { Pop-Location }
