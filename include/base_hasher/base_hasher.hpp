#pragma once

#include <cstdint>
#include <cstring>


namespace mphf::base_hasher {

struct BaseHasher {
    virtual uint64_t operator()(const void* key, size_t len, uint64_t seed = 0) const = 0;
};

/**
 * Reverse bytes in big-endian architectures
 */
template <typename T>
inline T read_unaligned(const void* buf) {
    static_assert(sizeof(T) == __WORDSIZE / 8,
                  "read_unaligned is not defined for the currentd data type");
    T ret;
    memcpy(&ret, buf, sizeof(T));
    return ret;
}

/**
 * Reverse bytes in big-endian architectures
 */
template <typename T>
inline T read_unaligned(const T buf) {
    static_assert(sizeof(T) == __WORDSIZE / 8,
                  "read_unaligned is not defined for the currentd data type");
    T ret;
    memcpy(&ret, static_cast<const void*>(&buf), sizeof(T));
    return ret;
}

}  // namespace mphf::base_hasher