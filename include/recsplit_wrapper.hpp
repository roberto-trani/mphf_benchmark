#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/sux/sux/function/RecSplit.hpp"


namespace mphf {

template <size_t LEAF_SIZE, sux::util::AllocType AT = sux::util::AllocType::MALLOC>
class RecSplitWrapper {
public:
    struct Builder {
        Builder(uint64_t bucket_size)
            : m_bucket_size(bucket_size) {

            std::stringstream ss;
            ss << "RecSplit(leaf_size=" << LEAF_SIZE;
            ss << ", bucket_size=" << bucket_size;
            ss << ")";
            m_name = ss.str();
        }

        template <typename T>
        RecSplitWrapper build(const std::vector<T>& keys, uint64_t seed = 0, bool verbose = false) const {
            RecSplitWrapper recsplit_wrapper;
            build(recsplit_wrapper, keys, seed, verbose);
            return recsplit_wrapper;
        }

        template <typename T>
        void build(RecSplitWrapper& recsplit_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            if (verbose) {
                std::cerr << "\tstarted remapping" << std::endl;
            }
            std::vector<hash128_t> remapped_keys;
            remapped_keys.reserve(keys.size());
            for (size_t i = 0, i_end=keys.size(); i < i_end; ++i) {
                remapped_keys.push_back(adapt_key(keys[i]));
            }
            if (verbose) {
                std::cerr << "\tconstruction started" << std::endl;
            }
            recsplit_wrapper.m_recsplit = sux::function::RecSplit<LEAF_SIZE, AT>(remapped_keys, m_bucket_size);
        }

        std::string name() const {
            return m_name;
        }

    private:
        uint64_t m_bucket_size;
        std::string m_name;
    };

    template <typename T>
    inline uint64_t operator()(const T& key) {  // this should be const but `lookup` is not const
        return m_recsplit(adapt_key(key));
    }

    inline size_t num_bits() const {
        std::stringstream ss;
        ss << m_recsplit;
        ss.seekg(0, std::ios::end);
        return 8 * ss.tellg();
    }

private:
    using hash128_t = sux::function::hash128_t;

    template <typename T>
    static inline hash128_t adapt_key(T const& key) {
        return sux::function::first_hash(&key, sizeof(T));
    }

//    template <>
    static inline hash128_t adapt_key(std::string const& key) {
        return sux::function::first_hash(key.data(), key.size());
    }

    sux::function::RecSplit<LEAF_SIZE, AT> m_recsplit;
};

}  // namespace mphf