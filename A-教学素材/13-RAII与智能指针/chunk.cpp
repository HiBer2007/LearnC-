/**
 * @file chunk.cpp
 * @brief Implementation of Chunk.
 */

#include "nbtcpp/chunk.h"
#include "nbtcpp/region_file.h"
#include "nbtcpp/nbt_file.h"
#include "nbtcpp/nbt_binary_writer.h"
#include "nbtcpp/nbt_exception.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <zlib.h>
#include <vector>

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

Chunk::Chunk(RegionFile* region, int x, int z)
    : region_(region), x_(x), z_(z)
{
}

std::unique_ptr<Chunk> Chunk::create_empty(std::shared_ptr<NbtCompound> data,
                                            int x, int z) {
    auto chunk = std::make_unique<Chunk>(nullptr, x, z);
    chunk->set_data(data ? std::move(data) : std::make_shared<NbtCompound>(""));
    chunk->has_unsaved_changes_ = true;
    return chunk;
}

void Chunk::set_data(std::shared_ptr<NbtCompound> data) {
    data_ = std::move(data);
    // Subscribe to change notifications
    if (data_) {
        data_->set_change_callback([this](NbtTag*) {
            has_unsaved_changes_ = true;
        });
    }
}

// ─── Loading ─────────────────────────────────────────────────────────────────

void Chunk::load() {
    if (is_corrupt_ || is_external_ || !region_) return;

    std::ifstream file(region_->filepath(), std::ios::binary);
    if (!file) {
        is_corrupt_ = true;
        return;
    }

    // Seek to chunk data (offset + 4 to skip the length prefix)
    int data_offset = offset_ + 4;
    file.seekg(data_offset);
    if (!file) {
        is_corrupt_ = true;
        return;
    }

    int compression_byte = file.get();
    if (compression_byte == std::char_traits<char>::eof()) {
        is_corrupt_ = true;
        return;
    }

    if (compression_byte & (1 << 7)) {
        // External chunk (bit 7 set)
        is_external_ = true;
        compression_type_ = static_cast<uint8_t>(compression_byte);
        return;
    }

    // Read rest of data
    int remaining = size_ - 5;  // subtract 4-byte length + 1-byte compression
    if (remaining <= 0) {
        is_corrupt_ = true;
        return;
    }

    std::vector<uint8_t> chunk_data(static_cast<size_t>(remaining));
    file.read(reinterpret_cast<char*>(chunk_data.data()), remaining);
    auto got = static_cast<size_t>(file.gcount());
    if (got < static_cast<size_t>(remaining)) {
        chunk_data.resize(got);
    }

    // Parse NBT from chunk data
    try {
        // Map compression byte to NbtCompression enum
        NbtCompression chunk_compression;
        switch (compression_byte) {
            case 1:  chunk_compression = NbtCompression::GZip; break;
            case 2:  chunk_compression = NbtCompression::ZLib; break;
            case 3:  chunk_compression = NbtCompression::None; break;
            default: throw NbtFormatException("Unknown chunk compression: " +
                                               std::to_string(compression_byte));
        }

        // Cache the original raw chunk data (5-byte header + compressed payload)
        // so that save_bytes() can return it as-is for unmodified chunks.
        // This ensures bit-identical round-trip data.
        original_data_.clear();
        // Read the full chunk payload from the file: 4-byte length + (1-byte
        // compression + compressed data).  The length field tells us the
        // compressed payload size that follows it.
        file.clear();
        file.seekg(offset_);
        uint8_t len_be[4];
        if (file.read(reinterpret_cast<char*>(len_be), 4)) {
            int32_t payload_len = (static_cast<int32_t>(len_be[0]) << 24) |
                                  (static_cast<int32_t>(len_be[1]) << 16) |
                                  (static_cast<int32_t>(len_be[2]) << 8)  |
                                  (static_cast<int32_t>(len_be[3]));
            // Total = 4 (length field) + payload_len (compression + data)
            int32_t total = 4 + payload_len;
            if (total > 0 && total <= size_) {
                original_data_.resize(static_cast<size_t>(total));
                file.seekg(offset_);
                file.read(reinterpret_cast<char*>(original_data_.data()),
                          static_cast<std::streamsize>(total));
            }
        }

        // Wrap in a stream (membuf is non-seekable, so pass compression explicitly)
        struct membuf : std::streambuf {
            membuf(uint8_t* data, size_t len) {
                setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data),
                     reinterpret_cast<char*>(data) + static_cast<std::streamsize>(len));
            }
        };
        membuf sb(chunk_data.data(), chunk_data.size());
        std::istream data_stream(&sb);

        NbtFile nbt_file;
        nbt_file.load_from_stream(data_stream, chunk_compression);
        // Keep the Minecraft region-format compression byte (1=GZip, 2=ZLib, 3=None)
        // rather than the NbtCompression enum value
        // compression_type_ stays as the original compression_byte

        auto root = nbt_file.root_tag_as<NbtCompound>();
        if (!root) {
            throw NbtFormatException("Chunk root is not a compound");
        }
        set_data(root);
    } catch (const std::exception&) {
        is_corrupt_ = true;
    }
}

// ─── Serialization ───────────────────────────────────────────────────────────

std::vector<uint8_t> Chunk::save_bytes() {
    // 5-byte header: [4 bytes length][1 byte compression type]
    std::vector<uint8_t> result;

    if (is_external_) {
        result.resize(static_cast<size_t>(size_ + 5));
        // Length = 1 (just the compression byte)
        uint8_t len_be[4];
        int32_t len = 1;
        len_be[0] = static_cast<uint8_t>((len >> 24) & 0xFF);
        len_be[1] = static_cast<uint8_t>((len >> 16) & 0xFF);
        len_be[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
        len_be[3] = static_cast<uint8_t>(len & 0xFF);
        std::memcpy(result.data(), len_be, 4);
        result[4] = static_cast<uint8_t>(compression_type_);
        return result;
    }

    if (!is_loaded()) {
        // Not loaded: read raw bytes from region file
        if (region_) {
            std::ifstream file(region_->filepath(), std::ios::binary);
            if (file) {
                result.resize(static_cast<size_t>(size_));
                file.seekg(offset_);
                file.read(reinterpret_cast<char*>(result.data()),
                          static_cast<std::streamsize>(size_));
            }
        }
        return result;
    }

    if (is_corrupt_) return {};  // return empty for corrupt

    // For unmodified chunks with cached original data, return the original
    // compressed bytes as-is. This avoids re-compression differences and
    // enables bit-identical round-trips.
    if (!has_unsaved_changes_ && !original_data_.empty()) {
        return original_data_;
    }

    // Serialize the NBT compound directly (chunk roots are unnamed)
    std::ostringstream nbt_stream(std::ios::binary);
    NbtBinaryWriter writer(nbt_stream, true);  // big-endian
    data_->write_tag(writer);
    auto raw_nbt = nbt_stream.str();

    // Choose compression type (Minecraft region file format:
    // 1=GZip, 2=ZLib, 3=uncompressed)
    uint8_t minecraft_compression = compression_type_;
    NbtCompression compression = NbtCompression::ZLib;
    if (compression_type_ == 1) {
        compression = NbtCompression::GZip;
    }

    // Compress raw NBT data using zlib directly
    // (We bypass NbtFile because chunk roots have empty names, which NbtFile rejects)
    std::vector<uint8_t> compressed;
    {
        int window_bits = (compression == NbtCompression::GZip) ? (15 | 16) : 15;

        z_stream zs = {};
        if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         window_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            throw std::runtime_error("deflateInit2 failed");

        zs.avail_in = static_cast<uInt>(raw_nbt.size());
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(raw_nbt.data()));

        std::vector<uint8_t> buf(16384);
        int ret;
        do {
            zs.avail_out = static_cast<uInt>(buf.size());
            zs.next_out = buf.data();
            ret = deflate(&zs, Z_FINISH);
            auto have = buf.size() - zs.avail_out;
            compressed.insert(compressed.end(), buf.data(), buf.data() + have);
        } while (ret != Z_STREAM_END);

        deflateEnd(&zs);
    }

    // Build with 5-byte header
    result.resize(compressed.size() + 5);
    int32_t data_len = static_cast<int32_t>(compressed.size());
    result[0] = static_cast<uint8_t>((data_len >> 24) & 0xFF);
    result[1] = static_cast<uint8_t>((data_len >> 16) & 0xFF);
    result[2] = static_cast<uint8_t>((data_len >> 8) & 0xFF);
    result[3] = static_cast<uint8_t>(data_len & 0xFF);
    result[4] = minecraft_compression;  // 1=GZip, 2=ZLib
    std::memcpy(result.data() + 5, compressed.data(), compressed.size());

    has_unsaved_changes_ = false;
    return result;
}

void Chunk::remove() {
    if (region_) region_->remove_chunk(x_, z_);
}

} // namespace nbtcpp
