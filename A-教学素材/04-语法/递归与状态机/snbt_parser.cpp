/**
 * @file snbt_parser.cpp
 * @brief SNBT parser implementation (recursive descent).
 *
 * Grammar (simplified):
 *   value     → compound | list | array | typed_value
 *   compound  → '{' (named_value (',' named_value)*)? '}'
 *   list      → '[' value (',' value)* ']'
 *   array     → '[' prefix ';' value (',' value)* ']'
 *   named_value → key ':' value
 *   key       → string
 *   typed_value → string_literal | number_suffix | bool
 */

#include "nbtcpp/snbt/snbt_parser.h"
#include "nbtcpp/tags/nbt_byte.h"
#include "nbtcpp/tags/nbt_short.h"
#include "nbtcpp/tags/nbt_int.h"
#include "nbtcpp/tags/nbt_long.h"
#include "nbtcpp/tags/nbt_float.h"
#include "nbtcpp/tags/nbt_double.h"
#include "nbtcpp/tags/nbt_string.h"
#include "nbtcpp/tags/nbt_byte_array.h"
#include "nbtcpp/tags/nbt_int_array.h"
#include "nbtcpp/tags/nbt_long_array.h"
#include "nbtcpp/tags/nbt_list.h"
#include "nbtcpp/nbt_exception.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace nbtcpp {
namespace snbt {

using Constants = snbt::Constants;

// ─── Lexer ───────────────────────────────────────────────────────────────────

class StringStream {
public:
    explicit StringStream(std::string input) : input_(std::move(input)) {}

    bool can_read(int offset = 0) const {
        return (cursor_ + offset) < static_cast<int>(input_.size());
    }

    char peek(int offset = 0) const {
        return input_[static_cast<size_t>(cursor_ + offset)];
    }

    char read() {
        if (!can_read()) throw NbtFormatException("Unexpected end of SNBT input");
        return input_[static_cast<size_t>(cursor_++)];
    }

    void skip() { cursor_++; }

    void skip_whitespace() {
        while (can_read() && std::isspace(static_cast<unsigned char>(peek()))) {
            skip();
        }
    }

    void expect(char c) {
        skip_whitespace();
        if (!can_read() || read() != c) {
            throw NbtFormatException(std::string("Expected '") + c + "' in SNBT");
        }
    }

    static bool is_quote(char c) {
        return c == Constants::kPrimaryQuote || c == Constants::kSecondaryQuote;
    }

    static bool is_unquoted_allowed(char c) {
        return (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') ||
               c == '_' || c == '-' || c == '.' || c == '+' ||
               static_cast<unsigned char>(c) == 0xE2; // ∞ (UTF-8 multi-byte)
    }

    std::string read_string() {
        skip_whitespace();
        if (!can_read()) return {};
        if (is_quote(peek())) {
            char quote = read();
            return read_quoted_string(quote);
        }
        return read_unquoted_string();
    }

    std::string read_quoted_string(char quote) {
        std::string result;
        bool escaped = false;
        while (can_read()) {
            char c = read();
            if (escaped) {
                if (c == quote || c == '\\') {
                    result += c;
                } else if (c == 'n') {
                    result += '\n';
                } else if (c == 't') {
                    result += '\t';
                } else if (c == 'r') {
                    result += '\r';
                } else {
                    result += '\\';
                    result += c;
                }
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == quote) {
                return result;
            } else {
                result += c;
            }
        }
        throw NbtFormatException("Unterminated string in SNBT");
    }

    std::string read_unquoted_string() {
        std::string result;
        while (can_read() && is_unquoted_allowed(peek())) {
            result += read();
        }
        return result;
    }

    int cursor() const { return cursor_; }

private:
    std::string input_;
    int cursor_ = 0;
};

// ─── Parser ──────────────────────────────────────────────────────────────────

class Parser {
public:
    explicit Parser(std::string input) : reader_(std::move(input)) {}

    NbtTagPtr parse_value() {
        reader_.skip_whitespace();
        if (!reader_.can_read()) throw NbtFormatException("Expected a value in SNBT");
        char next = reader_.peek();
        if (next == Constants::kCompoundOpen) return parse_compound();
        if (next == Constants::kListOpen) return parse_list_like();
        return parse_typed_value();
    }

    NbtTagPtr parse_named_value() {
        std::string key = reader_.read_string();
        reader_.expect(Constants::kNameValueSep);
        NbtTagPtr value = parse_value();
        value->set_name(key);
        return value;
    }

    void finish() {
        reader_.skip_whitespace();
        if (reader_.can_read()) {
            throw NbtFormatException("Trailing data after position " +
                                     std::to_string(reader_.cursor()));
        }
    }

private:
    StringStream reader_;

    NbtTagPtr parse_compound() {
        reader_.expect(Constants::kCompoundOpen);
        auto compound = std::make_unique<NbtCompound>();

        reader_.skip_whitespace();
        while (reader_.can_read() && reader_.peek() != Constants::kCompoundClose) {
            auto tag = parse_named_value();
            compound->add(std::move(tag));
            if (!read_separator()) break;
        }
        reader_.expect(Constants::kCompoundClose);
        return compound;
    }

    bool read_separator() {
        reader_.skip_whitespace();
        if (reader_.can_read() && reader_.peek() == Constants::kValueSep) {
            reader_.skip();
            reader_.skip_whitespace();
            return true;
        }
        return false;
    }

    NbtTagPtr parse_list_like() {
        // Peek ahead to detect array vs list
        // Array: [B; ...], [I; ...], [L; ...]
        if (reader_.can_read(3) &&
            !StringStream::is_quote(reader_.peek(1)) &&
            reader_.peek(2) == Constants::kArrayDelim) {
            return parse_array();
        }
        return parse_list();
    }

    NbtTagPtr parse_array() {
        reader_.expect(Constants::kListOpen);
        char type_char = reader_.read();
        reader_.read(); // skip ';'
        reader_.skip_whitespace();

        if (!reader_.can_read()) throw NbtFormatException("Expected array to end, but reached end of data");

        NbtTagType elem_type;
        if (type_char == Constants::kByteArrayPrefix) elem_type = NbtTagType::Byte;
        else if (type_char == Constants::kLongArrayPrefix) elem_type = NbtTagType::Long;
        else if (type_char == Constants::kIntArrayPrefix) elem_type = NbtTagType::Int;
        else throw NbtFormatException(std::string("Invalid array prefix: '") + type_char + "'");

        if (elem_type == NbtTagType::Byte) {
            std::vector<uint8_t> values;
            while (reader_.peek() != Constants::kListClose) {
                auto tag = parse_value();
                values.push_back(static_cast<uint8_t>(tag->byte_value()));
                if (!read_separator()) break;
            }
            reader_.expect(Constants::kListClose);
            return std::make_unique<NbtByteArray>(std::move(values));
        } else if (elem_type == NbtTagType::Long) {
            std::vector<int64_t> values;
            while (reader_.peek() != Constants::kListClose) {
                auto tag = parse_value();
                values.push_back(tag->long_value());
                if (!read_separator()) break;
            }
            reader_.expect(Constants::kListClose);
            return std::make_unique<NbtLongArray>(std::move(values));
        } else {
            std::vector<int32_t> values;
            while (reader_.peek() != Constants::kListClose) {
                auto tag = parse_value();
                values.push_back(tag->int_value());
                if (!read_separator()) break;
            }
            reader_.expect(Constants::kListClose);
            return std::make_unique<NbtIntArray>(std::move(values));
        }
    }

    NbtTagPtr parse_list() {
        reader_.expect(Constants::kListOpen);
        reader_.skip_whitespace();
        auto list = std::make_unique<NbtList>();

        while (reader_.can_read() && reader_.peek() != Constants::kListClose) {
            auto tag = parse_value();
            list->add(std::move(tag));
            if (!read_separator()) break;
        }
        reader_.expect(Constants::kListClose);
        return list;
    }

    NbtTagPtr parse_typed_value() {
        reader_.skip_whitespace();
        if (StringStream::is_quote(reader_.peek())) {
            char quote = reader_.read();
            return std::make_unique<NbtString>(reader_.read_quoted_string(quote));
        }

        std::string str = reader_.read_unquoted_string();
        if (str.empty()) throw NbtFormatException("Expected typed value to be non-empty");
        return type_tag(str);
    }

    NbtTagPtr type_tag(const std::string& str) {
        // Try with suffix first
        if (str.size() >= 2) {
            char last = str.back();
            std::string num_part = str.substr(0, str.size() - 1);

            if (last == Constants::kFloatSuffix) {
                auto sf = try_parse_special_float(num_part);
                if (sf) return std::make_unique<NbtFloat>(*sf);
                char* end = nullptr;
                float val = std::strtof(num_part.c_str(), &end);
                if (*end == '\0') return std::make_unique<NbtFloat>(val);
            }
            if (last == Constants::kDoubleSuffix) {
                auto sf = try_parse_special_float(num_part);
                if (sf) return std::make_unique<NbtDouble>(static_cast<double>(*sf));
                char* end = nullptr;
                double val = std::strtod(num_part.c_str(), &end);
                if (*end == '\0') return std::make_unique<NbtDouble>(val);
            }
            if (last == Constants::kByteSuffix) {
                char* end = nullptr;
                long val = std::strtol(num_part.c_str(), &end, 10);
                if (*end == '\0') {
                    if (val >= -128 && val <= 127)
                        return std::make_unique<NbtByte>(static_cast<uint8_t>(static_cast<int8_t>(val)));
                }
            }
            if (last == Constants::kShortSuffix) {
                char* end = nullptr;
                long val = std::strtol(num_part.c_str(), &end, 10);
                if (*end == '\0' && val >= -32768 && val <= 32767)
                    return std::make_unique<NbtShort>(static_cast<int16_t>(val));
            }
            if (last == Constants::kLongSuffix) {
                char* end = nullptr;
                long long val = std::strtoll(num_part.c_str(), &end, 10);
                if (*end == '\0') return std::make_unique<NbtLong>(static_cast<int64_t>(val));
            }
        }

        // Try integer (no suffix)
        {
            char* end = nullptr;
            long val = std::strtol(str.c_str(), &end, 10);
            if (*end == '\0') {
                if (val >= -2147483648LL && val <= 2147483647LL)
                    return std::make_unique<NbtInt>(static_cast<int32_t>(val));
                return std::make_unique<NbtLong>(static_cast<int64_t>(val));
            }
        }

        // Try double (no suffix, with decimal point)
        {
            char* end = nullptr;
            double val = std::strtod(str.c_str(), &end);
            if (*end == '\0' && str.find('.') != std::string::npos) {
                return std::make_unique<NbtDouble>(val);
            }
        }

        // Special values
        {
            auto sp = try_parse_special(str);
            if (sp) return sp;
        }

        // Boolean
        if (str == "true") return std::make_unique<NbtByte>(static_cast<uint8_t>(1));
        if (str == "false") return std::make_unique<NbtByte>(static_cast<uint8_t>(0));

        // Fallback: treat as string
        return std::make_unique<NbtString>(str);
    }

    static std::optional<float> try_parse_special_float(const std::string& s) {
        if (s == "Infinity" || s == "+Infinity") return std::numeric_limits<float>::infinity();
        if (s == "-Infinity") return -std::numeric_limits<float>::infinity();
        if (s == "NaN") return std::numeric_limits<float>::quiet_NaN();
        return std::nullopt;
    }

    static NbtTagPtr try_parse_special(const std::string& str) {
        if (str == "Infinity" || str == "+Infinity") return std::make_unique<NbtDouble>(std::numeric_limits<double>::infinity());
        if (str == "-Infinity") return std::make_unique<NbtDouble>(-std::numeric_limits<double>::infinity());
        if (str == "NaN") return std::make_unique<NbtDouble>(std::numeric_limits<double>::quiet_NaN());
        return nullptr;
    }
};

// ─── Public API ──────────────────────────────────────────────────────────────

NbtTagPtr parse(const std::string& snbt, bool named) {
    Parser parser(snbt);
    NbtTagPtr result;
    if (named) {
        // Read named value: key: value
        result = parser.parse_named_value();
    } else {
        result = parser.parse_value();
    }
    parser.finish();
    return result;
}

NbtTagPtr try_parse(const std::string& snbt, bool named) noexcept {
    try {
        return parse(snbt, named);
    } catch (...) {
        return nullptr;
    }
}

} // namespace snbt
} // namespace nbtcpp
