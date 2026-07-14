#Requires -Version 5.1
<#
.SYNOPSIS
  Check or apply clang-format on App/ and BSP/ sources (CI-friendly).

.DESCRIPTION
  Formats only self-owned firmware code. Skips Core/, Drivers/, MDK-ARM/,
  and generated font_*.{c,h} files.

.PARAMETER Fix
  If set, write formatted files in place. Default is dry-run check (exit 1 on drift).

.PARAMETER ClangFormat
  Path to clang-format executable. Default: clang-format on PATH.

.EXAMPLE
  .\tools\clang-format-check.ps1
  .\tools\clang-format-check.ps1 -Fix
#>
param(
    [switch]$Fix,
    [string]$ClangFormat = "clang-format"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

function Test-ClangFormat {
    param([string]$Cmd)
    try {
        $null = & $Cmd --version 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

if (-not (Test-ClangFormat -Cmd $ClangFormat)) {
    Write-Error "clang-format not found ('$ClangFormat'). Install LLVM and add clang-format to PATH."
}

$roots = @(
    "firmware/MiniAudioPlayerST/App",
    "firmware/MiniAudioPlayerST/BSP"
)

$excludeNamePattern = '^(font_cn|font_en|font_file_cn)\.(c|h)$'

$files = @()
foreach ($root in $roots) {
    $full = Join-Path $repoRoot $root
    if (-not (Test-Path $full)) {
        Write-Warning "Skip missing path: $root"
        continue
    }
    Get-ChildItem -Path $full -Recurse -File -Include *.c, *.h |
        Where-Object { $_.Name -notmatch $excludeNamePattern } |
        ForEach-Object { $files += $_.FullName }
}

if ($files.Count -eq 0) {
    Write-Host "No source files to check."
    exit 0
}

Write-Host "clang-format target files: $($files.Count)"
& $ClangFormat --version

$failed = $false
if ($Fix) {
    & $ClangFormat -i @files
    if ($LASTEXITCODE -ne 0) {
        Write-Error "clang-format -i failed with exit code $LASTEXITCODE"
    }
    Write-Host "Formatted $($files.Count) file(s)."
    exit 0
}

# Check mode: --dry-run --Werror (clang-format 10+)
$output = & $ClangFormat --dry-run --Werror @files 2>&1
$code = $LASTEXITCODE
if ($code -ne 0) {
    $output | ForEach-Object { Write-Host $_ }
    Write-Host ""
    Write-Host "clang-format check failed. Run: .\tools\clang-format-check.ps1 -Fix"
    exit 1
}

Write-Host "clang-format check passed ($($files.Count) file(s))."
exit 0
