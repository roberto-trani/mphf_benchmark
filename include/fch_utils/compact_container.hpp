#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "compact_vector.hpp"

struct compact_container {
    void init(std::vector<uint64_t> const& values) {
        m_values.build(values.begin(), values.size());
    }

    inline uint64_t size() const {
        return m_values.size();
    }

    inline uint64_t num_bits() const {
        return m_values.bytes() * 8;
    }

    inline uint64_t operator[](uint64_t i) const {
        assert(i < m_values.size());
        return m_values.access(i);
    }

    static std::string name() {
        return "Compact";
    }

private:
    compact_vector m_values;
};