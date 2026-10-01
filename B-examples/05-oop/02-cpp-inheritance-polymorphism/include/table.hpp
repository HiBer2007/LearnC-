/**
 * table.hpp —— 要被导出的表格数据
 *
 * 这是一个纯数据的小类型：不依赖界面，也不依赖导出器。
 * 导出器只管「怎么把表格变成文本」，表格本身不知道有哪些格式。
 *
 * 对应教材：《05-类与面向对象/02-类是一种类型.md》第 1、4 节
 */
#ifndef TABLE_HPP
#define TABLE_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct Table {
    std::string title;                          /* 表名，JSON 里会用到 */
    std::vector<std::string> headers;           /* 列名 */
    std::vector<std::vector<std::string>> rows; /* 每行的单元格 */

    /** 列数 */
    std::size_t column_count() const { return headers.size(); }

    /** 行数 */
    std::size_t row_count() const { return rows.size(); }

    /** 每行的单元格个数都与列数一致时才算合法 */
    bool is_valid() const
    {
        for (const std::vector<std::string> &row : rows) {
            if (row.size() != headers.size()) {
                return false;
            }
        }
        return true;
    }

    /** 取某个单元格，越界时返回空串（只读，不改表格） */
    const std::string &cell(std::size_t row, std::size_t column) const
    {
        static const std::string empty;
        if (row >= rows.size() || column >= rows[row].size()) {
            return empty;
        }
        return rows[row][column];
    }
};

#endif /* TABLE_HPP */
