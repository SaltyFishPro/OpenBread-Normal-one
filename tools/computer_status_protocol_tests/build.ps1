$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force tmp/computer-status-protocol-tests | Out-Null
    uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++20 -O0 `
        -DOB_COMPUTER_STATUS_LOG_ENABLED=0 `
        -I tools/computer_status_protocol_tests/stubs -I . `
        tools/computer_status_protocol_tests/protocol_tests.cpp `
        src/services/ComputerStatusProtocol.cpp `
        -o tmp/computer-status-protocol-tests/protocol_tests.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & tmp/computer-status-protocol-tests/protocol_tests.exe
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
