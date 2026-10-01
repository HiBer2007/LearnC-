#Requires -Version 7.0
<#
.SYNOPSIS
    检查一套 Qt 的版本、套件、是动态还是静态、运行期依赖是否齐全，
    并给出「用它构建本仓库范例」的现成命令。

.DESCRIPTION
    本仓库不安装 Qt，也不随附 Qt。范例要用 Qt 界面时，
    读者需要自己装一套 Qt，然后把它的**套件目录**接进范例工程。

    本脚本不下载、不安装、不修改任何东西，只读几个文件后给出判断。
    它回答五个问题：

      1. 这是哪个版本的 Qt
      2. 是哪个套件（mingw_64 / msvc2022_64 / llvm-mingw_64 …）
      3. 动态版还是静态版
      4. 运行期要用的 DLL 齐不齐，以及怎么让程序找到它们
      5. 该用哪条命令构建本仓库的范例

    路径从 -QtRoot 或环境变量 QT_ROOT 读入。
    **仓库里不写死任何本机路径**，因为别人把 Qt 装在哪里与本机无关。

.PARAMETER QtRoot
    Qt 的套件目录，形如 <Qt>/6.11.1/mingw_64。
    留空则读环境变量 QT_ROOT。

.PARAMETER ProjectDir
    一并检查某个范例工程能不能接上这套 Qt。留空则跳过。

.EXAMPLE
    pwsh -File .\检查Qt.ps1 -QtRoot '<Qt>/6.11.1/mingw_64'

.EXAMPLE
    $env:QT_ROOT = '<Qt>/6.11.1/mingw_64'
    pwsh -File .\检查Qt.ps1 -ProjectDir ..\..\B-examples\10-qt-gui
#>
[CmdletBinding()]
param(
    [string] $QtRoot = '',
    [string] $ProjectDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Item { param([string]$Label, [string]$Value, [string]$Mark = '  ')
    # 不用 -f 的定宽对齐：中文按字符数算宽度，排出来仍然歪。
    Write-Host "$Mark$Label：$Value"
}

# ---------------------------------------------------------------- 取路径
if ([string]::IsNullOrWhiteSpace($QtRoot)) { $QtRoot = $env:QT_ROOT }

if ([string]::IsNullOrWhiteSpace($QtRoot)) {
    Write-Host '用法：pwsh -File .\检查Qt.ps1 -QtRoot <Qt 的套件目录>'
    Write-Host '或者先设置环境变量：'
    Write-Host "  `$env:QT_ROOT = '<Qt 的套件目录>'"
    Write-Host ''
    Write-Host '目录应当形如 <Qt>/6.11.1/mingw_64，指到编译器那一层，'
    Write-Host '其中含 bin、lib、include、mkspecs、plugins。'
    exit 2
}

$QtRoot = [IO.Path]::GetFullPath($QtRoot)
Write-Host "检查目录：$QtRoot"
Write-Host ''

if (-not (Test-Path -LiteralPath $QtRoot)) {
    Write-Host '结论：目录不存在，检查中止。' -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- 确认是套件目录
$configFile = Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'
if (-not (Test-Path -LiteralPath $configFile)) {
    Write-Host '这个目录下没有 lib\cmake\Qt6\Qt6Config.cmake，不是一套 Qt 套件目录。'
    Write-Host ''
    Write-Host '常见情况是指到了版本号那一层。下面这些子目录才是套件，请用其中一个重试：'
    $cands = Get-ChildItem -LiteralPath $QtRoot -Directory -ErrorAction SilentlyContinue |
             Where-Object { Test-Path (Join-Path $_.FullName 'lib\cmake\Qt6\Qt6Config.cmake') }
    if ($cands) {
        $cands | ForEach-Object { Write-Host "  $($_.FullName)" }
    }
    else {
        Write-Host '  （这个目录下也没有找到任何套件）'
    }
    Write-Host ''
    Write-Host '结论：路径给错了。' -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- 读 qconfig.pri
$qconfigPath = Join-Path $QtRoot 'mkspecs\qconfig.pri'
$qconfig = ''
if (Test-Path -LiteralPath $qconfigPath) { $qconfig = [IO.File]::ReadAllText($qconfigPath) }

function Get-QconfigValue { param([string]$Name)
    # qconfig.pri 里既有 'QT_VERSION = 6.11.1' 也有 'QT_CONFIG += shared ...'，
    # 因此运算符要同时容忍 = 与 +=。
    if ($qconfig -match "(?m)^$([regex]::Escape($Name))\s*\+?=\s*(.+?)\s*$") { return $Matches[1] }
    return $null
}

$version  = Get-QconfigValue 'QT_VERSION'
$arch     = Get-QconfigValue 'QT_ARCH'
$configLn = Get-QconfigValue 'QT_CONFIG'
$msvcMaj  = Get-QconfigValue 'QT_MSVC_MAJOR_VERSION'
$msvcMin  = Get-QconfigValue 'QT_MSVC_MINOR_VERSION'
$msvcPat  = Get-QconfigValue 'QT_MSVC_PATCH_VERSION'

$isShared = $configLn -match '\bshared\b'
$isStatic = $configLn -match '\bstatic\b'
$hasDebugAndRelease = $qconfig -match '\bdebug_and_release\b'

# ---------------------------------------------------------------- 套件与工具链
$kit = Split-Path $QtRoot -Leaf
switch -Regex ($kit) {
    '^msvc'          { $toolchain = 'MSVC' }
    '^llvm-mingw'    { $toolchain = 'LLVM-MinGW（clang）' }
    '^mingw'         { $toolchain = 'MinGW-w64（gcc）' }
    default {
        $toolchain = if ($msvcMaj) { 'MSVC' } else { 'MinGW-w64（gcc）' }
    }
}

# ---------------------------------------------------------------- 运行期文件
$binDir      = Join-Path $QtRoot 'bin'
$dllCount    = (Get-ChildItem -LiteralPath $binDir -Filter 'Qt6*.dll' -ErrorAction SilentlyContinue | Measure-Object).Count
$hasCoreDll  = Test-Path -LiteralPath (Join-Path $binDir 'Qt6Core.dll')
$hasWidgetsDll = Test-Path -LiteralPath (Join-Path $binDir 'Qt6Widgets.dll')
$hasDeploy   = Test-Path -LiteralPath (Join-Path $binDir 'windeployqt.exe')
$hasQmake    = Test-Path -LiteralPath (Join-Path $binDir 'qmake.exe')
$pluginDir   = Join-Path $QtRoot 'plugins\platforms'
$platformDll = (Test-Path -LiteralPath (Join-Path $pluginDir 'qwindows.dll')) -or
               (Test-Path -LiteralPath (Join-Path $pluginDir 'libqwindows.a'))
$hasLibWidgets = (Test-Path -LiteralPath (Join-Path $QtRoot 'lib\Qt6Widgets.lib')) -or
                 (Test-Path -LiteralPath (Join-Path $QtRoot 'lib\libQt6Widgets.a'))

# MSVC 套件常见形态是 lib 目录下带 d 后缀的调试版一起放。
$hasDebugLib = (Get-ChildItem -LiteralPath (Join-Path $QtRoot 'lib') -Filter 'Qt6Widgetsd.lib' -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0

# ---------------------------------------------------------------- 报告
Write-Host '---- 这套 Qt 的基本情况 ----'
Write-Item '版本'   ($(if ($version) { $version } else { '未读到' }))
Write-Item '架构'   ($(if ($arch) { $arch } else { '未读到' }))
Write-Item '套件'   "$kit（$toolchain）"
Write-Item 'QT_CONFIG' ($(if ($configLn) { $configLn } else { '未读到' }))
Write-Item '链接方式' ($(if ($isShared) { '动态版（shared）' } elseif ($isStatic) { '静态版（static）' } else { '未读到' }))
Write-Item '构建类型' ($(if ($hasDebugAndRelease) { 'Debug 与 Release 都有' } else { '仅 Release' }))

if ($msvcMaj) {
    $v = "$msvcMaj.$msvcMin"
    if ($msvcPat) { $v += ".$msvcPat" }
    Write-Item '构建它的编译器' "MSVC $v"
}

Write-Host ''
Write-Host '---- 文件齐不齐 ----'
Write-Item 'cmake/Qt6'          '在'
Write-Item 'Qt6Widgets.lib/.a'  ($(if ($hasLibWidgets) { '在' } else { '缺' }))
Write-Item 'Qt6Core.dll'        ($(if ($hasCoreDll) { '在' } else { '无（静态版属正常）' }))
Write-Item 'Qt6Widgets.dll'     ($(if ($hasWidgetsDll) { '在' } else { '无（静态版属正常）' }))
Write-Item 'bin 下 Qt6*.dll 数量' $dllCount
Write-Item 'plugins/platforms/qwindows' ($(if ($platformDll) { '在' } else { '缺' }))
Write-Item 'windeployqt.exe'    ($(if ($hasDeploy) { '在' } else { '缺' }))
Write-Item 'qmake.exe'          ($(if ($hasQmake) { '在' } else { '缺' }))

Write-Host ''

# ---------------------------------------------------------------- 结论
$ok = $true
if (-not $hasLibWidgets) {
    Write-Host '结论：lib 目录里找不到 Qt6Widgets，这套 Qt 缺 GUI 模块。' -ForegroundColor Red
    $ok = $false
}
if (-not $platformDll) {
    Write-Host '结论：缺 platforms/qwindows，界面程序起不来。' -ForegroundColor Red
    $ok = $false
}
if ($isShared -and -not $hasCoreDll) {
    Write-Host '结论：动态版却找不到 Qt6Core.dll，安装不完整。' -ForegroundColor Red
    $ok = $false
}
if (-not $isShared -and -not $isStatic) {
    Write-Host '结论：qconfig.pri 里既没有 shared 也没有 static，无法判断链接方式。' -ForegroundColor Yellow
}

if ($ok) {
    Write-Host '结论：可以用来构建本仓库的 Qt 版范例。' -ForegroundColor Green
    Write-Host ''
    Write-Host '---- 构建命令 ----'

    $genLine = ''
    if ($toolchain -like 'MSVC*') {
        $genLine = 'cmake -S <工程目录> -B build-qt -G "Visual Studio 17 2022" -A x64 -DWITH_QT=ON'
    }
    else {
        $genLine = 'cmake -S <工程目录> -B build-qt -G Ninja -DWITH_QT=ON'
    }
    Write-Host "  `$env:QT_ROOT = '$QtRoot'"
    Write-Host "  $genLine"
    if ($toolchain -like 'MSVC*') {
        Write-Host '  cmake --build build-qt --config Release'
    }
    else {
        Write-Host '  cmake --build build-qt'
    }

    Write-Host ''
    Write-Host '---- 编译器匹配 ----'
    Write-Host "  这套 Qt 是 $toolchain 编的，因此范例也必须用 $toolchain 构建。"
    Write-Host '  C++ 没有跨编译器的稳定 ABI，混用会在链接期报出一大片未解析符号。'

    if ($isShared) {
        Write-Host ''
        Write-Host '---- 运行期要能找到 DLL ----'
        Write-Host '  动态版的程序启动时要加载 Qt6Core.dll 等文件。两种做法：'
        Write-Host ''
        Write-Host '  其一，把 exe 与它需要的 DLL 放到一起（发布用）：'
        Write-Host "    & '$binDir\windeployqt.exe' --release <exe 的完整路径>"
        Write-Host '    它会把 Qt 的 DLL、平台插件与所需运行库拷到 exe 旁边。'
        Write-Host ''
        Write-Host '  其二，把 Qt 的 bin 目录加进 PATH（开发时方便）：'
        Write-Host "    `$env:Path = '$binDir;' + `$env:Path"
        Write-Host ''
        Write-Host '  只做第二种时，程序在别的机器上会因为找不到 DLL 而起不来。'
    }
    else {
        Write-Host ''
        Write-Host '---- 静态版注意 ----'
        Write-Host '  静态版编出来的程序是单文件，不需要 windeployqt；'
        Write-Host '  但它受 LGPLv3 的重链接义务约束，且 Debug/Release 必须与 Qt 一致。'
        Write-Host '  见 README 第六节。'
    }
}

# ---------------------------------------------------------------- 一并检查工程
if (-not [string]::IsNullOrWhiteSpace($ProjectDir)) {
    Write-Host ''
    Write-Host '---- 工程检查 ----'
    $proj = [IO.Path]::GetFullPath($ProjectDir)
    $cl = Join-Path $proj 'CMakeLists.txt'
    if (-not (Test-Path -LiteralPath $cl)) {
        Write-Host "  $proj 下没有 CMakeLists.txt，跳过。"
    }
    else {
        $cm = [IO.File]::ReadAllText($cl)
        Write-Item 'WITH_QT 开关'         ($(if ($cm -match 'option\(\s*WITH_QT') { '有' } else { '缺' }))
        Write-Item 'find_package(Qt6)'    ($(if ($cm -match 'find_package\(\s*Qt6') { '有' } else { '缺' }))
        Write-Item '包含 qt-dynamic.cmake' ($(if ($cm -match 'qt-dynamic\.cmake') { '有' } else { '无' }))
        Write-Item '包含 qt-static.cmake（旧名）' ($(if ($cm -match 'qt-static\.cmake') { '有（建议改成 qt-dynamic.cmake）' } else { '无' }))
        if (-not ($cm -match 'option\(\s*WITH_QT' -and $cm -match 'find_package\(\s*Qt6')) {
            Write-Host '  Qt 接入不完整：需要 option(WITH_QT ...) 与 find_package(Qt6 ...)，' -ForegroundColor Yellow
            Write-Host '  并把它包在 if(WITH_QT) 里，默认关闭。' -ForegroundColor Yellow
        }
    }
}

Write-Host ''
if (-not $ok) { exit 1 }
exit 0
