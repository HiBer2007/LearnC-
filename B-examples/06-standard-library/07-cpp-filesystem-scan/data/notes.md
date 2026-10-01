# 扫描笔记

- `directory_iterator` 只走一层，`recursive_directory_iterator` 连子目录一起走
- 遍历顺序不由标准规定，要确定的顺序就得自己排序
- `depth()` 在迭代器上，不在 `directory_entry` 上
- `last_write_time` 返回的不是 `system_clock` 的时间点
