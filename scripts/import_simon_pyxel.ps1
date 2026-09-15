param(
    [Parameter(Mandatory=$true)][string]$Pyxel,
    [string]$OutputDirectory = "."
)
$ErrorActionPreference = "Stop"
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("sciv-pyxel-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    tar -xf $Pyxel -C $temporary
    $document = Get-Content (Join-Path $temporary "docData.json") -Raw | ConvertFrom-Json
    $layer = $document.canvas.layers.'2'
    if ($layer.name -ne "MainSimon" -or $document.canvas.width -ne 608 -or $document.canvas.height -ne 1024) {
        throw "Expected the 608x1024 MainSimon layout"
    }
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $layout = New-Object 'System.Int16[]' (38 * 64)
    [Array]::Fill($layout, [int16]-1)
    foreach ($property in $layer.tileRefs.PSObject.Properties) {
        $layout[[int]$property.Name] = [int16]$property.Value.index
    }
    $bytes = New-Object byte[] ($layout.Length * 2)
    [Buffer]::BlockCopy($layout, 0, $bytes, 0, $bytes.Length)
    [IO.File]::WriteAllBytes((Join-Path $OutputDirectory "simon_full_frames.layout"), $bytes)
    Copy-Item (Join-Path $temporary "layer2.png") (Join-Path $OutputDirectory "simon_full_frames.png") -Force
    Copy-Item (Join-Path $temporary "layer2.png") (Join-Path $OutputDirectory "simon_full_frames.reference.png") -Force
    Write-Host "Imported MainSimon editable PNG, reference PNG, and layout"
} finally {
    if (Test-Path $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
