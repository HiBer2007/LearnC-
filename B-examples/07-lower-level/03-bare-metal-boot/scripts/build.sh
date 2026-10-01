#!/usr/bin/env bash
# 03-bare-metal-boot 的构建脚本（在 WSL 的 Ubuntu 里跑）。
#
#   wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/07-lower-level/03-bare-metal-boot/scripts/build.sh"
#
# 做三件事：交叉编译、看段表与 Reset_Handler、在 QEMU 里真跑。
set -u

HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HERE/build"
ELF="$OUT/boot_demo.elf"

mkdir -p "$OUT"
cd "$HERE"

echo "=== 1. 交叉编译（链接脚本与启动文件都在 linker/ 下）"
echo "    arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \\"
echo "      -nostartfiles --specs=nosys.specs \\"
echo "      -T linker/STM32F103C8_FLASH.ld linker/startup_stm32f103xe.s src/boot_demo.c \\"
echo "      -o $ELF"
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
  -nostartfiles --specs=nosys.specs \
  -T linker/STM32F103C8_FLASH.ld linker/startup_stm32f103xe.s src/boot_demo.c \
  -o "$ELF" || exit 1
ls -l "$ELF" | sed 's/^/    /'

echo
echo "=== 2. 段表：.isr_vector 钉在 flash 起始，.data 有两套地址"
arm-none-eabi-objdump -h "$ELF" | sed -n '4,14p' | sed 's/^/    /'

echo
echo "=== 3. 各段地址（size 与 nm）"
arm-none-eabi-size "$ELF" | sed 's/^/    /'
arm-none-eabi-nm "$ELF" | grep -E " (_estack|_sdata|_edata|_sidata|_sbss|_ebss|g_from_data|g_from_bss|g_rodata)$" | sed 's/^/    /'

echo
echo "=== 4. Reset_Handler 逐条（设栈是硬件做的，这里从搬 .data 开始）"
arm-none-eabi-objdump -d --disassemble=Reset_Handler "$ELF" \
  | sed -n '/<Reset_Handler>:/,$p' | grep -E "^ +[0-9a-f]+:" | head -26 | sed 's/^/    /'

echo
echo "=== 5. 在 QEMU 里真跑（semihosting 输出接到 stdout）"
timeout 8 qemu-system-arm -M netduinoplus2 -kernel "$ELF" -nographic \
  -semihosting-config enable=on,target=native 2>&1 | sed 's/^/    /'
echo "    （超时结束说明程序停在最后的死循环里，这是正常的）"

echo
echo "镜像：$ELF"
