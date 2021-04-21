#pragma once

#include <cmath>
#include <sstream>
#include <stdexcept>

#include "fch_utils/buckets.hpp"
#include "fch_utils/unbalanced_bucketer.hpp"
#include "fch_utils/compact_container.hpp"
#include "utils.hpp"
#include "fch_utils/fastmod.h"

namespace mphf {

template <typename Hasher>
struct FCH {
    struct Builder {
        Builder(double bits_per_key, double perc_keys_first_part = 0.6,
                double perc_buckets_first_part = 0.3, uint32_t num_restarts = 5,
                uint32_t num_search_restarts = 10, uint32_t num_search_reseeds = 1000)
            : m_bits_per_key(bits_per_key)
            , m_perc_keys_first_part(perc_keys_first_part)
            , m_perc_buckets_first_part(perc_buckets_first_part)
            , m_num_restarts(num_restarts)
            , m_num_search_restarts(num_search_restarts)
            , m_num_search_reseeds(num_search_reseeds) {
            if (bits_per_key < 1.45) {
                throw std::invalid_argument("`bits_per_key` must be greater or equal to 1.45");
            }
            if (perc_keys_first_part < 0.0 || perc_keys_first_part > 1.0) {
                throw std::invalid_argument(
                    "`perc_keys_first_part` must be between 0 and 1, boundaries included");
            }
            if (perc_buckets_first_part < 0.0 || perc_buckets_first_part > 1.0) {
                throw std::invalid_argument(
                    "`perc_buckets_first_part` must be between 0 and 1, boundaries included");
            }

            std::stringstream ss;
            ss << "FCH(bits_per_key=" << bits_per_key;
            ss << ", perc_keys_first_part=" << perc_keys_first_part;
            ss << ", perc_buckets_first_part=" << perc_buckets_first_part;
            ss << ")";
            m_name = ss.str();
        }

        template <typename T>
        FCH build(const std::vector<T>& keys, uint64_t seed = 0, bool verbose = false) const {
            FCH fch;
            build(fch, keys, seed, verbose);
            return fch;
        }

        template <typename T>
        void build(FCH& fch, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            std::mt19937_64 generator(seed);
            Chrono chrono;

            fch.m_num_keys = keys.size();
            fch.m_num_keys_M = fastmod::computeM_u64(fch.m_num_keys);

            uint64_t num_buckets =
                floor((m_bits_per_key * fch.m_num_keys) / ceil(log2(fch.m_num_keys) + 1));

            for (uint32_t fit_restart = 0; true; ++fit_restart) {
                try {
                    // mapping
                    if (verbose) { chrono.reset_and_start(); }
                    fch.m_bucketer.init(keys, num_buckets, generator(), m_perc_keys_first_part,
                                        m_perc_buckets_first_part);
                    Buckets<T> buckets(keys, fch.m_bucketer);
                    if (verbose) {
                        chrono.stop();
                        std::cerr << "Time spent in mapping " << TimeFormatter::format(chrono.elapsed_time(), 1) << std::endl;
                    }

                    // ordering
                    if (verbose) { chrono.reset_and_start(); }
                    std::vector<uint64_t> buckets_order = buckets.get_order_by_size();
                    if (verbose) {
                        chrono.stop();
                        std::cerr << "Time spent in ordering " << TimeFormatter::format(chrono.elapsed_time(), 1) << std::endl;
                    }

                    // searching
                    if (verbose) { chrono.reset_and_start(); }
                    std::vector<uint64_t> shifts;
                    for (uint32_t search_restart = 0; true; ++search_restart) {
                        fch.m_seed = get_seed_with_no_inbucket_collisions(buckets, generator);
                        try {
                            if (verbose) {
                                shifts = search<T, true>(buckets, buckets_order, fch.m_seed);
                            } else {
                                shifts = search<T, false>(buckets, buckets_order, fch.m_seed);
                            }
                            break;
                        } catch (std::runtime_error& e) {
                            if (search_restart >= m_num_search_restarts) { throw e; }
                            if (verbose) {
                                std::cerr << "fit_restart #" << (fit_restart+1) << " caused by: " << e.what() << std::endl;
                            }
                        }
                    }
                    if (verbose) {
                        chrono.stop();
                        std::cerr << "Time spent in searching " << TimeFormatter::format(chrono.elapsed_time(), 1) << std::endl;
                    }

                    // encoding
                    if (verbose) { chrono.reset_and_start(); }
                    fch.m_shifts.init(shifts);
                    if (verbose) {
                        chrono.stop();
                        std::cerr << "Time spent in encoding " << TimeFormatter::format(chrono.elapsed_time(), 1) << std::endl;
                    }
                    break;
                } catch (std::runtime_error& e) {
                    if (fit_restart >= m_num_restarts) { throw e; }
                    if (verbose) {
                        std::cerr << "fit_restart #" << (fit_restart+1) << " caused by: " << e.what() << std::endl;
                    }
                }
            }
        }

        std::string name() const {
            return m_name;
        }

    private:
        /**
         * Returns a seed that does not cause collisions among the keys of each bucket
         */
        template <typename T>
        uint64_t get_seed_with_no_inbucket_collisions(const Buckets<T>& buckets,
                                                      std::mt19937_64& generator) const {
            Hasher hasher;
            const uint64_t num_keys = buckets.num_keys(), num_buckets = buckets.num_buckets();
            __uint128_t num_keys_M = fastmod::computeM_u64(num_keys);

            std::vector<uint64_t> bucket_pattern;
            bucket_pattern.reserve(buckets.size_biggest_bucket());

            for (uint32_t reseed = 0; true; ++reseed) {
                if (reseed > m_num_search_reseeds) {
                    throw std::runtime_error("The seed causes in-bucket collisions");
                }

                uint64_t seed = generator();
                bool collision = false;

                // check whether the seed causes collisions among the keys of each bucket
                for (uint64_t bucket = 0; bucket < num_buckets; ++bucket) {
                    // compose the pattern, as it does not depend on the shift
                    bucket_pattern.clear();
                    for (auto it = buckets.begin(bucket), it_end = buckets.end(bucket);
                         it != it_end; ++it) {
                        uint64_t pos = fastmod::fastmod_u64(hasher(**it, seed), num_keys_M, num_keys);
                        bucket_pattern.push_back(pos);
                    }  // end loop over keys of a bucket

                    std::sort(bucket_pattern.begin(), bucket_pattern.end());
                    if (std::adjacent_find(bucket_pattern.begin(), bucket_pattern.end()) != bucket_pattern.end()) {
                        collision = true;
                        break;
                    }
                }  // end loop over buckets

                if (!collision) { return seed; }
            }  // reseed
        }

        template <typename T, bool debug=false>
        std::vector<uint64_t> search(const Buckets<T>& buckets,
                                     const std::vector<uint64_t>& buckets_order,
                                     uint64_t seed) const {
            Hasher hasher;
            const uint64_t num_keys = buckets.num_keys(), num_buckets = buckets.num_buckets();
            __uint128_t num_keys_M = fastmod::computeM_u64(num_keys);

            // result vector that will be filled with the shifts
            std::vector<uint64_t> shifts(num_buckets);

            // create and fill the random and map tables
            uint64_t filled_count = 0;
            std::vector<uint64_t> random_table(num_keys);
            std::vector<uint64_t> map_table(num_keys);
            for (uint64_t i = 0, i_end = num_keys; i < i_end; ++i) { random_table[i] = i; }
            std::shuffle(random_table.begin(), random_table.end(), std::mt19937_64(seed));
            for (uint64_t i = 0, i_end = num_keys; i < i_end; ++i) {
                map_table[random_table[i]] = i;
            }

            // vector to store the positions (without shifts) of the keys of a bucket
            std::vector<uint64_t> bucket_pattern;
            bucket_pattern.reserve(buckets.size_biggest_bucket());

            // the number of max attempts per bucket is based on the number of special bits
            const uint64_t max_bucket_attempts = 2;

            // iterate over the buckets
            for (auto bucket : buckets_order) {
                uint64_t shift = 0;
                bool shift_found = false;
                uint64_t bucket_attempt = 0;

                // change behaviour based on the bucket size
                if (buckets.size(bucket) == 0) { continue; }
                for (; bucket_attempt < max_bucket_attempts; ++bucket_attempt) {
                    // the seed depends from the bucket attempt
                    uint64_t attempt_seed = seed + bucket_attempt;

                    // compose the pattern, as it does not depend on the shift
                    bucket_pattern.clear();
                    for (auto it = buckets.begin(bucket), it_end = buckets.end(bucket);
                         it != it_end; ++it) {
                        bucket_pattern.push_back(
                            fastmod::fastmod_u64(hasher(**it, attempt_seed), num_keys_M, num_keys));
                    }

                    // check if the pattern contains duplicates (otherwise no shift can satisfy it)
                    // the first bucket has been already checked at the beginning of the search
                    // phase
                    if (bucket_attempt > 0) {
                        std::sort(bucket_pattern.begin(),  bucket_pattern.end());
                        if (std::adjacent_find(bucket_pattern.begin(), bucket_pattern.end()) != bucket_pattern.end()) {
                            // if there is an in-bucket collision continue with the next attempt,
                            // which will change the pattern
                            continue;
                        }
                    }

                    // consider only the valid shifts among the available ones
                    for (uint64_t random_table_pos = filled_count; random_table_pos < num_keys; ++random_table_pos) {
                        shift = fastmod::fastmod_u64(num_keys - bucket_pattern[0] + random_table[random_table_pos], num_keys_M, num_keys);

                        shift_found = true;
                        // I do not check the in-bucket collisions here as they were checked before
                        for (uint64_t pos : bucket_pattern) {
                            pos = fastmod::fastmod_u64(pos + shift, num_keys_M, num_keys);
                            if (map_table[pos] < filled_count) {
                                shift_found = false;
                                break;
                            }
                        }

                        // I found a valid shift then I can update the map and random tables and go
                        // on with the next bucket
                        if (shift_found) {
                            for (uint64_t pos : bucket_pattern) {
                                pos = fastmod::fastmod_u64(pos + shift, num_keys_M, num_keys);

                                uint64_t y = map_table[pos];
                                uint64_t ry = random_table[y];
                                random_table[y] = random_table[filled_count];
                                random_table[filled_count] = ry;
                                map_table[random_table[y]] = y;
                                map_table[random_table[filled_count]] = filled_count;
                                filled_count++;
                            }
                            break;
                        }
                    }

                    if (shift_found) { break; }
                }  // end of bucket attempt

                if (shift_found) {
                    // save the shift
                    shifts[bucket] = (shift << 1) | (bucket_attempt);
                } else {
                    throw std::runtime_error("Unable to find a satisfying shift");
                }
            }

            return shifts;
        }

    private:
        double m_bits_per_key;
        double m_perc_keys_first_part, m_perc_buckets_first_part;
        uint32_t m_num_restarts, m_num_search_restarts, m_num_search_reseeds;
        std::string m_name;
    };  // end Builder

    template <typename T>
    inline uint64_t operator()(const T& key) const {
        auto bucket = m_bucketer(key);
        auto shift = m_shifts[bucket];

        // unpack
        uint64_t seed = m_seed + (shift & 1);
        shift >>= 1;
        return fastmod::fastmod_u64(m_hasher(key, seed) + shift, m_num_keys_M, m_num_keys);
    }

    inline size_t num_bits() const {
        return 8 * (sizeof(m_num_keys) + sizeof(m_seed)) + m_bucketer.num_bits() +
               m_shifts.num_bits();
    }

private:
    Hasher m_hasher;
    uint64_t m_num_keys, m_seed;
    __uint128_t m_num_keys_M;

    unbalanced_bucketer<Hasher> m_bucketer;
    compact_container m_shifts;
};

}  // namespace mphf