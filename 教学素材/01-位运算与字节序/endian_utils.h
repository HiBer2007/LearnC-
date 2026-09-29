#ifndef NBTCPP_ENDIAN_UTILS_H
#define NBTCPP_ENDIAN_UTILS_H

/**
 * @file endian_utils.h
 * @brief Portable byte-order swap utilities for the nbtcpp library.
 *
 * All NBT multi-byte values are stored in big-endian (Java Edition) or
 * little-endian (Bedrock Edition).  These helpers convert between the
 * host native order and the desired wire order.
 *
 * @note C++23 provides std::byteswap; until then we implement manually.
 */

#include <cstdint>
#include <type_traits>
#include <cstring>

namespace nbtcpp {
namespace detail {

// ─── Platform endian detection (C++17 compatible) ──────────────────────────
// Use an inline function evaluated at runtime (called once during init).
// MSVC does not support reinterpret_cast in constexpr.

/// @brief true when the host platform is little-endian (x86, x64, ARM, etc.)
inline bool kNativeIsLittle() noexcept {
    const uint32_t test = 0x01020304;
    return reinterpret_cast<const uint8_t*>(&test)[0] == 0x04;
}

/// @brief true when the host platform is big-endian (rare: SPARC, some MIPS, etc.)
inline bool kNativeIsBig() noexcept { return !kNativeIsLittle(); }

// ─── 16-bit swap ────────────────────────────────────────────────────────────

/// @brief Reverse the bytes of a 16-bit unsigned integer.
inline constexpr uint16_t byteswap(uint16_t v) noexcept {
    return (v >> 8) | (v << 8);
}

/// @brief Reverse the bytes of a 16-bit signed integer.
inline constexpr int16_t byteswap(int16_t v) noexcept {
    return static_cast<int16_t>(byteswap(static_cast<uint16_t>(v)));
}

// ─── 32-bit swap ────────────────────────────────────────────────────────────

/// @brief Reverse the bytes of a 32-bit unsigned integer.
inline constexpr uint32_t byteswap(uint32_t v) noexcept {
    return ((v >> 24) & 0x000000FFU) |
           ((v >>  8) & 0x0000FF00U) |
           ((v <<  8) & 0x00FF0000U) |
           ((v << 24) & 0xFF000000U);
}

/// @brief Reverse the bytes of a 32-bit signed integer.
inline constexpr int32_t byteswap(int32_t v) noexcept {
    return static_cast<int32_t>(byteswap(static_cast<uint32_t>(v)));
}

// ─── 64-bit swap ────────────────────────────────────────────────────────────

/// @brief Reverse the bytes of a 64-bit unsigned integer.
inline constexpr uint64_t byteswap(uint64_t v) noexcept {
    uint32_t lo = static_cast<uint32_t>(v);
    uint32_t hi = static_cast<uint32_t>(v >> 32);
    return (static_cast<uint64_t>(byteswap(lo)) << 32) |
            static_cast<uint64_t>(byteswap(hi));
}

/// @brief Reverse the bytes of a 64-bit signed integer.
inline constexpr int64_t byteswap(int64_t v) noexcept {
    return static_cast<int64_t>(byteswap(static_cast<uint64_t>(v)));
}

// ─── Float / double swap ────────────────────────────────────────────────────

/// @brief Swap byte order of a 32-bit float via uint32_t reinterpretation.
inline float byteswap(float v) noexcept {
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = byteswap(bits);
    std::memcpy(&v, &bits, sizeof(bits));
    return v;
}

/// @brief Swap byte order of a 64-bit double via uint64_t reinterpretation.
inline double byteswap(double v) noexcept {
    static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = byteswap(bits);
    std::memcpy(&v, &bits, sizeof(bits));
    return v;
}

// ─── EndianConverter ────────────────────────────────────────────────────────

/**
 * @brief A lightweight adapter that conditionally swaps values based on the
 *        desired NBT byte order.
 *
 * Example usage:
 * @code
 *   EndianConverter conv(true);  // true = big-endian (Java), false = little-endian (Bedrock)
 *   int32_t net = conv.convert(host_value);   // swaps if LE host
 * @endcode
 */
class EndianConverter {
    bool swap_;  ///< true → bytes need swapping
public:
    /// @param  big_endian  true for Java Edition NBT, false for Bedrock Edition.
    explicit EndianConverter(bool big_endian) noexcept
        : swap_(kNativeIsLittle() == big_endian) {}

    /// @return true if bytes will be swapped.
    bool needs_swap() const noexcept { return swap_; }

    // ── Conditionally swap ──────────────────────────────────────────────

    int16_t  convert(int16_t  v) const noexcept { return swap_ ? byteswap(v) : v; }
    uint16_t convert(uint16_t v) const noexcept { return swap_ ? byteswap(v) : v; }
    int32_t  convert(int32_t  v) const noexcept { return swap_ ? byteswap(v) : v; }
    uint32_t convert(uint32_t v) const noexcept { return swap_ ? byteswap(v) : v; }
    int64_t  convert(int64_t  v) const noexcept { return swap_ ? byteswap(v) : v; }
    uint64_t convert(uint64_t v) const noexcept { return swap_ ? byteswap(v) : v; }
    float    convert(float    v) const noexcept { return swap_ ? byteswap(v) : v; }
    double   convert(double   v) const noexcept { return swap_ ? byteswap(v) : v; }

    // ── Write a value in native order that will become wire order after swap ──
    // For writing: we need the inverse — given a host value, produce the
    // wire representation.  Because swap is its own inverse (xor), convert()
    // works for both directions.

    /// Convenience: write raw bytes of a value in wire order into a buffer.
    template <typename T>
    void write_be(T value, uint8_t* buf) const noexcept {
        if (swap_) value = byteswap(value);
        std::memcpy(buf, &value, sizeof(T));
    }
};

} // namespace detail
} // namespace nbtcpp

#endif // NBTCPP_ENDIAN_UTILS_H
