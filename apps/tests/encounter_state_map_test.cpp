// Offline concurrency tests for EncounterStateMap / EncounterSharedStateMap / EncounterStateSet.
// Build: cl /std:c++20 /EHsc /W4 /WX /O2 /I src/Util apps/tests/encounter_state_map_test.cpp
// ASan:  add /fsanitize=address /Zi (MSVC has no ThreadSanitizer; passing runs do not prove race freedom).
//
// Threads model map threads. Each owns its "instances" (instance-id keys) and "bots" (GUID keys), as the
// raid helpers do, while all of them insert, look up and erase in the same shared containers.
//
// Run with --negative-control to perform the one access the handle contract forbids (use a handle after
// its own key was erased). Under ASan that must report heap-use-after-free; it shows the sanitizer is live.
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "EncounterStateMap.h"

namespace
{
void Check(bool condition, char const* what)
{
    if (!condition)
        throw std::runtime_error(what);
}

constexpr int Threads = 8;

std::uint32_t InstanceKey(int thread, std::uint32_t n) { return static_cast<std::uint32_t>(thread) * 100000u + n; }

void Semantics()
{
    EncounterStateMap<std::uint32_t, std::uint32_t> timers;
    Check(timers.find(1) == timers.end(), "missing key finds end()");
    Check(timers.try_emplace(1, 10u).second, "try_emplace inserts");
    Check(!timers.try_emplace(1, 20u).second, "try_emplace keeps existing");
    Check(timers.find(1)->second == 10, "value kept");
    timers[1] += 5;
    Check(timers.find(1)->second == 15, "operator[] returns the stored value");
    Check(!timers.insert_or_assign(1, 3u).second && timers.find(1)->second == 3, "insert_or_assign assigns");
    Check(timers.count(1) == 1 && timers.contains(1), "count/contains");
    timers.erase(timers.find(1));
    Check(!timers.contains(1), "erase(handle)");
    Check(timers.erase(1) == 0, "erase of missing key");

    EncounterSharedStateMap<std::uint64_t, int> shared;
    Check(!shared.get(5), "shared get of missing key");
    Check(shared.try_emplace(5, 1) && !shared.try_emplace(5, 2), "shared try_emplace");
    Check(shared.get(5).value() == 1, "shared get copies value");
    shared.insert_or_assign(5, 7);
    Check(shared.get(5).value() == 7 && shared.contains(5), "shared insert_or_assign");
    Check(shared.erase(5) == 1 && !shared.get(5), "shared erase");

    EncounterStateSet<std::uint32_t> applied;
    Check(applied.insert(7) && !applied.insert(7), "set insert");
    Check(applied.contains(7) && applied.count(7) == 1, "set membership");
    Check(applied.erase(7) == 1 && !applied.contains(7), "set erase");
}

// Concurrent insertion and rehashing: owners keep using handles while other threads grow the table.
void InsertionAndRehash()
{
    constexpr int Rounds = 20000;
    constexpr std::uint32_t Owned = 64;
    EncounterStateMap<std::uint32_t, std::uint64_t> state;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            for (int r = 0; r < Rounds; ++r)
            {
                std::uint32_t const key = InstanceKey(t, static_cast<std::uint32_t>(r) % Owned);
                std::uint64_t& value = state[key];
                // Grow and shrink the table while the reference is held.
                state.try_emplace(InstanceKey(t, 50000u + static_cast<std::uint32_t>(r)), 1u);
                value += 1;
                auto handle = state.find(key);
                if (handle == state.end() || &handle->second != &value)
                    errors.fetch_add(1);
                if (r % 3 == 0)
                    state.erase(InstanceKey(t, 50000u + static_cast<std::uint32_t>(r)));
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(errors.load() == 0, "handles stay valid across other threads' inserts and rehashes");
    for (int t = 0; t < Threads; ++t)
    {
        std::uint64_t total = 0;
        for (std::uint32_t k = 0; k < Owned; ++k)
            total += state.find(InstanceKey(t, k))->second;
        Check(total == Rounds, "no lost updates on owned keys");
    }
}

// Erase while another thread holds a handle: other threads erase *their own* keys (the only erase the
// contract allows) while each owner holds and reuses a handle to its key across those erases.
void EraseWhileHandleHeld()
{
    constexpr int Rounds = 20000;
    EncounterStateMap<std::uint32_t, std::uint32_t> state;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            std::uint32_t const held = InstanceKey(t, 0);
            auto handle = state.try_emplace(held, 0u).first;
            for (int r = 0; r < Rounds; ++r)
            {
                std::uint32_t const temp = InstanceKey(t, 1u + static_cast<std::uint32_t>(r) % 32u);
                state[temp] = static_cast<std::uint32_t>(r);
                state.erase(temp);
                handle->second += 1;  // still valid: only other keys were erased
            }
            if (state.find(held)->second != static_cast<std::uint32_t>(Rounds))
                errors.fetch_add(1);
        });
    for (auto& thread : threads)
        thread.join();
    Check(errors.load() == 0, "a held handle survives concurrent erases of other keys");
}

// Cross-player state: one thread reads/writes a key another thread keeps erasing and recreating. Only
// EncounterSharedStateMap may be used for this; every value seen must be a complete, valid value.
void CrossPlayerSharedState()
{
    constexpr int Rounds = 50000;
    struct Lineup { float x, y, z; std::uint32_t check; };
    EncounterSharedStateMap<std::uint64_t, Lineup> lineup;
    std::atomic<bool> stop{false};
    std::atomic<int> torn{0};
    std::uint64_t const passer = 42;

    std::thread owner([&]()
    {
        for (int r = 0; r < Rounds; ++r)
        {
            float const v = static_cast<float>(r);
            lineup.try_emplace(passer, Lineup{ v, v, v, static_cast<std::uint32_t>(r) });
            if (auto own = lineup.get(passer))
                if (own->x != own->y || own->y != own->z || static_cast<std::uint32_t>(own->x) != own->check)
                    torn.fetch_add(1);
            lineup.insert_or_assign(passer, Lineup{ v + 1, v + 1, v + 1, static_cast<std::uint32_t>(r + 1) });
        }
        stop = true;
    });
    std::vector<std::thread> others;
    for (int t = 0; t < 3; ++t)
        others.emplace_back([&, t]()
        {
            while (!stop)
            {
                if (t == 0)
                    lineup.erase(passer);  // the generator action erasing every handler's entry
                else if (auto seen = lineup.get(passer))
                    if (seen->x != seen->y || seen->y != seen->z || static_cast<std::uint32_t>(seen->x) != seen->check)
                        torn.fetch_add(1);
            }
        });
    owner.join();
    for (auto& thread : others)
        thread.join();
    Check(torn.load() == 0, "cross-player reads never see a torn or freed value");
}

// Instance teardown and recreation: a reset erases an instance's state; the same instance id is then
// reused (as MapMgr reuses freed ids) and must start from a fresh value, while other instances churn.
void InstanceTeardownAndRecreation()
{
    constexpr int Rounds = 5000;
    struct State { std::uint32_t timer = 0; bool collapsed = false; std::vector<int> orbs; };
    EncounterStateMap<std::uint32_t, State> states;
    EncounterStateSet<std::uint32_t> collapse;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            std::uint32_t const instance = InstanceKey(t, 7);
            for (int r = 0; r < Rounds; ++r)
            {
                State& state = states[instance];
                if (state.timer != 0 || state.collapsed || !state.orbs.empty())
                    errors.fetch_add(1);  // recreated state must be fresh
                state.timer = static_cast<std::uint32_t>(r) + 1;
                state.collapsed = true;
                state.orbs.assign(8, r);
                collapse.insert(instance);
                // Encounter reset / instance unload.
                if (states.erase(instance) != 1 || collapse.erase(instance) != 1)
                    errors.fetch_add(1);
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(errors.load() == 0, "teardown removes state and a reused instance id starts fresh");
}

// Bot logout and cleanup: GUID-keyed entries are created by each bot and erased when it logs out (or by
// its own reset action); re-logins reuse the GUID. Erasing a missing GUID is harmless.
void BotLogoutAndCleanup()
{
    constexpr int Rounds = 10000;
    EncounterStateMap<std::uint64_t, std::uint8_t> steps;
    EncounterStateSet<std::uint64_t> reached;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            for (int r = 0; r < Rounds; ++r)
            {
                std::uint64_t const guid = (static_cast<std::uint64_t>(t) << 32) | static_cast<std::uint32_t>(r % 25);
                std::uint8_t& step = steps.try_emplace(guid, std::uint8_t(0)).first->second;
                step = static_cast<std::uint8_t>(step + 1);
                reached.insert(guid);
                if (r % 4 == 3)
                {
                    // Logout: cleanup runs for this bot only.
                    steps.erase(guid);
                    reached.erase(guid);
                    if (steps.contains(guid) || reached.contains(guid) || steps.erase(guid) != 0)
                        errors.fetch_add(1);
                }
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(errors.load() == 0, "logout cleanup removes only that bot's entries");
}

// Nested mutable containers (Magtheridon cube assignments, Void Reaver orbs): the owning instance mutates
// the inner container through its handle while other instances insert into and erase from the outer one.
void NestedMutableContainers()
{
    constexpr int Rounds = 10000;
    EncounterStateMap<std::uint32_t, std::unordered_map<std::uint64_t, int>> cubes;
    EncounterStateMap<std::uint32_t, std::vector<int>> orbs;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            std::uint32_t const instance = InstanceKey(t, 3);
            for (int r = 0; r < Rounds; ++r)
            {
                auto& assignment = cubes[instance];
                assignment[static_cast<std::uint64_t>(r % 5)] = r;
                auto& list = orbs[instance];
                list.push_back(r);
                if (list.size() > 16)
                    list.erase(list.begin(), list.begin() + 8);
                // Outer-container churn from this "map thread".
                cubes.try_emplace(InstanceKey(t, 1000u + static_cast<std::uint32_t>(r)));
                cubes.erase(InstanceKey(t, 1000u + static_cast<std::uint32_t>(r)));
                if (assignment.size() > 5 || list.empty() || list.back() != r)
                    errors.fetch_add(1);
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(errors.load() == 0, "nested containers stay consistent under outer churn");
}

// Repeated access from multiple map threads to read-only lookups of absent keys (continent bots asking
// about instance 0) while others write: lookups must not insert.
void RepeatedAbsentLookups()
{
    EncounterStateMap<std::uint32_t, std::uint32_t> timers;
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([&, t]()
        {
            for (int r = 0; r < 20000; ++r)
            {
                (void)timers.find(0);
                (void)timers.contains(0);
                (void)timers.count(0);
                timers.insert_or_assign(InstanceKey(t, 1), static_cast<std::uint32_t>(r));
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(!timers.contains(0), "lookups never insert");
}

int NegativeControl()
{
    // Single-threaded and deterministic: the forbidden pattern (handle used after its own key is erased).
    EncounterStateMap<std::uint32_t, std::uint64_t> state;
    auto handle = state.try_emplace(1, 1u).first;
    state.erase(1);
    volatile std::uint64_t read = handle->second;  // ASan: heap-use-after-free
    return static_cast<int>(read & 1);
}
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--negative-control") == 0)
        return NegativeControl();

    try
    {
        Semantics();
        InsertionAndRehash();
        EraseWhileHandleHeld();
        CrossPlayerSharedState();
        InstanceTeardownAndRecreation();
        BotLogoutAndCleanup();
        NestedMutableContainers();
        RepeatedAbsentLookups();
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }

    std::cout << "PASS: EncounterStateMap semantics and 7 concurrency scenarios\n";
    return 0;
}
