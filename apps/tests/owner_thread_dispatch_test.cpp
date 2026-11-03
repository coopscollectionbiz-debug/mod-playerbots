/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Offline test of the owner-thread dispatch used by PlayerbotAI::HandleCommand / packet handlers.
// Build: cl /std:c++20 /EHsc /W4 /WX /O2 /I src/Bot /I src/Util apps/tests/owner_thread_dispatch_test.cpp
// ASan:  add /fsanitize=address /Zi. MSVC has no ThreadSanitizer: passing runs do not prove race freedom.
//
// PlayerbotAI needs a server, so ModelBot reproduces its dispatch rules with the production pieces
// (OwnerThreadInbox, PlayerbotThreadOwnership::IsOwnerContext, EncounterStateMap):
//   run now only if nothing is queued, no world batch is pending and the thread owns the bot;
//   otherwise queue; drain in order at the start of the bot's update on its map thread;
//   logout and everything after it go to the world thread as one batch, tagged with the AI's token.
// apps/tests/test_owner_thread_dispatch.py checks the real code follows the same rules.
//
// Threads: several "map threads" each update their maps between barriers; a "world thread" runs between
// map phases, as World::Update does around MapMgr::Update.
#include <atomic>
#include <barrier>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "EncounterStateMap.h"
#include "PlayerbotThreadOwnership.h"

namespace
{
void Check(bool condition, char const* what)
{
    if (!condition)
        throw std::runtime_error(what);
}

struct FakeMap { int id; };

thread_local FakeMap const* tUpdatingMap = nullptr;   // stands in for Map::GetUpdatingMapOfCurrentThread()
thread_local bool tWorldThread = false;               // stands in for PlayerbotThreadOwnership::IsWorldThread()

struct Command
{
    std::string text;
    int sender;
    int seq;
};

struct ModelBot;

// World-thread batches (PlayerbotWorldThreadProcessor stand-in, FIFO).
struct WorldQueue
{
    struct Batch { ModelBot* bot; std::uint64_t token; std::vector<Command> work; };
    std::mutex lock;
    std::vector<Batch> batches;
};

struct Execution
{
    int seq;
    FakeMap const* map;      // map being updated when it ran (nullptr = world thread)
    bool world;
};

struct ModelBot
{
    explicit ModelBot(int botId, FakeMap const* startMap, WorldQueue& queue, std::uint64_t aiToken)
        : id(botId), map(startMap), world(queue), token(aiToken) { }

    int id;
    std::atomic<FakeMap const*> map;
    WorldQueue& world;
    std::uint64_t token;
    std::atomic<int> master{0};
    bool loggedOut = false;
    OwnerThreadInbox<Command> inbox;
    std::atomic<std::uint32_t> worldBatches{0};
    std::vector<Execution> executed;          // touched only by the owner context (that is the point)
    std::atomic<int> violations{0};

    bool Owner() const
    {
        return PlayerbotThreadOwnership::IsOwnerContext(tUpdatingMap, map.load(), tWorldThread && !tUpdatingMap);
    }

    bool RunsOnOwnerThread() const { return !inbox.HasPending() && worldBatches.load() == 0 && Owner(); }
    static bool IsLifecycle(std::string const& text) { return text == "logout" || text == "logout cancel"; }

    // PlayerbotAI::HandleCommand
    void HandleCommand(Command command)
    {
        if (!RunsOnOwnerThread() || IsLifecycle(command.text))
        {
            Check(inbox.Push(std::move(command)), "inbox capacity");
            return;
        }
        Execute(command);
    }

    // PlayerbotAI::HandleCommandOnOwnerThread (security re-checked here, at execution time)
    void Execute(Command const& command)
    {
        if (!Owner())
            violations.fetch_add(1);
        if (command.sender != master.load())
            return;  // sender is no longer allowed: dropped
        if (command.text == "logout")
            loggedOut = true;
        executed.push_back({ command.seq, tUpdatingMap, tWorldThread && !tUpdatingMap });
    }

    // Start of PlayerbotAI::UpdateAI, on the bot's map thread
    void Update()
    {
        if (!inbox.HasPending())
            return;

        std::vector<Command> work = inbox.Take();
        for (std::size_t i = 0; i < work.size(); ++i)
        {
            if (worldBatches.load() == 0 && !IsLifecycle(work[i].text))
            {
                Execute(work[i]);
                continue;
            }
            std::vector<Command> rest(work.begin() + static_cast<std::ptrdiff_t>(i), work.end());
            worldBatches.fetch_add(1);
            std::lock_guard<std::mutex> guard(world.lock);
            world.batches.push_back({ this, token, std::move(rest) });
            return;
        }
    }

    // PlayerbotAI::ExecuteWorldThreadBatch
    void ExecuteWorldBatch(std::uint64_t batchToken, std::vector<Command>& work)
    {
        if (batchToken != token)
            return;
        for (Command const& command : work)
            Execute(command);
        worldBatches.fetch_sub(1);
    }
};

void RunWorldBatches(WorldQueue& world)
{
    std::vector<WorldQueue::Batch> batches;
    {
        std::lock_guard<std::mutex> guard(world.lock);
        batches.swap(world.batches);
    }
    for (auto& batch : batches)
        batch.bot->ExecuteWorldBatch(batch.token, batch.work);
}

std::vector<int> Sequence(ModelBot const& bot)
{
    std::vector<int> seq;
    for (Execution const& e : bot.executed)
        seq.push_back(e.seq);
    return seq;
}

// 1. Same map: a command delivered on the bot's own map thread runs at once.
void SameMap()
{
    WorldQueue world;
    FakeMap a{1};
    ModelBot bot(1, &a, world, 1);
    bot.master = 100;
    tUpdatingMap = &a;
    bot.HandleCommand({ "follow", 100, 1 });
    Check(bot.executed.size() == 1 && bot.executed[0].map == &a, "same-map command runs immediately");
    Check(!bot.inbox.HasPending(), "nothing queued for same-map command");
    tUpdatingMap = nullptr;
}

// 2/3. Different maps and a teleport between enqueue and execution, with real threads and barriers.
void CrossMapAndTeleport()
{
    WorldQueue world;
    FakeMap a{1}, b{2}, c{3};
    ModelBot bot(1, &a, world, 1);
    bot.master = 100;
    std::barrier sync(3);
    std::thread mapA([&]()
    {
        tUpdatingMap = &a;
        // Do not drain during the enqueue-only phase: that would race the assertion on map C.
        sync.arrive_and_wait();                     // C sends during tick 1
        sync.arrive_and_wait();                     // world teleports bot to B
        sync.arrive_and_wait();
        tUpdatingMap = nullptr;
    });
    std::thread mapC([&]()
    {
        tUpdatingMap = &c;
        bot.HandleCommand({ "do attack", 100, 1 });  // bot is on A: not the owner
        bot.HandleCommand({ "follow", 100, 2 });
        Check(bot.executed.empty(), "cross-map commands are queued, not run on C's thread");
        sync.arrive_and_wait();
        sync.arrive_and_wait();
        sync.arrive_and_wait();
        tUpdatingMap = nullptr;
    });
    sync.arrive_and_wait();
    bot.map = &b;                                    // teleport (world thread, between updates)
    sync.arrive_and_wait();
    std::thread mapB([&]()
    {
        tUpdatingMap = &b;
        bot.Update();                               // drained on the new owner's thread
        tUpdatingMap = nullptr;
    });
    mapB.join();
    sync.arrive_and_wait();
    mapA.join();
    mapC.join();
    Check(Sequence(bot) == std::vector<int>({ 1, 2 }), "queued commands keep their order");
    for (Execution const& e : bot.executed)
        Check(e.map == &b, "commands ran on the map the bot was on when it updated");
    Check(bot.violations == 0, "no execution off the owner thread");
}

// 4. Logout before execution, and a relogged bot (new AI token) not replaying the old batch.
void LogoutAndRemoval()
{
    WorldQueue world;
    FakeMap a{1}, c{3};
    {
        auto bot = std::make_unique<ModelBot>(1, &a, world, 7);
        bot->master = 100;
        tUpdatingMap = &c;
        bot->HandleCommand({ "follow", 100, 1 });
        tUpdatingMap = nullptr;
        bot.reset();  // bot removed before its next update: the inbox dies with the AI, nothing runs
    }

    ModelBot bot(2, &a, world, 8);
    bot.master = 100;
    tUpdatingMap = &c;
    bot.HandleCommand({ "follow", 100, 1 });
    bot.HandleCommand({ "logout", 100, 2 });
    bot.HandleCommand({ "stay", 100, 3 });
    tUpdatingMap = &a;
    bot.Update();
    Check(Sequence(bot) == std::vector<int>({ 1 }), "items before logout run on the map thread");
    Check(bot.worldBatches == 1, "logout and later items handed to the world thread");
    Check(!bot.loggedOut, "logout never runs inside the map update");
    bot.HandleCommand({ "follow", 100, 4 });         // on the owner thread, but behind the pending batch
    Check(bot.inbox.HasPending(), "new work queues behind a pending world batch");
    tUpdatingMap = nullptr;
    tWorldThread = true;
    RunWorldBatches(world);
    tWorldThread = false;
    Check(bot.loggedOut && Sequence(bot) == std::vector<int>({ 1, 2, 3 }), "world batch keeps the order");
    Check(bot.executed[1].world, "logout ran on the world thread between map updates");

    // Relog: a batch built for token 9 reaches an AI with token 10.
    ModelBot relogged(3, &a, world, 10);
    relogged.master = 100;
    std::vector<Command> stale{ { "logout", 100, 1 } };
    relogged.worldBatches = 1;
    tWorldThread = true;
    relogged.ExecuteWorldBatch(9, stale);
    tWorldThread = false;
    Check(relogged.executed.empty(), "a relogged bot does not replay the old AI's batch");
}

// 5/6. Many senders on several map threads, many bots: per-sender order kept, each command once.
void OrderingAndFanOut()
{
    constexpr int Bots = 16;
    constexpr int Senders = 4;
    constexpr int PerSender = 200;
    WorldQueue world;
    std::vector<FakeMap> maps{ {1}, {2}, {3}, {4} };
    std::vector<std::unique_ptr<ModelBot>> bots;
    for (int i = 0; i < Bots; ++i)
    {
        bots.push_back(std::make_unique<ModelBot>(i, &maps[static_cast<std::size_t>(i % 4)], world, 100u + i));
        bots.back()->master = 1000;
    }
    std::barrier phase(Senders);
    std::vector<std::thread> threads;
    for (int s = 0; s < Senders; ++s)
        threads.emplace_back([&, s]()
        {
            FakeMap const* own = &maps[static_cast<std::size_t>(s)];
            for (int tick = 0; tick < PerSender; ++tick)
            {
                tUpdatingMap = own;
                // Update this thread's bots, then (as a bot on this map) send one command to every bot.
                for (auto& bot : bots)
                    if (bot->map.load() == own)
                        bot->Update();
                for (auto& bot : bots)
                    bot->HandleCommand({ "cmd", 1000, s * 100000 + tick });
                tUpdatingMap = nullptr;
                phase.arrive_and_wait();
            }
            tUpdatingMap = own;
            for (auto& bot : bots)
                if (bot->map.load() == own)
                    bot->Update();
            tUpdatingMap = nullptr;
        });
    for (auto& thread : threads)
        thread.join();
    for (auto& bot : bots)
    {
        bot->Update();  // map threads are gone; nothing left that is not the owner (bots drained above)
        Check(bot->violations == 0, "fan-out never ran a command off its owner thread");
        Check(bot->executed.size() == static_cast<std::size_t>(Senders * PerSender), "each command ran once");
        std::vector<int> last(Senders, -1);
        for (Execution const& e : bot->executed)
        {
            int const sender = e.seq / 100000;
            int const n = e.seq % 100000;
            Check(n > last[static_cast<std::size_t>(sender)], "per-sender order kept for every bot");
            last[static_cast<std::size_t>(sender)] = n;
        }
    }
}

// 7. Permissions change while a command is queued: it is re-checked when it runs and dropped.
void PermissionChange()
{
    WorldQueue world;
    FakeMap a{1}, c{3};
    ModelBot bot(1, &a, world, 1);
    bot.master = 100;
    tUpdatingMap = &c;
    bot.HandleCommand({ "do attack", 100, 1 });
    tUpdatingMap = nullptr;
    bot.master = 200;  // ownership changed before the bot's update
    tUpdatingMap = &a;
    bot.Update();
    tUpdatingMap = nullptr;
    Check(bot.executed.empty(), "command from a no-longer-allowed sender is dropped at execution");
}

void WorldChatLifecycle()
{
    WorldQueue world;
    FakeMap map{1};
    ModelBot bot(1, &map, world, 1);
    bot.master = 100;
    tWorldThread = true;
    bot.HandleCommand({"logout", 100, 1});
    Check(!bot.loggedOut && bot.inbox.HasPending(), "world chat callback retains a live receiver");
    tWorldThread = false;
    tUpdatingMap = &map;
    bot.Update();
    tUpdatingMap = nullptr;
    tWorldThread = true;
    RunWorldBatches(world);
    tWorldThread = false;
    Check(bot.loggedOut, "lifecycle executes after its chat caller returns");
}

// 8. Raid actions reached through commands touch EncounterStateMap only from the key's owner thread.
void RaidStateFromOwnerThread()
{
    constexpr int Instances = 4;
    constexpr int Ticks = 500;
    WorldQueue world;
    std::vector<FakeMap> maps{ {11}, {12}, {13}, {14} };
    std::vector<std::unique_ptr<ModelBot>> bots;
    for (int i = 0; i < Instances; ++i)
    {
        bots.push_back(std::make_unique<ModelBot>(i, &maps[static_cast<std::size_t>(i)], world, 200u + i));
        bots.back()->master = 1;
    }
    struct Owned { std::thread::id owner; std::uint32_t count = 0; };
    EncounterStateMap<int, Owned> raidState;   // keyed by instance id, like the raid helpers
    std::atomic<int> foreign{0};
    auto raidAction = [&](ModelBot& bot)
    {
        // What a "do <raid action>" ends up doing: read/modify this instance's state through a handle.
        Owned& state = raidState[bot.map.load()->id];
        if (state.count == 0)
            state.owner = std::this_thread::get_id();
        else if (state.owner != std::this_thread::get_id())
            foreign.fetch_add(1);
        ++state.count;
    };
    std::barrier phase(Instances);
    std::vector<std::thread> threads;
    for (int t = 0; t < Instances; ++t)
        threads.emplace_back([&, t]()
        {
            ModelBot& mine = *bots[static_cast<std::size_t>(t)];
            for (int tick = 0; tick < Ticks; ++tick)
            {
                tUpdatingMap = &maps[static_cast<std::size_t>(t)];
                // Drain: queued commands execute here, on the owning map thread.
                std::vector<Command> work = mine.inbox.Take();
                for (Command const& command : work)
                    if (command.sender == mine.master.load())
                        raidAction(mine);
                // Send a raid command to every other instance's bot (cross-map) and to our own.
                for (auto& bot : bots)
                {
                    if (bot.get() == &mine)
                        raidAction(mine);                       // same map: immediate
                    else
                        Check(!bot->RunsOnOwnerThread(), "another instance's bot is never owned here"),
                            bot->inbox.Push({ "do raid", 1, tick });
                }
                tUpdatingMap = nullptr;
                phase.arrive_and_wait();
            }
        });
    for (auto& thread : threads)
        thread.join();
    Check(foreign == 0, "raid state was only touched by its instance's map thread");
}
}

int main()
{
    try
    {
        SameMap();
        CrossMapAndTeleport();
        LogoutAndRemoval();
        OrderingAndFanOut();
        PermissionChange();
        WorldChatLifecycle();
        RaidStateFromOwnerThread();
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }

    std::cout << "PASS: owner-thread dispatch, 9 scenarios\n";
    return 0;
}
