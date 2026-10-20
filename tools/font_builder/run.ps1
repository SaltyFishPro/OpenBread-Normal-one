$ErrorActionPreference = 'Stop'
$fontPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $fontPython)) {
    python -m venv (Join-Path $PSScriptRoot '.venv')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $fontPython -c "import tkinter, PIL, fontTools; from PIL import ImageFont; assert hasattr(ImageFont, 'Layout')"
if ($LASTEXITCODE -ne 0) {
    & $fontPython -m pip install -r (Join-Path $PSScriptRoot 'requirements.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $fontPython -X utf8 (Join-Path $PSScriptRoot 'font_builder_gui.py')
exit $LASTEXITCODE
