/**
 * @file lasterrorhandler.cpp
 * @details 最后的错误相关的函数与类工具 — 实现
 * @author HiBer2007
 */

#include "lasterrorhandler.h"

#include <algorithm>

//=====================================================================
// 构造 / 析构
//=====================================================================

LastError::LastError(int ErrorCode)
    : LastErrorCode(ErrorCode), OutputLanguage("zh_cn") {}

LastError::LastError(const LastError& other)
    : LastErrorCode(other.LastErrorCode), OutputLanguage(other.OutputLanguage) {}

LastError::LastError()
    : LastErrorCode(0), OutputLanguage("zh_cn") {}

//=====================================================================
// 内部辅助
//=====================================================================

std::string LastError::localize() const {
    if (OutputLanguage.empty()) {
        return ErrorCodeToKey(LastErrorCode);
    }

    json langFile = LoadLanguageFile(OutputLanguage);
    std::string key = ErrorCodeToKey(LastErrorCode);

    if (key == ErrorKey::CODE_FMT) {
        // 对于带占位符的键，先查翻译，再将 {0} 替换为实际错误码
        std::string fmt = LookupKey(key, langFile);
        std::string placeholder = "{0}";
        size_t pos = fmt.find(placeholder);
        if (pos != std::string::npos) {
            fmt.replace(pos, placeholder.length(), std::to_string(LastErrorCode));
        }
        return fmt;
    }

    return LookupKey(key, langFile);
}

//=====================================================================
// 公开成员函数
//=====================================================================

int LastError::getLastErrorCode() const {
    return LastErrorCode;
}

std::string LastError::stringify(const std::string& Language) const {
    json langFile = LoadLanguageFile(Language);
    std::string key = ErrorCodeToKey(LastErrorCode);

    if (key == ErrorKey::CODE_FMT) {
        std::string fmt = LookupKey(key, langFile);
        std::string placeholder = "{0}";
        size_t pos = fmt.find(placeholder);
        if (pos != std::string::npos) {
            fmt.replace(pos, placeholder.length(), std::to_string(LastErrorCode));
        }
        return fmt;
    }

    return LookupKey(key, langFile);
}

bool LastError::SetOstreamOutputLanguage(const std::string& Language) {
    OutputLanguage = Language;
    return true;  // 现在支持任意语言代码
}

//=====================================================================
// 非成员运算符重载
//=====================================================================

/**
 * @brief 使用OS流输出使用的重载运算符
 * @param os 上一级传递的输出流对象
 * @param err LastError对象的引用
 * @return std::ostream& 传递到下一级的输出流对象
 */
std::ostream& operator<<(std::ostream& os, const LastError& error) {
    os << error.stringify(error.OutputLanguage);
    return os;
}

//=====================================================================
// 工具函数
//=====================================================================

/**
 * @brief 将指针内的LastError对象替换（堆版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是堆内存（new 分配），函数会 delete 旧对象后再分配新对象
 * @param pointer LastError类型指针
 * @param newErrorID 新的错误对象的ID
 * @return LastError 原始的LastError对象的副本
 */
LastError HeapReplaceLastError(LastError*& pointer, int newErrorID) {
    LastError oldError = *pointer;
    delete pointer;
    pointer = new LastError(newErrorID);
    return oldError;
}

/**
 * @brief 将指针内的LastError对象替换（栈版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是栈内存，函数不会 delete 旧对象
 * @param pointer LastError类型指针
 * @param newErrorID 新的错误对象的ID
 * @return LastError 原始的LastError对象的副本
 */
LastError StackReplaceLastError(LastError*& pointer, int newErrorID) {
    LastError oldError = *pointer;
    pointer = new LastError(newErrorID);
    return oldError;
}

/**
 * @brief 将指针内的LastError对象替换（堆版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是堆内存（new 分配），函数会 delete 旧对象再指向新对象
 * @param pointer LastError类型指针
 * @param newError 新的LastError对象指针（由调用方管理）
 * @return LastError 原始的LastError对象的副本
 */
LastError HeapReplaceLastError(LastError*& pointer, LastError* newError) {
    if (pointer == newError)
        return *pointer;

    LastError oldError = *pointer;
    delete pointer;
    pointer = newError;
    return oldError;
}

/**
 * @brief 将指针内的LastError对象替换（栈版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是栈内存，函数不会 delete 旧对象
 * @param pointer LastError类型指针
 * @param newError 新的LastError对象指针（由调用方管理）
 * @return LastError 原始的LastError对象的副本
 */
LastError StackReplaceLastError(LastError*& pointer, LastError* newError) {
    if (pointer == newError)
        return *pointer;

    LastError oldError = *pointer;
    pointer = newError;
    return oldError;
}