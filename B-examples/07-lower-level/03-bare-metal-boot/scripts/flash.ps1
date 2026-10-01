# flash.ps1 —— 把 03-bare-metal-boot 烧进真板并读回结论
#
# 用法（在示例目录下，板子与 CMSIS-DAP 已插好）：
#   pwsh -File scripts\flash.ps1
#
# 三条纪律：
#   1. 调试器同一时刻只能被一个进程占用，因此整个脚本只启动一次 openocd；
#   2. 只写 flash 主区，不碰 option bytes、不碰读保护；
#   3. OpenOCD 的 -c 按 Tcl 规则解析，路径一律用正斜杠，否则 \U \b 会被当转义吃掉。
#
# 跑完停在 halt 再 shutdown，不让目标跑飞。

$ErrorActionPreference = "Stop"

$openocd = "H:\OpenOCD\bin\openocd.exe"
$scripts = "H:\OpenOCD\share\openocd\scripts"

$root = Split-Path -Parent $PSScriptRoot
$elf = ($root -replace '\\', '/') + "/build/boot_demo.elf"

if (-not (Test-Path ($elf -replace '/', '\'))) {
    throw "找不到 $elf，先在 WSL 里跑 scripts/build.sh"
}

# 程序把结论写在固定的 0x20004000：magic / pass / total / _estack / _sdata / _sidata / _sbss / _ebss
$cmd = @(
    "init",
    "reset halt",
    "arm semihosting enable",
    "flash write_image erase $elf",
    "reset run",
    # semihosting 的每一次 bkpt 都要跟调试器来回一趟，输出比 QEMU 慢得多，
    # 这里给足时间；不够就会停在打印中途，结果区还没写。
    "sleep 12000",
    "halt",
    "echo {== result block in RAM (0x20004000) ==}",
    "mdw 0x20004000 8",
    "echo {== reset vector (first two words of flash) ==}",
    "mdw 0x08000000 2",
    "echo {== .data initial value still in flash at its LMA ==}",
    "mdw 0x08000ca0 2",
    "reg pc",
    "reg msp",
    # 收尾：停在 halt
    "halt",
    "shutdown"
)

$args = @("-s", $scripts, "-f", "interface/cmsis-dap.cfg", "-f", "target/stm32f1x.cfg")
foreach ($c in $cmd) { $args += @("-c", $c) }

& $openocd @args 2>&1
exit $LASTEXITCODE
