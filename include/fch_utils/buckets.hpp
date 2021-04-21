#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

template <typename T>
struct Buckets {
    typedef typename std::vector<const T*>::const_iterator const_iterator;

    Buckets() {
        m_bucket_keys = {};
        m_bucket_offsets = {0};
        m_size_biggest_bucket = 0;
    }

    template <class Bucketer>
    Buckets(const std::vector<T>& keys, const Bucketer& bucketer) {
        const uint64_t num_buckets = bucketer.num_buckets();

        // compute the bucket of each key
        std::vector<uint64_t> buckets(keys.size());
        for (size_t i = 0, i_end = keys.size(); i < i_end; ++i) { buckets[i] = bucketer(keys[i]); }

        m_bucket_keys.resize(keys.size());
        m_bucket_offsets.resize(num_buckets + 1);

        // compute the `m_bucket_offsets`
        // number of keys inside each bucket
        std::fill(m_bucket_offsets.begin(), m_bucket_offsets.end(), 0);
        for (size_t i = 0, i_end = keys.size(); i < i_end; ++i) {
            ++m_bucket_offsets[buckets[i] + 1];
        }
        uint64_t biggest_bucket = m_bucket_offsets[0];
        // cumulative sum of the sizes
        for (size_t i = 1, i_end = num_buckets + 1; i < i_end; ++i) {
            if (m_bucket_offsets[i] > biggest_bucket) { biggest_bucket = m_bucket_offsets[i]; }
            m_bucket_offsets[i] += m_bucket_offsets[i - 1];
        }
        m_size_biggest_bucket = biggest_bucket;

        // reorder the (pointer to the) keys within the `m_bucket_keys` vector
        std::vector<uint64_t> buckets_cursors(num_buckets);
        std::copy(m_bucket_offsets.begin(), m_bucket_offsets.end() - 1, buckets_cursors.begin());
        for (size_t i = 0, i_end = keys.size(); i < i_end; ++i) {
            const uint64_t bucket = buckets[i];
            m_bucket_keys[buckets_cursors[bucket]++] = &keys[i];
        }
    }

    std::vector<uint64_t> get_order_by_size() const {
        const uint64_t num_buckets = this->num_buckets();
        const uint64_t size_biggest_bucket = this->size_biggest_bucket();

        // counting sort

        // compute the offset of each `size`
        // note: +2 means +1 to include `size=size_biggest_bucket` and +1 to include the offset 0
        std::vector<uint64_t> offsets(size_biggest_bucket + 2, 0);
        // count the number of keys in each bucket
        for (uint64_t i = 0; i < num_buckets; ++i) { ++offsets[size(i)]; }
        // cumsum right-to-left to update the offsets
        for (uint64_t i = size_biggest_bucket; i > 0; --i) { offsets[i - 1] += offsets[i]; }

        std::vector<uint64_t> buckets_order(num_buckets);
        for (uint64_t i = 0; i < num_buckets; ++i) { buckets_order[offsets[size(i) + 1]++] = i; }

        return buckets_order;
    }

    /**
     * @param i The index of the bucket to query
     * @return An iterator to the begin of the i-th bucket
     */
    inline const_iterator begin(uint64_t i) const {
        return m_bucket_keys.cbegin() + m_bucket_offsets[i];
    }

    /**
     * @param i The index of the bucket to query
     * @return An iterator to the end of the i-th bucket
     */
    inline const_iterator end(uint64_t i) const {
        return m_bucket_keys.cbegin() + m_bucket_offsets[i + 1];
    }

    /**
     * @param i The index of the bucket to query
     * @return The number of keys of the i-th bucket
     */
    inline uint64_t size(uint64_t i) const {
        return m_bucket_offsets[i + 1] - m_bucket_offsets[i];
    }

    /**
     * @return The number of keys of the biggest bucket
     */
    inline uint64_t size_biggest_bucket() const {
        return m_size_biggest_bucket;
    }

    /**
     * @return The number of all keys
     */
    inline uint64_t num_keys() const {
        return m_bucket_keys.size();
    }

    /**
     * @return The number of buckets
     */
    inline uint64_t num_buckets() const {
        return m_bucket_offsets.size() - 1;
    }

private:
    std::vector<const T*> m_bucket_keys;
    std::vector<uint64_t> m_bucket_offsets;
    uint64_t m_size_biggest_bucket;
};
