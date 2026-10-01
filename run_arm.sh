#!/usr/bin/env bash
# ARM 侧：观察 nosys 与 rdimon 两套桩，QEMU 先跑一遍
set -u
cd /tmp/b11s
LD=/tmp/b11s/STM32F103C8_FLASH.ld
SU=/tmp/b11s/startup_stm32f103xe.s
CF="-mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles"

echo "########## 0. specs 文件内容"
for s in nosys rdimon; do
  f=$(arm-none-eabi-gcc -print-file-name=$s.specs)
  echo "---- $f"
  cat "$f"
done

echo "########## 1. libnosys.a 里定义了哪些桩"
S=$(arm-none-eabi-gcc -print-file-name=libnosys.a)
arm-none-eabi-nm --defined-only "$S" | sort -k3

echo "########## 2. libnosys.a 的 _write / _sbrk / _exit 反汇编"
arm-none-eabi-objdump -d "$S" | awk '/^[0-9a-f]+ <_(write|sbrk|exit|close|fstat|isatty|read|open|kill|getpid)>:/{p=1} p{print} /^$/{p=0}' | head -80

echo "########## 3. 不带 --specs 直接链接一个用 printf 的程序"
arm-none-eabi-gcc $CF -T $LD $SU hello.c -o hello_nospec.elf 2>&1 | head -20

echo "########## 4. 带 --specs=nosys.specs 链接"
arm-none-eabi-gcc $CF --specs=nosys.specs -T $LD $SU hello.c -o hello_nosys.elf && echo "链接成功"
arm-none-eabi-size hello_nosys.elf

echo "########## 5. 带 --specs=rdimon.specs 链接"
arm-none-eabi-gcc $CF --specs=rdimon.specs -T $LD $SU hello.c -o hello_rdimon.elf 2>&1 | head -20
arm-none-eabi-size hello_rdimon.elf 2>/dev/null

echo "########## 6. 两个镜像里 _write/_sbrk/printf/malloc 分别来自哪里"
for f in hello_nosys.elf hello_rdimon.elf; do
  echo "---- $f"
  arm-none-eabi-nm "$f" | grep -iE " (_write|_sbrk|_exit|printf|malloc|initialise_monitor_handles)$" | sort -k3
  arm-none-eabi-nm "$f" | grep -i "initialise_monitor" | head -3
done

echo "########## 7. heap_probe 两个版本"
arm-none-eabi-gcc $CF --specs=nosys.specs -T $LD $SU heap_probe.c -o heap_ok.elf && echo "heap_ok 链接成功"
arm-none-eabi-gcc $CF -DNO_LIMIT_CHECK --specs=nosys.specs -T $LD $SU heap_probe.c -o heap_bad.elf && echo "heap_bad 链接成功"

echo "########## 8. g_result 与 _end/_estack 的地址"
for f in hello_nosys.elf hello_rdimon.elf heap_ok.elf heap_bad.elf; do
  echo "---- $f"
  arm-none-eabi-nm "$f" | grep -E " (g_result|_end|_estack|_Min_Stack_Size|main|_write|_sbrk)$"
done

echo "########## 9. QEMU 里跑 nosys 版（输出应该什么都不出现）"
timeout 20 qemu-system-arm -M netduinoplus2 -cpu cortex-m3 -nographic \
  -semihosting-config enable=on,target=native -kernel hello_nosys.elf 2>&1 | head -20
echo "退出码 $?"

echo "########## 10. QEMU 里跑 rdimon 版"
timeout 20 qemu-system-arm -M netduinoplus2 -cpu cortex-m3 -nographic \
  -semihosting-config enable=on,target=native -kernel hello_rdimon.elf 2>&1 | head -20
echo "退出码 $?"
