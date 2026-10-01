/* startup_min.s —— 02 示例自带的精简启动文件
 *
 * 完整的向量表与逐条讲解见《06-更底层/07-链接脚本与启动代码.md》第 3 节；
 * 这里只做三件事：摆出向量表、把 .data 从 flash 搬到 RAM、把 .bss 清零。
 *
 * 本文件只需要汇编，不需要预处理：
 *   arm-none-eabi-gcc ... startup_min.s volatile_flag.c -o flag.elf
 */
    .syntax unified
    .cpu cortex-m3
    .thumb

/* ==================== 向量表 ==================== */
    .section .isr_vector,"a",%progbits
    .type g_vectors, %object
    .global g_vectors
g_vectors:
    .word _estack                  /* 0  第 0 个字：栈顶，硬件复位时装进 SP */
    .word Reset_Handler            /* 1  第 1 个字：复位向量，硬件从这里开始执行 */
    .word Default_Handler          /* 2  NMI */
    .word Default_Handler          /* 3  HardFault */
    .word Default_Handler          /* 4  MemManage */
    .word Default_Handler          /* 5  BusFault */
    .word Default_Handler          /* 6  UsageFault */
    .word 0                        /* 7  保留 */
    .word 0                        /* 8  保留 */
    .word 0                        /* 9  保留 */
    .word 0                        /* 10 保留 */
    .word Default_Handler          /* 11 SVCall */
    .word Default_Handler          /* 12 DebugMonitor */
    .word 0                        /* 13 保留 */
    .word Default_Handler          /* 14 PendSV */
    .word SysTick_Handler          /* 15 SysTick，本示例的「定时器」就挂在这里 */
    /* 外部中断 0 到 27 都走兜底处理函数。
     * 有了这一段，第 28 号（TIM2）才能放在它后面。
     * 向量表不够长时，CPU 会从表的后面取到一个并非处理函数地址的字，
     * 一跳就不知道去哪里了——本示例量中断延迟要用 TIM2，因此必须补齐。 */
    .rept 28
    .word Default_Handler
    .endr
    .word TIM2_IRQHandler          /* 28 TIM2，只用来量中断延迟 */
    .size g_vectors, .-g_vectors

/* ==================== 复位入口 ==================== */
    .section .text.Reset_Handler,"ax",%progbits
    .type Reset_Handler, %function
    .global Reset_Handler
Reset_Handler:
    /* .data：从 flash 里的 LMA 搬到 RAM 里的 VMA */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
    b       2f
1:
    ldr     r3, [r2], #4
    str     r3, [r0], #4
2:
    cmp     r0, r1
    bcc     1b

    /* .bss：清零 */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r3, #0
    b       4f
3:
    str     r3, [r0], #4
4:
    cmp     r0, r1
    bcc     3b

    bl      main
5:
    b       5b
    .size Reset_Handler, .-Reset_Handler

    .align 2
    .ltorg

/* ==================== 兜底处理函数 ==================== */
    .section .text.Default_Handler,"ax",%progbits
    .type Default_Handler, %function
    .global Default_Handler
Default_Handler:
    b       .
    .size Default_Handler, .-Default_Handler
