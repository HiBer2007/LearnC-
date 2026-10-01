/* link_override.c —— 应用侧的强定义，覆盖 core 里的弱定义
 *
 * 它只被编进可执行文件，不进静态库。
 * 链接器看到同一个名字既有弱定义又有强定义时，选强的那一份；
 * 这就是真启动文件里 `weak SVC_Handler` 被用户自己的实现顶掉的机制。
 *
 * 想看不覆盖时会怎样：把本文件从 CMakeLists.txt 的目标里去掉再编译，
 * 自测第 1 项就会变成 [失败]，名字回到 core-weak-default。
 */
#include "link_lab.h"

const char *ll_provider_name(void) { return "app-strong"; }
