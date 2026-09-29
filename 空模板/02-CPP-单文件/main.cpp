/**
 * 空模板 02 · C++ 单文件
 *
 * 和模板 01 结构一样，只换成 C++。
 * 配置时多一个坑：GDB 路线要用 g++，不能用 gcc。
 *
 * 请照着同目录的《配置步骤.md》把 .vscode 配出来。
 */
#include <iostream>
#include <numeric>
#include <vector>

/* 返回引用，演示「引用」在调试器里怎么显示 */
static int &at(std::vector<int> &v, std::size_t i)
{
    return v[i];
}

int main()
{
    std::vector<int> nums = {3, 1, 4, 1, 5};

    std::cout << "原始数据: ";
    for (int x : nums) {
        std::cout << x << " ";
    }
    std::cout << "\n";

    at(nums, 0) = 99;   /* 通过引用改掉第一个元素 */

    const int total = std::accumulate(nums.begin(), nums.end(), 0);
    std::cout << "改后总和: " << total << "\n";

    return 0;
}
