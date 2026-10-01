#ifndef NBTCPP_TAGS_NBT_CONTAINER_TAG_H
#define NBTCPP_TAGS_NBT_CONTAINER_TAG_H

/**
 * @file nbt_container_tag.h
 * @brief Abstract base class for container tags (NbtCompound and NbtList).
 *
 * Provides the IList<NbtTag> interface shared by both compound and list.
 */

#include "nbtcpp/tags/nbt_tag.h"

#include <cstddef>
#include <vector>

namespace nbtcpp {

/**
 * @brief Abstract base for tags that contain child tags (Compound and List).
 *
 * Implements a common list-like interface:
 * - Add, Insert, Remove, Clear
 * - Index and name-based access
 * - Iteration via begin()/end()
 */
class NbtContainerTag : public NbtTag {
public:
    ~NbtContainerTag() override = default;

    // ─── Container interface ────────────────────────────────────────────

    /** @brief Number of child tags. */
    virtual size_t size() const noexcept = 0;

    /** @brief Whether there are no child tags. */
    virtual bool empty() const noexcept = 0;

    /** @brief Get child tag at index. */
    virtual NbtTag* at(size_t index) = 0;

    /** @brief Get child tag at index (const). */
    virtual const NbtTag* at(size_t index) const = 0;

    /** @brief Add a child tag. The container takes ownership. */
    virtual void add(NbtTagPtr tag) = 0;

    /** @brief Insert a child tag at the given index. */
    virtual void insert(size_t index, NbtTagPtr tag) = 0;

    /** @brief Remove and return a child tag by pointer. Returns nullptr if not found. */
    virtual NbtTagPtr remove(NbtTag* tag) = 0;

    /** @brief Remove and return a child tag at index. */
    virtual NbtTagPtr remove_at(size_t index) = 0;

    /** @brief Remove and return a child tag by name (only valid for NbtCompound). */
    virtual NbtTagPtr remove(const std::string& name) = 0;

    /** @brief Remove all child tags, returning them. */
    virtual std::vector<NbtTagPtr> clear() = 0;

    /** @brief Check whether a tag is valid for adding to this container (always true for Compound, ListType-dependent for List). */
    virtual bool can_add(NbtTagType type) const noexcept = 0;

    /** @brief Recursively collect all descendant tags. */
    virtual std::vector<NbtTag*> get_all_tags();

    /** @brief Recursively collect all descendant tags (const). */
    virtual std::vector<const NbtTag*> get_all_tags() const;

    // ─── Iteration ──────────────────────────────────────────────────────

    /** @brief Iterator access (must be overridden by subclasses). */
    virtual std::vector<NbtTag*>::iterator begin() = 0;
    virtual std::vector<NbtTag*>::iterator end() = 0;
    virtual std::vector<NbtTag*>::const_iterator begin() const = 0;
    virtual std::vector<NbtTag*>::const_iterator end() const = 0;

protected:
    NbtContainerTag() = default;
    explicit NbtContainerTag(std::string name) : NbtTag(std::move(name)) {}
    NbtContainerTag(const NbtContainerTag& other) : NbtTag(other) {}

    /** @brief Adopt a tag (set its parent and change callback). */
    void adopt(NbtTag* tag);
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_CONTAINER_TAG_H
