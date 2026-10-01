# asm-compare.ps1 —— 02-volatile-and-registers 的汇编对照脚本
#
# 做两件事：
#   1. 把 src/volatile_regs.c 编成 -O0 与 -O2 两个目标文件
#   2. 用 objdump 把四个函数各反汇编一次，摆在一起
#
# 用法（在示例目录下）：
#   pwsh -File tools\asm-compare.ps1
#
# 依赖 MinGW 的 gcc 与 objdump 在 PATH 上。

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "build\asm"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$src = Join-Path $root "src\volatile_regs.c"
$inc = Join-Path $root "include"

foreach ($opt in @("-O0", "-O2")) {
    $obj = Join-Path $outDir ("vr" + $opt + ".o")
    Write-Output "=== 编译 $opt -> $obj"
    & gcc -std=c17 $opt -Wall -Wextra -I $inc -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "编译失败：$opt" }
}

foreach ($opt in @("-O0", "-O2")) {
    $obj = Join-Path $outDir ("vr" + $opt + ".o")
    foreach ($fn in @("vr_wait_volatile", "vr_wait_plain", "vr_sum_reads")) {
        Write-Output ""
        Write-Output "=== $opt  $fn"
        $lines = & objdump -d "--disassemble=$fn" $obj
        $started = $false
        foreach ($line in $lines) {
            if (-not $started) {
                if ($line -match "^[0-9a-f]+ <$fn>:") { $started = $true }
                continue
            }
            if ($line -notmatch "\S") { break }
            Write-Output ("    " + $line.TrimEnd())
        }
    }
}
