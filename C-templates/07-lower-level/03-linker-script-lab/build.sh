#!/usr/bin/env bash
# build.sh —— 一键：编译链接 → 看段表 → 转 .bin 看体积 → 在 QEMU 里跑
#
# 在 WSL 的 Ubuntu 里执行：
#     wsl -d Ubuntu -e bash build.sh
#
# 链接参数照抄 CubeMX 生成的 gcc-arm-none-eabi.cmake：
#     -fdata-sections -ffunction-sections  →  每个函数/数据各自成段
#     -Wl,--gc-sections                    →  链接器回收没人引用的段
#     -Wl,-Map=build/app.map               →  段与符号的完整清单
# 这三个参数与链接脚本里的 KEEP 有直接关系，改脚本之前先看清它们的作用。
set -u

cd "$(dirname "$0")" || exit 1
mkdir -p build

CC=arm-none-eabi-gcc
CFLAGS="-mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs -fdata-sections -ffunction-sections"
LDFLAGS="-Wl,--gc-sections -Wl,-Map=build/app.map"
LD=STM32F103C8_FLASH.ld
STARTUP=startup_stm32f103xe.s
APP=app/main.c
QEMU="qemu-system-arm -M netduinoplus2 -nographic -semihosting-config enable=on,target=native"

echo "=== 1. 编译与链接"
$CC $CFLAGS $LDFLAGS -T $LD $STARTUP $APP -o build/app.elf || {
    echo "编译或链接失败，先看这一条报错，再决定改哪里"
    exit 1
}
echo "链接成功（注意：能链接不等于能跑）"

echo
echo "=== 2. 段表：向量表在不在、.data 的两套地址对不对"
arm-none-eabi-objdump -h build/app.elf | sed -n '1,12p'

echo
echo "=== 3. flash 起始处放的是什么（复位时硬件从这里取栈顶与入口）"
arm-none-eabi-objdump -s -j .isr_vector build/app.elf 2>/dev/null | sed -n '1,5p' \
    || echo "    没有 .isr_vector 这个段"

echo
echo "=== 4. 转成烧写用的 .bin，看体积"
arm-none-eabi-objcopy -O binary build/app.elf build/app.bin
ls -l build/app.bin | awk '{ print "    " $5 " 字节" }'

echo
echo "=== 5. 在 QEMU 里跑（看有没有输出）"
timeout 5 $QEMU -kernel build/app.elf
rc=$?
echo "（退出码 $rc：0 = 程序自己结束；124 = 超时被杀；134 = QEMU 因非法状态自己中止）"
