$ErrorActionPreference='Stop'
Push-Location (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
try {
  python tools/font_setup_tests/prepare.py
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  uv run --with ziglang==0.13.0 python -m ziglang cc -O1 -I lib/ST7305_MonoTFT_Library/src -c tmp/boot-fonts.c -o tmp/boot-fonts.o
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 -DOB_LOG_LEVEL=0 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
    -I tools/font_setup_tests/stubs -I tools/wifi_device_tests/stubs -I tools/sd_font_tests/stubs -I tools/ui_sd_font_preview/stubs -I tools/answers_book_preview/stubs -I . -I lib/ST7305_MonoTFT_Library/src `
    tools/font_setup_tests/test.cpp src/services/FontSetupService.cpp src/services/WifiProvisionService.cpp src/services/WifiNetworkStore.cpp `
    src/ui/pages/FirstBootPage.cpp src/ui/pages/HomePage.cpp src/ui/IconBitmap.cpp tmp/boot-fonts.o `
    src/ui/Segment7Font.cpp src/ui/assets/main_menu/icons8-clock.cpp src/ui/assets/sleep/standby_emoji.cpp `
    lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp lib/ST7305_MonoTFT_Library/src/U8g2_for_ST73XX.cpp `
    -o tmp/font-setup-test.exe
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  & tmp/font-setup-test.exe
  exit $LASTEXITCODE
} finally {Pop-Location}
