#pragma once

#include "../base_hasher/murmur2_base_hasher.hpp"

namespace mphf::hasher {

template <typename BaseHasher = mphf::base_hasher::Murmur2BaseHasher>
struct Hasher {
    template <typename T>
    inline uint64_t operator()(const T& key, uint64_t seed = 0) const {
        return m_base_hasher(&key, sizeof(T), seed);
    };

    //    template <>
    inline uint64_t operator()(const std::string& key, uint64_t seed = 0) const {
        return m_base_hasher(key.data(), key.length(), seed);
    };

private:
    BaseHasher m_base_hasher;
};

}  // namespace mphf::hasher