/* stm32f103c8_min.ld —— 02 示例自带的精简链接脚本
 *
 * 完整逐段讲解见《07-更底层/07-链接脚本与启动代码.md》第 2 节；
 * 这里只留跑起来必需的那几段：向量表放最前、.data 两套地址、.bss 不占文件。
 *
 * 内存大小按这块真板写：STM32F103C8 是 64 KiB flash / 20 KiB RAM。
 * 照抄大容量型号的脚本（48K RAM / 256K flash）会让栈顶落到 RAM 之外，
 * 复位后第一次压栈就越界。
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶 = RAM 的最高地址。硬件复位时从向量表第 0 个字取它装进 SP。 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* 向量表必须钉在 flash 的第一个字节：硬件复位后固定从这里取前两个字。 */
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } >FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } >FLASH

  /* .data 的两套地址：VMA 在 RAM（程序运行时用它），
   * LMA 在 flash（上电时初值还在 flash 里，由 startup 搬过去）。 */
  _sidata = LOADADDR(.data);

  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } >RAM AT> FLASH

  /* .bss 不占 flash：它只有地址，没有初值，启动代码把它清零。 */
  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } >RAM
}
