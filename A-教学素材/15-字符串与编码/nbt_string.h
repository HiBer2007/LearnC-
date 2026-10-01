#ifndef NBTCPP_TAGS_NBT_STRING_H
#define NBTCPP_TAGS_NBT_STRING_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a UTF-8 string (TAG_String, ID 8).
 *
 * The string is stored as std::string (UTF-8).  On the wire it is
 * length-prefixed: [int16 byte_count][UTF-8 bytes].
 */
class NbtString final : public NbtTag {
public:
    NbtString() = default;
    explicit NbtString(std::string value) : value_(std::move(value)) {}
    NbtString(std::string name, std::string value)
        : NbtTag(std::move(name)), value_(std::move(value)) {}
    NbtString(const NbtString& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::String; }

    const std::string& value() const noexcept { return value_; }
    void set_value(const std::string& v) { value_ = v; fire_changed(); }
    const std::string& string_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtString>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    std::string value_;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_STRING_H
