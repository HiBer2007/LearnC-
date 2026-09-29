<#
.SYNOPSIS
    创建一台用于远程调试练习的 Ubuntu 虚拟机。

.DESCRIPTION
    本脚本以 PowerShell 7 编写，需要以管理员身份运行。

    它会完成：
      1. 检查 Hyper-V 与所需镜像是否就绪
      2. 创建一台第 2 代（UEFI）虚拟机
      3. 配置内存、处理器、磁盘与网络
      4. 挂载 Ubuntu Server 安装镜像并设置启动顺序
      5. 启动虚拟机，进入安装程序

    脚本可以重复运行：若同名虚拟机已存在，不会重复创建。

.PARAMETER VMName
    虚拟机名称，默认为 Ubuntu-Debug。

.PARAMETER MemoryGB
    启动内存（GB），默认为 8。会同时启用动态内存。

.PARAMETER CPUCount
    虚拟处理器数量，默认为 4。

.PARAMETER DiskGB
    虚拟磁盘大小（GB），默认为 60。

.PARAMETER IsoPath
    Ubuntu Server 安装镜像的完整路径。

.EXAMPLE
    pwsh -File .\创建Ubuntu虚拟机.ps1
    pwsh -File .\创建Ubuntu虚拟机.ps1 -VMName Ubuntu-Test -MemoryGB 4

.NOTES
    安装系统时的建议选项见同目录的《虚拟机与远程调试.md》。
#>

[CmdletBinding()]
param(
    [string] $VMName    = 'Ubuntu-Debug',
    [int]    $MemoryGB  = 8,
    [int]    $CPUCount  = 4,
    [int]    $DiskGB    = 60,
    [string] $IsoPath   = 'K:\系统镜像\ubuntu-24.04.5-live-server-amd64.iso'
)

$ErrorActionPreference = 'Stop'

function Write-Step { param([string]$Text) Write-Host "`n=== $Text ===" -ForegroundColor Cyan }
function Write-Ok   { param([string]$Text) Write-Host "  [OK]   $Text" -ForegroundColor Green }
function Write-Warn { param([string]$Text) Write-Host "  [注意] $Text" -ForegroundColor Yellow }
function Write-Err  { param([string]$Text) Write-Host "  [错误] $Text" -ForegroundColor Red }

# ---------------------------------------------------------------- 权限检查
Write-Step '检查运行权限'

$identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Err '本脚本需要管理员权限。'
    Write-Host ''
    Write-Host '  请右键点击「终端」或「PowerShell 7」，选择「以管理员身份运行」，然后重新执行：' -ForegroundColor Yellow
    Write-Host "    pwsh -File `"$PSCommandPath`"" -ForegroundColor White
    Write-Host ''
    exit 1
}
Write-Ok '已获得管理员权限'

# ---------------------------------------------------------------- 环境检查
Write-Step '检查 Hyper-V'

$hyperv = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -ErrorAction SilentlyContinue
if ($hyperv -and $hyperv.State -ne 'Enabled') {
    Write-Err 'Hyper-V 功能未启用。'
    Write-Host '  启用方法（需重启）：' -ForegroundColor Yellow
    Write-Host '    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -All' -ForegroundColor White
    exit 1
}

try {
    $null = Get-VMHost
    Write-Ok 'Hyper-V 服务可用'
} catch {
    Write-Err "无法访问 Hyper-V：$($_.Exception.Message)"
    exit 1
}

Write-Step '检查安装镜像'

if (-not (Test-Path -LiteralPath $IsoPath)) {
    Write-Err "找不到镜像文件：$IsoPath"
    Write-Host ''
    Write-Host '  请确认已下载 Ubuntu Server 镜像，或改用 -IsoPath 指定其他位置。' -ForegroundColor Yellow
    Write-Host '  官方下载地址：' -ForegroundColor Yellow
    Write-Host '    https://releases.ubuntu.com/24.04.5/ubuntu-24.04.5-live-server-amd64.iso' -ForegroundColor White
    Write-Host ''
    Write-Host '  当前已有的镜像：' -ForegroundColor Yellow
    Get-ChildItem 'K:\系统镜像' -Filter '*.iso' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match 'ubuntu' } |
        ForEach-Object { Write-Host "    $($_.FullName)" }
    exit 1
}
$isoItem = Get-Item -LiteralPath $IsoPath
Write-Ok ("镜像就绪：{0}（{1:N2} GB）" -f $isoItem.Name, ($isoItem.Length / 1GB))

Write-Step '检查虚拟交换机'

$switch = Get-VMSwitch -ErrorAction SilentlyContinue | Where-Object { $_.SwitchType -eq 'Internal' -or $_.Name -eq 'Default Switch' } | Select-Object -First 1
if (-not $switch) {
    Write-Warn '未找到可用的虚拟交换机，将尝试创建一台内部交换机。'
    $switch = New-VMSwitch -Name 'VM-Debug-Switch' -SwitchType Internal
}
Write-Ok ("使用虚拟交换机：{0}（{1}）" -f $switch.Name, $switch.SwitchType)

if ($switch.Name -ne 'Default Switch') {
    Write-Warn '内部交换机默认没有网络地址转换，虚拟机可能无法访问外网。'
    Write-Warn '若安装程序提示网络不可用，可先跳过，系统装好后再配置。'
}

# ---------------------------------------------------------------- 创建虚拟机
Write-Step "创建虚拟机 $VMName"

$existing = Get-VM -Name $VMName -ErrorAction SilentlyContinue
if ($existing) {
    Write-Warn "虚拟机 $VMName 已存在（状态：$($existing.State)），跳过创建。"
    Write-Host "  如需重建，请先执行：  Remove-VM -Name $VMName -Force" -ForegroundColor Yellow
    Write-Host "  并删除磁盘文件后重新运行本脚本。" -ForegroundColor Yellow
} else {
    $vmRoot = Join-Path (Get-VMHost).VirtualMachinePath $VMName
    $vhdDir = Join-Path (Get-VMHost).VirtualHardDiskPath $VMName
    New-Item -ItemType Directory -Force -Path $vmRoot, $vhdDir | Out-Null

    $vm = New-VM -Name $VMName `
                 -Generation 2 `
                 -MemoryStartupBytes ($MemoryGB * 1GB) `
                 -NewVHDPath (Join-Path $vhdDir "$VMName.vhdx") `
                 -NewVHDSizeBytes ($DiskGB * 1GB) `
                 -SwitchName $switch.Name `
                 -Path $vmRoot
    Write-Ok "虚拟机已创建（第 2 代 / UEFI）"

    # 内存与处理器
    Set-VM -Name $VMName -ProcessorCount $CPUCount -DynamicMemory `
           -MemoryMinimumBytes 2GB -MemoryMaximumBytes ($MemoryGB * 2 * 1GB) | Out-Null
    Set-VM -Name $VMName -AutomaticCheckpointsEnabled $false | Out-Null
    Write-Ok ("内存 {0} GB（动态，上限 {1} GB），处理器 {2} 个" -f $MemoryGB, ($MemoryGB * 2), $CPUCount)

    # 关闭 Secure Boot，避免部分镜像无法引导
    Set-VMFirmware -VMName $VMName -EnableSecureBoot Off | Out-Null
    Write-Ok 'Secure Boot 已关闭'
}

# ---------------------------------------------------------------- 挂载镜像
Write-Step '挂载安装镜像并设置启动顺序'

$dvd = Get-VMDvdDrive -VMName $VMName -ErrorAction SilentlyContinue
if ($dvd -and $dvd.Path -eq $IsoPath) {
    Write-Ok '镜像已挂载，跳过'
} else {
    Add-VMDvdDrive -VMName $VMName -Path $IsoPath
    Write-Ok '镜像已挂载到虚拟光驱'
}

Set-VMFirmware -VMName $VMName -FirstBootDevice (Get-VMDvdDrive -VMName $VMName) | Out-Null
Write-Ok '启动顺序：光驱优先'

# ---------------------------------------------------------------- 启动
Write-Step '启动虚拟机'

$state = (Get-VM -Name $VMName).State
if ($state -eq 'Off') {
    Start-VM -Name $VMName
    Write-Ok '虚拟机已启动'
} else {
    Write-Warn "虚拟机当前状态为 $state，未重新启动。"
}

# ---------------------------------------------------------------- 后续提示
Write-Host ''
Write-Host ('─' * 68) -ForegroundColor DarkGray
Write-Host ' 安装系统时请留意以下几点' -ForegroundColor Cyan
Write-Host ('─' * 68) -ForegroundColor DarkGray
Write-Host ''
Write-Host '  1. 安装类型选择「Ubuntu Server」，不要选 minimized 版本'
Write-Host '  2. 网络配置：若自动获取到地址则直接继续'
Write-Host '  3. 存储：使用整块磁盘，不要启用 LVM 加密（练习环境不需要）'
Write-Host '  4. 务必勾选「Install OpenSSH server」——远程调试依赖它'
Write-Host '  5. 建议创建一个普通用户，例如 student'
Write-Host ''
Write-Host ' 安装完成后，回到 Windows 用这条命令确认虚拟机地址：' -ForegroundColor Cyan
Write-Host "   Get-VMNetworkAdapter -VMName $VMName | Select-Object -ExpandProperty IPAddresses" -ForegroundColor White
Write-Host ''
Write-Host ' 随后即可用 SSH 连接（用户名与地址按实际替换）：' -ForegroundColor Cyan
Write-Host '   ssh student@<虚拟机地址>' -ForegroundColor White
Write-Host ''
Write-Host ('─' * 68) -ForegroundColor DarkGray
Write-Host ''
Write-Host ' 虚拟机的连接方式（Hyper-V 管理器 → 双击虚拟机 → 连接）' -ForegroundColor Gray
Write-Host ' 无法使用时，也可用 vmconnect.exe 打开控制台。' -ForegroundColor Gray
Write-Host ''
