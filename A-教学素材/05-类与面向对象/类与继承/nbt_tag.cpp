/**
 * @file nbt_tag.cpp
 * @brief Implementation of NbtTag base class.
 */

#include "nbtcpp/tags/nbt_tag.h"
#include "nbtcpp/tags/nbt_container_tag.h"
#include "nbtcpp/tags/nbt_list.h"     // for dynamic_cast in path()

#include <sstream>

namespace nbtcpp {

// ─── Name ────────────────────────────────────────────────────────────────────

void NbtTag::set_name(const std::string& name) {
    name_ = name;
    fire_changed();
}

// ─── Path ────────────────────────────────────────────────────────────────────

std::string NbtTag::path() const {
    if (!parent_) return name_.empty() ? "" : name_;

    auto* parent_as_list = dynamic_cast<NbtList*>(parent_);
    if (parent_as_list) {
        for (size_t i = 0; i < parent_as_list->size(); ++i) {
            if ((*parent_as_list)[i] == this) {
                return parent_as_list->path() + "[" + std::to_string(i) + "]";
            }
        }
        return parent_as_list->path() + "[?]";
    }

    auto parent_path = parent_->path();
    if (parent_path.empty()) return name_;
    return parent_path + "." + name_;
}

// ─── Value accessors (base: throw) ───────────────────────────────────────────

uint8_t NbtTag::byte_value() const {
    throw std::bad_cast();
}
int16_t NbtTag::short_value() const {
    throw std::bad_cast();
}
int32_t NbtTag::int_value() const {
    throw std::bad_cast();
}
int64_t NbtTag::long_value() const {
    throw std::bad_cast();
}
float NbtTag::float_value() const {
    throw std::bad_cast();
}
double NbtTag::double_value() const {
    throw std::bad_cast();
}
const std::string& NbtTag::string_value() const {
    throw std::bad_cast();
}

// ─── Change notification ─────────────────────────────────────────────────────

void NbtTag::fire_changed() {
    if (change_cb_) change_cb_(this);
    if (parent_) parent_->fire_changed();
}

// ─── String representation ───────────────────────────────────────────────────

std::string NbtTag::to_string() const {
    std::string out;
    pretty_print(out, "  ", 0);
    return out;
}

std::string NbtTag::to_string(const std::string& indent) const {
    std::string out;
    pretty_print(out, indent, 0);
    return out;
}

void NbtTag::pretty_print(std::string& out, const std::string& indent_string,
                          int indent_level) const {
    for (int i = 0; i < indent_level; ++i) out += indent_string;
    if (!name_.empty()) out += name_ + ": ";
    out += "?";
}

// ─── Indexers ────────────────────────────────────────────────────────────────

NbtTag* NbtTag::operator[](const std::string&) { return nullptr; }
const NbtTag* NbtTag::operator[](const std::string&) const { return nullptr; }
NbtTag* NbtTag::operator[](int) { return nullptr; }
const NbtTag* NbtTag::operator[](int) const { return nullptr; }

} // namespace nbtcpp
