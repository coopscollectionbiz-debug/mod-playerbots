/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CityLifeMgr.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Creature.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "TravelMgr.h"
#include "UpdateTime.h"
#include "WorldSession.h"

CityLifeMgr& CityLifeMgr::Instance()
{
    static CityLifeMgr instance;
    return instance;
}

void CityLifeMgr::OnEntry(Player* player, uint32 zone)
{
    int index = CityLifePolicy::CityIndex(zone);
    if (index < 0 || !player || player->GetTeamId() >= 2 || !player->GetSession() || player->GetSession()->IsBot() ||
        !CityLifePolicy::LevelAllowed(zone, player->GetLevel()) ||
        !CityLifePolicy::FactionAllowed(zone, player->GetTeamId()))
        return;
    _requests.fetch_or(1u << (index * 2 + player->GetTeamId()), std::memory_order_relaxed);
}

bool CityLifeMgr::IsObserved(uint32 zone, uint32 team) const
{
    int index = CityLifePolicy::CityIndex(zone);
    return index >= 0 && team < 2 && (_observed.load(std::memory_order_relaxed) & (1u << (index * 2 + team)));
}

void CityLifeMgr::Count(Metric metric)
{
    if (sPlayerbotAIConfig.cityLifeDiagnostics)
        _metrics[static_cast<std::size_t>(metric)].fetch_add(1, std::memory_order_relaxed);
}

void CityLifeMgr::Update(uint32 diff)
{
    if (!sPlayerbotAIConfig.enabled || !sPlayerbotAIConfig.cityLifeNaturalActivation ||
        !sPlayerbotAIConfig.enableNewRpgStrategy)
    {
        _requests.store(0, std::memory_order_relaxed);
        _observed.store(0, std::memory_order_relaxed);
        _activated = 0;
        _events.Reset();
        _samples.clear();
        _snapshotValid = false;
        _activation = {};
        for (auto& candidates : _candidates)
            candidates.clear();
        _occupied.clear();
        _scheduled = false;
        return;
    }
    if (!_scheduled)
    {
        _events.ScheduleEvent(1, Milliseconds(0));
        _scheduled = true;
    }
    _events.Update(diff);
    if (_events.ExecuteEvent())
    {
        Process();
        // Do not catch up several admission batches after a slow world update.
        _events.ScheduleEvent(1, Milliseconds(CityLifePolicy::ActivationInterval));
    }
}

bool CityLifeMgr::Eligible(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->IsLoggingOut() ||
        bot->IsDuringRemoveFromWorld() || !bot->GetMap() || bot->GetMap()->IsDungeon() || bot->GetGroup() ||
        bot->IsInCombat() || bot->isDead() || bot->InBattleground() || bot->InBattlegroundQueue() ||
        bot->IsBeingTeleported() || bot->IsInFlight() || bot->HasUnitState(UNIT_STATE_IN_FLIGHT) ||
        bot->GetTransport() || bot->GetVehicle() || bot->IsFlying() ||
        sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
        return false;
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    if (!ai || ai->HasGameClientMaster() || ai->GetSocialPauseUntil() > time(nullptr) ||
        !ai->HasStrategy("new rpg", BOT_STATE_NON_COMBAT))
        return false;
    NewRpgStatus status = ai->rpgInfo.GetStatus();
    return status == RPG_IDLE || status == RPG_REST || status == RPG_WANDER_RANDOM || status == RPG_WANDER_NPC;
}

bool CityLifeMgr::Hidden(WorldLocation const& destination, Player* source, std::vector<Player*> const& observers)
{
    for (Player* observer : observers)
    {
        // Conservatively include all phases and GM observers. Farsight also counts.
        float range = std::max(observer->GetVisibilityRange(), observer->GetSightRange(source));
        if (source && observer->HaveAtClient(source))
            return false;
        if (observer->GetMapId() == destination.GetMapId())
        {
            if (!CityLifePolicy::Hidden(observer->GetExactDistSq(destination), range, false))
                return false;
            if (!source && observer->isMoving() &&
                CityLifePolicy::InForwardCone(destination.GetPositionX() - observer->GetPositionX(),
                                              destination.GetPositionY() - observer->GetPositionY(),
                                              std::cos(observer->GetOrientation()),
                                              std::sin(observer->GetOrientation())))
                return false;
        }
        WorldObject* viewpoint = observer->GetViewpoint();
        if (viewpoint && viewpoint->GetMapId() == destination.GetMapId() &&
            !CityLifePolicy::Hidden(viewpoint->GetExactDistSq(destination), range, false))
            return false;
    }
    return true;
}

void CityLifeMgr::Process()
{
    uint32 started = getMSTime();
    for (auto it = _arrivalFailures.begin(); it != _arrivalFailures.end();)
        if (CityLifePolicy::Elapsed(started, it->second, CityLifePolicy::ArrivalFailureCooldown))
            it = _arrivalFailures.erase(it);
        else
            ++it;
    for (auto it = _placementFailures.begin(); it != _placementFailures.end();)
        if (CityLifePolicy::Elapsed(started, it->second, CityLifePolicy::PlacementFailureCooldown))
            it = _placementFailures.erase(it);
        else
            ++it;
    std::vector<Player*> observers;
    uint32 present = 0;
    for (Player* player : sRandomPlayerbotMgr.GetPlayers())
    {
        if (!player || !player->IsInWorld() || !player->GetSession() || player->GetSession()->IsBot() ||
            player->GetSession()->IsLoggingOut() || player->IsDuringRemoveFromWorld())
            continue;
        observers.push_back(player);
        int index = CityLifePolicy::CityIndex(player->GetZoneId());
        if (index >= 0 && CityLifePolicy::LevelAllowed(player->GetZoneId(), player->GetLevel()) &&
            CityLifePolicy::FactionAllowed(player->GetZoneId(), player->GetTeamId()))
            present |= 1u << (index * 2 + player->GetTeamId());
    }
    uint32 previous = _activated;
    _activated = CityLifePolicy::ObserverMask(_activated, _requests.exchange(0, std::memory_order_relaxed), present);
    _observed.store(_activated, std::memory_order_relaxed);

    for (uint32 key = 0; key < _activation.size(); ++key)
        _activation[key].Observe((_activated & (1u << key)) != 0, started);
    // Only a transition from no observed city invalidates a dormant snapshot early.
    // Repeated entry requests never trigger repeated population scans.
    if (!previous && _activated)
        _snapshotValid = false;
    bool refreshed =
        !_snapshotValid || CityLifePolicy::Elapsed(started, _snapshotStart, CityLifePolicy::AdmissionInterval);
    if (refreshed)
        RefreshPopulation(started, observers);
    bool maintenanceDue = CityLifePolicy::Elapsed(started, _maintenanceStart, CityLifePolicy::AdmissionInterval);
    if (maintenanceDue)
        _maintenanceStart = started;
    if (_activated)
        Admit(started, observers, maintenanceDue);

    if (!sPlayerbotAIConfig.cityLifeDiagnostics)
        return;
    uint32 elapsed = GetMSTimeDiffToNow(started);
    _processingTotal += elapsed;
    _processingMax = std::max(_processingMax, elapsed);
    if (refreshed && ++_diagnosticCycles >= CityLifePolicy::DiagnosticIntervalPasses)
    {
        _diagnosticCycles = 0;
        LOG_INFO("playerbots",
                 "CityLife: pending {}, residents {}, active {}, admissions {}, teleports {}, moves {}, "
                 "resumptions {}, failures {}, ordinary starts (sampled) {}, moving visibility (sampled) {}, "
                 "processing total {}ms max {}ms",
                 _pending, _residents, _active, _metrics[0].exchange(0), _metrics[1].exchange(0),
                 _metrics[2].exchange(0), _metrics[3].exchange(0), _metrics[4].exchange(0), _metrics[5].exchange(0),
                 _metrics[6].exchange(0), _processingTotal, _processingMax);
        _processingTotal = 0;
        _processingMax = 0;
    }
}

void CityLifeMgr::RefreshPopulation(uint32 started, std::vector<Player*> const& observers)
{
    _assigned.fill(0);
    _natural.fill(0);
    for (auto& candidates : _candidates)
        candidates.clear();
    _occupied.clear();
    _online = 0;
    _globalAssigned = 0;
    _pending = 0;
    _residents = 0;
    _active = 0;
    _managedCount = sRandomPlayerbotMgr.GetPlayerbotsCount();
    _snapshotStart = started;
    _snapshotValid = true;
    ++_cycle;
    std::map<uint64, std::array<bool, 2>> samples;
    for (auto it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* bot = it->second;
        if (!bot || !bot->GetSession() || bot->GetSession()->IsLoggingOut() || !sRandomPlayerbotMgr.IsRandomBot(bot))
            continue;
        ++_online;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (!ai)
            continue;
        auto* city = std::get_if<NewRpgInfo::CityLife>(&ai->rpgInfo.data);
        if (!bot->IsInWorld() && !city)
            continue;
        if (sPlayerbotAIConfig.cityLifeDiagnostics && bot->IsInWorld())
        {
            bool visible = false;
            for (Player* observer : observers)
                if (observer->HaveAtClient(bot))
                {
                    visible = true;
                    break;
                }
            uint64 guid = bot->GetGUID().GetRawValue();
            bool moving = bot->isMoving();
            auto previous = _samples.find(guid);
            if (previous != _samples.end())
            {
                if (!city && moving && !previous->second[0])
                    Count(Metric::OrdinaryMovement);
                if (moving && previous->second[0] && visible && !previous->second[1])
                    Count(Metric::MovingVisibility);
            }
            samples[guid] = {moving, visible};
        }
        if (city)
        {
            auto release = [&]()
            {
                ai->rpgInfo.ChangeToIdle();
                if (!bot->IsInWorld())
                    return;
                int physicalCity = CityLifePolicy::CityIndex(bot->GetZoneId());
                if (physicalCity >= 0)
                    ++_natural[physicalCity * 2 + bot->GetTeamId()];
                _occupied.emplace_back(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                                       bot->GetOrientation());
            };
            if (bot->GetGroup() || ai->HasGameClientMaster() ||
                sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE ||
                (bot->IsInWorld() && city->Arrived && !bot->IsBeingTeleported() &&
                 (bot->GetMapId() != city->cityPos.GetMapId() || bot->GetZoneId() != city->cityZoneId)))
            {
                release();
                continue;
            }
            if (bot->IsInWorld() && city->Managed && !city->Arrived && !bot->IsBeingTeleported() &&
                bot->GetMapId() == city->cityPos.GetMapId() && bot->GetZoneId() == city->cityZoneId)
            {
                city->Arrived = true;
                CityLifePolicy::Begin(city->Schedule, bot->GetGUID().GetRawValue(), started, city->Schedule.Intent);
            }
            bool observed = IsObserved(city->cityZoneId, bot->GetTeamId());
            if ((!observed && CityLifePolicy::Elapsed(started, city->leaseStart, CityLifePolicy::LeaseDuration)) ||
                (city->Managed && !city->Arrived &&
                 CityLifePolicy::Elapsed(started, city->leaseStart, CityLifePolicy::ArrivalTimeout)))
            {
                if (city->Managed && !city->Arrived)
                    _arrivalFailures[bot->GetGUID().GetRawValue()] = started;
                release();
                continue;
            }
            if (observed)
                ++_active;
            if (!observed)
                city->Schedule.Suspended = true;
            else if (CityLifePolicy::Elapsed(started, city->leaseStart, CityLifePolicy::LeaseDuration))
                city->leaseStart = started;
            int index = CityLifePolicy::CityIndex(city->cityZoneId);
            if (index >= 0)
                ++_assigned[index * 2 + bot->GetTeamId()];
            ++_globalAssigned;
            _occupied.push_back(city->cityPos);
            if (!bot->IsInWorld() || bot->IsBeingTeleported() || bot->GetZoneId() != city->cityZoneId)
                ++_pending;
            else
                ++_residents;
        }
        else
        {
            int index = CityLifePolicy::CityIndex(bot->GetZoneId());
            if (index >= 0)
                ++_natural[index * 2 + bot->GetTeamId()];
            if (_activated && Eligible(bot))
                _candidates[bot->GetTeamId()].push_back(bot->GetGUID());
        }
        // Actual bodies also consume local space. Assigned hubs reserve future space.
        if (bot->IsInWorld() && CityLifePolicy::CityIndex(bot->GetZoneId()) >= 0 &&
            (!city || bot->GetMapId() != city->cityPos.GetMapId() ||
             bot->GetExactDist(city->cityPos) > CityLifePolicy::OccupancyRadius))
            _occupied.emplace_back(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                                   bot->GetOrientation());
    }

    _samples.swap(samples);
}

void CityLifeMgr::Admit(uint32 started, std::vector<Player*> const& observers, bool maintenanceDue)
{
    // A login/logout invalidates the snapshot's population denominator. Wait for the next
    // scheduled refresh rather than repeatedly scanning while population is changing.
    if (_managedCount != sRandomPlayerbotMgr.GetPlayerbotsCount() ||
        _arrivalFailures.size() >= CityLifePolicy::FailureCacheLimit ||
        _placementFailures.size() >= CityLifePolicy::FailureCacheLimit)
        return;
    bool hasInitial = false;
    for (auto const& activation : _activation)
        hasInitial |= activation.Initial(started, sPlayerbotAIConfig.CityLifeActivationDurationMs);
    uint32 requested = hasInitial ? sPlayerbotAIConfig.CityLifeActivationBudget
                                  : (maintenanceDue ? sPlayerbotAIConfig.cityLifeAdmissionBudget : 0);
    CityLifePolicy::AdmissionWork work;
    // This is the measured start-to-start world tick interval, including normal tick sleep.
    // It is not an estimate of CPU use. Reading it here is on its owning world thread.
    work.Limit = CityLifePolicy::PressureBudget(requested, sWorldUpdateTime.GetLastUpdateTime(),
                                                sPlayerbotAIConfig.CityLifeActivationMaxTickMs);
    uint32 keyLimit = static_cast<uint32>(_activation.size()) * work.Limit;
    for (uint32 turn = 0; turn < keyLimit && work.Attempts < work.Limit; ++turn)
    {
        if (GetMSTimeDiffToNow(started) >= CityLifePolicy::SliceTimeBudget ||
            work.Candidates >= CityLifePolicy::CandidateBudget ||
            work.Destinations >= CityLifePolicy::DestinationBudget ||
            _arrivalFailures.size() >= CityLifePolicy::FailureCacheLimit ||
            _placementFailures.size() >= CityLifePolicy::FailureCacheLimit)
            break;
        uint32 key = CityLifePolicy::NextCity(_cursor);
        uint32 zone = CityLifePolicy::Cities[key / CityLifePolicy::TeamCount];
        uint32 team = key % CityLifePolicy::TeamCount;
        bool initial = _activation[key].Initial(started, sPlayerbotAIConfig.CityLifeActivationDurationMs);
        if (!work.Allows(initial, maintenanceDue, sPlayerbotAIConfig.cityLifeAdmissionBudget) ||
            !CityLifePolicy::Available(_assigned[key], _natural[key], _globalAssigned, _online,
                                       (_activated & (1u << key)) != 0))
            continue;
        auto& candidates = _candidates[team];
        auto const& hubs = sTravelMgr.GetNaturalCityLifeHubs(zone);
        if (candidates.empty() || hubs.empty() || !work.Candidate())
            continue;
        uint32 index = CityLifePolicy::Hash(_cycle, work.Candidates) % static_cast<uint32>(candidates.size());
        ObjectGuid guid = candidates[index];
        candidates[index] = candidates.back();
        candidates.pop_back();
        // Each GUID is considered at most once per population snapshot, even when exhausted/unsafe.
        Player* bot = ObjectAccessor::FindPlayer(guid);
        if (!bot || !sRandomPlayerbotMgr.IsRandomBot(bot) || !Eligible(bot) || bot->GetTeamId() != team ||
            bot->GetZoneId() == zone || _arrivalFailures.count(guid.GetRawValue()) ||
            !CityLifePolicy::LevelAllowed(zone, bot->GetLevel()) ||
            !Hidden(WorldLocation(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                                  bot->GetOrientation()),
                    bot, observers))
            continue;
        TravelMgr::CityLifeHub const* selected = nullptr;
        float bestScore = std::numeric_limits<float>::max();
        uint32 offset = CityLifePolicy::Hash(guid.GetRawValue(), _cycle) % static_cast<uint32>(hubs.size());
        for (uint32 sample = 0; sample < std::min<std::size_t>(hubs.size(), CityLifePolicy::PlacementBudget); ++sample)
        {
            if (GetMSTimeDiffToNow(started) >= CityLifePolicy::SliceTimeBudget || !work.Destination())
                break;
            auto const& hub = hubs[(offset + sample) % hubs.size()];
            if (_placementFailures.count({hub.Location.GetMapId(), hub.SpawnId}) ||
                !Hidden(hub.Location, nullptr, observers))
                continue;
            Map* map = sMapMgr->FindMap(hub.Location.GetMapId(), 0);
            if (!map)
                continue;
            auto range = map->GetCreatureBySpawnIdStore().equal_range(hub.SpawnId);
            Creature* anchor = range.first == range.second ? nullptr : range.first->second;
            if (!anchor || !anchor->IsInWorld() || anchor->isDead() || anchor->IsInCombat() || anchor->isMoving() ||
                anchor->GetDefaultMovementType() != IDLE_MOTION_TYPE)
            {
                _placementFailures[{hub.Location.GetMapId(), hub.SpawnId}] = started;
                continue;
            }
            if (anchor->IsHostileTo(bot) || !anchor->InSamePhase(bot))
                continue;
            uint32 occupancy = 0;
            for (WorldLocation const& position : _occupied)
                if (position.GetMapId() == hub.Location.GetMapId() &&
                    position.GetExactDistSq(hub.Location) <
                        CityLifePolicy::OccupancyRadius * CityLifePolicy::OccupancyRadius)
                    if (++occupancy >= sPlayerbotAIConfig.cityLifeLocationCapacity)
                        break;
            if (occupancy >= sPlayerbotAIConfig.cityLifeLocationCapacity)
                continue;
            float score = std::numeric_limits<float>::max();
            for (Player* observer : observers)
                if (observer->GetMapId() == hub.Location.GetMapId() && observer->GetZoneId() == zone &&
                    observer->GetTeamId() == team)
                    score = std::min(
                        score, CityLifePolicy::PlacementScore(
                                   observer->GetExactDist(hub.Location),
                                   std::max(observer->GetVisibilityRange(), observer->GetSightRange()), occupancy));
            if (score < bestScore)
            {
                bestScore = score;
                selected = &hub;
            }
        }
        if (!selected || GetMSTimeDiffToNow(started) >= CityLifePolicy::SliceTimeBudget)
            continue;
        Map* map = sMapMgr->FindMap(selected->Location.GetMapId(), 0);
        if (!map)
            continue;
        // Retain the original destination-map geometry and compatibility validation.
        auto range = map->GetCreatureBySpawnIdStore().equal_range(selected->SpawnId);
        Creature* anchor = range.first == range.second ? nullptr : range.first->second;
        if (!anchor || !anchor->IsInWorld() || anchor->isDead() || anchor->IsInCombat() || anchor->isMoving() ||
            anchor->GetDefaultMovementType() != IDLE_MOTION_TYPE || anchor->IsHostileTo(bot) ||
            !anchor->InSamePhase(bot))
            continue;
        WorldLocation location = selected->Location;
        float x = location.GetPositionX(), y = location.GetPositionY(), z = location.GetPositionZ();
        if (!map->CanReachPositionAndGetValidCoords(anchor, x, y, z) ||
            map->IsInWater(PHASEMASK_NORMAL, x, y, z, anchor->GetCollisionHeight()) ||
            location.GetExactDist(x, y, z) > 2.0f)
        {
            _placementFailures[{selected->Location.GetMapId(), selected->SpawnId}] = started;
            Count(Metric::DestinationFailures);
            continue;
        }
        location.Relocate(x, y, z, location.GetOrientation());
        if (GetMSTimeDiffToNow(started) >= CityLifePolicy::SliceTimeBudget ||
            map->GetZoneId(PHASEMASK_NORMAL, x, y, z) != zone || !Hidden(location, nullptr, observers) ||
            !Eligible(bot))
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        ai->rpgInfo.ChangeToCityLife(location, zone);
        auto& state = std::get<NewRpgInfo::CityLife>(ai->rpgInfo.data);
        state.Managed = true;
        CityLifePolicy::Begin(state.Schedule, guid.GetRawValue(), started, selected->Activity);
        // Reserve capacity before transfer; carry it between fast slices and roll back failure.
        ++_assigned[key];
        ++_globalAssigned;
        ++_pending;
        _occupied.push_back(location);
        work.Attempt(initial);
        if (!bot->TeleportTo(location))
        {
            --_assigned[key];
            --_globalAssigned;
            --_pending;
            _occupied.pop_back();
            _arrivalFailures[guid.GetRawValue()] = started;
            ai->rpgInfo.ChangeToIdle();
            Count(Metric::DestinationFailures);
            continue;
        }
        Count(Metric::Admissions);
        Count(Metric::Teleports);
    }
}
