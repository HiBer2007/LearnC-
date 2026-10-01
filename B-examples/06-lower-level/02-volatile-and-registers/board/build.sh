#!/usr/bin/env bash
# 02-volatile-and-registers 的板级构建脚本（在 WSL 的 Ubuntu 里跑）。
#
#   wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/06-lower-level/02-volatile-and-registers/board/build.sh"
#
# 编出四份镜像：
#   flag_systick_vol.elf   SysTick 触发 + 带 volatile
#   flag_systick_nv.elf    SysTick 触发 + 去掉 volatile
#   flag_mww_vol.elf       调试器 mww 触发 + 带 volatile
#   flag_mww_nv.elf        调试器 mww 触发 + 去掉 volatile
# 后两份是给真板用的，脚本只负责编，烧写见 README。
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/build"
CC=arm-none-eabi-gcc
CFLAGS="-mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra -nostartfiles --specs=nosys.specs"
SRCS="startup_min.s volatile_flag.c"

mkdir -p "$OUT"
cd "$HERE"

build() {
  local name="$1"; shift
  echo "=== 编译 $name.elf  额外宏：${*:-（无）}"
  # shellcheck disable=SC2086
  $CC $CFLAGS "$@" -T stm32f103c8_min.ld $SRCS -o "$OUT/$name.elf" || exit 1
}

build flag_systick_vol -DTRIGGER_SYSTICK
build flag_systick_nv  -DTRIGGER_SYSTICK -DNO_VOLATILE
build flag_mww_vol
build flag_mww_nv      -DNO_VOLATILE

echo
echo "=== 段布局（.isr_vector 在 flash 起始，.data 两套地址）"
arm-none-eabi-objdump -h "$OUT/flag_systick_vol.elf" | sed -n '4,12p'

echo
echo "=== 等待循环的反汇编对照（-O2）"
echo "    带 volatile 的那一份，循环里每一次都有一条 ldr 去读 g_flag；"
echo "    去掉 volatile 的那一份，读只有一条，循环退化成 b.n 跳到自身。"
for v in vol nv; do
  echo "--- flag_systick_$v.elf 的 main："
  arm-none-eabi-objdump -d --disassemble=main "$OUT/flag_systick_$v.elf" \
    | sed -n '/<main>:/,/^$/p' | grep -E "ldr|str|cmp|cbnz|cbz|beq|bne|b\.n" \
    | sed 's/^/    /'
done

echo
echo "=== g_flag 的地址（真板上调试器 mww 要用）"
for f in flag_mww_vol flag_mww_nv; do
  printf '  %-16s ' "$f"
  arm-none-eabi-nm "$OUT/$f.elf" | grep -E ' g_flag$' || echo "（找不到）"
done

echo
echo "=== 在 QEMU 里跑 SysTick 那一对（-M netduinoplus2，semihosting 输出）"
for v in vol nv; do
  echo "--- flag_systick_$v.elf"
  timeout 5 qemu-system-arm -M netduinoplus2 -kernel "$OUT/flag_systick_$v.elf" \
    -nographic -semihosting-config enable=on,target=native 2>&1 | head -6 | sed 's/^/    /'
  echo "    （命令超时结束说明程序停在死循环里；带 volatile 的那一份应当先打印 escaped）"
done

echo
echo "四份镜像都在 $OUT"
