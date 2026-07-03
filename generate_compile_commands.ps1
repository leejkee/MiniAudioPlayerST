<#
.SYNOPSIS
    从 MDK-ARM .uvprojx 生成 compile_commands.json，供 clangd LSP 使用。
.DESCRIPTION
    解析 Keil MDK 工程文件 (.uvprojx)，提取 include 路径、宏定义和源文件列表，
    生成 clangd 所需的 compile_commands.json 编译数据库。

    每次 CubeMX 重新生成代码后，在仓库根目录运行此脚本即可：
      .\generate_compile_commands.ps1
#>
param(
    [string]$UvprojxPath = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

# ============================================================================
# 关键目录变量 (改为 Windows 路径, 使用时按需转 Unix 斜杠)
# ============================================================================

# 仓库根目录 (脚本所在目录)
$RepoRoot = (Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path

# CubeMX 固件工程根目录 (与 .ioc 同级, 含 Core/Drivers/BSP/App)
$FirmwareRoot = Join-Path $RepoRoot "firmware\MiniAudioPlayerST"

# MDK-ARM 目录 (含 .uvprojx)
$MDKDir = Join-Path $FirmwareRoot "MDK-ARM"

# ---- 参数默认值 ----
if (-not $UvprojxPath) {
    $UvprojxPath = Join-Path $MDKDir "MiniAudioPlayerST.uvprojx"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $RepoRoot "compile_commands.json"
}

if (-not (Test-Path $UvprojxPath)) {
    Write-Error "找不到工程文件: $UvprojxPath"
    exit 1
}

Write-Host "=== 解析 MDK-ARM 工程 ==="
Write-Host "工程文件: $UvprojxPath"

# ---- 解析 XML ----
[xml]$xml = Get-Content $UvprojxPath -Encoding UTF8

$target = $xml.Project.Targets.Target
Write-Host "目标名称: $($target.TargetName)"

# ---- 提取 CPU 架构 ----
$cpuText = $target.TargetOption.TargetCommonOption.Cpu
$cpuType = "cortex-m0"  # 默认值
if ($cpuText -match 'CPUTYPE\("([^"]+)"\)') {
    $cpuType = ($Matches[1] -replace '\s+', '-').ToLower()
}
Write-Host "CPU 架构: $cpuType"

# ---- 提取宏定义 ----
$definesStr = $target.TargetOption.TargetArmAds.Cads.VariousControls.Define
$defineList = @()
if ($definesStr) {
    $defineList = $definesStr -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
}
Write-Host "宏定义  : $($defineList -join ', ')"

# ---- 提取 include 路径 (相对于 MDK-ARM 目录) ----
$includeStr = $target.TargetOption.TargetArmAds.Cads.VariousControls.IncludePath
$includeList = @()
if ($includeStr) {
    $includeList = $includeStr -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
}
Write-Host "Include : $($includeList -join '; ')"

# ---- 构建固件层基础编译参数 (MDK-ARM 相对路径) ----
$firmwareBaseArgs = @(
    "--target=arm-none-eabi",
    "-mcpu=$cpuType",
    "-mthumb",
    "-mfloat-abi=soft",
    "-c"
)
foreach ($d in $defineList) { $firmwareBaseArgs += "-D$d" }
foreach ($inc in $includeList) { $firmwareBaseArgs += "-I$inc" }
# 补充业务层 include 路径 (App/BSP), MDK-ARM 目录下的相对路径为 ../App/include 和 ../BSP/include
$firmwareBaseArgs += "-I../App/include"
$firmwareBaseArgs += "-I../BSP/include"

# ---- 构建固件工程根目录基准编译参数 (BSP/App 业务层使用) ----
# Unix 斜杠版本
$RepoRootUnix    = $RepoRoot    -replace '\\', '/'
$FirmwareRootUnix = $FirmwareRoot -replace '\\', '/'
$MDKDirUnix       = $MDKDir       -replace '\\', '/'

# 业务层 include 路径 (相对于 $FirmwareRoot)
$businessIncludes = @(
    "App/include",
    "BSP/include",
    "Core/Inc",
    "Drivers/STM32F0xx_HAL_Driver/Inc",
    "Drivers/STM32F0xx_HAL_Driver/Inc/Legacy",
    "Drivers/CMSIS/Device/ST/STM32F0xx/Include",
    "Drivers/CMSIS/Include"
)

$businessBaseArgs = @(
    "--target=arm-none-eabi",
    "-mcpu=$cpuType",
    "-mthumb",
    "-mfloat-abi=soft",
    "-c"
)
foreach ($d in $defineList) { $businessBaseArgs += "-D$d" }
foreach ($inc in $businessIncludes) { $businessBaseArgs += "-I$inc" }

# ---- 提取所有 C 源文件 (Keil 工程中的 .c 文件) ----
$fileNodes = $xml.SelectNodes("//FilePath")
$result = @()

foreach ($node in $fileNodes) {
    $relPath = $node.InnerText.Trim()

    # 只处理 .c 文件
    if ($relPath -notmatch '\.c$') { continue }

    # 解析为绝对路径并转为正斜杠 (clangd 跨平台兼容)
    $absFile = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($MDKDir, $relPath))
    $absFile = $absFile -replace '\\', '/'
    $absDir = $MDKDirUnix

    $entry = [ordered]@{
        directory = $absDir
        arguments = @($firmwareBaseArgs + $relPath)
        file      = $absFile
    }
    $result += $entry
}

Write-Host "固件源文件: $($result.Count)"

# ---- 扫描 BSP/ 和 App/ 业务层 .c 文件 (位于固件工程根目录下) ----
$businessSources = @()

# BSP/src/*.c
$bspSrcDir = Join-Path $FirmwareRoot "BSP\src"
if (Test-Path $bspSrcDir) {
    $bspFiles = Get-ChildItem -Path $bspSrcDir -Filter "*.c" -File
    foreach ($f in $bspFiles) {
        $businessSources += @{
            FullPath = $f.FullName
            Relative  = $f.FullName.Replace($FirmwareRoot + '\', '') -replace '\\', '/'
        }
    }
}

# App/ 递归扫描 *.c (覆盖 src/ test/ 等所有子目录)
$appDir = Join-Path $FirmwareRoot "App"
if (Test-Path $appDir) {
    $appFiles = Get-ChildItem -Path $appDir -Recurse -Filter "*.c" -File
    foreach ($f in $appFiles) {
        $businessSources += @{
            FullPath = $f.FullName
            Relative  = $f.FullName.Replace($FirmwareRoot + '\', '') -replace '\\', '/'
        }
    }
}

Write-Host "业务层源文件: $($businessSources.Count)"

foreach ($src in $businessSources) {
    $absFile = $src.FullPath -replace '\\', '/'

    $entry = [ordered]@{
        directory = $FirmwareRootUnix
        arguments = @($businessBaseArgs + $src.Relative)
        file      = $absFile
    }
    $result += $entry
}

Write-Host "总源文件数: $($result.Count)"

# ---- 写入 compile_commands.json (UTF-8 无 BOM) ----
$json = $result | ConvertTo-Json -Depth 5
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($OutputPath, $json, $utf8NoBom)

Write-Host "=== 完成 ==="
Write-Host "输出: $OutputPath"
Write-Host ""
Write-Host "clangd 现在应该可以正常跳转和补全。"
Write-Host "每次 CubeMX 重新生成代码后，重新运行此脚本即可："
Write-Host "  .\generate_compile_commands.ps1"
