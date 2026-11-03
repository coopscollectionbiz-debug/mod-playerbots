/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ENCOUNTERSTATEMAP_H
#define PLAYERBOTS_ENCOUNTERSTATEMAP_H

#include <cstddef>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// Raid strategy state shared by every instance of a raid. Different instances run on different map
// threads, so the container itself (buckets, rehash, insert, erase) is only touched under a lock.
//
// Lookups hand back a handle to the node rather than a std iterator. unordered_map nodes never move,
// so a handle stays valid while other threads insert or erase other keys; it only dies when its own key
// is erased. Every key here is an instance id or a bot GUID, and that key is only used by the map
// thread updating that instance, which is the same ownership rule the per-instance state of upstream
// mod-playerbots 52178c72 relies on. No lock is held while the caller works with the value.
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class EncounterStateMap
{
    using Container = std::unordered_map<Key, Value, Hash>;

public:
    using value_type = typename Container::value_type;

    class Handle
    {
    public:
        Handle() = default;
        explicit Handle(value_type* node) : _node(node) { }

        value_type* operator->() const { return _node; }
        value_type& operator*() const { return *_node; }
        bool operator==(Handle const& other) const { return _node == other._node; }
        bool operator!=(Handle const& other) const { return _node != other._node; }

    private:
        value_type* _node = nullptr;
    };

    Handle find(Key const& key)
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto it = _map.find(key);
        return it == _map.end() ? Handle() : Handle(&*it);
    }

    Handle end() const { return Handle(); }

    Value& operator[](Key const& key)
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _map[key];
    }

    template <typename... Args>
    std::pair<Handle, bool> try_emplace(Key const& key, Args&&... args)
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto [it, inserted] = _map.try_emplace(key, std::forward<Args>(args)...);
        return { Handle(&*it), inserted };
    }

    template <typename V>
    std::pair<Handle, bool> insert_or_assign(Key const& key, V&& value)
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto [it, inserted] = _map.insert_or_assign(key, std::forward<V>(value));
        return { Handle(&*it), inserted };
    }

    std::size_t erase(Key const& key)
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _map.erase(key);
    }

    void erase(Handle handle)
    {
        if (handle == end())
            return;

        Key const key = handle->first;
        erase(key);
    }

    std::size_t count(Key const& key) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _map.count(key);
    }

    bool contains(Key const& key) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _map.contains(key);
    }

private:
    mutable std::mutex _lock;
    Container _map;
};

// Set form of EncounterStateMap; membership only, so nothing outlives the lock.
template <typename Key, typename Hash = std::hash<Key>>
class EncounterStateSet
{
public:
    bool insert(Key const& key)
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _set.insert(key).second;
    }

    std::size_t erase(Key const& key)
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _set.erase(key);
    }

    std::size_t count(Key const& key) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _set.count(key);
    }

    bool contains(Key const& key) const
    {
        std::lock_guard<std::mutex> guard(_lock);
        return _set.contains(key);
    }

private:
    mutable std::mutex _lock;
    std::unordered_set<Key, Hash> _set;
};

#endif
