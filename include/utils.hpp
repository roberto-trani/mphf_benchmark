#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string.h>
#include <sys/time.h>
#include <unordered_set>

/**
 * @author: Folly
 * @source: https://github.com/facebook/folly/blob/master/folly/Benchmark.h
 */
template <class T>
inline void do_not_optimize_away(T&& datum) {
    asm volatile("" : "+r"(datum));
}

class Chrono {
public:
    inline void start() {
        m_start = ClockType::now();
    }

    inline void stop(const std::string& label = "") {
        stop_and_start(label);
        m_start = m_zero_time_point;
    }

    inline void stop_and_start(const std::string& label = "") {
        m_stop = ClockType::now();
        if (m_start == m_zero_time_point) {
            throw std::runtime_error("The chrono is not measuring");
        }
        auto elapsed = m_stop - m_start;
        m_start = m_stop;
        m_elapsed += elapsed;
        m_labels_durations.push_back({label, elapsed});
    }

    inline void reset() {
        m_start = m_stop = m_zero_time_point;
        m_elapsed = m_zero_duration;
        m_labels_durations.clear();
    }

    inline void reset_and_start() {
        m_stop = m_zero_time_point;
        m_elapsed = m_zero_duration;
        m_labels_durations.clear();

        m_start = ClockType::now();
    }

    inline double elapsed_time() const {
        return static_cast<double>(
                   std::chrono::duration_cast<std::chrono::nanoseconds>(m_elapsed).count()) /
               1e9;
    }

    inline double average_time() const {
        if (m_labels_durations.size() == 0) {
            throw std::runtime_error("The chrono did not measure any duration");
        }
        return elapsed_time() / num_timings();
    }

    inline size_t num_timings() const {
        return m_labels_durations.size();
    }

private:
    using ClockType = std::chrono::steady_clock;

    struct TimeEntry {
        std::string label;
        ClockType::duration duration;
    };

private:
    const ClockType::duration m_zero_duration = ClockType::duration(0);
    const ClockType::time_point m_zero_time_point = ClockType::time_point(m_zero_duration);
    ClockType::time_point m_start = Chrono::m_zero_time_point;
    ClockType::time_point m_stop = Chrono::m_zero_time_point;
    ClockType::duration m_elapsed = m_zero_duration;
    std::vector<TimeEntry> m_labels_durations;
};

class TimeFormatter {
public:
    TimeFormatter(uint8_t max_consecutive_options = 2, bool brief = true,
                  bool fill_empty_options = true)
        : m_max_consecutive_options(max_consecutive_options)
        , m_brief(brief)
        , m_fill_empty_options(fill_empty_options) {}

    std::string operator()(double seconds) {
        return format(seconds, m_max_consecutive_options, m_fill_empty_options, m_brief);
    }

    static std::string format(double seconds, uint8_t max_consecutive_options = 2,
                              bool fill_empty_options = true, bool brief = true) {
        assert(seconds > 0);

        const FormatRule rules[] = {{3600, "hours", "h"},         {60, "minutes", "m"},
                                    {1, "seconds", "s"},          {1e-3, "milliseconds", "ms"},
                                    {1e-6, "microseconds", "μs"}, {1e-9, "nanoseconds", "ns"}};
        const int num_rules = 6;
        std::vector<int> applied_rules;
        applied_rules.reserve(num_rules);

        std::stringstream ss;
        for (int r = 0; r < num_rules; ++r) {
            FormatRule rule = rules[r];

            if (!applied_rules.empty()) {
                if (max_consecutive_options > 0 &&
                    applied_rules[0] + max_consecutive_options <= r) {
                    break;
                }
            }
            if (seconds >= rule.seconds) {
                int rule_amount = floor(seconds / rule.seconds);

                if (!applied_rules.empty()) {
                    if (fill_empty_options && applied_rules.back() + 1 < r) {
                        for (int i = applied_rules.back(); i < r; ++i) {
                            if (brief) {
                                ss << " 0" << rules[i].shortname;
                            } else {
                                ss << " 0 " << rules[i].fullname;
                            }
                        }
                    }

                    ss << " ";
                }

                if (applied_rules.empty() && (r == num_rules - 1 || max_consecutive_options == 1)) {
                    ss << std::fixed << std::setprecision(3) << seconds / rule.seconds;
                } else {
                    ss << rule_amount;
                }

                if (brief) {
                    ss << rule.shortname;
                } else if (rule_amount == 1) {
                    ss << " " << std::string(rule.fullname).substr(0, strlen(rule.fullname) - 1);
                } else {
                    ss << " " << rule.fullname;
                }
                applied_rules.push_back(r);
                seconds -= rule_amount * rule.seconds;
            }
        }
        return ss.str();
    }

private:
    struct FormatRule {
        const double seconds;
        const char* fullname;
        const char* shortname;
    };

private:
    int m_max_consecutive_options;
    bool m_brief;
    bool m_fill_empty_options;
};

template <typename T>
std::vector<T> create_random_distinct_keys(uint64_t num_keys, uint64_t seed) {
    std::mt19937_64 random_generator(seed + num_keys);

    std::vector<T> keys(num_keys + num_keys * 0.25);
    std::generate(keys.begin(), keys.end(), random_generator);
    std::sort(keys.begin(), keys.end());
    std::unique(keys.begin(), keys.end());

    uint64_t curr_size = keys.size();
    while (curr_size < num_keys) {
        keys.resize(curr_size + num_keys);

        std::generate(keys.begin() + curr_size, keys.end(), random_generator);
        std::sort(keys.begin() + curr_size, keys.end());
        std::inplace_merge(keys.begin(), keys.begin() + curr_size, keys.end());
        std::unique(keys.begin(), keys.end());
        curr_size = keys.size();
    }

    std::shuffle(keys.begin(), keys.end(), random_generator);
    keys.resize(num_keys);

    return keys;
}

std::vector<uint32_t> create_xorshift32_keys(uint32_t num_keys, uint32_t seed) {
    if (seed == 0) seed = 1234;
    std::vector<uint32_t> result;
    result.reserve(num_keys);
    for (uint32_t i = 0; i < num_keys; ++i) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        result.push_back(seed);
    }
    return result;
}

std::vector<uint64_t> create_xorshift64_keys(uint64_t num_keys, uint64_t seed) {
    if (seed == 0) seed = 1234;
    std::vector<uint64_t> result;
    result.reserve(num_keys);
    for (uint64_t i = 0; i < num_keys; ++i) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        result.push_back(seed);
    }
    return result;
}

std::vector<std::string> read_keys_from_stream(std::istream& is, char delimiter = '\n', uint64_t num_keys = 0) {
    std::vector<std::string> keys;
    std::string line;
    if (num_keys != 0) {
        keys.reserve(num_keys);
        while (num_keys > 0 && getline(is, line, delimiter)) {
            keys.push_back(line);
            --num_keys;
        }
    } else
        while (getline(is, line, delimiter)) { keys.push_back(line); }
    return keys;
}
