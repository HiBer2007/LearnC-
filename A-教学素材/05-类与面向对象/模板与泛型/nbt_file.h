#ifndef NBTCPP_NBT_FILE_H
#define NBTCPP_NBT_FILE_H

/**
 * @file nbt_file.h
 * @brief High-level NBT file I/O.
 *
 * NbtFile is the main entry point for loading and saving NBT data from/to
 * files, streams, or memory buffers.  It handles:
 * - Automatic compression detection
 * - GZip, ZLib, and uncompressed formats
 * - Java Edition (big-endian) and Bedrock Edition (little-endian)
 * - Tag selector callbacks for selective loading
 */

#include "nbtcpp/nbt_compression.h"
#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/tags/nbt_tag.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <cstdint>
#include <memory>
#include <string>
#include <istream>
#include <ostream>
#include <vector>

namespace nbtcpp {

/**
 * @brief Tag selector callback type.
 *
 * Return true to include the tag, false to skip it.
 * The callback is invoked before the tag's value is read.
 */
using TagSelector = bool (*)(const std::string& name, NbtTagType type);

/**
 * @brief Represents a complete NBT file with root tag, compression, and endianness.
 *
 * Thread safety: not guaranteed; do not access the same NbtFile from
 * multiple threads concurrently.
 */
class NbtFile {
public:
    // ─── Constructors ───────────────────────────────────────────────────

    /** @brief Create an empty NbtFile with an unnamed root compound. */
    NbtFile();

    /**
     * @brief Create an NbtFile with a given root tag.
     * @param root_tag  Root compound. Must be named.
     * @throws std::invalid_argument if root_tag is unnamed.
     */
    explicit NbtFile(std::shared_ptr<NbtCompound> root_tag);

    /**
     * @brief Load from a file, auto-detecting compression.
     * @param filepath  Path to the NBT file.
     */
    explicit NbtFile(const std::string& filepath);

    // ─── Properties ─────────────────────────────────────────────────────

    /** @brief Get the root tag. */
    std::shared_ptr<NbtCompound> root_tag() noexcept { return root_tag_; }

    /** @brief Get the root tag (const). */
    std::shared_ptr<const NbtCompound> root_tag() const noexcept { return root_tag_; }

    /** @brief Set the root tag. Must be non-null and named. */
    void set_root_tag(std::shared_ptr<NbtCompound> tag);

    /** @brief Convenience: get root tag cast to a specific type. */
    template<typename T>
    std::shared_ptr<T> root_tag_as() { return std::dynamic_pointer_cast<T>(root_tag_); }

    /** @brief File name last loaded from / saved to (may be empty). */
    const std::string& file_name() const noexcept { return file_name_; }

    /** @brief Get the compression format detected/used. */
    NbtCompression file_compression() const noexcept { return file_compression_; }

    /** @brief Whether data is big-endian (true) or little-endian (false). */
    bool is_big_endian() const noexcept { return big_endian_; }
    void set_big_endian(bool v) noexcept { big_endian_ = v; }

    /** @brief Buffer size for stream I/O (0 = no buffering). */
    int buffer_size() const noexcept { return buffer_size_; }
    void set_buffer_size(int size);

    /** @brief Default buffer size for new instances. */
    static int default_buffer_size() noexcept { return s_default_buffer_size; }
    static void set_default_buffer_size(int size);

    /** @brief Default endianness for new instances. */
    static bool big_endian_by_default() noexcept { return s_big_endian_by_default; }
    static void set_big_endian_by_default(bool v) noexcept { s_big_endian_by_default = v; }

    // ─── Loading ────────────────────────────────────────────────────────

    /** @brief Load from file with auto-detected compression. */
    int64_t load_from_file(const std::string& filepath);

    /**
     * @brief Load from file with explicit settings.
     * @param filepath    Path to file.
     * @param compression Compression mode (AutoDetect allowed).
     * @param selector    Optional tag selector callback.
     * @return Number of bytes read.
     */
    int64_t load_from_file(const std::string& filepath, NbtCompression compression,
                           TagSelector selector = nullptr);

    /**
     * @brief Load from a byte buffer.
     * @param buffer      Data buffer.
     * @param offset      Start offset in buffer.
     * @param length      Maximum bytes to read.
     * @param compression Compression mode.
     * @param selector    Optional tag selector.
     * @return Number of bytes read.
     */
    int64_t load_from_buffer(const uint8_t* buffer, size_t offset, size_t length,
                             NbtCompression compression, TagSelector selector = nullptr);

    /**
     * @brief Load from a stream.
     * @param stream      Input stream.
     * @param compression Compression mode (AutoDetect requires seekable stream).
     * @param selector    Optional tag selector.
     * @return Number of bytes read.
     */
    int64_t load_from_stream(std::istream& stream, NbtCompression compression,
                             TagSelector selector = nullptr);

    // ─── Saving ─────────────────────────────────────────────────────────

    /** @brief Save to file. */
    int64_t save_to_file(const std::string& filepath, NbtCompression compression);

    /** @brief Save to a byte buffer. Returns the number of bytes written. */
    int64_t save_to_buffer(uint8_t* buffer, size_t capacity, NbtCompression compression);

    /** @brief Save to a new byte vector. */
    std::vector<uint8_t> save_to_buffer(NbtCompression compression);

    /** @brief Save to a stream. */
    int64_t save_to_stream(std::ostream& stream, NbtCompression compression);

    // ─── Utilities ──────────────────────────────────────────────────────

    /**
     * @brief Read only the root tag name from a file (fast, no full parse).
     * @param filepath  Path to NBT file.
     * @return Root tag name.
     */
    static std::string read_root_tag_name(const std::string& filepath);

    /** @brief Convert to human-readable string. */
    std::string to_string() const;

    /** @brief Convert to human-readable string with custom indent. */
    std::string to_string(const std::string& indent) const;

private:
    std::shared_ptr<NbtCompound> root_tag_;
    std::string file_name_;
    NbtCompression file_compression_ = NbtCompression::None;
    bool big_endian_ = true;
    int buffer_size_ = 8192;

    static int s_default_buffer_size;
    static bool s_big_endian_by_default;

    // Compression detection
    static NbtCompression detect_compression(std::istream& stream);

    // Internal load (already decompressed)
    void load_from_stream_internal(std::istream& stream, TagSelector selector);

    // Internal: read root name from already-decompressed stream
    static std::string get_root_name_internal(std::istream& stream, bool big_endian);

    // ZLib helpers
    static constexpr uint8_t kZLibHeader1 = 0x78;
    static constexpr int kWriteBufferSize = 8192;
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_FILE_H
