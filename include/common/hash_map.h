//
// Created by peng on 2021/1/16.
//

#ifndef NEUBLOCKCHAIN_HASHMAP_H
#define NEUBLOCKCHAIN_HASHMAP_H

#include <unordered_map>
#include <mutex>
#include "common/compile_config.h"

template <class KeyType, class ValueType, std::size_t N=997>
class HashMap {
public:
    inline bool remove(const KeyType &key) {
        std::lock_guard lock(get_mutex_ref(key));
        auto& map = get_map_ref(key);
        auto it = map.find(key);
        if (it == map.end()) {
            return false;
        }
        map.erase(it);
        return true;
    }

    inline bool contains(const KeyType &key) const {
        std::lock_guard lock(get_mutex_ref(key));
        auto& map = get_map_ref(key);
        return map.find(key) != map.end();
    }

    inline bool insert(const KeyType &key, const ValueType &value) {
        std::lock_guard lock(get_mutex_ref(key));
        auto& map = get_map_ref(key);
        if (map.find(key) != map.end()) {
            return false;
        }
        map[key] = value;
        return true;
    }

    inline ValueType &operator[](const KeyType &key) {
        std::lock_guard lock(get_mutex_ref(key));
        auto& map = get_map_ref(key);
        return map[key];
    }

    inline const ValueType &operator[](const KeyType &key) const {
        std::lock_guard lock(get_mutex_ref(key));
        auto& map = get_map_ref(key);
        return map[key];
    }

    inline std::size_t size() const {
        std::size_t total_size = 0;
        for (auto i=0; i<N ; i++) {
            const auto& map = maps[i];
            std::lock_guard lock(get_mutex_ref(i));
            total_size += map.size();
        }
        return total_size;
    }

    inline void clear() {
        for (std::size_t i=0; i<N ; i++) {
            auto& map = maps[i];
            std::lock_guard lock(get_mutex_ref(i));
            map.clear();
        }
    }

protected:
    // map need both const and non-const ref, mutex not
    inline auto& get_map_ref(const KeyType &key) { return maps[hash(key) % N]; }
    inline const auto& get_map_ref(const KeyType &key) const { return maps[hash(key) % N]; }

    inline auto& get_mutex_ref(const KeyType &key) const { return mutex[hash(key) % N]; }

private:
    std::hash<KeyType> hash;
    std::unordered_map<KeyType, ValueType> maps[N];
    mutable std::mutex mutex[N];
};


#endif //NEUBLOCKCHAIN_HASHMAP_H
