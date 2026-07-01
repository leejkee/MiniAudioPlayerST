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

# ---- 路径解析 ----
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not $UvprojxPath) {
    $UvprojxPath = Join-Path $ScriptDir "firmware\MiniAudioPlayerST\MDK-ARM\MiniAudioPlayerST.uvprojx"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $ScriptDir "compile_commands.json"
}

if (-not (Test-Path $UvprojxPath)) {
    Write-Error "找不到工程文件: $UvprojxPath"
    exit 1
}

$MDKDir = (Resolve-Path (Split-Path -Parent $UvprojxPath)).Path

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

# ---- 构建基础编译参数 ----
$baseArgs = @(
    "-mcpu=$cpuType",
    "-mthumb",
    "-mfloat-abi=soft",
    "-c"
)
foreach ($d in $defineList) { $baseArgs += "-D$d" }
foreach ($inc in $includeList) { $baseArgs += "-I$inc" }

# ---- 提取所有 C 源文件 (跳过 .s 汇编文件和 .h 头文件) ----
$fileNodes = $xml.SelectNodes("//FilePath")
$result = @()

foreach ($node in $fileNodes) {
    $relPath = $node.InnerText.Trim()

    # 只处理 .c 文件
    if ($relPath -notmatch '\.c$') { continue }

    # 解析为绝对路径并转为正斜杠 (clangd 跨平台兼容)
    $absFile = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($MDKDir, $relPath))
    $absFile = $absFile -replace '\\', '/'
    $absDir = $MDKDir -replace '\\', '/'

    $entry = [ordered]@{
        directory = $absDir
        arguments = @($baseArgs + $relPath)
        file      = $absFile
    }
    $result += $entry
}

Write-Host "源文件数: $($result.Count)"

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
