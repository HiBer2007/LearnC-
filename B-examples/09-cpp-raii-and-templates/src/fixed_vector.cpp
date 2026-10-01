/**
 * fixed_vector.cpp —— 两个全特化的定义
 *
 * 全特化已经不是模板了，而是一个普通函数，
 * 因此它的定义要放在某个 .cpp 里（声明留在头文件，见 fixed_vector.hpp）。
 */
#include "fixed_vector.hpp"

/* bool 默认打成 1 或 0，这里改成是或否 */
template <>
std::string to_text<bool>(const bool &value)
{
    return value ? "是" : "否";
}

/* double 默认六位有效数字，这里固定三位小数 */
template <>
std::string to_text<double>(const double &value)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(3) << value;
    return os.str();
}
