/* shapes.h —— 三种排布的结构体，练习模板 01-layout-probe 阶段 3、4 的素材
 *
 * 三个类型的成员完全一样，只有排布方式不同：
 *   LpPlain    自然对齐（编译器自己插填充）
 *   LpPacked   紧凑排布（#pragma pack(1)，等价于 GCC 的 __attribute__((packed))）
 *   LpAligned  整体 16 字节对齐（C11 的 _Alignas，C23 起可以写成 alignas）
 *
 * 不要改这里的定义：阶段 3 要报的是这三种排布的真实结果。
 */
#ifndef SHAPES_H
#define SHAPES_H

#include <stdint.h>

typedef struct {
    uint8_t  tag;
    uint32_t value;
    uint16_t flags;
    uint8_t  kind;
} LpPlain;

#pragma pack(push, 1)
typedef struct {
    uint8_t  tag;
    uint32_t value;
    uint16_t flags;
    uint8_t  kind;
} LpPacked;
#pragma pack(pop)

typedef struct {
    _Alignas(16) uint8_t block[16];
    uint32_t             tail;
} LpAligned;

#endif /* SHAPES_H */
