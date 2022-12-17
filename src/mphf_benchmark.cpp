#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "../include/base_hasher/murmur2_base_hasher.hpp"
#include "../include/hasher/hasher.hpp"
#include "../include/bbhash_wrapper.hpp"
#include "../include/chd_wrapper.hpp"
#include "../include/emphf_wrapper.hpp"
#include "../include/emphf_hem_wrapper.hpp"
#include "../include/fch.hpp"
#include "../include/pthash_wrapper.hpp"
#ifndef __APPLE__
#include "../include/recsplit_wrapper.hpp"
#endif
#include "../include/utils.hpp"
#include "../external/cmd_line_parser/include/parser.hpp"

template <typename T>
struct TestEnvironment {
    TestEnvironment(std::vector<T>&& keys, uint32_t num_construction_runs = 1,
                    uint32_t num_lookup_runs = 5, uint64_t seed = 0, bool verbose = false)
        : keys(std::move(keys))
        , num_construction_runs(num_construction_runs)
        , num_lookup_runs(num_lookup_runs)
        , seed(seed)
        , verbose(verbose) {
        if (num_construction_runs < 1) {
            throw std::runtime_error("`num_construction_runs` must be strictly greater than zero");
        }
    }

    template <typename Builder>
    void test(Builder const& builder) const {
        std::cerr << "Algorithm " << builder.name() << std::endl;

        Chrono chrono;
        TimeFormatter timeFormatter(2, true, true);
        uint64_t num_bits = 0;

        // fit the mphf
        chrono.start();
        auto mphf = builder.build(keys, seed, verbose);
        chrono.stop();
        num_bits = mphf.num_bits();
        for (uint32_t run = 1; run != num_construction_runs; ++run) {
            chrono.start();
            builder.build(mphf, keys, seed, verbose);
            chrono.stop();
            num_bits += mphf.num_bits();
        }
        double average_construction_time = chrono.average_time();
        double space_usage = 1.0 * num_bits / (keys.size() * num_construction_runs);
        std::cerr << "Average Construction time: " << timeFormatter(average_construction_time);
        std::cerr << " (" << std::round(average_construction_time * 1000.0) / 1000.0 << "s)"
                  << std::endl;
        std::cerr << "Average Space usage: " << std::round(100.0 * space_usage) / 100.0
                  << " bits/key" << std::endl;

        // check the construction (this operation also warms-up the cache)
        const __uint128_t num_keys = keys.size();
        __uint128_t sum = 0;
        for (const T& key : keys) {
            uint64_t pos = mphf(key);
            if (pos >= num_keys) { throw std::runtime_error("MPHF contains out of range values"); }
            sum += pos;
        }
        if (sum != (num_keys - 1) * (num_keys) / 2) {
            throw std::runtime_error("MPHF contains duplicates");
        }

        // assess the random access lookup time
        if (num_lookup_runs != 0) {
            chrono.reset_and_start();
            for (uint64_t run = 0; run != num_lookup_runs; ++run) {
                for (const T& key : keys) { do_not_optimize_away(mphf(key)); }
            }
            chrono.stop();
            double average_lookup_time = chrono.elapsed_time() / (keys.size() * num_lookup_runs);
            std::cerr << "Average Lookup time: " << timeFormatter(average_lookup_time) << std::endl;
        }
        std::cerr << std::endl;
    }

    const std::vector<T> keys;
    const uint32_t num_construction_runs, num_lookup_runs;
    const uint64_t seed;
    const bool verbose;
};

enum Algorithm { FCH, CHD, BBhash, EMPHF, RecSplit, PTHash, PPTHash, ALL };

template <typename T>
void test_algorithms(TestEnvironment<T> const& testenv, Algorithm const& algorithm, unsigned variant, unsigned threads_num) {
    using Hasher = mphf::hasher::Hasher<mphf::base_hasher::Murmur2BaseHasher>;
    // test the algorithms
    if (algorithm == FCH || algorithm == ALL) {
        if (variant == 3 || variant == 0) testenv.test(mphf::FCH<Hasher>::Builder(3, 0.6, 0.3));
        if (variant == 4 || variant == 0) testenv.test(mphf::FCH<Hasher>::Builder(4, 0.6, 0.3));
        if (variant == 5 || variant == 0) testenv.test(mphf::FCH<Hasher>::Builder(5, 0.6, 0.3));
        if (variant == 6 || variant == 0) testenv.test(mphf::FCH<Hasher>::Builder(6, 0.6, 0.3));
        if (variant == 7 || variant == 0) testenv.test(mphf::FCH<Hasher>::Builder(7, 0.6, 0.3));
    }

    if (algorithm == CHD || algorithm == ALL) {
        if (variant == 1 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(1.0));
        if (variant == 2 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(2.0));
        if (variant == 3 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(3.0));
        if (variant == 4 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(4.0));
        if (variant == 5 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(5.0));
        if (variant == 6 || variant == 0) testenv.test(mphf::CHDWrapper::Builder(6.0));
    }

    if (algorithm == EMPHF || algorithm == ALL) {
        if (variant == 1 || variant == 0) testenv.test(mphf::EMPHFWrapper::Builder());
        if (variant == 2 || variant == 0) testenv.test(mphf::EMPHFHEMWrapper::Builder());
    }

    if (algorithm == BBhash || algorithm == ALL) {
        if (variant == 1 || variant == 0) testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(1.0));
        //testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(1.5));
        //testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(1.5, 4));
        if (variant == 2 || variant == 0) testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(2.0));
        if (threads_num > 1) {
            if (variant == 3 || variant == 0)
                testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(1.0, threads_num));
            if (variant == 4 || variant == 0)
                testenv.test(typename mphf::BBhashWrapper<T, Hasher>::Builder(2.0, threads_num));
        }
    }

    if (algorithm == RecSplit || algorithm == ALL) {
#ifdef __APPLE__
        if (algorithm == RecSplit) {
            throw std::runtime_error("RecSplit algorithm is not implemented on Apple");
        } else {
            std::cerr << "RecSplit algorithm is not implemented on Apple" << std::endl;
        }
#else
        if (variant == 1 || variant == 5 || variant == 0) testenv.test(typename mphf::RecSplitWrapper<5>::Builder(5));
        if (variant == 2 || variant == 8 || variant == 0) testenv.test(typename mphf::RecSplitWrapper<8>::Builder(100));
        if (variant == 3 || variant == 12 || variant == 0) testenv.test(typename mphf::RecSplitWrapper<12>::Builder(9));
#endif
    }

    if (algorithm == PTHash || algorithm == ALL) {
        if (variant == 1 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::compact_compact>::Builder(7.0, 0.99));
        if (variant == 2 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::dictionary_dictionary>::Builder(11.0, 0.88));
        if (variant == 3 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::elias_fano>::Builder(6.0, 0.99));
        if (variant == 4 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::dictionary_dictionary>::Builder(7.0, 0.94));
        if (threads_num > 1) {
            if (variant == 5 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::compact_compact>::Builder(7.0, 0.99, threads_num));
            if (variant == 6 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::dictionary_dictionary>::Builder(11.0, 0.88, threads_num));
            if (variant == 7 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::elias_fano>::Builder(6.0, 0.99, threads_num));
            if (variant == 8 || variant == 0) testenv.test(typename mphf::PTHashWrapper<false, pthash::dictionary_dictionary>::Builder(7.0, 0.94, threads_num));
        }
    }

    if (algorithm == PPTHash || algorithm == ALL) {
        auto keys_num = testenv.keys.size();
        if (variant == 1 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::compact_compact>::Builder(7.0, 0.99, 1, keys_num));
        if (variant == 2 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::dictionary_dictionary>::Builder(11.0, 0.88, 1, keys_num));
        if (variant == 3 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::elias_fano>::Builder(6.0, 0.99, 1, keys_num));
        if (variant == 4 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::dictionary_dictionary>::Builder(7.0, 0.94, 1, keys_num));
        if (threads_num > 1) {
            if (variant == 5 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::compact_compact>::Builder(7.0, 0.99, threads_num, keys_num));
            if (variant == 6 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::dictionary_dictionary>::Builder(11.0, 0.88, threads_num, keys_num));
            if (variant == 7 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::elias_fano>::Builder(6.0, 0.99, threads_num, keys_num));
            if (variant == 8 || variant == 0) testenv.test(typename mphf::PTHashWrapper<true, pthash::dictionary_dictionary>::Builder(7.0, 0.94, threads_num, keys_num));
        }
    }
}

int main(int argc, char** argv) {
    cmd_line_parser::parser parser(argc, argv);
    parser.add("algorithm",
               "The name of the algorithm to run. One among `fch`, `chd`, "
               "`bbhash`, `emphf`, `recsplit`, `pthash`, `ppthash`.");
    parser.add("variant",
               "Variant of the selected algorithm to test, interpretation depends on method. (default: 0 = all variants)",
               "--variant", false);
    parser.add("num_keys",
               "The number of random keys to use for the test. "
               "If it is not provided, then keys are read from the input (one "
               "per line).",
               "-n", false);
    parser.add("num_construction_runs", "Number of times to perform the construction. (default: 1)",
               "--num_construction_runs", false);
    parser.add("num_lookup_runs", "Number of times to perform the lookup test. (default: 1)",
               "--num_lookup_runs", false);
    parser.add("verbose", "Verbose output during construction. (default: false)", "--verbose",
               true);
    parser.add("seed", "Seed used for construction. (default: 0)", "--seed", false);
    parser.add("threads", "Number of threads used in multi-threaded calculations. (default: 0 = auto)", "--threads", false);
    parser.add("generator", "The method of generating keys, one of: "
                "`64` (default), `xs32` (xor-shift 32), `xs64` (xor-shift 64)", "--gen", false);
    if (!parser.parse()) { return 1; }

    std::string algorithm_name = parser.get<std::string>("algorithm");
    unsigned variant = parser.parsed("variant") ? parser.get<uint64_t>("variant") : 0;
    uint64_t num_keys = parser.parsed("num_keys") ? parser.get<uint64_t>("num_keys") : 0;
    bool verbose = parser.parsed("verbose") && parser.get<bool>("verbose");
    uint32_t num_construction_runs =
        parser.parsed("num_construction_runs") ? parser.get<uint64_t>("num_construction_runs") : 1;
    uint32_t num_lookup_runs =
        parser.parsed("num_lookup_runs") ? parser.get<uint64_t>("num_lookup_runs") : 1;
    uint64_t seed = parser.parsed("seed") ? parser.get<uint64_t>("seed") : 0;
    std::string generator = parser.parsed("generator") ? parser.get<std::string>("generator") : "64";

    // recognize the algorithm
    const std::unordered_map<std::string, Algorithm> name_to_algorithm{
        {"fch", FCH},           {"chd", CHD},       {"bbhash", BBhash}, {"emphf", EMPHF},
        {"recsplit", RecSplit}, {"pthash", PTHash}, {"ppthash", PPTHash}, {"all", ALL},
    };
    auto algorithm_it = name_to_algorithm.find(algorithm_name);
    if (algorithm_it == name_to_algorithm.end()) {
        std::cerr << "Invalid algorithm name. Valid names are: ";
        bool is_first = true;
        for (auto it : name_to_algorithm) {
            if (is_first) {
                std::cerr << "`" << it.first << "`";
                is_first = false;
            } else {
                std::cerr << ", `" << it.first << "`";
            }
        }
        std::cerr << "." << std::endl;
        return 1;
    }
    Algorithm algorithm = algorithm_it->second;

    unsigned threads_num = parser.parsed("threads") ? parser.get<unsigned>("threads") : 0;
    if (threads_num == 0) threads_num = std::max(std::thread::hardware_concurrency(), 1u);
    std::cout << threads_num << " threads available for multi-threaded calculations" << std::endl;

    if (!parser.parsed("num_keys")) {
        std::cout << "Reading keys from stdin" << std::endl;
        std::vector<std::string> keys = read_keys_from_stream(std::cin, '\n');
        double average_size =
            std::accumulate(keys.begin(), keys.end(), 0.0,
                            [](double sum, const std::string& key) { return sum + key.size(); }) /
            keys.size();
        std::cout << "Read " << keys.size() << " keys, with average length "
                  << std::round(average_size * 100) / 100 << std::endl;
        TestEnvironment<std::string> testenv(std::move(keys), num_construction_runs,
                                             num_lookup_runs, seed, verbose);
        test_algorithms(testenv, algorithm, variant, threads_num);
    } else {
        if (generator != "64" && generator != "xs32" && generator != "xs64") {
            std::cerr << "Wrong generator name." << std::endl;
            return 1;
        }
        if (num_keys == 0) {
            std::cerr << "The number of keys cannot be zero" << std::endl;
            return 1;
        }
        std::cout << "Generating " << num_keys << " random keys by " << generator << " generator (the name of the generator contains the size of each key in bits)." << std::endl;
        if (generator == "64" || generator == "xs64") {
            std::vector<uint64_t> keys = generator == "64" ?
                        create_random_distinct_keys<uint64_t>(num_keys, seed) :
                        create_xorshift64_keys(num_keys, seed);
            TestEnvironment<uint64_t> testenv(std::move(keys), num_construction_runs, num_lookup_runs,
                                              seed, verbose);
            test_algorithms(testenv, algorithm, variant, threads_num);
        } else {
            std::vector<uint32_t> keys = create_xorshift32_keys(num_keys, seed);
            TestEnvironment<uint32_t> testenv(std::move(keys), num_construction_runs, num_lookup_runs,
                                              seed, verbose);
            test_algorithms(testenv, algorithm, variant, threads_num);
        }
    }
    return 0;
}
