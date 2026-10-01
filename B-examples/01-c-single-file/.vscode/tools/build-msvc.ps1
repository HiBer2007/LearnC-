<#
.SYNOPSIS
    用 MSVC (cl.exe) 编译单个 .c 文件，产物可直接用 VS2022 调试器调试。

.DESCRIPTION
    由 .vscode/tasks.json 自动调用，一般不需要手动运行。

    做三件事：
      1. 用 vswhere 定位 Visual Studio（优先 VS2022）
      2. 执行 vcvars64.bat，把 INCLUDE / LIB / PATH 导入当前进程
      3. 调用 cl.exe 编译

    为什么用 PowerShell 而不是 .cmd：
      工程所在路径含中文时（本工作区即是如此）。cmd.exe 按“当前代码页”逐字节
      解析批处理文件，中文注释会被误读成控制字符；而且带尾反斜杠的参数
      交给 CRT 解析时，/Fo:"...\" 结尾的 \" 会被当成转义引号，把后面的
      源文件名一起吞掉（cl 报 D8003 缺少源文件名）。
      PowerShell 全程 UTF-16，没有这两类问题。

.PARAMETER Source
    要编译的 .c 源文件路径。

.PARAMETER Output
    输出的 .exe 路径。

.EXAMPLE
    pwsh -File .vscode\tools\build-msvc.ps1 -Source main.c -Output build\single-file\main.exe
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------
#  1. 定位 Visual Studio
# ---------------------------------------------------------------
function Get-VsInstallPath {
    $pf86 = ${env:ProgramFiles(x86)}
    if (-not $pf86) { $pf86 = 'C:\Program Files (x86)' }

    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "找不到 vswhere.exe，无法定位 Visual Studio：$vswhere"
    }

    $installs = @(& $vswhere -products '*' -property installationPath 2>$null)
    $usable = @($installs | Where-Object {
        $_ -and (Test-Path -LiteralPath (Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat'))
    })
    if ($usable.Count -eq 0) {
        throw '没有找到安装了 C++ 生成工具的 Visual Studio。'
    }

    # 优先级：VS2022 Community > 其它 VS2022 > 任意版本
    # 这样和你平时在 VS2022 里的使用习惯保持一致
    $preferred = @($usable | Where-Object { $_ -match '\\2022\\Community$' })
    if ($preferred.Count -eq 0) {
        $preferred = @($usable | Where-Object { $_ -match '\\2022\\' })
    }
    if ($preferred.Count -gt 0) { return $preferred[0] }
    return $usable[0]
}

# ---------------------------------------------------------------
#  2. 加载 MSVC 环境变量
# ---------------------------------------------------------------
function Import-MsvcEnvironment {
    param([Parameter(Mandatory = $true)][string]$VsPath)

    $vcvars = Join-Path $VsPath 'VC\Auxiliary\Build\vcvars64.bat'
    Write-Host "[INFO] Visual Studio: $VsPath"

    # 在 cmd 里执行一次 vcvars64.bat，再用 set 把环境导出并回填到本进程。
    #
    # 这是本脚本中唯一使用 cmd.exe 的位置，无法避免：
    # vcvars64.bat 是批处理脚本，只能由 cmd 解释执行。
    # 调用被限制在此处，传入的是单一字符串参数，不涉及 VS Code 任务的传参转义问题。
    $lines = & cmd.exe /d /c "call `"$vcvars`" >nul 2>&1 && set"
    foreach ($line in $lines) {
        $i = $line.IndexOf('=')
        if ($i -gt 0) {
            $name = $line.Substring(0, $i)
            $value = $line.Substring($i + 1)
            Set-Item -Path "Env:$name" -Value $value -ErrorAction SilentlyContinue
        }
    }
}

# ---------------------------------------------------------------
#  3. 编译
# ---------------------------------------------------------------
$srcPath = (Resolve-Path -LiteralPath $Source).Path
$outFull = [System.IO.Path]::GetFullPath($Output)
$outDir = Split-Path -Parent $outFull

if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

Import-MsvcEnvironment -VsPath (Get-VsInstallPath)

# 统一换成正斜杠：Windows 文件 API 都认，
# 而且不会触发“结尾反斜杠 + 引号”被 CRT 当成转义引号的问题。
$srcArg = $srcPath -replace '\\', '/'
$feArg = $outFull -replace '\\', '/'
$foArg = (($outDir -replace '\\', '/').TrimEnd('/')) + '/'

Write-Host "[BUILD] source: $srcPath"
Write-Host "[BUILD] output: $outFull"

Push-Location -LiteralPath $outDir
# /nologo                    不打印版权头
# /std:c17                   使用 C17 标准（对 .cpp 会自动忽略）
# /EHsc                      标准 C++ 异常模型。编 C++ 时应当加上：
#                            缺了它，标准库内部用到异常的地方会报 C4530
# /source-charset:utf-8      源码按 UTF-8 读取
# /execution-charset:gbk     字符串常量按 GBK 写入 exe
#
# 关于字符集：这里不能用 /utf-8。
#   /utf-8 等价于 /source-charset:utf-8 与 /execution-charset:utf-8 的合并写法，
#   会把执行字符集也设为 UTF-8。中文 Windows 的控制台按 CP936(GBK) 解释字节，
#   UTF-8 的中文会显示成乱码（实测）。
#   拆开书写即可：源码用 UTF-8，exe 内的字符串用 GBK。
#
# /Zi 生成 PDB 调试信息 | /Od 关闭优化便于单步 | /W3 警告等级
& cl.exe /nologo /std:c17 /EHsc /source-charset:utf-8 /execution-charset:gbk `
         /Zi /Od /W3 "/Fe:$feArg" "/Fo:$foArg" $srcArg
$rc = $LASTEXITCODE
Pop-Location

if ($rc -ne 0) {
    Write-Host "[ERROR] 编译失败，cl.exe 退出码 $rc"
    exit $rc
}

Write-Host "[OK] 已生成 $outFull"
exit 0
