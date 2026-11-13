<#
  生成「便携版 VSCode」的用户设置。
  用法: powershell -NoProfile -ExecutionPolicy Bypass -File make_portable_settings.ps1 -Src <家里 settings.json> -DstUser <U盘 data\user-data\User>
  做两件事:
    1) 备份家里的原设置成 settings.home-orig.json
    2) 复制原设置，但剔除指死本机 C: 的项（换电脑必然失效），再补上实验室必须的项
       (关闭受限模式/自动更新/遥测，避免离线机器上弹窗和扩展被禁用)
#>
param(
    [string]$Src = "",
    [string]$DstUser
)

New-Item -ItemType Directory -Force -Path $DstUser | Out-Null
$dst = Join-Path $DstUser 'settings.json'
$bak = Join-Path $DstUser 'settings.home-orig.json'

$drop = @(
    'cmake.environment',
    'stm32cube-ide-core.configuration.productSTM32CubeMX.executablePath'
)
$add = [ordered]@{
    'security.workspace.trust.enabled'        = $false
    'security.workspace.trust.untrustedFiles' = 'open'
    'extensions.autoCheckUpdates'             = $false
    'extensions.autoUpdate'                   = $false
    'extensions.ignoreRecommendations'        = $true
    'update.mode'                             = 'none'
    'telemetry.telemetryLevel'                = 'off'
    'cmake.configureOnOpen'                   = $false
    'workbench.tips.enabled'                  = $false
}

$obj = [ordered]@{}
if ($Src -and (Test-Path $Src)) {
    if ([System.IO.Path]::GetFullPath($Src) -ne [System.IO.Path]::GetFullPath($bak)) { Copy-Item $Src $bak -Force }
    try {
        $j = Get-Content -Raw -Encoding UTF8 $Src | ConvertFrom-Json
        foreach ($p in $j.PSObject.Properties) {
            if ($drop -notcontains $p.Name) { $obj[$p.Name] = $p.Value }
        }
    } catch {
        Write-Host ("      [警告] 原设置解析失败，改用默认设置：" + $_.Exception.Message)
    }
} else {
    Write-Host "      [跳过] 本机没有用户设置，直接生成默认便携设置"
}

foreach ($k in $add.Keys) { $obj[$k] = $add[$k] }

$json = $obj | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($dst, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("      已写入 " + $dst + " ( " + $obj.Count + " 项 )")
