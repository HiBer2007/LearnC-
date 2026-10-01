#ifndef NBTCPP_NBT_EXCEPTION_H
#define NBTCPP_NBT_EXCEPTION_H

/**
 * @file nbt_exception.h
 * @brief Exception types for NBT format errors.
 */

#include <stdexcept>
#include <string>

namespace nbtcpp {

/**
 * @brief Exception thrown when malformed NBT data is encountered.
 *
 * Corresponds to the C# NbtFormatException in the fNbt library.
 */
class NbtFormatException : public std::runtime_error {
public:
    explicit NbtFormatException(const std::string& message)
        : std::runtime_error("NBT format error: " + message) {}

    explicit NbtFormatException(const char* message)
        : std::runtime_error(std::string("NBT format error: ") + message) {}
};

/**
 * @brief Exception thrown when an invalid reader state is detected.
 *
 * Corresponds to the C# InvalidReaderStateException.
 */
class InvalidReaderStateException : public std::logic_error {
public:
    explicit InvalidReaderStateException(const std::string& message)
        : std::logic_error("Invalid NBT reader state: " + message) {}

    explicit InvalidReaderStateException(const char* message)
        : std::logic_error(std::string("Invalid NBT reader state: ") + message) {}
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_EXCEPTION_H
