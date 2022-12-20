#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/pthash/include/pthash.hpp"
#include "../external/pthash/include/encoders/encoders.hpp"
#include "../external/pthash/include/utils/hasher.hpp"

namespace mphf {

template <bool partitioned, typename Encoder>
struct PTHashWrapper {
    struct Builder {
        Builder(float c, float alpha, uint64_t num_threads = 1, uint64_t num_of_keys = 0)
            : m_c(c), m_alpha(alpha), m_num_threads(num_threads) {
            if (c < 1.45) { throw std::invalid_argument("`c` must be greater or equal to 1.45"); }
            if (alpha <= 0 || 1 < alpha) {
                throw std::invalid_argument(
                    "`alpha` must be between 0 (excluded) and 1 (included)");
            }

            std::stringstream ss;
            ss << (partitioned ? "P" : "") << "PTHash(encoder=" << Encoder::name();
            ss << ", c=" << c;
            ss << ", alpha=" << alpha;
            ss << ", threads=" << num_threads;
            if (partitioned) {
                num_of_keys /= 5000000;
                if (num_of_keys == 0) {
                    m_partitions = 1;
                } else {
                    num_of_keys--;  // calc nearest power of two
                    num_of_keys |= num_of_keys >> 1;
                    num_of_keys |= num_of_keys >> 2;
                    num_of_keys |= num_of_keys >> 4;
                    num_of_keys |= num_of_keys >> 8;
                    num_of_keys |= num_of_keys >> 16;
                    num_of_keys++;
                    m_partitions = num_of_keys;
                }
                ss << ", partitions=" << m_partitions;
            }
            ss << ")";
            m_name = ss.str();
        }

        template <typename T>
        PTHashWrapper build(const std::vector<T>& keys, uint64_t seed = 0,
                            bool verbose = false) const {
            PTHashWrapper pthash_wrapper;
            build(pthash_wrapper, keys, seed, verbose);
            return pthash_wrapper;
        }

        template <typename T>
        void build(PTHashWrapper& pthash_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            pthash::build_configuration config;
            config.c = m_c;
            config.alpha = m_alpha;
            config.num_threads = m_num_threads;
            if constexpr (partitioned) config.num_partitions = m_partitions;
            config.minimal_output = true;
            config.verbose_output = verbose;
            config.seed = seed;

            if constexpr (!partitioned) {
                uint64_t num_bytes_for_construction = pthash::internal_memory_builder_single_phf<
                    pthash::murmurhash2_64>::estimate_num_bytes_for_construction(keys.size(),
                                                                                 config);
                std::cout << "Estimated num_bytes for construction: " << num_bytes_for_construction
                          << " (" << static_cast<double>(num_bytes_for_construction) / keys.size()
                          << " bytes/key)" << std::endl;
            }

            pthash_wrapper.m_pthash.build_in_internal_memory(keys.begin(), keys.size(), config);
        }

        std::string name() const {
            return m_name;
        }

    private:
        float m_c, m_alpha;
        std::string m_name;
        uint64_t m_num_threads;
        uint64_t m_partitions;
    };

    template <typename T>
    inline uint64_t operator()(const T& key) const {
        return m_pthash(key);
    }

    inline size_t num_bits() const {
        return m_pthash.num_bits();
    }

private:
    std::conditional_t<partitioned, pthash::partitioned_phf<pthash::murmurhash2_64, Encoder, true>,
                       pthash::single_phf<pthash::murmurhash2_64, Encoder, true> >
        m_pthash;
};

}  // namespace mphf
