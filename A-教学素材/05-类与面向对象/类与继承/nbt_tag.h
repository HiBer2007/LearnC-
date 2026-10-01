#ifndef NBTCPP_TAGS_NBT_TAG_H
#define NBTCPP_TAGS_NBT_TAG_H

/**
 * @file nbt_tag.h
 * @brief Abstract base class for all NBT tag types.
 *
 * The NBT tag hierarchy mirrors the Minecraft NBT specification:
 *
 * ```
 * NbtTag (abstract)
 * ├── NbtContainerTag (abstract, implements IList<NbtTag>)
 * │   ├── NbtCompound   — map of named tags
 * │   └── NbtList       — ordered list of unnamed tags
 * ├── NbtByte           — single byte
 * ├── NbtShort          — 16-bit integer
 * ├── NbtInt            — 32-bit integer
 * ├── NbtLong           — 64-bit integer
 * ├── NbtFloat          — 32-bit float
 * ├── NbtDouble         — 64-bit double
 * ├── NbtString         — UTF-8 string
 * ├── NbtByteArray      — byte array
 * ├── NbtIntArray       — 32-bit integer array
 * └── NbtLongArray      — 64-bit integer array
 * ```
 */

#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/nbt_binary_reader.h"
#include "nbtcpp/nbt_binary_writer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace nbtcpp {

// Forward declarations
class NbtContainerTag;

/**
 * @brief Base class for all NBT tag types.
 *
 * Each tag has a:
 * - Type (NbtTagType enum)
 * - Name (nullable; null for list elements, empty string for root)
 * - Parent (pointer to containing NbtContainerTag)
 *
 * Tags support:
 * - Cloning via clone()
 * - Binary serialization via read_tag()/write_tag()
 * - Change notification via change_callback
 */
class NbtTag {
public:
    /// @brief Virtual destructor.
    virtual ~NbtTag() = default;

    // ─── Type ───────────────────────────────────────────────────────────

    /** @brief Return the concrete NbtTagType of this tag. */
    virtual NbtTagType tag_type() const noexcept = 0;

    /** @brief Whether this tag type carries a scalar value. */
    bool has_value() const noexcept {
        return nbtcpp::has_value(tag_type());
    }

    // ─── Name ───────────────────────────────────────────────────────────

    /** @brief Get the tag name (may be empty for unnamed tags). */
    const std::string& name() const noexcept { return name_; }

    /**
     * @brief Set the tag name.
     *
     * If this tag is inside an NbtCompound, the compound's key table is
     * also updated (renaming the entry).
     */
    virtual void set_name(const std::string& name);

    // ─── Parent ─────────────────────────────────────────────────────────

    /** @brief Get the parent container, or nullptr if this is a root tag. */
    NbtContainerTag* parent() noexcept { return parent_; }

    /** @brief Get the parent container (const). */
    const NbtContainerTag* parent() const noexcept { return parent_; }

    /**
     * @brief Get the full dotted path from the root.
     *
     * Examples:
     *   - Root compound → ""
     *   - Child "foo" → "foo"
     *   - Nested "bar" inside "foo" → "foo.bar"
     *   - List element at index 2 → "parent[2]"
     */
    std::string path() const;

    // ─── Change notification ────────────────────────────────────────────

    /** @brief Callback type for change events. */
    using ChangeCallback = std::function<void(NbtTag* tag)>;

    /** @brief Register a change listener (called when this tag or any child changes). */
    void set_change_callback(ChangeCallback cb) { change_cb_ = std::move(cb); }

    /** @brief Get the current change callback. */
    const ChangeCallback& change_callback() const noexcept { return change_cb_; }

    // ─── Clone ──────────────────────────────────────────────────────────

    /** @brief Deep-clone this tag. */
    virtual std::unique_ptr<NbtTag> clone() const = 0;

    // ─── Serialization (internal) ───────────────────────────────────────

    /**
     * @brief Read tag payload from a binary stream.
     *
     * @param reader  The binary reader.
     * @return true if the tag was actually read; false if it was skipped by
     *         the TagSelector.
     */
    virtual bool read_tag(NbtBinaryReader& reader) = 0;

    /**
     * @brief Skip the tag payload in a binary stream without constructing it.
     */
    virtual void skip_tag(NbtBinaryReader& reader) = 0;

    /**
     * @brief Write the complete tag (type byte + name + data) to a binary stream.
     */
    virtual void write_tag(NbtBinaryWriter& writer) const = 0;

    /**
     * @brief Write only the tag data (no type byte, no name) to a binary stream.
     */
    virtual void write_data(NbtBinaryWriter& writer) const = 0;

    // ─── Value accessors (convenience, throw if wrong type) ─────────────

    /** @brief Get the byte value (only for NbtByte). */
    virtual uint8_t byte_value() const;
    /** @brief Get the short value (only for NbtShort/NbtByte). */
    virtual int16_t short_value() const;
    /** @brief Get the int value (only for NbtInt). */
    virtual int32_t int_value() const;
    /** @brief Get the long value (only for NbtLong). */
    virtual int64_t long_value() const;
    /** @brief Get the float value (only for NbtFloat). */
    virtual float float_value() const;
    /** @brief Get the double value (only for NbtDouble). */
    virtual double double_value() const;
    /** @brief Get the string value (only for NbtString). */
    virtual const std::string& string_value() const;

    // ─── Utility ────────────────────────────────────────────────────────

    /** @brief Convert to a human-readable string representation. */
    virtual std::string to_string() const;

    /** @brief Convert to a human-readable string with custom indentation. */
    virtual std::string to_string(const std::string& indent) const;

    /** @brief Pretty-print to a string builder. */
    virtual void pretty_print(std::string& out, const std::string& indent_string,
                              int indent_level) const;

    // ─── Indexers (convenience for NbtCompound/NbtList) ─────────────────

    /** @brief Access child by name (only valid for NbtCompound). */
    virtual NbtTag* operator[](const std::string& name);

    /** @brief Access child by index (only valid for NbtContainerTag). */
    virtual NbtTag* operator[](int index);

    /** @brief Const version of operator[]. */
    virtual const NbtTag* operator[](const std::string& name) const;

    /** @brief Const version of operator[]. */
    virtual const NbtTag* operator[](int index) const;

protected:
    NbtTag() = default;

    /** @brief Construct with a name. */
    explicit NbtTag(std::string name) : name_(std::move(name)) {}

    /** @brief Copy constructor (does NOT copy parent). */
    NbtTag(const NbtTag& other) : name_(other.name_) {}

public:
    /** @brief Set parent pointer (called by container classes). */
    void set_parent(NbtContainerTag* parent) noexcept { parent_ = parent; }

protected:
    /** @brief Fire change notification and propagate upward. */
    void fire_changed();

    std::string name_;
    NbtContainerTag* parent_ = nullptr;
    ChangeCallback change_cb_;

    friend class NbtContainerTag;
};

/** @brief Shared/unique pointer aliases. */
using NbtTagPtr = std::unique_ptr<NbtTag>;

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_TAG_H
