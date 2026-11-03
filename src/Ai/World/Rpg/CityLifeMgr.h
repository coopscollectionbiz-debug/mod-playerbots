/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CITYLIFEMGR_H
#define PLAYERBOTS_CITYLIFEMGR_H

#include <array>
#include <atomic>
#include <map>
#include <vector>

#include "CityLifePolicy.h"
#include "EventMap.h"
#include "ObjectGuid.h"
#include "Position.h"

class Player;
class WorldLocation;

class CityLifeMgr
{
public:
    enum class Metric : std::size_t
    {
        Admissions,
        Teleports,
        MovementStarts,
        Resumptions,
        DestinationFailures,
        OrdinaryMovement,
        MovingVisibility,
        Count
    };

    static CityLifeMgr& Instance();
    void OnEntry(Player* player, std::uint32_t zone);
    void Update(std::uint32_t diff);
    bool IsObserved(std::uint32_t zone, std::uint32_t team) const;
    void Count(Metric metric);

private:
    void Process();
    void RefreshPopulation(std::uint32_t now, std::vector<Player*> const& observers);
    void Admit(std::uint32_t started, std::vector<Player*> const& observers, bool maintenanceDue);
    static bool Eligible(Player* bot);
    static bool Hidden(WorldLocation const& destination, Player* source, std::vector<Player*> const& observers);

    // Map threads publish only bits/counters. All other members are world-thread owned,
    // processed by OnWorldUpdate after MapMgr joins map workers. Snapshots retain GUIDs/values only.
    std::atomic<std::uint32_t> _requests{0};
    std::atomic<std::uint32_t> _observed{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Metric::Count)> _metrics{};
    std::uint32_t _activated = 0;
    std::uint32_t _cursor = 0;
    std::uint32_t _cycle = 0;
    std::array<CityLifePolicy::Activation, CityLifePolicy::CityTeamCount> _activation;
    std::array<std::uint32_t, CityLifePolicy::CityTeamCount> _assigned{};
    std::array<std::uint32_t, CityLifePolicy::CityTeamCount> _natural{};
    std::array<std::vector<ObjectGuid>, CityLifePolicy::TeamCount> _candidates;
    std::vector<WorldLocation> _occupied;
    std::uint32_t _online = 0;
    std::uint32_t _globalAssigned = 0;
    std::uint32_t _pending = 0;
    std::uint32_t _residents = 0;
    std::uint32_t _active = 0;
    std::uint32_t _managedCount = 0;
    std::uint32_t _snapshotStart = 0;
    std::uint32_t _maintenanceStart = 0;
    bool _snapshotValid = false;
    std::uint32_t _diagnosticCycles = 0;
    std::uint64_t _processingTotal = 0;
    std::uint32_t _processingMax = 0;
    std::map<std::uint64_t, std::array<bool, 2>> _samples;
    std::map<std::uint64_t, std::uint32_t> _arrivalFailures;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> _placementFailures;
    bool _scheduled = false;
    EventMap _events;
};

#endif
