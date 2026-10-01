#!/usr/bin/env bash
# 05-interrupt-and-atomic 的板级构建脚本（在 WSL 的 Ubuntu 里跑）。
#
#   wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/07-lower-level/05-interrupt-and-atomic/board/build.sh"
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/build"
ELF="$OUT/race_board.elf"

mkdir -p "$OUT"
cd "$HERE"

echo "=== 交叉编译"
echo "    arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \\"
echo "      -nostartfiles --specs=nosys.specs \\"
echo "      -T stm32f103c8_min.ld startup_min.s race_board.c -o $ELF"
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
  -nostartfiles --specs=nosys.specs \
  -T stm32f103c8_min.ld startup_min.s race_board.c -o "$ELF" || exit 1
ls -l "$ELF" | sed 's/^/    /'

echo
echo "=== 两种自增的指令序列对照（-O2，Cortex-M3）"
arm-none-eabi-objdump -d "$ELF" | grep -nE "ldrex|strex|dmb|cpsid|cpsie" | head -14 | sed 's/^/    /'

echo
echo "=== 在 QEMU 里真跑（semihosting 输出接到 stdout）"
timeout 20 qemu-system-arm -M netduinoplus2 -kernel "$ELF" -nographic \
  -semihosting-config enable=on,target=native 2>&1 | sed 's/^/    /'
echo "    （超时结束说明程序停在最后的死循环里，这是正常的）"

echo
echo "镜像：$ELF"
