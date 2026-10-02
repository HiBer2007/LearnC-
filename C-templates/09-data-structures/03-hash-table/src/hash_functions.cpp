/* hash_functions.cpp —— 练习模板 03 的字符串哈希（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * DefaultHash<std::string> 调用的就是这个函数。它不参与练习，不需要改；
 * 想换一个哈希函数做对照时，把 HashTable 的第三个模板实参换掉即可，
 * 不必动这个文件。
 */
#include "hash_table.hpp"

namespace dsh {

std::size_t hash_string(const std::string &text)
{
    std::size_t h = 5381U;
    for (char ch : text) {
        /* 经典写法：h = h * 33 + c，用移位与加法代替乘法 */
        h = ((h << 5) + h) + static_cast<unsigned char>(ch);
    }
    return h;
}

} /* namespace dsh */
