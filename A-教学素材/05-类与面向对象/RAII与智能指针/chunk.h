#ifndef NBTCPP_CHUNK_H
#define NBTCPP_CHUNK_H

/**
 * @file chunk.h
 * @brief Represents a single chunk in a Minecraft region file.
 *
 * Each chunk is stored in the region file with a 5-byte header:
 * - 4 bytes: data length (big-endian)
 * - 1 byte: compression type (1=GZip, 2=ZLib, 3=uncompressed)
 * - N bytes: compressed/packed NBT data
 *
 * Chunks with compression byte having bit 7 set are "external" chunks
 * (stored in separate files).  Corrupt chunks are flagged and skipped.
 */

#include "nbtcpp/tags/nbt_compound.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nbtcpp {

class RegionFile;  // forward declaration

/**
 * @brief A single chunk inside a region file.
 *
 * Chunks are identified by their (x, z) coordinates within the region
 * (0..31 each).  They contain an NbtCompound with the chunk data.
 */
class Chunk {
public:
    /// @brief Block dimensions of a chunk (16×16 blocks).
    static constexpr int kBlocksX = 16;
    static constexpr int kBlocksZ = 16;

    // ─── Construction ───────────────────────────────────────────────────

    /**
     * @brief Construct an empty (new) chunk.
     * @param region  Owning region (may be null).
     * @param x       X coordinate in region (0-31).
     * @param z       Z coordinate in region (0-31).
     */
    Chunk(RegionFile* region, int x, int z);

    /// @brief Create an empty chunk with given NBT data.
    static std::unique_ptr<Chunk> create_empty(std::shared_ptr<NbtCompound> data,
                                                int x = -1, int z = -1);

    // ─── Accessors ──────────────────────────────────────────────────────

    /** @brief Owning region file (may be null). */
    RegionFile* region() const noexcept { return region_; }
    void set_region(RegionFile* r) noexcept { region_ = r; }

    /** @brief X coordinate within the region [0, 31]. */
    int x() const noexcept { return x_; }

    /** @brief Z coordinate within the region [0, 31]. */
    int z() const noexcept { return z_; }

    /** @brief Chunk NBT data (may be null if not loaded). */
    std::shared_ptr<NbtCompound> data() const noexcept { return data_; }

    /** @brief Whether chunk data has been loaded into memory. */
    bool is_loaded() const noexcept { return data_ != nullptr; }

    /** @brief Whether chunk data has unsaved changes. */
    bool has_unsaved_changes() const noexcept { return has_unsaved_changes_; }

    /** @brief Whether this chunk is corrupted (cannot be loaded). */
    bool is_corrupt() const noexcept { return is_corrupt_; }

    /** @brief Whether this chunk is stored externally (separate file). */
    bool is_external() const noexcept { return is_external_; }

    // ─── Operations ─────────────────────────────────────────────────────

    /** @brief Load chunk data from the owning region file's stream. */
    void load();

    /**
     * @brief Serialize chunk to bytes (with 5-byte header).
     * @return Byte vector ready to write to region file.
     */
    std::vector<uint8_t> save_bytes();

    /** @brief Mark as having unsaved changes. */
    void mark_dirty() { has_unsaved_changes_ = true; }

    /** @brief Remove this chunk from its owning region. */
    void remove();

private:
    RegionFile* region_ = nullptr;
    int x_ = 0;
    int z_ = 0;
    std::shared_ptr<NbtCompound> data_;
    bool has_unsaved_changes_ = false;
    bool is_corrupt_ = false;
    bool is_external_ = false;

    // Region file offset/size (for lazy loading)
    int offset_ = 0;
    int size_ = 0;
    uint8_t compression_type_ = 2;  // default: ZLib

    // Cached original chunk data (5-byte header + compressed payload).
    // Populated during load() and returned by save_bytes() when the chunk
    // hasn't been modified, enabling bit-identical round-trips.
    std::vector<uint8_t> original_data_;

    void set_data(std::shared_ptr<NbtCompound> data);

    friend class RegionFile;
};

} // namespace nbtcpp

#endif // NBTCPP_CHUNK_H
