/**
 * @file nbt_binary_writer.cpp
 * @brief Implementation of NbtBinaryWriter.
 */

#include "nbtcpp/nbt_binary_writer.h"
#include "nbtcpp/nbt_exception.h"

#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace nbtcpp {

// ─── Construction ─────────────────────────────────────────────────────────────

NbtBinaryWriter::NbtBinaryWriter(std::ostream& stream, bool big_endian) noexcept
    : stream_(stream)
    , conv_(big_endian)
{
}

// ─── Write primitives ────────────────────────────────────────────────────────

void NbtBinaryWriter::write(uint8_t value) {
    stream_.put(static_cast<char>(value));
    bytes_written_ += 1;
}

void NbtBinaryWriter::write(NbtTagType type) {
    write(static_cast<uint8_t>(type));
}

void NbtBinaryWriter::write(int16_t value) {
    int16_t net = conv_.convert(value);
    write_buf(reinterpret_cast<const uint8_t*>(&net), 2);
}

void NbtBinaryWriter::write(int32_t value) {
    int32_t net = conv_.convert(value);
    write_buf(reinterpret_cast<const uint8_t*>(&net), 4);
}

void NbtBinaryWriter::write(int64_t value) {
    int64_t net = conv_.convert(value);
    write_buf(reinterpret_cast<const uint8_t*>(&net), 8);
}

void NbtBinaryWriter::write(float value) {
    float net = conv_.convert(value);
    write_buf(reinterpret_cast<const uint8_t*>(&net), 4);
}

void NbtBinaryWriter::write(double value) {
    double net = conv_.convert(value);
    write_buf(reinterpret_cast<const uint8_t*>(&net), 8);
}

void NbtBinaryWriter::write(const std::string& value) {
    // Write length as int16 (number of UTF-8 bytes)
    auto byte_count = static_cast<int16_t>(value.size());
    write(byte_count);
    write_raw(reinterpret_cast<const uint8_t*>(value.data()), byte_count);
}

void NbtBinaryWriter::write_raw(const uint8_t* data, int32_t count) {
    if (count <= 0) return;
    constexpr int32_t kMaxChunk = 4 * 1024 * 1024;  // 4 MiB
    int32_t offset = 0;
    while (offset < count) {
        int32_t chunk = std::min(count - offset, kMaxChunk);
        stream_.write(reinterpret_cast<const char*>(data + offset), chunk);
        bytes_written_ += chunk;
        offset += chunk;
    }
}

// ─── Stream access ───────────────────────────────────────────────────────────

std::ostream& NbtBinaryWriter::stream() {
    stream_.flush();
    return stream_;
}

void NbtBinaryWriter::flush() {
    stream_.flush();
}

// ─── Internal helpers ────────────────────────────────────────────────────────

void NbtBinaryWriter::write_buf(const uint8_t* buf, int count) {
    stream_.write(reinterpret_cast<const char*>(buf), count);
    bytes_written_ += count;
}

} // namespace nbtcpp
