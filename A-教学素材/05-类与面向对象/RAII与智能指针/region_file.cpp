/**
 * @file region_file.cpp
 * @brief Implementation of RegionFile (Minecraft Anvil region file).
 */

#include "nbtcpp/region_file.h"
#include "nbtcpp/nbt_file.h"
#include "nbtcpp/nbt_binary_reader.h"
#include "nbtcpp/nbt_exception.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

RegionFile::RegionFile(const std::string& filepath)
    : filepath_(filepath)
{
    load();
}

RegionFile RegionFile::create_empty() {
    RegionFile rf;  // uses private default constructor
    rf.has_chunk_changes_ = false;
    rf.chunk_count_ = 0;
    return rf;
}

// ─── Loading ─────────────────────────────────────────────────────────────────

void RegionFile::load() {
    if (filepath_.empty()) return;

    std::ifstream file(filepath_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open region file: " + filepath_);
    }

    // Read headers
    file.read(reinterpret_cast<char*>(locations_), kSectorSize);
    file.read(reinterpret_cast<char*>(timestamps_), kSectorSize);

    if (!file) {
        throw std::runtime_error("Failed to read region file headers");
    }

    // Clear existing chunks
    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            chunks_[x][z].reset();
        }
    }
    chunk_count_ = 0;

    // Parse chunk table
    auto file_len = file.tellg();
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();

    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            int off = chunk_offset(x, z);
            int sz = chunk_size(x, z);

            if (off > 0 && off < 2) {
                // Offset 0 or 1 means the chunk is in the header area — invalid
                throw std::runtime_error("Invalid region file: chunk offset in header area");
            }
            if (off * static_cast<int64_t>(kSectorSize) > file_size) {
                throw std::runtime_error("Invalid region file: chunk offset beyond file end");
            }
            if (sz > 0) {
                chunk_count_++;
                auto chunk = std::make_unique<Chunk>(this, x, z);
                chunk->offset_ = off * kSectorSize;  // convert sectors → bytes
                chunk->size_ = sz * kSectorSize;
                chunks_[x][z] = std::move(chunk);

                // Load the first chunk to verify this is really a region file
                if (chunk_count_ == 1) {
                    chunks_[x][z]->load();
                }
            }
        }
    }

    if (chunk_count_ == 0) {
        throw std::runtime_error("Region file contains no chunks");
    }

    has_chunk_changes_ = false;
}

// ─── Chunk access ───────────────────────────────────────────────────────────

Chunk* RegionFile::get_chunk(int x, int z) {
    if (x < 0 || x >= kChunkDimX || z < 0 || z >= kChunkDimZ) return nullptr;
    return chunks_[x][z].get();
}

const Chunk* RegionFile::get_chunk(int x, int z) const {
    if (x < 0 || x >= kChunkDimX || z < 0 || z >= kChunkDimZ) return nullptr;
    return chunks_[x][z].get();
}

void RegionFile::set_chunk(int x, int z, std::unique_ptr<Chunk> chunk) {
    if (x < 0 || x >= kChunkDimX || z < 0 || z >= kChunkDimZ) {
        throw std::out_of_range("Chunk coordinates out of range");
    }
    if (chunk) {
        chunk->x_ = x;
        chunk->z_ = z;
        chunk->region_ = this;
        if (!chunks_[x][z]) chunk_count_++;
        chunks_[x][z] = std::move(chunk);
    }
    has_chunk_changes_ = true;
}

std::unique_ptr<Chunk> RegionFile::remove_chunk(int x, int z) {
    if (x < 0 || x >= kChunkDimX || z < 0 || z >= kChunkDimZ) return nullptr;
    auto chunk = std::move(chunks_[x][z]);
    if (chunk) {
        chunk->region_ = nullptr;
        chunk_count_--;
        has_chunk_changes_ = true;
    }
    return chunk;
}

std::vector<Chunk*> RegionFile::all_chunks() {
    std::vector<Chunk*> result;
    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            if (chunks_[x][z]) result.push_back(chunks_[x][z].get());
        }
    }
    return result;
}

std::vector<std::pair<int, int>> RegionFile::get_available_coords(int start_x, int start_z) const {
    std::vector<std::pair<int, int>> result;
    for (int x = start_x; x < kChunkDimX; ++x) {
        int z_begin = (x == start_x) ? start_z : 0;
        for (int z = z_begin; z < kChunkDimZ; ++z) {
            if (!chunks_[x][z]) result.emplace_back(x, z);
        }
    }
    return result;
}

// ─── Saving ─────────────────────────────────────────────────────────────────

void RegionFile::save() {
    if (filepath_.empty()) return;

    // Calculate new layout
    int current_offset_sectors = 2;  // 2 sectors = 8192 bytes for headers
    std::vector<std::pair<int, std::vector<uint8_t>>> chunk_data;
    chunk_data.reserve(kChunkDimX * kChunkDimZ);

    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            auto& chunk = chunks_[x][z];

            // Get chunk bytes
            std::vector<uint8_t> data;
            bool preserve_original = false;
            if (chunk && !chunk->is_corrupt_) {
                data = chunk->save_bytes();
                // If the chunk has not been modified, save_bytes returns the
                // original cached bytes — in that case we keep the original
                // location-table entry too (for bit-identical round-trip).
                preserve_original = !chunk->has_unsaved_changes();
            }

            // Location table entry
            int sector_offset = 0;
            int sector_count = 0;

            if (!data.empty()) {
                if (preserve_original) {
                    // Reuse original sector offset + count (chunk data is
                    // byte-identical, so sectors are the same).
                    int loc = chunk_data_location(x, z);
                    sector_offset = this->chunk_offset(x, z);
                    sector_count = this->chunk_size(x, z);
                } else {
                    sector_offset = current_offset_sectors;
                    sector_count = static_cast<int>((data.size() + kSectorSize - 1) / kSectorSize);
                    current_offset_sectors += sector_count;
                }
                chunk_data.emplace_back(sector_offset, std::move(data));
            } else {
                chunk_data.emplace_back(0, std::vector<uint8_t>());
            }

            // Update header
            int loc = chunk_data_location(x, z);
            if (sector_offset > 0) {
                // Write 3-byte offset + 1-byte size (big-endian)
                locations_[loc + 0] = static_cast<uint8_t>((sector_offset >> 16) & 0xFF);
                locations_[loc + 1] = static_cast<uint8_t>((sector_offset >> 8) & 0xFF);
                locations_[loc + 2] = static_cast<uint8_t>(sector_offset & 0xFF);
                locations_[loc + 3] = static_cast<uint8_t>(sector_count & 0xFF);
            } else {
                locations_[loc + 0] = 0;
                locations_[loc + 1] = 0;
                locations_[loc + 2] = 0;
                locations_[loc + 3] = 0;
            }

            // Update timestamp (only if not preserving original)
            if (preserve_timestamps_ == false) {
                if (chunk && chunk->is_loaded()) {
                    auto now = static_cast<int32_t>(time(nullptr));
                    uint8_t ts[4];
                    // Big-endian timestamp
                    ts[0] = static_cast<uint8_t>((now >> 24) & 0xFF);
                    ts[1] = static_cast<uint8_t>((now >> 16) & 0xFF);
                    ts[2] = static_cast<uint8_t>((now >> 8) & 0xFF);
                    ts[3] = static_cast<uint8_t>(now & 0xFF);
                    std::memcpy(timestamps_ + loc, ts, 4);
                }
            }
        }
    }

    // Write to file
    std::ofstream file(filepath_, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Cannot write region file: " + filepath_);
    }

    // Write headers
    file.write(reinterpret_cast<const char*>(locations_), kSectorSize);
    file.write(reinterpret_cast<const char*>(timestamps_), kSectorSize);

    // Write chunk data (each chunk at its assigned sector offset)
    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            auto& entry = chunk_data[z * kChunkDimX + x];
            if (!entry.second.empty()) {
                int64_t byte_offset = static_cast<int64_t>(entry.first) * kSectorSize;
                file.seekp(byte_offset);
                file.write(reinterpret_cast<const char*>(entry.second.data()),
                           static_cast<std::streamsize>(entry.second.size()));
                // Pad with zeros to sector boundary
                int64_t padding = kSectorSize - (entry.second.size() % kSectorSize);
                if (padding < kSectorSize) {
                    std::vector<uint8_t> zeros(static_cast<size_t>(padding), 0);
                    file.write(reinterpret_cast<const char*>(zeros.data()),
                               static_cast<std::streamsize>(zeros.size()));
                }
            }
        }
    }

    has_chunk_changes_ = false;
}

void RegionFile::save_as(const std::string& filepath) {
    // Before changing filepath_, ensure all chunks are loaded from the current file.
    // Otherwise save_bytes() for unloaded chunks would read from the new filepath,
    // which doesn't exist yet.
    std::string orig_path = filepath_;
    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            auto& chunk = chunks_[x][z];
            if (chunk && !chunk->is_loaded() && !chunk->is_corrupt() && !chunk->is_external()) {
                chunk->load();
            }
        }
    }
    // Now load all chunks using the current filepath, then switch
    filepath_ = orig_path;
    filepath_ = filepath;
    save();
}

bool RegionFile::has_unsaved_changes() const {
    if (has_chunk_changes_) return true;
    for (int z = 0; z < kChunkDimZ; ++z) {
        for (int x = 0; x < kChunkDimX; ++x) {
            if (chunks_[x][z] && chunks_[x][z]->has_unsaved_changes()) return true;
        }
    }
    return false;
}

// ─── Internal helpers ───────────────────────────────────────────────────────

int RegionFile::chunk_data_location(int x, int z) {
    return ((x % kChunkDimX) + (z % kChunkDimZ) * kChunkDimZ) * 4;
}

int RegionFile::chunk_offset(int x, int z) const {
    int loc = chunk_data_location(x, z);
    // 3-byte big-endian offset
    return (static_cast<int>(locations_[loc + 0]) << 16) |
           (static_cast<int>(locations_[loc + 1]) << 8) |
           (static_cast<int>(locations_[loc + 2]));
}

int RegionFile::chunk_size(int x, int z) const {
    int loc = chunk_data_location(x, z);
    return static_cast<int>(locations_[loc + 3]);
}

} // namespace nbtcpp
