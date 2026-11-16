$ErrorActionPreference='Stop'
Push-Location (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
try {
  python tools/ui_sd_font_preview/prepare.py
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  uv run --with ziglang==0.13.0 python -m ziglang cc -O1 -I lib/ST7305_MonoTFT_Library/src -c tmp/ui-sd-font/fonts.c -o tmp/ui-sd-font/fonts.o
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  $sources=@(
    'tools/txt_reader_tests/test.cpp','tmp/ui-sd-font/fonts.o',
    'src/services/TxtReaderService.cpp','src/ui/pages/TxtReaderPage.cpp',
    'src/services/UiFontCache.cpp','src/services/SdBitmapFont.cpp',
    'src/bsp/SdFontFile.cpp','src/bsp/SdFontCacheBuffer.cpp',
    'src/ui/SdBitmapText.cpp','src/ui/SdUiText.cpp',
    'lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp',
    'lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp',
    'lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp')
  uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 -DOB_LOG_LEVEL=0 -DOB_SD_FONT_LOG_ENABLED=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
    -I tools/txt_reader_tests/stubs -I tools/wifi_device_tests/stubs -I tools/reader_font_preview/stubs -I tools/sd_font_tests/stubs -I tools/answers_book_preview/stubs -I . -I lib/ST7305_MonoTFT_Library/src @sources -o tmp/txt-reader-test.exe
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  & tmp/txt-reader-test.exe
  exit $LASTEXITCODE
} finally {Pop-Location}
