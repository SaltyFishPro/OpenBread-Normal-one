$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force tmp | Out-Null
    foreach ($test in @('store', 'service')) {
        $sources = @("tools/wifi_device_tests/${test}_test.cpp", 'src/services/WifiNetworkStore.cpp')
        if ($test -eq 'service') { $sources += 'src/services/WifiProvisionService.cpp' }
        uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 -DOB_LOG_LEVEL=0 `
            -I tools/wifi_device_tests/stubs -I tools/sd_font_tests/stubs -I tools/ui_sd_font_preview/stubs -I . `
            @sources -o "tmp/wifi-$test-test.exe"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & "tmp/wifi-$test-test.exe"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally { Pop-Location }
