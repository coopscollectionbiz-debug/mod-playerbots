// Offline concurrency test for CityLifeRequestQueue (no server dependencies).
// Build: cl /std:c++20 /EHsc /W4 /WX /I src/Ai/World/Rpg apps/tests/citylife_request_queue_test.cpp
#include <atomic>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "CityLifeRequestQueue.h"

namespace
{
void Check(bool condition, char const* what)
{
    if (!condition)
        throw std::runtime_error(what);
}

void SingleThreaded()
{
    CityLifeRequestQueue queue;
    Check(queue.Push(1, 1519), "first push accepted");
    Check(!queue.Push(1, 1519), "duplicate (player, zone) coalesced");
    Check(queue.Push(1, 1637), "same player, other zone accepted");
    Check(queue.Push(2, 1519), "other player accepted");
    auto taken = queue.Take();
    Check(taken.size() == 3, "take returns every distinct request");
    Check(queue.Take().empty(), "take empties the queue");
    Check(queue.Push(1, 1519), "request accepted again after drain");

    CityLifeRequestQueue bounded;
    for (std::uint64_t i = 0; i < CityLifeRequestQueue::Capacity; ++i)
        Check(bounded.Push(i, 1519), "within capacity");
    Check(!bounded.Push(CityLifeRequestQueue::Capacity, 1519), "capacity bound enforced");
    Check(bounded.Take().size() == CityLifeRequestQueue::Capacity, "bounded take size");
}

// Map threads push distinct (player, zone) pairs while the world thread drains concurrently.
// Every pushed request must come out exactly once.
void Concurrent()
{
    constexpr int Producers = 4;
    constexpr int PerProducer = 20000;
    CityLifeRequestQueue queue;
    std::atomic<int> producersDone{0};
    std::atomic<std::uint64_t> accepted{0};
    std::set<std::pair<std::uint64_t, std::uint32_t>> seen;
    std::uint64_t drained = 0;

    std::thread consumer([&]()
    {
        while (true)
        {
            bool finished = producersDone.load() == Producers;
            for (auto const& request : queue.Take())
            {
                Check(seen.emplace(request.player, request.zone).second, "request delivered twice");
                ++drained;
            }
            if (finished)
                break;
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < Producers; ++p)
        producers.emplace_back([&, p]()
        {
            for (int i = 0; i < PerProducer; ++i)
            {
                std::uint64_t player = static_cast<std::uint64_t>(p) * PerProducer + i;
                // Retry while the queue is full: the consumer keeps draining.
                while (!queue.Push(player, 1519))
                    std::this_thread::yield();
                accepted.fetch_add(1);
            }
            producersDone.fetch_add(1);
        });

    for (auto& producer : producers)
        producer.join();
    consumer.join();
    for (auto const& request : queue.Take())
    {
        Check(seen.emplace(request.player, request.zone).second, "request delivered twice");
        ++drained;
    }

    Check(accepted.load() == static_cast<std::uint64_t>(Producers) * PerProducer, "all pushes accepted");
    Check(drained == accepted.load(), "every accepted request drained exactly once");
}
}

int main()
{
    try
    {
        SingleThreaded();
        Concurrent();
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }

    std::cout << "PASS: CityLifeRequestQueue single-threaded and concurrent checks\n";
    return 0;
}
