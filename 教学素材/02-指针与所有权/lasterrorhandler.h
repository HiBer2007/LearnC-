/**
 * @file lasterrorhandler.h
 * @brief 最后的错误相关的函数与类工具 — 声明
 * @author HiBer2007
 */

#ifndef LASTERRORHANDLER_H
#define LASTERRORHANDLER_H

#include <iostream>
#include <string>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <nlohmann/json.hpp>

#include "errorkeys.h"

using json = nlohmann::json;

//=====================================================================
// 语言文件加载与查询（内联函数）
//=====================================================================

/**
 * @brief 获取语言文件所在目录路径
 */
static inline std::string langDirPath() {
    return QCoreApplication::applicationDirPath().toStdString()
         + "/../resources/lang";
}

/**
 * @brief 加载指定语言代码的 JSON 语言文件
 * @param Language 语言代码（如 "zh_cn"、"en_us"）
 * @return json 解析后的 JSON 对象；失败时返回 nullptr 的 json
 */
inline json LoadLanguageFile(const std::string& Language) {
    QDir dir(QString::fromStdString(langDirPath()));
    QString filePath = dir.absoluteFilePath(
        QString::fromStdString(Language) + ".json");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return json();  // 返回空 json（is_null() == true）
    }

    QTextStream in(&file);
    std::string content = in.readAll().toStdString();
    file.close();

    try {
        return json::parse(content);
    } catch (...) {
        return json();  // 解析失败也返回空 json
    }
}

/**
 * @brief 从 JSON 语言文件中查找键对应的文本
 * @param Key 翻译键
 * @param LangFile 已解析的 JSON 语言文件对象
 * @return std::string 翻译文本；若键不存在则返回原始键
 */
inline std::string LookupKey(const std::string& Key, const json& LangFile) {
    if (LangFile.is_null() || !LangFile.contains(Key)) {
        return Key;
    }
    return LangFile[Key].get<std::string>();
}

/**
 * @class LastError
 * @brief LastError实例
 */
class LastError {
    private:
        int LastErrorCode;             ///< 最后的报错信息对应的ID
        std::string OutputLanguage;    ///< 输出流使用的语言

        /**
         * @brief 根据当前 OutputLanguage 查找本地化文本
         * @return std::string 本地化文本
         */
        std::string localize() const;

    public:
        /**
         * @brief 传递错误代码的构造函数
         * @param ErrorCode 错误代码
         */
        explicit LastError(int ErrorCode);

        /**
         * @brief 拷贝构造函数
         * @param other 其他LastError对象的引用
         */
        LastError(const LastError& other);

        /**
         * @brief 无传递值的默认构造函数
         */
        LastError();

        /**
         * @brief 默认析构函数
         */
        ~LastError() = default;

        /**
         * @brief 获取最后的错误代码
         * @return int 返回最后的错误代码
         */
        int getLastErrorCode() const;

        /**
         * @brief 文本化错误信息
         * @param Language 语言代码，默认值为"zh_cn"，表示简体中文
         * @return std::string 返回文本化的错误信息
         */
        std::string stringify(const std::string& Language = "zh_cn") const;

        /**
         * @brief 设置使用输出流时的语言
         * @param Language 语言代码（如 "zh_cn"、"en_us"）
         * @return bool 表示是否设置成功
         */
        bool SetOstreamOutputLanguage(const std::string& Language);

        /// @brief 声明输出流重载运算符友元函数
        friend std::ostream& operator<<(std::ostream& os, const LastError& error);
};

/**
 * @brief 使用OS流输出使用的重载运算符
 * @param os 上一级传递的输出流对象
 * @param err LastError对象的引用
 * @return std::ostream& 传递到下一级的输出流对象
 */
std::ostream& operator<<(std::ostream& os, const LastError& error);

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
LastError HeapReplaceLastError(LastError*& pointer, int newErrorID);

/**
 * @brief 将指针内的LastError对象替换（栈版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是栈内存，函数不会 delete 旧对象
 * @param pointer LastError类型指针
 * @param newErrorID 新的错误对象的ID
 * @return LastError 原始的LastError对象的副本
 */
LastError StackReplaceLastError(LastError*& pointer, int newErrorID);

/**
 * @brief 将指针内的LastError对象替换（堆版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是堆内存（new 分配），函数会 delete 旧对象再指向新对象
 * @param pointer LastError类型指针
 * @param newError 新的LastError对象指针（由调用方管理）
 * @return LastError 原始的LastError对象的副本
 */
LastError HeapReplaceLastError(LastError*& pointer, LastError* newError);

/**
 * @brief 将指针内的LastError对象替换（栈版本），并返回原始的LastError对象
 * @details 要求 pointer 指向的是栈内存，函数不会 delete 旧对象
 * @param pointer LastError类型指针
 * @param newError 新的LastError对象指针（由调用方管理）
 * @return LastError 原始的LastError对象的副本
 */
LastError StackReplaceLastError(LastError*& pointer, LastError* newError);

#endif // LASTERRORHANDLER_H
