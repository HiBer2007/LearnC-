/* link_cpp_side.cpp —— C++ 翻译单元，供 C 侧调用
 *
 * 编译：由 CMakeLists.txt 与 C 源文件一起链进可执行目标。
 * 手工编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c src/link_cpp_side.cpp -o link_cpp_side.o
 *
 * 这里有两个符号：
 *   ll_cpp_name   用 extern "C" 导出，符号名就是 ll_cpp_name，C 侧找得到
 *   ll::cpp_probe 是 C++ 链接的函数，符号名会被修饰成 _ZN2ll9cpp_probeB5cxx11Ev 之类
 * 用 nm 看同一个目标文件里的这两个名字，差别一目了然。
 */
#include "link_lab.h"

namespace ll {

/* C++ 链接：外部链接，名字按 Itanium ABI 修饰。
 * 它被下面的 extern "C" 函数调用，因此不会被优化掉。 */
const char *cpp_probe(void) { return "cpp-mangled"; }

} /* namespace ll */

extern "C" const char *ll_cpp_name(void) { return "cpp-side"; }

extern "C" const char *ll_cpp_tag(void) { return ll::cpp_probe(); }
