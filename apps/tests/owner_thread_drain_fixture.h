/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Minimal dependencies for executing the actual PlayerbotAI drain and owner-gate function bodies.
// Lifecycle destruction is tested separately using the actual world-batch function.
#include "PlayerbotThreadOwnership.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using uint64 = std::uint64_t;
using uint32 = std::uint32_t;
using ObjectGuid = int;
#define PLAYERBOT_CHECK_OWNER_THREAD(bot) ((void)0)
#define LOG_ERROR(...) ((void)0)

class Player
{
public:
    bool teleporting = false;
    bool owner = true;
    bool IsBeingTeleported() const { return teleporting; }
    ObjectGuid GetGUID() const { return 1; }
};

bool PlayerbotThreadOwnership::IsOwnerContextFor(Player const* bot) { return bot->owner; }

struct PlayerbotAI
{
    struct OwnerThreadWork
    {
        enum class Kind { Command, Packet };
        Kind kind = Kind::Command;
        std::string text;
        int sequence = 0;
    };
    Player player;
    Player* bot = &player;
    uint64 ownerThreadToken = 1;
    OwnerThreadInbox<OwnerThreadWork> ownerThreadInbox;
    std::atomic<uint32> worldThreadBatches{0};
    bool ownerThreadDraining = false;
    std::vector<int> executed;
    bool RunsOnOwnerThread() const;
    void DrainOwnerThreadWork();
    bool IsLifecycleCommand(std::string const& text) { return text == "logout"; }
    void Submit(OwnerThreadWork work)
    {
        if (!RunsOnOwnerThread() || IsLifecycleCommand(work.text))
        {
            if (!ownerThreadInbox.Push(std::move(work)))
                throw std::runtime_error("unexpected saturation");
            return;
        }
        ExecuteOwnerThreadWork(work);
    }
    void ExecuteOwnerThreadWork(OwnerThreadWork& work)
    {
        executed.push_back(work.sequence);
        if (work.text == "reenter")
            Submit({OwnerThreadWork::Kind::Command, "later", 3});
        if (work.text == "teleport")
            player.teleporting = true;
    }
};

PlayerbotAI* currentAI = nullptr;
struct OwnerThreadWorkOperation
{
    uint64 token;
    std::vector<PlayerbotAI::OwnerThreadWork> work;
    OwnerThreadWorkOperation(ObjectGuid, uint64 generation, std::vector<PlayerbotAI::OwnerThreadWork> items)
        : token(generation), work(std::move(items)) { }
    void Execute()
    {
        if (currentAI->ownerThreadToken != token)
            return;
        for (auto& item : work)
            currentAI->ExecuteOwnerThreadWork(item);
        --currentAI->worldThreadBatches;
    }
};

struct PlayerbotWorldThreadProcessor
{
    bool reject = false;
    std::vector<std::unique_ptr<OwnerThreadWorkOperation>> queued;
    static PlayerbotWorldThreadProcessor& instance()
    {
        static PlayerbotWorldThreadProcessor processor;
        return processor;
    }
    bool QueueOperation(std::unique_ptr<OwnerThreadWorkOperation> operation)
    {
        if (reject)
            return false;
        queued.push_back(std::move(operation));
        return true;
    }
    void Run()
    {
        auto batch = std::move(queued);
        queued.clear();
        for (auto& operation : batch)
            operation->Execute();
    }
};

void Check(bool condition)
{
    if (!condition)
        throw std::runtime_error("production drain contract failed");
}

int main()
{
    using Kind = PlayerbotAI::OwnerThreadWork::Kind;
    auto& world = PlayerbotWorldThreadProcessor::instance();
    {
        PlayerbotAI ai;
        currentAI = &ai;
        Check(ai.ownerThreadInbox.Push({Kind::Command, "reenter", 1}));
        Check(ai.ownerThreadInbox.Push({Kind::Packet, "packet", 2}));
        ai.DrainOwnerThreadWork();
        Check(ai.executed == std::vector<int>({1, 2}));
        Check(!ai.ownerThreadDraining && ai.ownerThreadInbox.HasPending());
        ai.DrainOwnerThreadWork();
        Check(ai.executed == std::vector<int>({1, 2, 3}));
    }
    {
        PlayerbotAI ai;
        currentAI = &ai;
        Check(ai.ownerThreadInbox.Push({Kind::Command, "first", 1}));
        Check(ai.ownerThreadInbox.Push({Kind::Command, "logout", 2}));
        Check(ai.ownerThreadInbox.Push({Kind::Packet, "packet", 3}));
        ai.DrainOwnerThreadWork();
        ai.Submit({Kind::Command, "behind world batch", 4});
        ai.DrainOwnerThreadWork();
        Check(ai.executed == std::vector<int>({1}) && ai.worldThreadBatches == 2);
        world.Run();
        Check(ai.executed == std::vector<int>({1, 2, 3, 4}) && ai.worldThreadBatches == 0);
    }
    {
        PlayerbotAI ai;
        currentAI = &ai;
        Check(ai.ownerThreadInbox.Push({Kind::Command, "teleport", 1}));
        Check(ai.ownerThreadInbox.Push({Kind::Packet, "packet", 2}));
        ai.DrainOwnerThreadWork();
        Check(ai.executed == std::vector<int>({1}) && ai.worldThreadBatches == 1);
        world.Run();
        Check(ai.executed == std::vector<int>({1, 2}));
    }
    {
        PlayerbotAI ai;
        currentAI = &ai;
        world.reject = true;
        Check(ai.ownerThreadInbox.Push({Kind::Command, "logout", 1}));
        ai.DrainOwnerThreadWork();
        Check(ai.worldThreadBatches == 0 && !ai.ownerThreadDraining);
        world.reject = false;
        ai.Submit({Kind::Command, "recovered", 2});
        Check(ai.executed == std::vector<int>({2}));
    }
    std::cout << "PASS: production drain, reentrancy, packet FIFO, world batches, teleport, rejection recovery\n";
}
