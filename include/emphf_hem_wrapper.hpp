#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/emphf/base_hash.hpp"
#include "../external/emphf/mphf_hem.hpp"
#include "../external/emphf/internal_memory_model.hpp"

namespace mphf {

struct EMPHFHEMWrapper {
    struct Builder {
        Builder() {
            m_name = "EMPHF_HEM()";
        }

        template <typename T>
        EMPHFHEMWrapper build(const std::vector<T>& keys, uint64_t seed = 0,
                              bool verbose = false) const {
            EMPHFHEMWrapper emphf_wrapper;
            build(emphf_wrapper, keys, seed, verbose);
            return emphf_wrapper;
        }

        template <typename T>
        void build(EMPHFHEMWrapper& emphf_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            emphf::jenkins64_hasher hasher;

            emphf::internal_memory_model mm;
            auto data_iterator = emphf::range(keys.begin(), keys.end());
            emphf_wrapper.m_emphf = emphf::mphf_hem<emphf::jenkins64_hasher>(
                mm, keys.size(), data_iterator, emphf_wrapper.m_adaptor);
        }

        std::string name() const {
            return m_name;
        }

    private:
        std::string m_name;
    };

    template <typename T>
    inline uint64_t operator()(const T& key) {  // this should be const but `lookup` is not const
        return m_emphf.lookup(key, m_adaptor);
    }

    inline size_t num_bits() const {
        std::stringstream ss;
        m_emphf.save(ss);
        ss.seekg(0, std::ios::end);
        return 8 * ss.tellg();
    }

private:
    struct Adaptor {
        template <typename T>
        emphf::byte_range_t operator()(T const& s) const {
            const uint8_t* buf = reinterpret_cast<uint8_t const*>(&s);
            const uint8_t* end = buf + sizeof(T);
            return emphf::byte_range_t(buf, end);
        }

        //        template <>
        emphf::byte_range_t operator()(std::string const& s) const {
            const uint8_t* buf = reinterpret_cast<uint8_t const*>(s.c_str());
            const uint8_t* end = buf + s.size() + 1; // add the null terminator
            return emphf::byte_range_t(buf, end);
        }
    };

    Adaptor m_adaptor;
    emphf::mphf_hem<emphf::jenkins64_hasher> m_emphf;
};

}  // namespace mphf