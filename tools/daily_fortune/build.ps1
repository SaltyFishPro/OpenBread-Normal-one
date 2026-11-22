$ErrorActionPreference='Stop'
Push-Location (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
try {
 python tools/daily_fortune/generate.py --check
 if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
 New-Item -ItemType Directory -Force tmp | Out-Null
 uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 -DOB_LOG_LEVEL=0 -I tools/wifi_device_tests/stubs -I tools/answers_book_preview/stubs -I tools/ui_sd_font_preview/stubs -I . tools/daily_fortune/test.cpp src/services/DailyFortuneService.cpp -o tmp/daily-fortune-test.exe
 if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
 & tmp/daily-fortune-test.exe
 exit $LASTEXITCODE
} finally { Pop-Location }
