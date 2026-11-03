/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTTHREADOWNERSHIP_H
#define PLAYERBOTS_PLAYERBOTTHREADOWNERSHIP_H

#include <atomic>
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

class Player;

// Which threads may touch a bot's PlayerbotAI and Player state.
//
// A bot belongs to the thread updating its map (core Map::GetUpdatingMapOfCurrentThread). Between map
// updates the world thread is the only thread running game code, so it may touch any bot, which is what
// core's thread-unsafe packet handlers (all chat opcodes) already rely on. Every other context (another
// map's thread, a socket thread, a script on another map) must hand the work to the bot instead.
namespace PlayerbotThreadOwnership
{
    // Records the world thread. Called from the world script update.
    void MarkWorldThread();
    bool IsWorldThread();
    // The world thread outside a map update: no map thread is running, so any bot may be touched.
    bool IsWorldThreadBetweenMapUpdates();

    // Pure decision, kept separate so it can be tested without a server.
    inline bool IsOwnerContext(void const* updatingMap, void const* botMap, bool worldThread)
    {
        if (updatingMap)
            return botMap && updatingMap == botMap;

        return worldThread;
    }

    // True if the calling thread may touch this bot right now.
    bool IsOwnerContextFor(Player const* bot);

    // Logs when bot state is touched from a thread that does not own it (PLAYERBOTS_THREAD_CHECKS builds).
    void ReportIfNotOwner(Player const* bot, char const* where);
}

#ifdef PLAYERBOTS_THREAD_CHECKS
#define PLAYERBOT_CHECK_OWNER_THREAD(bot) PlayerbotThreadOwnership::ReportIfNotOwner((bot), __FUNCTION__)
#else
#define PLAYERBOT_CHECK_OWNER_THREAD(bot) ((void)0)
#endif

// Work handed to a bot from a thread that does not own it, drained in arrival order by the owner.
// The lock only covers a push or a swap; items run after it is released.
template <typename T>
class OwnerThreadInbox
{
public:
    // Normal traffic is a handful of items between two bot updates; this only bounds a runaway sender.
    static constexpr std::size_t Capacity = 256;

    bool Push(T&& item)
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_items.size() >= Capacity)
            return false;

        _items.push_back(std::move(item));
        _pending.store(true, std::memory_order_release);
        return true;
    }

    std::vector<T> Take()
    {
        std::vector<T> items;
        std::lock_guard<std::mutex> guard(_lock);
        items.swap(_items);
        _pending.store(false, std::memory_order_release);
        return items;
    }

    bool HasPending() const { return _pending.load(std::memory_order_acquire); }

private:
    std::mutex _lock;
    std::vector<T> _items;
    std::atomic<bool> _pending{false};
};

#endif
