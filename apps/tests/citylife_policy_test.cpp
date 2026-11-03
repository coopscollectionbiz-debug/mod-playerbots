/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Standalone offline policy harness. Does not link or start a server.
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include "CityLifePolicy.h"

class CityLifePolicyTest
{
public:
    static void Run()
    {
        using Policy = CityLifePolicy;
        Require(Policy::Available(0, 0, 0, 4000, true) == 100, "initial deficit");
        unsigned assigned = 0;
        unsigned global = 0;
        for (unsigned budget = 2; budget && Policy::Available(assigned, 0, global, 4000, true); --budget)
        {
            ++assigned;
            ++global;
        }
        Require(assigned == 2, "bounded admission batch");
        Require(Policy::Available(70 + 30, 0, 100, 4000, true) == 0, "pending plus resident accounting");
        Require(Policy::Available(80, 80, 80, 4000, true) == 0, "natural visitors contribute");
        Require(Policy::Available(0, 0, 599, 3000, true) == 1, "global cap remaining");
        Require(Policy::Available(0, 0, 600, 3000, true) == 0, "global cap reached");
        Require(Policy::Available(0, 0, 800, 4000, true) == 0, "global cap at 4000");

        unsigned active = Policy::ObserverMask(0, 1, 1);
        for (unsigned entry = 0; entry < 100; ++entry)
            active = Policy::ObserverMask(active, 1, 1);
        Require(active == 1, "repeated entries coalesce");
        Require(Policy::ObserverMask(active, 0, 1) == 1, "second observer still present after first departs");
        Require(Policy::ObserverMask(active, 0, 0) == 0, "last observer departed");
        Require(Policy::Available(50, 0, 50, 4000, false) == 0, "no unattended admission");
        Require(Policy::ObserverMask(1, 4, 5) == 5, "multiple cities progress independently");
        Require(Policy::FactionAllowed(1519, 0) && !Policy::FactionAllowed(1519, 1), "capital faction");
        Require(Policy::FactionAllowed(4395, 0) && Policy::FactionAllowed(4395, 1), "neutral faction");
        Require(!Policy::LevelAllowed(4395, 67) && Policy::LevelAllowed(4395, 68), "Dalaran level");
        Require(!Policy::LevelAllowed(3703, 57) && Policy::LevelAllowed(3703, 58), "Shattrath level");

        Policy::Activation activation;
        activation.Observe(true, 100);
        Require(activation.Initial(100, 10000), "first observer starts rapid window");
        activation.Observe(true, 5000);
        Require(activation.Start == 100, "repeated entry or second observer does not restart window");
        Require(activation.Initial(10099, 10000), "initial window remains active until deadline");
        Require(!activation.Initial(10100, 10000), "initial window switches to maintenance");
        activation.Observe(false, 10200);
        Require(!activation.Initial(10200, 10000), "last observer stops initial admission");
        activation.Observe(true, 12000);
        Require(activation.Start == 12000, "new visit after last observer leaves gets new window");

        Policy::AdmissionWork bounded;
        bounded.Limit = 4;
        for (unsigned attempt = 0; attempt < 4; ++attempt)
        {
            Require(bounded.Allows(true, false, 2), "initial work allowed within budget");
            Require(bounded.Candidate() && bounded.Destination(), "candidate and destination budgets available");
            bounded.Attempt(true);
        }
        Require(!bounded.Allows(true, false, 2), "initial transfer budget stops the slice");
        Require(bounded.Attempts == 4, "failed or successful attempts consume same allowance");
        Policy::AdmissionWork maintenance;
        maintenance.Limit = 4;  // another city may still be in its initial window
        Require(!maintenance.Allows(false, false, 2), "maintenance not due cannot borrow initial budget");
        maintenance.Attempt(false);
        maintenance.Attempt(false);
        Require(!maintenance.Allows(false, true, 2), "maintenance has its own smaller allowance");
        Require(maintenance.Allows(true, false, 2), "initial city can use remaining shared allowance");

        Policy::AdmissionWork exhausted;
        exhausted.Limit = 8;
        for (unsigned probe = 0; probe < Policy::CandidateBudget; ++probe)
            Require(exhausted.Candidate(), "bounded candidate search");
        Require(!exhausted.Candidate() && !exhausted.Allows(true, false, 2), "candidate exhaustion stops work");
        for (unsigned destination = 0; destination < Policy::DestinationBudget; ++destination)
            Require(exhausted.Destination(), "bounded aggregate destination search");
        Require(!exhausted.Destination(), "destination exhaustion stops search");

        std::array<unsigned, Policy::CityTeamCount> opportunities{};
        unsigned cursor = 0;
        for (unsigned batch = 0; batch < 3; ++batch)
        {
            Policy::AdmissionWork fair;
            fair.Limit = 4;
            for (unsigned visit = 0; visit < Policy::CityTeamCount * fair.Limit && fair.Attempts < fair.Limit; ++visit)
            {
                unsigned key = Policy::NextCity(cursor);
                if ((key == 0 || key == 3 || key == 8) && fair.Allows(true, false, 2))
                {
                    ++opportunities[key];
                    fair.Attempt(true);
                }
            }
        }
        Require(opportunities[0] == 4 && opportunities[3] == 4 && opportunities[8] == 4,
                "multiple cities share successive slices fairly");

        unsigned reserved = 0;
        unsigned lastAdmission = 0;
        for (unsigned now = 0; now < 10000; now += Policy::ActivationInterval)
        {
            Policy::AdmissionWork rapid;
            rapid.Limit = 4;
            while (rapid.Allows(true, false, 2) && Policy::Available(reserved, 0, reserved, 4000, true))
            {
                ++reserved;  // all accepted residents may still be pending
                rapid.Attempt(true);
                lastAdmission = now;
            }
        }
        Require(reserved == 100 && lastAdmission == 6000, "calculated best case reserves 100 in six seconds");
        Require(Policy::Available(reserved, 0, reserved, 4000, true) == 0, "pending arrivals prevent burst overfill");
        Require(Policy::PressureBudget(4, 50, 100) == 4, "normal measured tick preserves activation budget");
        Require(Policy::PressureBudget(4, 75, 100) == 2, "elevated measured tick halves budget");
        Require(Policy::PressureBudget(4, 100, 100) == 0, "high measured tick defers all admissions");
        Require(Policy::PressureBudget(0, 90, 100) == 0, "pressure logic cannot invent allowance");
        Require(Policy::PlacementScore(150.0f, 90.0f, 0) < Policy::PlacementScore(600.0f, 90.0f, 0),
                "nearby hidden locations outrank arbitrary distant districts");
        Require(Policy::PlacementScore(150.0f, 90.0f, 7) > Policy::PlacementScore(180.0f, 90.0f, 0),
                "occupancy still spreads arrivals");
        Require(Policy::InForwardCone(150.0f, 0.0f, 1.0f, 0.0f), "moving player's forward corridor deferred");
        Require(!Policy::InForwardCone(-150.0f, 0.0f, 1.0f, 0.0f), "behind player is not forward corridor");
        Require(!Policy::InForwardCone(0.0f, 150.0f, 1.0f, 0.0f), "side district is not forward corridor");
        Policy::Activation wrapped;
        wrapped.Observe(true, 0xffffff00u);
        Require(wrapped.Initial(200, 1000), "activation deadline handles clock wrap");

        Policy::Schedule valid;
        Policy::Begin(valid, 42, 1000, Policy::Activity::Banking);
        unsigned duration = valid.ActivityDuration;
        valid.Suspended = true;
        Require(Policy::Reconcile(valid, 42, 2000), "inactive-to-active detected");
        Require(valid.ActivityStart == 1000 && valid.ActivityDuration == duration, "valid deadline preserved");
        Require(valid.Intent == Policy::Activity::Banking, "valid intent preserved");
        valid.Suspended = true;
        unsigned resumed = 1000 + duration + 100000;
        Require(Policy::Reconcile(valid, 42, resumed), "overdue resume detected");
        Require(!Policy::Elapsed(resumed, valid.ActivityStart, valid.ActivityDuration), "overdue work deferred");
        Require(!Policy::Reconcile(valid, 42, resumed + 1), "resume only once");

        std::set<unsigned> deadlines;
        unsigned longStays = 0;
        unsigned shortStays = 0;
        for (unsigned guid = 1; guid <= 1000; ++guid)
        {
            Policy::Schedule schedule;
            Policy::Begin(schedule, guid, 5000, Policy::Activity::Resting);
            Require(schedule.ActivityDuration >= 20000 && schedule.ActivityDuration <= 300000, "duration bounds");
            Require(!Policy::Elapsed(5000, schedule.ActivityStart, schedule.ActivityDuration), "stationary initially");
            deadlines.insert(schedule.ActivityDuration);
            longStays += schedule.ActivityDuration >= 180000;
            shortStays += schedule.ActivityDuration < 60000;
            Require(schedule.ActivityDuration == Policy::Duration(guid, 1), "stable individual schedule");
        }
        Require(deadlines.size() > 900 && longStays > 200 && shortStays > 50, "staggered short and long visits");

        Require(!Policy::Hidden(10000.0f, 90.0f, false), "nearby destination rejected");
        Require(!Policy::Hidden(1000000.0f, 90.0f, true), "visible source rejected even at distance");
        Require(Policy::Hidden(1000000.0f, 90.0f, false), "hidden distant placement");
        Require(!Policy::PreferPlacement(0, 8, false), "no invisible placement means defer");
        Require(!Policy::PreferPlacement(8, 8, true), "capacity reached");
        Require(Policy::PreferPlacement(1, 7, true), "less occupied service preferred");
        Require(!Policy::PreferPlacement(7, 1, true), "crowded bank loses to quiet service");
        Require(Policy::TargetInRange(59.0f, 59.0f), "local target valid");
        Require(!Policy::TargetInRange(61.0f, 20.0f), "bot-relative limit");
        Require(!Policy::TargetInRange(20.0f, 61.0f), "hub-relative limit");

        Policy::Fail(valid, 42, resumed);
        Require(Policy::CoolingDown(valid, resumed + 1000), "path failure cooldown");
        Require(valid.Intent == Policy::Activity::Waiting, "path failure becomes stationary activity");
        Require(!Policy::Elapsed(resumed, valid.ActivityStart, valid.ActivityDuration), "no immediate retry");
        Require(!Policy::CoolingDown(valid, resumed + valid.FailureDuration), "bounded failure cooldown");
        Require(Policy::Elapsed(20, 0xfffffff0u, 36), "monotonic clock wrap");
        Require(!Policy::Elapsed(20, 0xfffffff0u, 37), "clock wrap boundary");
        std::cout << "PASS: " << _checks << " production policy checks\n";
    }

private:
    static void Require(bool condition, std::string const& message)
    {
        ++_checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    static inline unsigned _checks = 0;
};

int main()
{
    try
    {
        CityLifePolicyTest::Run();
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
