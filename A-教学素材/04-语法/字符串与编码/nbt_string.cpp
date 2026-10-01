#include "nbtcpp/tags/nbt_string.h"

namespace nbtcpp {

bool NbtString::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_string();
    return true;
}

void NbtString::skip_tag(NbtBinaryReader& reader) {
    reader.skip_string();
}

void NbtString::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::String);
    writer.write(name());
    writer.write(value_);
}

void NbtString::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtString::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "\"" + value_ + "\"\n";
}

} // namespace nbtcpp
