#ifndef NBTCPP_TAGS_NBT_COMPOUND_H
#define NBTCPP_TAGS_NBT_COMPOUND_H

/**
 * @file nbt_compound.h
 * @brief An unordered map of named tags (TAG_Compound, ID 10).
 *
 * This is the root tag type for all NBT files.  Tags inside a compound
 * are identified by unique string names.
 */

#include "nbtcpp/tags/nbt_container_tag.h"
#include <map>
#include <vector>

namespace nbtcpp {

/**
 * @brief A tag containing an unordered map of named child tags.
 *
 * Names must be unique within a compound.  Insertion order is preserved
 * for iteration (via an ordered map + insertion-order tracking).
 *
 * Name access: compound["key"]
 * Index access: compound[0], compound[1], ...
 */
class NbtCompound final : public NbtContainerTag {
public:
    NbtCompound() = default;
    explicit NbtCompound(std::string name) : NbtContainerTag(std::move(name)) {}
    NbtCompound(const NbtCompound& other);

    NbtTagType tag_type() const noexcept override { return NbtTagType::Compound; }

    // ─── Container interface ────────────────────────────────────────────

    size_t size() const noexcept override { return order_.size(); }
    bool empty() const noexcept override { return order_.empty(); }

    NbtTag* at(size_t index) override;
    const NbtTag* at(size_t index) const override;

    void add(NbtTagPtr tag) override;
    void insert(size_t index, NbtTagPtr tag) override;
    NbtTagPtr remove(NbtTag* tag) override;
    NbtTagPtr remove_at(size_t index) override;
    NbtTagPtr remove(const std::string& name) override;
    std::vector<NbtTagPtr> clear() override;
    bool can_add(NbtTagType type) const noexcept override { return true; }

    /** @brief Check whether a tag with the given name exists. */
    bool contains(const std::string& name) const;

    /**
     * @brief Get a child tag by name (const).
     * @return Pointer to the tag, or nullptr if not found.
     */
    const NbtTag* get(const std::string& name) const;

    /**
     * @brief Get a child tag by name.
     * @return Pointer to the tag, or nullptr if not found.
     */
    NbtTag* get(const std::string& name);

    /**
     * @brief Get a child tag by name with type safe cast.
     */
    template<typename T>
    const T* get_as(const std::string& name) const {
        auto* tag = get(name);
        return (tag && tag->tag_type() == T().tag_type())
                   ? static_cast<const T*>(tag) : nullptr;
    }

    template<typename T>
    T* get_as(const std::string& name) {
        auto* tag = get(name);
        return (tag && tag->tag_type() == T().tag_type())
                   ? static_cast<T*>(tag) : nullptr;
    }

    /** @brief Get all tag names in insertion order. */
    std::vector<std::string> names() const;

    /** @brief Const iteration over the sorted name→tag map (alphabetical key order). */
    using SortedIterator = std::map<std::string, NbtTagPtr>::const_iterator;
    SortedIterator sorted_begin() const { return tags_.begin(); }
    SortedIterator sorted_end()   const { return tags_.end(); }

    // ─── Indexer ────────────────────────────────────────────────────────

    NbtTag* operator[](const std::string& name) override;
    const NbtTag* operator[](const std::string& name) const override;

    // ─── Iteration ──────────────────────────────────────────────────────

    std::vector<NbtTag*>::iterator begin() override;
    std::vector<NbtTag*>::iterator end() override;
    std::vector<NbtTag*>::const_iterator begin() const override;
    std::vector<NbtTag*>::const_iterator end() const override;

    // ─── Serialization ──────────────────────────────────────────────────

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string,
                      int indent_level) const override;

    std::unique_ptr<NbtTag> clone() const override;

    /** @brief Factory: create an empty tag of the given type (used during parsing). */
    static std::unique_ptr<NbtTag> create_tag(NbtTagType type);

private:
    std::map<std::string, NbtTagPtr> tags_;
    std::vector<NbtTag*> order_;   // insertion-order tracking
    std::vector<NbtTagPtr> orphans_;  // temporary storage during removal

    void do_add(NbtTagPtr tag);
    void rebuild_order();
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_COMPOUND_H
