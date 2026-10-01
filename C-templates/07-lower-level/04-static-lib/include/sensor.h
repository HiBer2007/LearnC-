/* sensor.h —— 练习模板 04-static-lib 的公共接口
 *
 * 这个模板练的是「符号在链接器眼里长什么样」：
 *   1. 内部链接与外部链接（static 挡掉了什么，nm 里怎么看）；
 *   2. 静态库的成员粒度与顺序敏感；
 *   3. 弱符号：库怎么发现「外部提供了自己的实现」；
 *   4. 强定义覆盖之后，行为换成驱动那一份。
 *
 * 各阶段的实现分散在：
 *   core/sensor.c          阶段 1 的 TODO
 *   core/sensor_extra.c    阶段 2 的 TODO（静态库的第二个成员）
 *   core/sensor_default.c  阶段 3 的 TODO（库自带的默认算法）
 *   drivers/driver_fast.c  阶段 4 的 TODO（外部驱动）
 */
#ifndef SENSOR_H
#define SENSOR_H

/* 通道个数：合法通道号是 0 到 SENSOR_CHANNELS-1 */
#define SENSOR_CHANNELS 4

/* 通道号检查：合法就原样返回，越界返回 -1。
 * 实现放在 core/sensor.c，内部调用 strip_channel。 */
int sensor_check_channel(int ch);

/* 库的版本号：实现在 core/sensor_extra.c（静态库的第二个成员） */
int sensor_version(void);

/* 库自带的采样算法：实现在 core/sensor_default.c。
 * 外部没有提供采样钩子时用它。 */
int sensor_sample_default(int ch);

/* 把上面几件事合起来：检查通道号，再用采样钩子或库自带的算法取一个值。
 * 采样钩子 sensor_sample_hook 不在这个头文件里声明：它是外部的可选实现，
 * 由 core/sensor.c 自己按弱符号去发现（见那个文件的注释）。 */
int sensor_read(int ch);

#endif /* SENSOR_H */
