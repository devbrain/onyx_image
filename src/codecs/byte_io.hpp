#pragma once

#include <cstdint>

namespace onyx_image {

// Little-endian readers
inline std::uint16_t read_le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           (static_cast<std::uint16_t>(p[1]) << 8);
}

inline std::uint32_t read_le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::int32_t read_le32_signed(const std::uint8_t* p) {
    return static_cast<std::int32_t>(read_le32(p));
}

// Big-endian readers
inline std::uint16_t read_be16(const std::uint8_t* p) {
    return (static_cast<std::uint16_t>(p[0]) << 8) |
           static_cast<std::uint16_t>(p[1]);
}

inline std::uint32_t read_be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

} // namespace onyx_image
