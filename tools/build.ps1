# ============================================================================
# 3xorStorage - Build completo: texturas PNG -> PAA -> PBO -> dist\@3xorStorage
# Uso:  .\tools\build.ps1               (build completo)
#       .\tools\build.ps1 -SkipTextures (no regenera texturas, solo el PBO)
# ============================================================================
param(
    [switch]$SkipTextures,
    [string]$Python = "C:\Users\Exor1\anaconda3\python.exe",
    [string]$ImageToPAA = "D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\ImageToPAA\ImageToPAA.exe"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path $Python)) { throw "No se encontro Python en $Python" }

if (-not $SkipTextures) {
    if (-not (Test-Path $ImageToPAA)) { throw "No se encontro ImageToPAA en $ImageToPAA (instala DayZ Tools)" }

    Write-Host "[1/3] Generando texturas PNG..." -ForegroundColor Cyan
    & $Python "$PSScriptRoot\gen_textures.py"
    if ($LASTEXITCODE -ne 0) { throw "gen_textures.py fallo" }

    Write-Host "[2/3] Convirtiendo PNG -> PAA..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force "$repo\src\ExorStorage\data" | Out-Null
    foreach ($name in @("exor_barrel_500_co")) {
        & $ImageToPAA "$repo\assets\textures\$name.png" "$repo\src\ExorStorage\data\$name.paa"
        if ($LASTEXITCODE -ne 0) { throw "ImageToPAA fallo para $name" }
    }
} else {
    Write-Host "[1-2/3] Texturas: SKIP" -ForegroundColor Yellow
}

Write-Host "[3/3] Empaquetando PBO..." -ForegroundColor Cyan
& $Python "$PSScriptRoot\pack_pbo.py"
if ($LASTEXITCODE -ne 0) { throw "pack_pbo.py fallo" }

# mod.cpp para la carpeta del mod (Workshop / launcher)
Copy-Item "$repo\mod\mod.cpp" "$repo\dist\@3xor_Vanilla_Optimization\mod.cpp" -Force

Write-Host ""
Write-Host "Build OK -> $repo\dist\@3xor_Vanilla_Optimization" -ForegroundColor Green
