#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/bbhash/BooPHF.h"

namespace mphf {

template <typename T, typename Hasher>
struct BBhashWrapper {
    struct Builder {
        Builder(double gamma, uint32_t num_threads = 1)
            : m_gamma(gamma), m_num_threads(num_threads) {
            if (gamma < 1) { throw std::invalid_argument("`gamma` must be greater or equal to 1"); }
            if (num_threads < 1) {
                throw std::invalid_argument("`num_threads` must be greater or equal to 1");
            }

            std::stringstream ss;
            ss << "BBhash(gamma=" << gamma;
            ss << ", num_threads=" << num_threads;
            ss << ")";
            m_name = ss.str();
        }

        BBhashWrapper build(const std::vector<T>& keys, uint64_t seed = 0,
                            bool verbose = false) const {
            BBhashWrapper bbhash_wrapper;
            build(bbhash_wrapper, keys, seed, verbose);
            return bbhash_wrapper;
        }

        void build(BBhashWrapper& bbhash_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {
            auto data_iterator = boomphf::iter_range(keys.begin(), keys.end());
            bbhash_wrapper.m_bbhash = boomphf::mphf<T, Hasher>(
                keys.size(), data_iterator, m_num_threads, m_gamma, true, verbose, 0.0);
        }

        std::string name() const {
            return m_name;
        }

    private:
        double m_gamma;
        uint32_t m_num_threads;
        std::string m_name;
    };

    inline uint64_t operator()(const T& key) {  // this should be const but `lookup` is not const
        return m_bbhash.lookup(key);
    }

    inline size_t num_bits() {  // this should be const but `totalBitSize` is not const
        // I must capture the stdout because the `totalBitSize` prints some debug information
        int old_fd, new_fd;
        fflush(stdout);
        old_fd = dup(1);
        new_fd = open("/dev/null", O_WRONLY);
        dup2(new_fd, 1);
        close(new_fd);

        uint64_t num_bits = m_bbhash.totalBitSize();

        fflush(stdout);
        dup2(old_fd, 1);
        close(old_fd);

        return num_bits;
    }

private:
    boomphf::mphf<T, Hasher> m_bbhash;
};

}  // namespace mphf
