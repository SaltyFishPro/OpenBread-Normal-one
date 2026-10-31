$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $root
try {
    New-Item -ItemType Directory -Force tmp/game-2048-render | Out-Null
    python tools/answers_book_preview/prepare_font.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src `
        -c tmp/answers-book-render/fonts.c `
        -o tmp/game-2048-render/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/game_2048_preview/test_game.cpp',
        'tmp/game-2048-render/fonts.o',
        'src/ui/pages/Game2048.cpp',
        'src/ui/Segment7Font.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++20 -O1 `
        -DOB_LOG_LEVEL=0 -I tools/game_2048_preview/stubs `
        -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src `
        @sources -o tmp/game-2048-render/test_game.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/game-2048-render/test_game.exe
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
