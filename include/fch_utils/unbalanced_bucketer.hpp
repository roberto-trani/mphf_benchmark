#pragma once

#include <math.h>
#include <stdexcept>

#include "fastmod.h"

template <typename Hasher>
struct unbalanced_bucketer {
    template <typename T>
    void init(const std::vector<T>& keys, uint64_t num_buckets, uint64_t seed = 0,
              double perc_keys_first_part = 0.6, double perc_buckets_first_part = 0.3) {
        if (num_buckets == 0 || num_buckets > keys.size()) {
            throw std::invalid_argument(
                "`num_buckets` must be between 1 and `keys`.size(), boundaries included");
        }
        if (perc_keys_first_part < 0.0 || perc_keys_first_part > 1.0) {
            throw std::invalid_argument(
                "`perc_keys_first_part` must be between 0 and 1, boundaries included");
        }
        if (perc_buckets_first_part < 0.0 || perc_buckets_first_part > 1.0) {
            throw std::invalid_argument(
                "`perc_buckets_first_part` must be between 0 and 1, boundaries included");
        }

        m_num_buckets = num_buckets;
        m_seed = seed;

        m_hash_threshold = round(UINT64_MAX * perc_keys_first_part);
        m_buckets_first_part = round(m_num_buckets * perc_buckets_first_part);
        m_buckets_second_part = m_num_buckets - m_buckets_first_part;

        m_buckets_first_part_M = fastmod::computeM_u64(m_buckets_first_part);
        m_buckets_second_part_M = fastmod::computeM_u64(m_buckets_second_part);
    }

    template <typename T>
    inline uint64_t operator()(T const& key) const {
        auto hash = m_hasher(key, m_seed);
        auto b =
            (hash < m_hash_threshold)
                ? fastmod::fastmod_u64(hash, m_buckets_first_part_M,
                                       m_buckets_first_part)  // dense set
                : m_buckets_first_part + fastmod::fastmod_u64(hash, m_buckets_second_part_M,
                                                              m_buckets_second_part);  // sparse set
        return b;
    }

    inline uint64_t num_buckets() const {
        return m_num_buckets;
    }

    inline uint64_t num_bits() const {
        return 8 * (sizeof(m_num_buckets) + sizeof(m_seed) + sizeof(m_hash_threshold) +
                    sizeof(m_buckets_first_part) + sizeof(m_buckets_second_part));
    }

private:
    Hasher m_hasher;
    uint64_t m_num_buckets, m_seed;

    uint64_t m_hash_threshold, m_buckets_first_part, m_buckets_second_part;
    __uint128_t m_buckets_first_part_M, m_buckets_second_part_M;
};
