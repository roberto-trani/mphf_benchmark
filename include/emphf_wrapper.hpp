#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/emphf/base_hash.hpp"
#include "../external/emphf/hypergraph_sorter_seq.hpp"
#include "../external/emphf/mphf.hpp"


namespace mphf {

struct EMPHFWrapper {
    struct Builder {
        Builder() {
            m_name = "EMPHF()";
        }

        template <typename T>
        EMPHFWrapper build(const std::vector<T>& keys, uint64_t seed = 0, bool verbose = false) const {
            EMPHFWrapper emphf_wrapper;
            build(emphf_wrapper, keys, seed, verbose);
            return emphf_wrapper;
        }

        template <typename T>
        void build(EMPHFWrapper& emphf_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            size_t max_nodes = (size_t(std::ceil(double(keys.size()) * 1.23)) + 2) / 3 * 3;
            emphf::jenkins64_hasher hasher;
            auto data_iterator = emphf::range(keys.begin(), keys.end());

            if (max_nodes >= uint64_t(1) << 32) {
                emphf::hypergraph_sorter_seq<emphf::hypergraph<uint64_t>> sorter;
                emphf_wrapper.m_emphf = emphf::mphf<emphf::jenkins64_hasher>(
                    sorter, keys.size(), data_iterator, emphf_wrapper.m_adaptor);
            } else {
                emphf::hypergraph_sorter_seq<emphf::hypergraph<uint32_t>> sorter;
                emphf_wrapper.m_emphf = emphf::mphf<emphf::jenkins64_hasher>(
                    sorter, keys.size(), data_iterator, emphf_wrapper.m_adaptor);
            }
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
    struct Adaptor
    {
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
    emphf::mphf<emphf::jenkins64_hasher> m_emphf;
};

}  // namespace mphf