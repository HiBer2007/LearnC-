#ifndef NBTCPP_TAGS_NBT_BYTE_H
#define NBTCPP_TAGS_NBT_BYTE_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a single byte (TAG_Byte, ID 1).
 *
 * Corresponds to the C# NbtByte class.
 * The value is stored as uint8_t, matching Minecraft's unsigned byte convention.
 */
class NbtByte final : public NbtTag {
public:
    NbtByte() = default;
    explicit NbtByte(uint8_t value) : value_(value) {}
    NbtByte(std::string name, uint8_t value) : NbtTag(std::move(name)), value_(value) {}
    NbtByte(const NbtByte& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Byte; }

    uint8_t value() const noexcept { return value_; }
    void set_value(uint8_t v) { value_ = v; fire_changed(); }
    uint8_t byte_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtByte>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    uint8_t value_ = 0;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_BYTE_H
