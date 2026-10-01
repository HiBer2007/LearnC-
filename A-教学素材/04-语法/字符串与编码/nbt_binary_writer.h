#ifndef NBTCPP_NBT_BINARY_WRITER_H
#define NBTCPP_NBT_BINARY_WRITER_H

/**
 * @file nbt_binary_writer.h
 * @brief Low-level binary writer for NBT streams.
 *
 * NbtBinaryWriter wraps an output stream and provides methods for writing
 * NBT primitives with correct byte-order.  Every write method also
 * contributes to an optional byte counter.
 *
 * This class is internal to the library; most users will prefer NbtWriter
 * (stream-based tree builder) or NbtFile (file-level I/O).
 */

#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/endian_utils.h"

#include <cstdint>
#include <iosfwd>
#include <string>

namespace nbtcpp {

/**
 * @brief Low-level binary writer for NBT data streams.
 *
 * Writes primitives to a std::ostream while handling:
 * - Byte-order conversion (big-endian Java vs little-endian Bedrock)
 * - Length-prefixed UTF-8 strings
 * - Chunked writes for large payloads
 */
class NbtBinaryWriter {
public:
    /**
     * @brief Construct a writer bound to an output stream.
     *
     * @param stream    The output stream to write to.  Must be writable.
     * @param big_endian  true for Java Edition (big-endian), false for Bedrock (little-endian).
     */
    explicit NbtBinaryWriter(std::ostream& stream, bool big_endian = true) noexcept;

    // ─── Write API ──────────────────────────────────────────────────────

    /** @brief Write a single byte. */
    void write(uint8_t value);

    /** @brief Write an NbtTagType as a single byte. */
    void write(NbtTagType type);

    /** @brief Write a signed 16-bit integer (with byte-order correction). */
    void write(int16_t value);

    /** @brief Write a signed 32-bit integer (with byte-order correction). */
    void write(int32_t value);

    /** @brief Write a signed 64-bit integer (with byte-order correction). */
    void write(int64_t value);

    /** @brief Write a 32-bit float (with byte-order correction). */
    void write(float value);

    /** @brief Write a 64-bit double (with byte-order correction). */
    void write(double value);

    /**
     * @brief Write a length-prefixed UTF-8 string.
     *
     * Wire format: [int16 byte_count][UTF-8 bytes].
     * The byte_count includes the length prefix itself (Minecraft convention).
     */
    void write(const std::string& value);

    /**
     * @brief Write a raw byte buffer.
     *
     * @param data   Pointer to data.
     * @param count  Number of bytes to write.
     */
    void write_raw(const uint8_t* data, int32_t count);

    // ─── Stream access ──────────────────────────────────────────────────

    /** @brief Flush and return the underlying output stream. */
    std::ostream& stream();

    /** @brief Flush the underlying stream. */
    void flush();

    // ─── Byte counting ──────────────────────────────────────────────────

    /** @brief Total number of bytes written so far. */
    int64_t bytes_written() const noexcept { return bytes_written_; }

private:
    std::ostream& stream_;
    detail::EndianConverter conv_;
    int64_t bytes_written_ = 0;

    /** Internal buffer for converting values to bytes. */
    uint8_t buffer_[8];

    /** UTF-8 encoding buffer (256 bytes covers most strings). */
    static constexpr int kBufferSize = 256;
    uint8_t encode_buffer_[kBufferSize];

    /** Write exactly count bytes from buffer. */
    void write_buf(const uint8_t* buf, int count);
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_BINARY_WRITER_H
