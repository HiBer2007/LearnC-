#!/usr/bin/env bash
# run_qemu.sh —— 阶段 3：volatile 有/无两个版本，各编一次、各跑一次
#
# 在 WSL 的 Ubuntu 里执行（工具链与 QEMU 的装法见《01-编译器/03-嵌入式与交叉编译.md》）：
#     wsl -d Ubuntu -e bash run_qemu.sh
#
# 说明：
#   1. 链接脚本是 STM32F103C8 专用的（RAM 20K / flash 64K）；
#   2. 输出走 semihosting（bkpt 0xAB + SYS_WRITE0），所以 QEMU 要带
#      -semihosting-config enable=on,target=native；串口在这块板子上打不出字；
#   3. QEMU 的 netduinoplus2 是 Cortex-M4 近似板，但 flash/RAM 基址与 F103 一致，
#      SysTick 是内核外设，与板级外设无关，因此这个实验的结果是可信的。
set -u

cd "$(dirname "$0")" || exit 1

CC=arm-none-eabi-gcc
CFLAGS="-mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs"
LD=STM32F103C8_FLASH.ld
STARTUP=startup_stm32f103xe.s
QEMU="qemu-system-arm -M netduinoplus2 -nographic -semihosting-config enable=on,target=native"

run_qemu() {
    local elf="$1"
    local note="$2"
    local out
    out=$(mktemp)
    timeout 5 $QEMU -kernel "$elf" >"$out" 2>&1
    local rc=$?
    cat "$out"
    rm -f "$out"
    echo "（退出码 $rc。$note）"
}

echo "=== 1. 编译两个版本（源码只差 -DUSE_VOLATILE）"
mkdir -p build
for v in 1 0; do
    echo "--- -DUSE_VOLATILE=$v"
    $CC $CFLAGS -DUSE_VOLATILE=$v -T $LD $STARTUP volatile_poll.c -o "build/poll_v$v.elf" || exit 1
done
ls -l build/poll_v0.elf build/poll_v1.elf | awk '{ print "    " $5 " 字节  " $9 }'

echo
echo "=== 2. 主循环那一句 while (g_ready == 0) 编译成了什么"
echo "    （看这几行：带 volatile 是「读一次、比较、跳回去再读」；"
echo "      不带 volatile 是「读一次、然后 b.n 跳到自己」）"
for v in 1 0; do
    echo "--- -DUSE_VOLATILE=$v 的 main"
    arm-none-eabi-objdump -d "build/poll_v$v.elf" | sed -n '/<main>:/,/^$/p' | sed -n '1,20p'
done

echo
echo "=== 3. 带 volatile（-DUSE_VOLATILE=1）：QEMU 里的真实输出"
run_qemu build/poll_v1.elf "超时被杀是预期结果：程序打印完三行后停在自己的 for (;;) 里"

echo
echo "=== 4. 去掉 volatile（-DUSE_VOLATILE=0）：QEMU 里的真实输出"
run_qemu build/poll_v0.elf "超时被杀：第一行之后没有任何输出，主循环永远出不来"
