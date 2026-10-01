#ifndef NBTCPP_REGION_FILE_H
#define NBTCPP_REGION_FILE_H

/**
 * @file region_file.h
 * @brief Minecraft Anvil Region (.mca / .mcr) file parser.
 *
 * Region files store 32×32 chunks in a single file, with a 4 KB header
 * for location data (sector offset + count) and a 4 KB header for
 * timestamps.  Chunks are sector-aligned (4096-byte sectors).
 *
 * Reference: https://wiki.vg/Region_Files
 */

#include "nbtcpp/chunk.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nbtcpp {

/**
 * @brief Represents a Minecraft region file containing up to 1024 chunks.
 *
 * Handles:
 * - Reading/writing the 4 KB location table and 4 KB timestamp table
 * - Sector-aligned chunk storage (4096-byte sectors)
 * - Chunk insertion and removal with undo support
 */
class RegionFile {
public:
    /// @brief Region dimensions (32×32 chunks).
    static constexpr int kChunkDimX = 32;
    static constexpr int kChunkDimZ = 32;
    static constexpr int kSectorSize = 4096;

    // ─── Construction ───────────────────────────────────────────────────

    /**
     * @brief Open (or create empty) a region file.
     * @param filepath  Path to the .mca / .mcr file.
     */
    explicit RegionFile(const std::string& filepath);

    /// @brief Create an empty (unlinked) region.
    static RegionFile create_empty();

    // ─── Chunk access ───────────────────────────────────────────────────

    /** @brief Total number of non-null chunks. */
    int chunk_count() const noexcept { return chunk_count_; }

    /** @brief Get chunk at position (x, z) where x,z are in [0, 31]. */
    Chunk* get_chunk(int x, int z);

    /** @brief Get chunk at position (x, z) (const). */
    const Chunk* get_chunk(int x, int z) const;

    /** @brief Set a chunk at position (x, z). Takes ownership. */
    void set_chunk(int x, int z, std::unique_ptr<Chunk> chunk);

    /** @brief Remove chunk at (x, z). Returns ownership. */
    std::unique_ptr<Chunk> remove_chunk(int x, int z);

    /** @brief Enumerate all non-null chunks. */
    std::vector<Chunk*> all_chunks();

    /** @brief Find available (x, z) coordinates, starting from given position. */
    std::vector<std::pair<int, int>> get_available_coords(int start_x = 0, int start_z = 0) const;

    // ─── File I/O ───────────────────────────────────────────────────────

    /** @brief Load/reload from the current file path. */
    void load();

    /** @brief Save to the current file path. */
    void save();

    /** @brief Save to a different path. */
    void save_as(const std::string& filepath);

    /** @brief Whether the region file has a path (can save). */
    bool can_save() const noexcept { return !filepath_.empty(); }

    /** @brief Get/set the file path. */
    const std::string& filepath() const noexcept { return filepath_; }

    // ─── Change tracking ────────────────────────────────────────────────

    /** @brief Whether chunks have been added/removed since last save. */
    bool has_chunk_changes() const noexcept { return has_chunk_changes_; }

    /** @brief Whether any chunk has unsaved data changes. */
    bool has_unsaved_changes() const;

    // ─── Timestamp preservation ─────────────────────────────────────────

    /**
     * @brief Control whether timestamps are updated on save.
     *
     * If true (default), the current time is written as each chunk's
     * timestamp.  Set to false to preserve the original timestamps
     * loaded from the file, so the binary output can be identical to
     * the input (useful for round-trip testing).
     */
    void set_preserve_timestamps(bool v) noexcept { preserve_timestamps_ = v; }
    bool preserve_timestamps() const noexcept { return preserve_timestamps_; }

    // ─── Coordinates ────────────────────────────────────────────────────

    /** @brief Parse region coordinates from filename (r.x.z.mca). */
    struct Coords { int x; int z; };
    static Coords parse_coords(const std::string& filename);

private:
    /** @brief Private default constructor (used by create_empty). */
    RegionFile() = default;

    std::string filepath_;
    std::unique_ptr<Chunk> chunks_[kChunkDimX][kChunkDimZ] = {};
    uint8_t locations_[kSectorSize] = {};   // location table (4 KB)
    uint8_t timestamps_[kSectorSize] = {};  // timestamp table (4 KB)
    int chunk_count_ = 0;
    bool has_chunk_changes_ = false;
    bool preserve_timestamps_ = false;

    // Internal helpers
    static int chunk_data_location(int x, int z);
    int chunk_offset(int x, int z) const;
    int chunk_size(int x, int z) const;
    void update_header(int x, int z, int offset_sectors, int size_sectors);
};

/**
 * @brief Parse region coordinates from a filename like "r.0.0.mca".
 * @return Coords with x,z, or {-1,-1} if parse fails.
 */
inline RegionFile::Coords RegionFile::parse_coords(const std::string& filename) {
    // Regex-free parsing: find "r." then two numbers separated by "."
    auto pos = filename.find("r.");
    if (pos == std::string::npos) return {-1, -1};
    pos += 2;
    auto dot1 = filename.find('.', pos);
    if (dot1 == std::string::npos) return {-1, -1};
    auto dot2 = filename.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return {-1, -1};
    try {
        int x = std::stoi(filename.substr(pos, dot1 - pos));
        int z = std::stoi(filename.substr(dot1 + 1, dot2 - dot1 - 1));
        return {x, z};
    } catch (...) {
        return {-1, -1};
    }
}

} // namespace nbtcpp

#endif // NBTCPP_REGION_FILE_H
