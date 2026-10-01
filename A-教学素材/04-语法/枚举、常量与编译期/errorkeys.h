/**
 * @file errorkeys.h
 * @brief 翻译键常量与错误码 → 键映射（纯头文件，所有函数均为内联）
 * @author HiBer2007
 *
 * @details
 * 如需添加新的错误码/翻译键，在此文件中：
 *   1. 在 namespace ErrorKey 中添加新的 inline const std::string 常量
 *   2. 在 ErrorCodeToKey() 的 switch 中添加对应的 case
 */

#ifndef ERRORKEYS_H
#define ERRORKEYS_H

#include <string>

//=====================================================================
// 翻译键常量
//=====================================================================

namespace ErrorKey {
    inline const std::string UNKNOWN    = "error.unknown";       ///< 未知错误
    inline const std::string CODE_FMT   = "error.code_format";   ///< 未知错误代码（含数字）
    // 在此处添加新翻译键常量...
} // namespace ErrorKey

//=====================================================================
// 错误码 → 翻译键 映射（内联函数）
//=====================================================================

/**
 * @brief 根据错误码获取对应的翻译键
 * @param ErrorCode 错误代码
 * @return std::string 对应的翻译键
 */
inline std::string ErrorCodeToKey(int ErrorCode) {
    switch (ErrorCode) {
        case 0:  return ErrorKey::UNKNOWN;
        
        //Git运行系列
        case 1001: return "error.git.timeout";        ///< Git命令执行超时
        case 1002: return "error.git.not_found";      ///< Git命令未找到
        case 1003: return "error.git.crash";          ///< Git命令执行崩溃
        case 1004: return "error.git.write_error";    ///< Git命令写入错误
        case 1005: return "error.git.read_error";     ///< Git命令读取错误
        

        default: return ErrorKey::CODE_FMT;
    }
}

#endif // ERRORKEYS_H
