/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CITYLIFEPOLICY_H
#define PLAYERBOTS_CITYLIFEPOLICY_H

#include <algorithm>
#include <array>
#include <cstdint>

// Pure policy shared by the runtime and the standalone offline harness.
class CityLifePolicy
{
public:
    enum class Activity : std::uint8_t
    {
        Waiting,
        Banking,
        Browsing,
        Shopping,
        Training,
        Resting,
        Traveling
    };

    static constexpr std::array<std::uint32_t, 10> Cities = {1519, 1637, 1537, 1497, 1657,
                                                             1638, 3557, 3487, 3703, 4395};
    static constexpr std::uint32_t CityTarget = 100;
    static constexpr std::uint32_t LeaseDuration = 30 * 60 * 1000;
    static constexpr std::uint32_t ArrivalTimeout = 30 * 1000;
    static constexpr std::uint32_t AdmissionInterval = 2000;
    static constexpr std::uint32_t ActivationInterval = 250;
    static constexpr std::uint32_t SliceTimeBudget = 4;
    static constexpr std::uint32_t CandidateBudget = 16;
    static constexpr std::uint32_t PlacementBudget = 32;
    static constexpr std::uint32_t DestinationBudget = 128;
    static constexpr std::uint32_t ArrivalFailureCooldown = 180000;
    static constexpr std::uint32_t PlacementFailureCooldown = 60000;
    static constexpr std::uint32_t FailureCacheLimit = 1024;
    static constexpr std::uint32_t DiagnosticIntervalPasses = 15;
    static constexpr std::uint32_t TeamCount = 2;
    static constexpr std::size_t CityTeamCount = Cities.size() * TeamCount;
    static constexpr float HomeRadius = 60.0f;
    static constexpr float OccupancyRadius = 25.0f;
    static constexpr float VisibilityMargin = 30.0f;

    struct Schedule
    {
        std::uint32_t ActivityStart = 0;
        std::uint32_t ActivityDuration = 0;
        std::uint32_t Sequence = 0;
        std::uint32_t LastUpdate = 0;
        std::uint32_t FailureStart = 0;
        std::uint32_t FailureDuration = 0;
        bool Suspended = false;
        bool Initialized = false;
        Activity Intent = Activity::Waiting;
    };

    struct Activation
    {
        std::uint32_t Start = 0;
        bool Observed = false;

        void Observe(bool present, std::uint32_t now)
        {
            if (present && !Observed)
                Start = now;
            Observed = present;
        }

        bool Initial(std::uint32_t now, std::uint32_t duration) const
        {
            return Observed && !Elapsed(now, Start, duration);
        }
    };

    struct AdmissionWork
    {
        std::uint32_t Limit = 0;
        std::uint32_t Attempts = 0;
        std::uint32_t MaintenanceAttempts = 0;
        std::uint32_t Candidates = 0;
        std::uint32_t Destinations = 0;

        bool Allows(bool initial, bool maintenanceDue, std::uint32_t maintenanceLimit) const
        {
            return Attempts < Limit && Candidates < CandidateBudget && Destinations < DestinationBudget &&
                   (initial || (maintenanceDue && MaintenanceAttempts < maintenanceLimit));
        }

        bool Candidate()
        {
            if (Candidates >= CandidateBudget)
                return false;
            ++Candidates;
            return true;
        }

        bool Destination()
        {
            if (Destinations >= DestinationBudget)
                return false;
            ++Destinations;
            return true;
        }

        void Attempt(bool initial)
        {
            ++Attempts;
            if (!initial)
                ++MaintenanceAttempts;
        }
    };

    static std::uint32_t NextCity(std::uint32_t& cursor)
    {
        std::uint32_t key = cursor;
        cursor = (cursor + 1) % static_cast<std::uint32_t>(CityTeamCount);
        return key;
    }

    static std::uint32_t PressureBudget(std::uint32_t budget, std::uint32_t measuredTick, std::uint32_t maxTick)
    {
        if (!budget || measuredTick >= maxTick)
            return 0;
        if (measuredTick >= maxTick * 3 / 4)
            return std::max(1u, budget / 2);
        return budget;
    }

    static bool InForwardCone(float deltaX, float deltaY, float facingX, float facingY)
    {
        float forward = deltaX * facingX + deltaY * facingY;
        float sideways = deltaY * facingX - deltaX * facingY;
        return forward > 0.0f && sideways * sideways < forward * forward;
    }

    static float PlacementScore(float observerDistance, float observerRange, std::uint32_t occupancy)
    {
        return observerDistance / std::max(1.0f, observerRange + VisibilityMargin) + static_cast<float>(occupancy);
    }

    static int CityIndex(std::uint32_t zone)
    {
        for (std::size_t i = 0; i < Cities.size(); ++i)
            if (Cities[i] == zone)
                return static_cast<int>(i);
        return -1;
    }

    static bool LevelAllowed(std::uint32_t zone, std::uint32_t level)
    {
        return (zone != 3703 || level >= 58) && (zone != 4395 || level >= 68);
    }

    static bool FactionAllowed(std::uint32_t zone, std::uint32_t team)
    {
        int index = CityIndex(zone);
        return index >= 0 && team < 2 && (index >= 8 || team == static_cast<std::uint32_t>(index % 2));
    }

    static std::uint32_t ObserverMask(std::uint32_t active, std::uint32_t requests, std::uint32_t present)
    {
        return (active | requests) & present;
    }

    static bool PreferPlacement(std::uint32_t occupancy, std::uint32_t best, bool hidden)
    {
        return hidden && occupancy < best;
    }

    static bool TargetInRange(float fromBot, float fromHub) { return fromBot <= HomeRadius && fromHub <= HomeRadius; }

    static std::uint32_t Hash(std::uint64_t guid, std::uint32_t salt)
    {
        std::uint32_t value = static_cast<std::uint32_t>(guid ^ (guid >> 32)) ^ (salt * 0x9e3779b9u);
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        return value ^ (value >> 16);
    }

    static bool Elapsed(std::uint32_t now, std::uint32_t start, std::uint32_t duration)
    {
        return static_cast<std::uint32_t>(now - start) >= duration;
    }

    static std::uint32_t Available(std::uint32_t assigned, std::uint32_t natural, std::uint32_t globalAssigned,
                                   std::uint32_t online, bool observed)
    {
        std::uint32_t effective = assigned + natural / 4;
        std::uint32_t cap = std::max(1u, online / 5);
        if (!observed || effective >= CityTarget || globalAssigned >= cap)
            return 0;
        return std::min(CityTarget - effective, cap - globalAssigned);
    }

    static std::uint32_t Duration(std::uint64_t guid, std::uint32_t sequence)
    {
        // Continuous range includes short errands and several-minute stationary visits.
        return 20000 + Hash(guid, sequence) % 280001;
    }

    static void Begin(Schedule& schedule, std::uint64_t guid, std::uint32_t now, Activity intent)
    {
        schedule.ActivityStart = now;
        schedule.ActivityDuration = Duration(guid, ++schedule.Sequence);
        schedule.Intent = intent;
        schedule.LastUpdate = now;
        schedule.Suspended = false;
        schedule.Initialized = true;
    }

    static bool Reconcile(Schedule& schedule, std::uint64_t guid, std::uint32_t now)
    {
        bool resumed = schedule.Suspended || (schedule.Initialized && Elapsed(now, schedule.LastUpdate, 15000));
        schedule.LastUpdate = now;
        schedule.Suspended = false;
        if (resumed && Elapsed(now, schedule.ActivityStart, schedule.ActivityDuration))
        {
            schedule.ActivityStart = now;
            schedule.ActivityDuration = Duration(guid, ++schedule.Sequence);
        }
        return resumed;
    }

    static bool Hidden(float distanceSquared, float observerRange, bool atClient)
    {
        float radius = observerRange + VisibilityMargin;
        return !atClient && distanceSquared > radius * radius;
    }

    static void Fail(Schedule& schedule, std::uint64_t guid, std::uint32_t now)
    {
        Begin(schedule, guid, now, Activity::Waiting);
        schedule.FailureStart = now;
        schedule.FailureDuration = 60000 + Hash(guid, schedule.Sequence) % 120001;
        schedule.ActivityDuration = schedule.FailureDuration;
    }

    static bool CoolingDown(Schedule const& schedule, std::uint32_t now)
    {
        return schedule.FailureDuration && !Elapsed(now, schedule.FailureStart, schedule.FailureDuration);
    }
};

#endif
