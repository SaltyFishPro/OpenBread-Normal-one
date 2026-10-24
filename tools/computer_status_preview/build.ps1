$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force tmp/computer-status-render | Out-Null
    # Reuse the existing extraction of the firmware's current resident font.
    python tools/answers_book_preview/prepare_font.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src `
        -c tmp/answers-book-render/fonts.c `
        -o tmp/computer-status-render/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/computer_status_preview/preview.cpp',
        'tmp/computer-status-render/fonts.o',
        'src/ui/pages/ComputerStatusPage.cpp',
        'src/ui/DetailHeader.cpp',
        'src/ui/IconBitmap.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 `
        -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src `
        @sources -o tmp/computer-status-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/computer-status-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with pillow python tools/computer_status_preview/compose.py
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
