#ifndef NBTCPP_NBT_TAG_TYPE_H
#define NBTCPP_NBT_TAG_TYPE_H

#include <cstdint>
#include <stdexcept>
#include <string>

namespace nbtcpp {

/**
 * @brief Enumeration of all NBT (Named Binary Tag) types, matching the Minecraft
 *        specification at https://wiki.vg/NBT.
 *
 * Each tag type has a fixed byte ID used in the binary encoding.  The order and
 * numeric values are defined by the NBT specification and MUST NOT be changed.
 */
enum class NbtTagType : uint8_t {
    /// Placeholder used when the actual type is not yet known (e.g. empty NbtList).
    Unknown   = 0xFF,

    /// TAG_End: marks the end of a TAG_Compound.  No name, no payload.
    End       = 0x00,

    /// TAG_Byte: a single signed/unsigned byte.
    Byte      = 0x01,

    /// TAG_Short: a signed 16-bit integer (big-endian in Java, little-endian in Bedrock).
    Short     = 0x02,

    /// TAG_Int: a signed 32-bit integer.
    Int       = 0x03,

    /// TAG_Long: a signed 64-bit integer.
    Long      = 0x04,

    /// TAG_Float: a 32-bit IEEE-754 single-precision floating-point number.
    Float     = 0x05,

    /// TAG_Double: a 64-bit IEEE-754 double-precision floating-point number.
    Double    = 0x06,

    /// TAG_Byte_Array: a length-prefixed array of bytes.
    ByteArray = 0x07,

    /// TAG_String: a length-prefixed UTF-8 string.
    String    = 0x08,

    /// TAG_List: an ordered list of unnamed tags, all of the same type.
    List      = 0x09,

    /// TAG_Compound: an unordered map of named tags.
    Compound  = 0x0A,

    /// TAG_Int_Array: a length-prefixed array of 32-bit integers.
    IntArray  = 0x0B,

    /// TAG_Long_Array: a length-prefixed array of 64-bit integers.
    LongArray = 0x0C
};

/**
 * @brief Convert an NbtTagType to its human-readable canonical name.
 *
 * Examples:
 *   - NbtTagType::Byte    → "TAG_Byte"
 *   - NbtTagType::List    → "TAG_List"
 *   - NbtTagType::Unknown → "UNKNOWN"
 *
 * @param type The tag type.
 * @return A std::string containing the canonical name.
 */
inline std::string to_string(NbtTagType type) {
    switch (type) {
        case NbtTagType::End:       return "TAG_End";
        case NbtTagType::Byte:      return "TAG_Byte";
        case NbtTagType::Short:     return "TAG_Short";
        case NbtTagType::Int:       return "TAG_Int";
        case NbtTagType::Long:      return "TAG_Long";
        case NbtTagType::Float:     return "TAG_Float";
        case NbtTagType::Double:    return "TAG_Double";
        case NbtTagType::ByteArray: return "TAG_Byte_Array";
        case NbtTagType::String:    return "TAG_String";
        case NbtTagType::List:      return "TAG_List";
        case NbtTagType::Compound:  return "TAG_Compound";
        case NbtTagType::IntArray:  return "TAG_Int_Array";
        case NbtTagType::LongArray: return "TAG_Long_Array";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief Check whether a tag type carries a value payload.
 *
 * Compound, List, End, and Unknown tags do not have a direct value;
 * all other types do.
 */
inline bool has_value(NbtTagType type) noexcept {
    switch (type) {
        case NbtTagType::Compound:
        case NbtTagType::End:
        case NbtTagType::List:
        case NbtTagType::Unknown:
            return false;
        default:
            return true;
    }
}

} // namespace nbtcpp

#endif // NBTCPP_NBT_TAG_TYPE_H
