/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CITYLIFEREQUESTQUEUE_H
#define PLAYERBOTS_CITYLIFEREQUESTQUEUE_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Capital entries are detected on the entering player's map thread, but populating a city reads and
// changes bots that other map threads are updating. Map threads only push (player, zone) here; the world
// thread drains the queue after MapMgr has joined its workers and does the population work there.
class CityLifeRequestQueue
{
public:
    struct Request
    {
        std::uint64_t player;
        std::uint32_t zone;
    };

    // Entries are rare (a human changing zone into a capital), so this bound is only a safety net.
    static constexpr std::size_t Capacity = 512;

    // Map threads. The lock covers a short scan and a push; nothing else runs under it.
    bool Push(std::uint64_t player, std::uint32_t zone)
    {
        std::lock_guard<std::mutex> guard(_lock);
        for (Request const& request : _pending)
            if (request.player == player && request.zone == zone)
                return false;

        if (_pending.size() >= Capacity)
            return false;

        _pending.push_back({player, zone});
        return true;
    }

    // World thread. Requests are processed after the lock is released.
    std::vector<Request> Take()
    {
        std::vector<Request> requests;
        std::lock_guard<std::mutex> guard(_lock);
        requests.swap(_pending);
        return requests;
    }

private:
    std::mutex _lock;
    std::vector<Request> _pending;
};

#endif
