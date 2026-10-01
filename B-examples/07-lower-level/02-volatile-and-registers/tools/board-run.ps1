# board-run.ps1 —— 把 02 示例的四份镜像烧进真板并读回结果
#
# 用法（在示例目录下，板子与 CMSIS-DAP 已插好）：
#   pwsh -File tools\board-run.ps1
#
# 三条纪律：
#   1. 调试器同一时刻只能被一个进程占用，因此整个脚本只启动一次 openocd；
#   2. 只写 flash 主区（flash write_image erase），不碰 option bytes、不碰读保护；
#   3. OpenOCD 的 -c 按 Tcl 规则解析，路径一律用正斜杠，否则 \U \b 会被当转义吃掉。
#
# 跑完停在 halt 再 shutdown，不让目标跑飞。

$ErrorActionPreference = "Stop"

$openocd = "H:\OpenOCD\bin\openocd.exe"
$scripts = "H:\OpenOCD\share\openocd\scripts"

$root = Split-Path -Parent $PSScriptRoot
# OpenOCD 只认正斜杠
$build = ($root -replace '\\', '/') + "/board/build"

$elf = @{
    mww_vol = "$build/flag_mww_vol.elf"
    mww_nv  = "$build/flag_mww_nv.elf"
    sys_vol = "$build/flag_systick_vol.elf"
    sys_nv  = "$build/flag_systick_nv.elf"
}

foreach ($k in $elf.Keys) {
    if (-not (Test-Path ($elf[$k] -replace '/', '\'))) { throw "找不到 $($elf[$k])，先在 WSL 里跑 board/build.sh" }
}

# 结果区：g_results[0..4] 在 0x20000008，共 5 个字；g_ticks 在 0x20000000
$cmd = @(
    "init", "reset halt", "arm semihosting enable",
    # 第一份：带 volatile，调试器用 mww 触发
    "flash write_image erase $($elf.mww_vol)",
    "reset run", "sleep 300",
    "mww 0x20000004 1",
    "sleep 500", "halt",
    "echo {== A: mww + volatile ==}",
    "mdw 0x20000008 5", "reg pc", "resume",
    # 第二份：去掉 volatile，同样 mww 触发 —— 内存改了程序不动
    "reset halt",
    "flash write_image erase $($elf.mww_nv)",
    "reset run", "sleep 300",
    "mww 0x20000004 1",
    "sleep 500", "halt",
    "echo {== B: mww + NO_VOLATILE ==}",
    "mdw 0x20000008 5", "reg pc", "resume",
    # 第三份：SysTick 触发 + 带 volatile
    "reset halt",
    "flash write_image erase $($elf.sys_vol)",
    "reset run", "sleep 500", "halt",
    "echo {== C: SysTick + volatile ==}",
    "mdw 0x20000000 6", "reg pc", "resume",
    # 第四份：SysTick 触发 + 去掉 volatile
    "reset halt",
    "flash write_image erase $($elf.sys_nv)",
    "reset run", "sleep 1500", "halt",
    "echo {== D: SysTick + NO_VOLATILE ==}",
    "mdw 0x20000000 6", "reg pc",
    # 收尾：停在 halt，不让它跑飞
    "halt", "shutdown"
)

$args = @("-s", $scripts, "-f", "interface/cmsis-dap.cfg", "-f", "target/stm32f1x.cfg")
foreach ($c in $cmd) { $args += @("-c", $c) }

& $openocd @args 2>&1
exit $LASTEXITCODE
