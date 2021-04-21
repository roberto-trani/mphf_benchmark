#pragma once

#include <fcntl.h>
#include <sstream>

#include "../external/cmph/src/cmph.h"
#include "../external/cmph/src/cmph_structs.h"

namespace mphf {

struct CHDWrapper {
    struct Builder {
        Builder(double lambda): m_lambda(lambda) {
            if (lambda < 1) {
                throw std::invalid_argument("`lambda` must be greater or equal to 1");
            }

            std::stringstream ss;
            ss << "CHD(lambda=" << lambda << ")";
            m_name = ss.str();
        }

        template <typename T>
        CHDWrapper build(const std::vector<T>& keys, uint64_t seed = 0, bool verbose = false) const {
            CHDWrapper chd_wrapper;
            build(chd_wrapper, keys, seed, verbose);
            return chd_wrapper;
        }

        template <typename T>
        void build(CHDWrapper& chd_wrapper, const std::vector<T>& keys, uint64_t seed = 0,
                   bool verbose = false) const {

            // Source of keys
            cmph_vector_adapter adapter(keys);
            cmph_config_t *config = cmph_config_new(&adapter);
            cmph_config_set_algo(config, CMPH_CHD);
            cmph_config_set_graphsize(config, 0.99); // load factor
            cmph_config_set_b(config, m_lambda);
            if (chd_wrapper.m_chd) {
                cmph_destroy(chd_wrapper.m_chd);
                chd_wrapper.m_chd = nullptr;
            }
            chd_wrapper.m_chd = cmph_new(config);
            cmph_config_destroy(config);
        }

        std::string name() const {
            return m_name;
        }

    private:
        template <typename T>
        struct cmph_vector_adapter: cmph_io_adapter_t {
            cmph_vector_adapter(std::vector<T> const& keys) {
                data = (void *) this;
                nkeys = keys.size();
                read = _read;
                dispose = _dispose;
                rewind = _rewind;

                m_keys = &keys;
                m_position = 0;
            }

        private:
            static int _read(void *data, char ** key, cmph_uint32 *len) {
                cmph_vector_adapter * adapter = (cmph_vector_adapter *) data;
                *len = input_adapter(
                    adapter->m_keys->operator[](adapter->m_position++), key);
                return *len;
            }

            static void _dispose(void *, char *, cmph_uint32) {}

            static void _rewind(void *data) {
                cmph_vector_adapter * adapter = (cmph_vector_adapter *) data;
                adapter->m_position = 0;
            }

            const std::vector<T> * m_keys;
            uint64_t m_position;
        };

        double m_lambda;
        std::string m_name;
    };

    ~CHDWrapper() {
        if (m_chd) {
            cmph_destroy(m_chd);
            m_chd = nullptr;
        }
    }

    template <typename T>
    inline uint64_t operator()(const T& key) {  // this should be const but `lookup` is not const
        char * char_ptr_key;
        cmph_uint32 size = input_adapter(key, &char_ptr_key);
        return cmph_search(m_chd, char_ptr_key, size);
    }

    inline size_t num_bits() {  // this should be const but `totalBitSize` is not const
        return cmph_packed_size(m_chd)*8;
    }


private:
    template <typename T>
    static inline cmph_uint32 input_adapter(const T& key, char ** char_ptr_key) {
        *char_ptr_key = (char *) &key;
        return sizeof(T);
    }

//    template <>
    static inline cmph_uint32 input_adapter(const std::string& key, char ** char_ptr_key) {
        *char_ptr_key = (char *) key.data();
        return key.size();
    }

    cmph_t * m_chd = nullptr;
};

}  // namespace mphf