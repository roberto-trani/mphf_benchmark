#pragma once

#include "base_hasher.hpp"

namespace mphf::base_hasher {

struct Murmur2BaseHasher : BaseHasher {
    // MurmurHash2, 64-bit versions, by Austin Appleby
    // Adapted from https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
    // The same caveats as 32-bit MurmurHash2 apply here - beware of alignment and
    // endian-ness issues if used across multiple platforms.
    uint64_t operator()(const void* key, size_t len, uint64_t seed = 0) const {
#if __WORDSIZE < 64
        // 64-bit hash for 32-bit platforms
        const uint32_t m = 0x5bd1e995;
        const int r = 24;

        uint32_t h1 = static_cast<uint32_t>(seed) ^ len;
        uint32_t h2 = static_cast<uint32_t>(seed >> 32);

        const uint32_t* data = static_cast<const uint32_t*>(key);

        while (len >= 8) {
            uint32_t k1 = read_unaligned<uint32_t>(*data++);

            k1 *= m;
            k1 ^= k1 >> r;
            k1 *= m;
            h1 *= m;
            h1 ^= k1;
            len -= 4;

            uint32_t k2 = read_unaligned<uint32_t>(*data++);
            k2 *= m;
            k2 ^= k2 >> r;
            k2 *= m;
            h2 *= m;
            h2 ^= k2;
            len -= 4;
        }

        if (len >= 4) {
            uint32_t k1 = read_unaligned<uint32_t>(*data++);
            k1 *= m;
            k1 ^= k1 >> r;
            k1 *= m;
            h1 *= m;
            h1 ^= k1;
            len -= 4;
        }

        switch (len) {
            case 3:
                h2 ^= static_cast<unsigned char*>(data)[2] << 16;
            case 2:
                h2 ^= static_cast<unsigned char*>(data)[1] << 8;
            case 1:
                h2 ^= static_cast<unsigned char*>(data)[0];
                h2 *= m;
        };

        h1 ^= h2 >> 18;
        h1 *= m;
        h2 ^= h1 >> 22;
        h2 *= m;
        h1 ^= h2 >> 17;
        h1 *= m;
        h2 ^= h1 >> 19;
        h2 *= m;

        uint64_t h = h1;

        h = (h << 32) | h2;

        return h;
#else
        // 64-bit hash for 64-bit platforms
        const uint64_t m = 0xc6a4a7935bd1e995LLU;
        const int r = 47;

        uint64_t h = seed ^ (len * m);

        const uint64_t* data = (const uint64_t*)key;
        const uint64_t* end = data + (len / 8);

        while (data != end) {
            uint64_t k = read_unaligned<uint64_t>(*data++);

            k *= m;
            k ^= k >> r;
            k *= m;

            h ^= k;
            h *= m;
        }

        const unsigned char* data2 = (const unsigned char*)data;

        switch (len & 7) {
            case 7:
                h ^= static_cast<uint64_t>(data2[6]) << 48;
            case 6:
                h ^= static_cast<uint64_t>(data2[5]) << 40;
            case 5:
                h ^= static_cast<uint64_t>(data2[4]) << 32;
            case 4:
                h ^= static_cast<uint64_t>(data2[3]) << 24;
            case 3:
                h ^= static_cast<uint64_t>(data2[2]) << 16;
            case 2:
                h ^= static_cast<uint64_t>(data2[1]) << 8;
            case 1:
                h ^= static_cast<uint64_t>(data2[0]);
                h *= m;
        }

        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
#endif
    }
};

}  // namespace mphf::base_hasher