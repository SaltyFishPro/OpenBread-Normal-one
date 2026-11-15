$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $root
try {
  New-Item -ItemType Directory -Force tmp/lrc-test | Out-Null
  $lines = @('[00:01.00]第二行','[00:03.00][00:04.00]重复','[00:00.50]开始','[ar:测试]')
  [System.IO.File]::WriteAllLines((Join-Path $root 'tmp/lrc-test/song.lrc'),$lines,[System.Text.UTF8Encoding]::new($false))
  Copy-Item tmp/lrc-test/song.lrc tmp/lrc-test/other.lrc -Force
  uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 tools/music_lyrics_tests/test.cpp src/services/LrcParser.cpp -o tmp/lrc-test.exe
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  & tmp/lrc-test.exe
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  uv run --with ziglang==0.13.0 python -m ziglang c++ -std=c++17 -O1 -DOB_LOG_LEVEL=0 -I tools/sd_font_tests/stubs -I . tools/music_lyrics_tests/reader_test.cpp src/services/MusicLyrics.cpp src/services/LrcParser.cpp src/bsp/SdFontCacheBuffer.cpp -o tmp/lrc-reader-test.exe
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  & tmp/lrc-reader-test.exe
  exit $LASTEXITCODE
} finally { Pop-Location }
