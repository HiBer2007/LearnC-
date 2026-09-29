<#
.SYNOPSIS
    用预装的 Ubuntu 云镜像创建虚拟机，不需要运行安装程序。

.DESCRIPTION
    背景：用 ISO 安装时，Hyper-V 的动态内存把虚拟机压到 2.1 GB，
    Ubuntu 安装程序（subiquity）在内存不足时崩溃，安装无法完成。

    本脚本改用 Ubuntu 官方的云镜像。该镜像内的系统已经装好，
    配合 cloud-init 种子盘可以在首次启动时自动创建用户、设置密码、装入 SSH 公钥，
    因此完全不需要安装程序，也就不会遇到安装器崩溃的问题。

    镜像的处理分两步：
      1. 在 WSL 里用 qemu-img 把 qcow2 转成 VHDX（这一步不需要管理员）
      2. 本脚本负责扩容、删旧机、建新机（需要管理员）

.PARAMETER VMName
    虚拟机名称，默认 Ubuntu-Debug。

.PARAMETER DiskImage
    已经转换好的 VHDX 路径。

.PARAMETER MemoryGB
    内存大小（GB），默认 8。使用固定内存，不启用动态内存。

.PARAMETER CPUCount
    虚拟处理器数量，默认 4。

.PARAMETER DiskGB
    虚拟磁盘最终大小（GB），默认 60。
    云镜像首次启动时会自动把根分区扩展到整块磁盘。

.PARAMETER RemoveOld
    是否先删除同名的旧虚拟机，默认是。

.EXAMPLE
    pwsh -File .\创建云镜像虚拟机.ps1
#>

[CmdletBinding()]
param(
    [string] $VMName     = 'Ubuntu-Debug',
    [string] $DiskImage  = 'K:\系统镜像\Ubuntu-Debug.vhdx',
    [int]    $MemoryGB   = 8,
    [int]    $CPUCount   = 4,
    [int]    $DiskGB     = 60,
    [switch] $KeepOldVM,
    [string] $SeedDir    = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

function Write-Step { param([string]$T) Write-Host "`n=== $T ===" -ForegroundColor Cyan }
function Write-Ok   { param([string]$T) Write-Host "  [OK]   $T" -ForegroundColor Green }
function Write-Warn { param([string]$T) Write-Host "  [注意] $T" -ForegroundColor Yellow }
function Write-Err  { param([string]$T) Write-Host "  [错误] $T" -ForegroundColor Red }

# ================================================================ 权限
Write-Step '检查运行权限'
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Err '本脚本需要管理员权限。'
    Write-Host "`n  请以管理员身份运行：`n    pwsh -File `"$PSCommandPath`"`n" -ForegroundColor Yellow
    exit 1
}
Write-Ok '已获得管理员权限'

# ================================================================ 检查镜像
Write-Step '检查磁盘映像'
if (-not (Test-Path -LiteralPath $DiskImage)) {
    Write-Err "找不到磁盘映像：$DiskImage"
    Write-Host "`n  请先完成转换（不需要管理员）：" -ForegroundColor Yellow
    Write-Host '    1. 下载 https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img' -ForegroundColor White
    Write-Host '    2. 在 WSL 中执行 工具\云镜像虚拟机\转换镜像.sh' -ForegroundColor White
    Write-Host "`n  也可用 -DiskImage 指定其他位置。`n" -ForegroundColor Yellow
    exit 1
}
$img = Get-Item -LiteralPath $DiskImage
Write-Ok ("磁盘映像就绪：{0}（{1:N2} GB）" -f $img.Name, ($img.Length / 1GB))

# ================================================================ 生成种子盘
Write-Step '生成 cloud-init 种子盘'
$userData = Join-Path $SeedDir 'user-data'
$metaData = Join-Path $SeedDir 'meta-data'
foreach ($f in @($userData, $metaData)) {
    if (-not (Test-Path $f)) { Write-Err "缺少文件：$f"; exit 1 }
}

$pubKeyFile = Join-Path $env:USERPROFILE '.ssh\ubuntu_vm.pub'
if (-not (Test-Path $pubKeyFile)) {
    Write-Err "找不到 SSH 公钥：$pubKeyFile"
    Write-Host "`n  请先生成：ssh-keygen -t ed25519 -f `"$env:USERPROFILE\.ssh\ubuntu_vm`" -N `"`"`n" -ForegroundColor Yellow
    exit 1
}
$pubKey = (Get-Content $pubKeyFile -Raw).Trim()

$work = Join-Path $env:TEMP "cloudinit_$VMName"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Force -Path $work | Out-Null

(Get-Content $userData -Raw).Replace('__SSH_PUBKEY__', $pubKey) |
    Set-Content (Join-Path $work 'user-data') -Encoding UTF8 -NoNewline
Copy-Item $metaData (Join-Path $work 'meta-data')

# cloud-init 要求 LF 换行
foreach ($n in 'user-data', 'meta-data') {
    $p = Join-Path $work $n
    $t = [IO.File]::ReadAllText($p) -replace "`r`n", "`n"
    [IO.File]::WriteAllText($p, $t, (New-Object Text.UTF8Encoding($false)))
}

$seedIso = Join-Path $SeedDir 'seed.iso'
if (Test-Path $seedIso) { Remove-Item $seedIso -Force }

function ConvertTo-WslPath {
    param([string]$WinPath)
    $s = $WinPath.Replace('\', '/')
    return '/mnt/' + $s.Substring(0,1).ToLower() + $s.Substring(2)
}
$wslWork = ConvertTo-WslPath $work
$wslSeed = ConvertTo-WslPath $seedIso

# 参数都是必需的：
#   -volid CIDATA   cloud-init 靠这个卷标寻找种子盘
#   -joliet         Joliet 目录树
#   -rock           Rock Ridge 扩展
#   -l              允许 31 字符长文件名
#   -iso-level 4    让 ISO9660 主目录树也用长文件名
# 只加 -l 时 ISO9660 树里的名字会变成 USER_DATA（下划线），
# 加上 -iso-level 4 才能两棵树都是 user-data。
$cmd = "genisoimage -output '$wslSeed' -volid CIDATA -joliet -rock -l -iso-level 4 " +
       "'$wslWork/user-data' '$wslWork/meta-data' 2>&1 | tail -1"
$null = & wsl.exe -d Ubuntu -- bash -c $cmd 2>&1

if (-not (Test-Path $seedIso)) { Write-Err "种子盘生成失败"; exit 1 }
Write-Ok ("种子盘已生成（{0:N0} 字节）" -f (Get-Item $seedIso).Length)

$names = (& wsl.exe -d Ubuntu -- bash -c "isoinfo -f -i '$wslSeed' 2>/dev/null") -join ' '
if ($names -match 'user-data' -and $names -match 'meta-data') {
    Write-Ok '盘内文件名核对通过'
} else {
    Write-Err "盘内文件名不正确：$names"; exit 1
}

# ================================================================ 删除旧机
if (-not $KeepOldVM) {
    Write-Step '删除旧的虚拟机'
    $old = Get-VM -Name $VMName -ErrorAction SilentlyContinue
    if ($old) {
        if ($old.State -ne 'Off') {
            Write-Warn "正在关闭（当前状态 $($old.State)）"
            Stop-VM -Name $VMName -TurnOff -Force
            Start-Sleep -Seconds 3
        }
        $oldDisks = @(Get-VMHardDiskDrive -VMName $VMName -ErrorAction SilentlyContinue |
                      ForEach-Object { $_.Path })
        Remove-VM -Name $VMName -Force
        Write-Ok '旧虚拟机已删除'

        foreach ($d in $oldDisks) {
            if ($d -and (Test-Path -LiteralPath $d) -and $d -ne $DiskImage) {
                Remove-Item -LiteralPath $d -Force -ErrorAction SilentlyContinue
                Write-Ok ("已删除旧磁盘：{0}" -f (Split-Path $d -Leaf))
            }
        }
    } else {
        Write-Ok '没有同名虚拟机'
    }
}

# ================================================================ 部署磁盘
Write-Step '部署虚拟磁盘'
$vhdDir = Join-Path (Get-VMHost).VirtualHardDiskPath $VMName
New-Item -ItemType Directory -Force -Path $vhdDir | Out-Null
$targetVhd = Join-Path $vhdDir "$VMName.vhdx"

if ((Resolve-Path -LiteralPath $DiskImage).Path -ne $targetVhd) {
    if (Test-Path $targetVhd) { Remove-Item $targetVhd -Force }
    Write-Host '  正在复制磁盘映像（约 2 GB，需要一点时间）'
    Copy-Item -LiteralPath $DiskImage -Destination $targetVhd -Force
    Write-Ok ("已复制到 {0}" -f $targetVhd)
} else {
    Write-Ok '映像已在目标位置'
}

# 扩容：qemu-img 不支持扩容 vhdx，所以在这一步做
$cur = (Get-VHD -Path $targetVhd).Size
$want = $DiskGB * 1GB
if ($cur -lt $want) {
    Resize-VHD -Path $targetVhd -SizeBytes $want
    Write-Ok ("已扩容：{0:N1} GB -> {1} GB" -f ($cur/1GB), $DiskGB)
} else {
    Write-Ok ("磁盘已是 {0:N1} GB" -f ($cur/1GB))
}

# ================================================================ 建机
Write-Step "创建虚拟机 $VMName"
$vmRoot = Join-Path (Get-VMHost).VirtualMachinePath $VMName
New-Item -ItemType Directory -Force -Path $vmRoot | Out-Null

$switch = Get-VMSwitch | Where-Object { $_.Name -eq 'Default Switch' } | Select-Object -First 1
if (-not $switch) { $switch = Get-VMSwitch | Select-Object -First 1 }
if (-not $switch) { Write-Err '没有可用的虚拟交换机'; exit 1 }

New-VM -Name $VMName -Generation 2 `
       -MemoryStartupBytes ($MemoryGB * 1GB) `
       -VHDPath $targetVhd `
       -SwitchName $switch.Name `
       -Path $vmRoot | Out-Null
Write-Ok '虚拟机已创建（第 2 代 / UEFI）'

# 固定内存：动态内存曾在安装时把内存压到 2.1 GB 导致崩溃
Set-VM -Name $VMName -ProcessorCount $CPUCount -StaticMemory
Set-VM -Name $VMName -AutomaticCheckpointsEnabled $false
Write-Ok ("内存 {0} GB（固定，不用动态内存），处理器 {1} 个" -f $MemoryGB, $CPUCount)

Set-VMFirmware -VMName $VMName -EnableSecureBoot Off
Write-Ok 'Secure Boot 已关闭'

Add-VMDvdDrive -VMName $VMName -Path $seedIso | Out-Null
Write-Ok '种子盘已挂载'

Set-VMFirmware -VMName $VMName -FirstBootDevice (Get-VMHardDiskDrive -VMName $VMName)
Write-Ok '启动顺序：硬盘优先'

# ================================================================ 启动
Write-Step '启动虚拟机'
Start-VM -Name $VMName
Write-Ok '已启动'

Write-Host ''
Write-Host ('─' * 70) -ForegroundColor DarkGray
Write-Host ' 后续' -ForegroundColor Cyan
Write-Host ('─' * 70) -ForegroundColor DarkGray
Write-Host ''
Write-Host ' 首次启动需要 1 至 3 分钟完成 cloud-init 初始化。'
Write-Host ''
Write-Host ' 查看分配到的地址：' -ForegroundColor Cyan
Write-Host "   Get-VMNetworkAdapter -VMName $VMName | Select-Object -ExpandProperty IPAddresses" -ForegroundColor White
Write-Host ''
Write-Host ' 用 SSH 登录（用户名与密码均为 ubuntu-debug）：' -ForegroundColor Cyan
Write-Host "   ssh -i `"`$env:USERPROFILE\.ssh\ubuntu_vm`" ubuntu-debug@<地址>" -ForegroundColor White
Write-Host ''
Write-Host ('─' * 70) -ForegroundColor DarkGray
Write-Host ''
