#ifndef NBTCPP_TAGS_NBT_LIST_H
#define NBTCPP_TAGS_NBT_LIST_H

/**
 * @file nbt_list.h
 * @brief An ordered list of unnamed tags, all of the same type (TAG_List, ID 9).
 */

#include "nbtcpp/tags/nbt_container_tag.h"
#include <vector>

namespace nbtcpp {

/**
 * @brief A tag containing an ordered list of unnamed tags, all of the same type.
 *
 * The list type (ListType) is inferred from the first added element.  All
 * subsequent elements must match.  An empty list may have ListType = Unknown
 * or End.
 *
 * Index access: list[0], list[1], ...
 */
class NbtList final : public NbtContainerTag {
public:
    NbtList() = default;
    explicit NbtList(std::string name) : NbtContainerTag(std::move(name)) {}
    explicit NbtList(NbtTagType list_type) : list_type_(list_type) {}
    NbtList(std::string name, NbtTagType list_type)
        : NbtContainerTag(std::move(name)), list_type_(list_type) {}
    NbtList(const NbtList& other);

    NbtTagType tag_type() const noexcept override { return NbtTagType::List; }

    // ─── List type ──────────────────────────────────────────────────────

    /** @brief Get the element type of this list. */
    NbtTagType list_type() const noexcept { return list_type_; }

    /**
     * @brief Set the list type.
     *
     * @throws std::invalid_argument if the type doesn't match existing elements.
     */
    void set_list_type(NbtTagType type);

    // ─── Container interface ────────────────────────────────────────────

    size_t size() const noexcept override { return tags_.size(); }
    bool empty() const noexcept override { return tags_.empty(); }

    NbtTag* at(size_t index) override;
    const NbtTag* at(size_t index) const override;

    void add(NbtTagPtr tag) override;
    void insert(size_t index, NbtTagPtr tag) override;
    NbtTagPtr remove(NbtTag* tag) override;
    NbtTagPtr remove_at(size_t index) override;
    NbtTagPtr remove(const std::string& name) override;
    std::vector<NbtTagPtr> clear() override;
    bool can_add(NbtTagType type) const noexcept override;

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

private:
    NbtTagType list_type_ = NbtTagType::Unknown;
    std::vector<NbtTagPtr> tags_;
    std::vector<NbtTag*> raw_ptrs_;  // for O(1) iteration

    void rebuild_raw_ptrs();
    void check_type(NbtTagType type) const;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_LIST_H
