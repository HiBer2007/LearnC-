/**
 * @file nbt_compound.cpp
 * @brief Implementation of NbtCompound.
 */

#include "nbtcpp/tags/nbt_compound.h"
#include "nbtcpp/tags/nbt_byte.h"
#include "nbtcpp/tags/nbt_short.h"
#include "nbtcpp/tags/nbt_int.h"
#include "nbtcpp/tags/nbt_long.h"
#include "nbtcpp/tags/nbt_float.h"
#include "nbtcpp/tags/nbt_double.h"
#include "nbtcpp/tags/nbt_string.h"
#include "nbtcpp/tags/nbt_byte_array.h"
#include "nbtcpp/tags/nbt_int_array.h"
#include "nbtcpp/tags/nbt_long_array.h"
#include "nbtcpp/tags/nbt_list.h"
#include "nbtcpp/nbt_exception.h"
#include <algorithm>
#include <sstream>

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

NbtCompound::NbtCompound(const NbtCompound& other)
    : NbtContainerTag(other)
{
    for (const auto& [key, tag] : other.tags_) {
        auto clone = tag->clone();
        adopt(clone.get());
        tags_[key] = std::move(clone);
        order_.push_back(tags_[key].get());
    }
}

// ─── Container interface ─────────────────────────────────────────────────────

NbtTag* NbtCompound::at(size_t index) {
    if (index >= order_.size()) throw std::out_of_range("NbtCompound index out of range");
    return order_[index];
}

const NbtTag* NbtCompound::at(size_t index) const {
    if (index >= order_.size()) throw std::out_of_range("NbtCompound index out of range");
    return order_[index];
}

void NbtCompound::add(NbtTagPtr tag) {
    do_add(std::move(tag));
    fire_changed();
}

void NbtCompound::insert(size_t index, NbtTagPtr tag) {
    if (!tag) throw std::invalid_argument("Cannot insert null tag into NbtCompound");
    if (tag->name().empty())
        throw std::invalid_argument("Tags added to NbtCompound must have a non-empty name");
    if (tag->parent())
        throw std::invalid_argument("Tag already has a parent");

    const auto& name = tag->name();
    if (tags_.find(name) != tags_.end())
        throw std::invalid_argument("Duplicate tag name in NbtCompound: " + name);

    adopt(tag.get());
    tags_[name] = std::move(tag);
    order_.insert(order_.begin() + static_cast<ptrdiff_t>(index), tags_[name].get());
    fire_changed();
}

NbtTagPtr NbtCompound::remove(NbtTag* tag) {
    if (!tag) return nullptr;
    auto it = tags_.find(tag->name());
    if (it == tags_.end() || it->second.get() != tag) return nullptr;

    auto ptr = std::move(it->second);
    tags_.erase(it);
    auto oit = std::find(order_.begin(), order_.end(), ptr.get());
    if (oit != order_.end()) order_.erase(oit);
    ptr->set_parent(nullptr);
    fire_changed();
    return ptr;
}

NbtTagPtr NbtCompound::remove_at(size_t index) {
    if (index >= order_.size()) throw std::out_of_range("NbtCompound remove_at index out of range");
    auto* tag = order_[index];
    auto ptr = std::move(tags_[tag->name()]);
    tags_.erase(tag->name());
    order_.erase(order_.begin() + static_cast<ptrdiff_t>(index));
    ptr->set_parent(nullptr);
    fire_changed();
    return ptr;
}

NbtTagPtr NbtCompound::remove(const std::string& name) {
    auto it = tags_.find(name);
    if (it == tags_.end()) return nullptr;

    auto ptr = std::move(it->second);
    tags_.erase(it);
    auto oit = std::find(order_.begin(), order_.end(), ptr.get());
    if (oit != order_.end()) order_.erase(oit);
    ptr->set_parent(nullptr);
    fire_changed();
    return ptr;
}

std::vector<NbtTagPtr> NbtCompound::clear() {
    std::vector<NbtTagPtr> result;
    for (auto& [key, tag] : tags_) {
        tag->set_parent(nullptr);
        result.push_back(std::move(tag));
    }
    tags_.clear();
    order_.clear();
    fire_changed();
    return result;
}

// ─── Lookup ──────────────────────────────────────────────────────────────────

bool NbtCompound::contains(const std::string& name) const {
    return tags_.find(name) != tags_.end();
}

const NbtTag* NbtCompound::get(const std::string& name) const {
    auto it = tags_.find(name);
    return (it != tags_.end()) ? it->second.get() : nullptr;
}

NbtTag* NbtCompound::get(const std::string& name) {
    auto it = tags_.find(name);
    return (it != tags_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> NbtCompound::names() const {
    std::vector<std::string> result;
    for (auto* tag : order_) result.push_back(tag->name());
    return result;
}

// ─── Indexers ────────────────────────────────────────────────────────────────

NbtTag* NbtCompound::operator[](const std::string& name) {
    return get(name);
}

const NbtTag* NbtCompound::operator[](const std::string& name) const {
    return get(name);
}

// ─── Internal ────────────────────────────────────────────────────────────────

void NbtCompound::do_add(NbtTagPtr tag) {
    if (!tag) throw std::invalid_argument("Cannot add null tag to NbtCompound");
    if (tag->name().empty())
        throw std::invalid_argument("Tags added to NbtCompound must have a name");

    const auto& name = tag->name();
    if (tags_.find(name) != tags_.end())
        throw std::invalid_argument("Duplicate tag name in NbtCompound: " + name);

    adopt(tag.get());
    order_.push_back(tag.get());
    tags_[name] = std::move(tag);
}

void NbtCompound::rebuild_order() {
    order_.clear();
    for (auto& [key, tag] : tags_) order_.push_back(tag.get());
}

// ─── Iteration ───────────────────────────────────────────────────────────────

std::vector<NbtTag*>::iterator NbtCompound::begin() { return order_.begin(); }
std::vector<NbtTag*>::iterator NbtCompound::end()   { return order_.end(); }
std::vector<NbtTag*>::const_iterator NbtCompound::begin() const { return order_.begin(); }
std::vector<NbtTag*>::const_iterator NbtCompound::end() const   { return order_.end(); }

// ─── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<NbtTag> NbtCompound::create_tag(NbtTagType type) {
    switch (type) {
        case NbtTagType::Byte:      return std::make_unique<NbtByte>();
        case NbtTagType::Short:     return std::make_unique<NbtShort>();
        case NbtTagType::Int:       return std::make_unique<NbtInt>();
        case NbtTagType::Long:      return std::make_unique<NbtLong>();
        case NbtTagType::Float:     return std::make_unique<NbtFloat>();
        case NbtTagType::Double:    return std::make_unique<NbtDouble>();
        case NbtTagType::ByteArray: return std::make_unique<NbtByteArray>();
        case NbtTagType::String:    return std::make_unique<NbtString>();
        case NbtTagType::List:      return std::make_unique<NbtList>();
        case NbtTagType::Compound:  return std::make_unique<NbtCompound>();
        case NbtTagType::IntArray:  return std::make_unique<NbtIntArray>();
        case NbtTagType::LongArray: return std::make_unique<NbtLongArray>();
        default:
            throw NbtFormatException("Unsupported tag type: " + std::to_string(static_cast<int>(type)));
    }
}

// ─── Serialization ───────────────────────────────────────────────────────────

bool NbtCompound::read_tag(NbtBinaryReader& reader) {
    while (true) {
        NbtTagType next_type = reader.read_tag_type();
        if (next_type == NbtTagType::End) return true;

        auto tag = create_tag(next_type);
        std::string tag_name = reader.read_string();
        tag->set_name(tag_name);
        adopt(tag.get());

        bool included = tag->read_tag(reader);
        if (included) {
            const auto& name = tag->name();
            order_.push_back(tag.get());
            tags_[name] = std::move(tag);
        }
        // If excluded by tag selector, tag is simply dropped
    }
}

void NbtCompound::skip_tag(NbtBinaryReader& reader) {
    while (true) {
        NbtTagType next_type = reader.read_tag_type();
        if (next_type == NbtTagType::End) return;

        auto tag = create_tag(next_type);
        std::string tag_name = reader.read_string();  // skip name
        tag->skip_tag(reader);
    }
}

void NbtCompound::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Compound);
    writer.write(name());
    write_data(writer);
}

void NbtCompound::write_data(NbtBinaryWriter& writer) const {
    for (const auto& [key, tag] : tags_) {
        tag->write_tag(writer);
    }
    writer.write(NbtTagType::End);
}

void NbtCompound::pretty_print(std::string& out, const std::string& indent,
                               int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "{\n";
    for (auto* tag : order_) {
        tag->pretty_print(out, indent, level + 1);
    }
    for (int i = 0; i < level; ++i) out += indent;
    out += "}\n";
}

std::unique_ptr<NbtTag> NbtCompound::clone() const {
    return std::make_unique<NbtCompound>(*this);
}

} // namespace nbtcpp
