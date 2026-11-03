/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Tests the production queue itself, including owned packet payloads, saturation, and concurrent drains.
#include "PlayerbotThreadOwnership.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
void Check(bool condition)
{
    if (!condition)
        throw std::runtime_error("production inbox contract failed");
}

struct Work
{
    int sender;
    int sequence;
    std::vector<unsigned char> packet;
};
}

int main()
{
    OwnerThreadInbox<Work> queue;
    for (std::size_t i = 0; i < queue.Capacity; ++i)
        Check(queue.Push({0, static_cast<int>(i), {1, 2, 3}}));
    Check(!queue.Push({0, 999, {}}));
    auto full = queue.Take();
    Check(full.size() == queue.Capacity && !queue.HasPending());
    for (std::size_t i = 0; i < full.size(); ++i)
        Check(full[i].sequence == static_cast<int>(i));

    constexpr int Producers = 8;
    constexpr int Items = 10000;
    std::atomic<int> done{0};
    std::array<std::thread, Producers> senders;
    for (int sender = 0; sender < Producers; ++sender)
        senders[sender] = std::thread([&, sender]()
        {
            for (int i = 0; i < Items; ++i)
            {
                Work work{sender, i, {static_cast<unsigned char>(sender), 42}};
                while (!queue.Push(std::move(work)))
                    std::this_thread::yield();
                // The queued payload owns its bytes after the producer's item dies.
            }
            ++done;
        });

    std::array<int, Producers> next{};
    int consumed = 0;
    while (done != Producers || queue.HasPending())
    {
        for (Work const& work : queue.Take())
        {
            Check(work.sequence == next[work.sender]++);
            Check(work.packet == std::vector<unsigned char>({static_cast<unsigned char>(work.sender), 42}));
            ++consumed;
        }
    }
    for (auto& sender : senders)
        sender.join();
    Check(consumed == Producers * Items);
    int a = 0, b = 0;
    Check(PlayerbotThreadOwnership::IsOwnerContext(&a, &a, false));
    Check(!PlayerbotThreadOwnership::IsOwnerContext(&a, &b, true));
    Check(PlayerbotThreadOwnership::IsOwnerContext(nullptr, &b, true));
    Check(!PlayerbotThreadOwnership::IsOwnerContext(nullptr, &b, false));
    std::cout << "PASS: production inbox, 80000 packet/command items, FIFO, saturation, owner contexts\n";
}
