#ifndef ENDIAN_HPP
#define ENDIAN_HPP

#include <cstdint>

namespace fallout {

template <typename T>
constexpr uint16_t LoadLE16(const T* b)
{
    static_assert(sizeof(T) == 1, "invalid argument");
    return (static_cast<uint8_t>(b[1]) << 8) | static_cast<uint8_t>(b[0]);
}

template <typename T>
constexpr uint32_t LoadLE32(const T* b)
{
    static_assert(sizeof(T) == 1, "invalid argument");
    return (static_cast<uint8_t>(b[3]) << 24) | (static_cast<uint8_t>(b[2]) << 16) | (static_cast<uint8_t>(b[1]) << 8) | static_cast<uint8_t>(b[0]);
}

} // namespace fallout

#endif /* ENDIAN_HPP */
