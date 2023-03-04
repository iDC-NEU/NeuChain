/*
 *  MVCC Hash Map -- overview --
 *
 *  KeyType -> std::list<std::tuple<std::string, ValueType>>,
 *  std::string: version, ValueType: value
 *
 *  By default, the first node is a sentinel node, then comes the newest version
 * (the largest value). The upper application (e.g., worker thread) is
 * responsible for data vacuum. Given a vacuum_version, all versions less than
 * or equal to vacuum_version will be garbage collected.
 */

#ifndef NEUBLOCKCHAIN_MVCCHASHMAP_H
#define NEUBLOCKCHAIN_MVCCHASHMAP_H

#include "spin_lock.h"
#include <unordered_map>
#include <mutex>
#include <list>
#include "common/compile_config.h"

template <std::size_t N, class KeyType, class ValueType> class MVCCHashMap {
public:
    using VersionTupleType = std::tuple<std::string, ValueType>;
    using MappedValueType = std::list<VersionTupleType>;
    using HashMapType = std::unordered_map<KeyType, MappedValueType>;
    using HasherType = typename HashMapType::hasher;

    // if a particular key exists.
    bool contains_key(const KeyType &key) {
        return apply(
                [&key](HashMapType &map) {
                    auto it = map.find(key);

                    if (it == map.end()) {
                        return false;
                    }

                    // check if the list is empty
                    auto &l = it->second;
                    return !l.empty();
                },
                bucket_number(key));
    }

    // if a particular key with a specific version exists.
    bool contains_key_version(const KeyType &key, const std::string& version) {
        return apply(
                [&key, version](HashMapType &map) {
                    auto it = map.find(key);

                    if (it == map.end()) {
                        return false;
                    }

                    auto &l = it->second;
                    for (VersionTupleType &vt : l) {
                        if (get_version(vt) == version) {
                            return true;
                        }
                    }
                    return false;
                },
                bucket_number(key));
    }

    // remove a particular key.
    bool remove_key(const KeyType &key) {
        return apply(
                [&key](HashMapType &map) {
                    auto it = map.find(key);

                    if (it == map.end()) {
                        return false;
                    }
                    map.erase(it);
                    return true;
                },
                bucket_number(key));
    }

    // remove a particular key with a specific version.
    bool remove_key_version(const KeyType &key, const std::string& version) {
        return apply(
                [&key, version](HashMapType &map) {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return false;
                    }
                    auto &l = it->second;

                    for (auto lit = l.begin(); lit != l.end(); lit++) {
                        if (get_version(*lit) == version) {
                            l.erase(lit);
                            return true;
                        }
                    }
                    return false;
                },
                bucket_number(key));
    }

    // insert a key with a specific version placeholder and return the reference
    ValueType &insert_key_version_holder(const KeyType &key, const std::string& version) {
        return apply_ref(
                [&key, version](HashMapType &map) -> ValueType & {
                    auto &l = map[key];
                    // always insert to the front if the list is empty
                    if (l.empty()) {
                        l.emplace_front();
                    } else {
                        // make sure the version is larger than the head, making sure the
                        // versions are always monotonically decreasing
                        auto &head = l.front();
                        auto head_version = get_version(head);
                        // CHECK(version > head_version)
                        //        << "the new version: " << version
                        //        << " is not larger than the current latest version: "
                        //        << head_version;
                        l.emplace_front();
                    }
                    // set the version
                    std::get<0>(l.front()) = version;
                    // std::get<0> returns the version
                    return std::get<1>(l.front());
                },
                bucket_number(key));
    }

    // return the number of versions of a particular key
    std::size_t version_count(const KeyType &key) {
        return apply(
                [&key](HashMapType &map) -> std::size_t {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return 0;
                    } else {
                        auto &l = it->second;
                        return l.size();
                    }
                },
                bucket_number(key));
    }

    // return the value of a particular key and a specific version
    // nullptr if not exists.
    ValueType *get_key_version(const KeyType &key, const std::string& version) {
        return apply(
                [&key, version](HashMapType &map) -> ValueType * {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return nullptr;
                    }
                    auto &l = it->second;
                    for (VersionTupleType &vt : l) {
                        if (get_version(vt) == version) {
                            return &get_value(vt);
                        }
                    }
                    return nullptr;
                },
                bucket_number(key));
    }
    // return the value of a particular key and the version older than the
    // specific version nullptr if not exists.
    ValueType *get_key_version_prev(const KeyType &key, const std::string& version) {
        return apply(
                [&key, version](HashMapType &map) -> ValueType * {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return nullptr;
                    }
                    auto &l = it->second;
                    for (VersionTupleType &vt : l) {
                        if (get_version(vt) < version) {
                            return &get_value(vt);
                        }
                    }
                    return nullptr;
                },
                bucket_number(key));
    }

    // remove all versions not equal to vacuum_version
    std::size_t vacuum_key_versions(const KeyType &key, const std::string& vacuum_version) {
        return apply(
                [&key, vacuum_version](HashMapType &map) -> std::size_t {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return 0;
                    }

                    std::size_t size = 0;
                    auto &l = it->second;
                    auto lit = l.end();

                    while (lit != l.begin()) {
                        lit--;
                        if (get_version(*lit) != vacuum_version) {
                            lit = l.erase(lit);
                            size++;
                        } else {
                            break;
                        }
                    }
                    return size;
                },
                bucket_number(key));
    }

    // remove all versions except the latest one
    std::size_t vacuum_key_keep_latest(const KeyType &key) {
        return apply(
                [&key](HashMapType &map) -> std::size_t {
                    auto it = map.find(key);
                    if (it == map.end()) {
                        return 0;
                    }

                    std::size_t size = 0;
                    auto &l = it->second;
                    auto lit = l.begin();
                    if (lit == l.end()) {
                        return 0;
                    }

                    lit++;
                    while (lit != l.end()) {
                        lit = l.erase(lit);
                        size++;
                    }
                    return size;
                },
                bucket_number(key));
    }

private:
    static std::string get_version(std::tuple<std::string, ValueType> &t) {
        return std::get<0>(t);
    }

    static ValueType &get_value(std::tuple<std::string, ValueType> &t) {
        return std::get<1>(t);
    }

private:
    auto bucket_number(const KeyType &key) { return hasher(key) % N; }

    template <class ApplyFunc>
    auto &apply_ref(ApplyFunc applyFunc, std::size_t i) {
        // DCHECK(i < N) << "index " << i << " is greater than " << N;
        locks[i].lock();
        auto &result = applyFunc(maps[i]);
        locks[i].unlock();
        return result;
    }

    template <class ApplyFunc> auto apply(ApplyFunc applyFunc, std::size_t i) {
        // DCHECK(i < N) << "index " << i << " is greater than " << N;
        locks[i].lock();
        auto result = applyFunc(maps[i]);
        locks[i].unlock();
        return result;
    }

private:
    HasherType hasher;
    HashMapType maps[N];
    HASH_MAP_LOCK_TYPE locks[N];
};

#endif //NEUBLOCKCHAIN_MVCCHASHMAP_H
