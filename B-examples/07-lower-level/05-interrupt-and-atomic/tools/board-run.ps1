# board-run.ps1 —— 把 05 示例烧进真板并读回结果
#
# 用法（在示例目录下，板子与 CMSIS-DAP 已插好）：
#   pwsh -File tools\board-run.ps1
#
# 三条纪律：
#   1. 调试器同一时刻只能被一个进程占用，因此整个脚本只启动一次 openocd；
#   2. 只写 flash 主区，不碰 option bytes、不碰读保护；
#   3. OpenOCD 的 -c 按 Tcl 规则解析，路径一律用正斜杠。
#
# 跑完停在 halt 再 shutdown，不让目标跑飞。

$ErrorActionPreference = "Stop"

$openocd = "H:\OpenOCD\bin\openocd.exe"
$scripts = "H:\OpenOCD\share\openocd\scripts"

$root = Split-Path -Parent $PSScriptRoot
$elf = ($root -replace '\\', '/') + "/board/build/race_board.elf"

if (-not (Test-Path ($elf -replace '/', '/'))) {
    throw "找不到 $elf，先在 WSL 里跑 board/build.sh"
}

# 结果区固定在 0x20004000：
#   magic / 版本 / 期望 / 实测 / 丢失 / 中断次数 / 延迟最小 / 延迟最大
$cmd = @(
    "init",
    "reset halt",
    "arm semihosting enable",
    "flash write_image erase $elf",
    "reset run",
    # semihosting 每次 bkpt 都要跟调试器来回一趟，输出慢，给足时间
    "sleep 15000",
    "halt",
    "echo {== results at 0x20004000 ==}",
    "mdw 0x20004000 8",
    "echo {== interrupt latency in DWT cycles ==}",
    "reg pc",
    "halt",
    "shutdown"
)

$args = @("-s", $scripts, "-f", "interface/cmsis-dap.cfg", "-f", "target/stm32f1x.cfg")
foreach ($c in $cmd) { $args += @("-c", $c) }

& $openocd @args 2>&1
exit $LASTEXITCODE
