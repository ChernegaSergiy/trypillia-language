$ErrorActionPreference = "Stop"

Write-Host "=== Trypillia Programming Language Installer ===" -ForegroundColor Cyan

# 1. Check dependencies
Write-Host "[1/5] Checking dependencies..." -ForegroundColor Cyan
$missingDeps = $false
foreach ($dep in @("git", "cmake")) {
    if (!(Get-Command $dep -ErrorAction SilentlyContinue)) {
        Write-Host "Error: '$dep' is not installed. Please install it first." -ForegroundColor Red
        $missingDeps = $true
    }
}
if ($missingDeps) {
    exit 1
}
Write-Host "Dependencies OK." -ForegroundColor Green

# 2. Clone repository
$SrcDir = "$env:USERPROFILE\.trypillia-src"
Write-Host "[2/5] Cloning repository to $SrcDir..." -ForegroundColor Cyan
if (Test-Path $SrcDir) {
    Write-Host "Directory already exists. Updating..."
    Set-Location $SrcDir
    git fetch --all
    git reset --hard origin/main
} else {
    git clone https://github.com/ChernegaSergiy/trypillia-language.git $SrcDir
    Set-Location $SrcDir
}

# 3. Build
Write-Host "[3/5] Building Trypillia (this might take a few minutes)..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path build | Out-Null
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4

# 4. Install
Write-Host "[4/5] Installing binary..." -ForegroundColor Cyan
$InstallDir = "$env:USERPROFILE\.local\bin"
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

$ExeFound = $false
# MSVC creates the executable inside build\Release
if (Test-Path "build\Release\trypillia.exe") {
    Copy-Item "build\Release\trypillia.exe" -Destination "$InstallDir\trypillia.exe" -Force
    if (Test-Path "build\Release\trypillia-lsp.exe") {
        Copy-Item "build\Release\trypillia-lsp.exe" -Destination "$InstallDir\trypillia-lsp.exe" -Force
    }
    $ExeFound = $true
} 
# Ninja/MinGW creates it directly in build
elseif (Test-Path "build\trypillia.exe") {
    Copy-Item "build\trypillia.exe" -Destination "$InstallDir\trypillia.exe" -Force
    if (Test-Path "build\trypillia-lsp.exe") {
        Copy-Item "build\trypillia-lsp.exe" -Destination "$InstallDir\trypillia-lsp.exe" -Force
    }
    $ExeFound = $true
}

if (-not $ExeFound) {
    Write-Host "Error: Failed to find compiled trypillia.exe" -ForegroundColor Red
    exit 1
}

Write-Host "Installed to $InstallDir\trypillia.exe" -ForegroundColor Green

# 5. Path Setup
Write-Host "[5/5] Checking PATH..." -ForegroundColor Cyan
$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($UserPath -notmatch [regex]::Escape($InstallDir)) {
    Write-Host "Adding $InstallDir to User PATH..."
    $NewPath = "$UserPath;$InstallDir"
    [Environment]::SetEnvironmentVariable("PATH", $NewPath, "User")
    Write-Host "PATH updated. Please restart your terminal!" -ForegroundColor Green
} else {
    Write-Host "PATH is already configured!" -ForegroundColor Green
}

Write-Host "=== Installation Complete! ===" -ForegroundColor Green
Write-Host "You can now run 'trypillia' from anywhere."
