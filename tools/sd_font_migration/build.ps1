$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    python tools/sd_font_migration/prepare_test.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src -c tmp/sd-font-migration/fonts.c -o tmp/sd-font-migration/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/sd_font_migration/test.cpp', 'tmp/sd-font-migration/fonts.o',
        'src/ui/pages/AnswersBookPage.cpp', 'src/ui/pages/SystemUpdatePage.cpp',
        'src/ui/pages/ReaderPage.cpp', 'src/services/ReaderService.cpp',
        'src/bsp/SdFontFile.cpp', 'src/bsp/SdFontCacheBuffer.cpp', 'src/services/SdBitmapFont.cpp',
        'src/ui/SdBitmapText.cpp', 'src/ui/SdUiText.cpp', 'src/services/UiFontCache.cpp', 'src/ui/DetailHeader.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -DOB_SD_FONT_LOG_ENABLED=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/reader_font_preview/stubs -I tools/sd_font_tests/stubs `
        -I tools/system_update_preview/stubs -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src @sources -o tmp/sd-font-migration/test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    foreach ($mode in @('psram', 'no-psram')) {
        & tmp/sd-font-migration/test.exe "--$mode"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        uv run --with pillow python tools/sd_font_migration/compare.py
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally { Pop-Location }
