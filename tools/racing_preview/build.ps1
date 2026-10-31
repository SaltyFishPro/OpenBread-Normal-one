$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force tmp/racing-render | Out-Null
    python tools/answers_book_preview/prepare_font.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang cc -O1 `
        -I lib/ST7305_MonoTFT_Library/src `
        -c tmp/answers-book-render/fonts.c `
        -o tmp/racing-render/fonts.o
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $sources = @(
        'tools/racing_preview/preview.cpp',
        'tmp/racing-render/fonts.o',
        'src/ui/pages/RacingGame.cpp',
        'src/ui/pages/RacingGameRender.cpp',
        'src/ui/pages/RacingCarMesh.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
        'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
        'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp'
    )
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++20 -O1 `
        -DOB_LOG_LEVEL=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -I tools/answers_book_preview/stubs -I . `
        -I lib/ST7305_MonoTFT_Library/src `
        @sources -o tmp/racing-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/racing-render/preview.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++20 -O1 `
        -DOB_LOG_LEVEL=0 -I tools/answers_book_preview/stubs -I . `
        tools/racing_preview/test_steering.cpp `
        src/services/RacingSteeringService.cpp `
        -o tmp/racing-render/test_steering.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/racing-render/test_steering.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++20 -O1 `
        -DOB_LOG_LEVEL=0 -I tools/answers_book_preview/stubs -I . `
        tools/racing_preview/test_game.cpp `
        src/ui/pages/RacingGame.cpp `
        -o tmp/racing-render/test_game.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/racing-render/test_game.exe
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
