/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXBOSSHELPER_H
#define PLAYERBOTS_NAXXBOSSHELPER_H

#include "AiObject.h"
#include "EventMap.h"
#include "Log.h"
#include "NamedObjectContext.h"
#include "NaxxHelpers.h"
#include "Naxx40HorsemenRules.h"
#include "NaxxSpellIds.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"
#include <array>
#include <string>

class Aura;

const uint32 NAXX_MAP_ID = 533;
// Individual Progression's Naxx40 Sir Zeliek: gates the WotLK Four Horsemen helper/multiplier off Naxx40.
constexpr uint32 NAXX40_ZELIEK_ENTRY =
    Naxx40HorsemenRules::ENTRY[Naxx40HorsemenRules::Idx(Naxx40HorsemenRules::Horse::Zeliek)];

template <class BossAiType>
class GenericBossHelper : public AiObject
{
public:
    GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
    virtual bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            _unit = nullptr;

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            _unit = nullptr;

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", _name);
            if (!_unit)
                return false;

            _target = _unit->ToCreature();
            if (!_target)
                return false;

            _ai = dynamic_cast<BossAiType*>(_target->GetAI());
            if (!_ai)
                return false;

            _event_map = &_ai->events;
            if (!_event_map)
                return false;
        }
        if (!_event_map)
            return false;

        _timer = getMSTime();
        return true;
    }
    virtual void Reset()
    {
        _unit = nullptr;
        _target = nullptr;
        _ai = nullptr;
        _event_map = nullptr;
        _timer = 0;
    }

protected:
    std::string _name;
    Unit* _unit = nullptr;
    Creature* _target = nullptr;
    BossAiType* _ai = nullptr;
    EventMap* _event_map = nullptr;
    uint32 _timer = 0;
};

// Heigan the Unclean ("safety dance").
//
// Everything here is derived from observable state only; the core boss script (boss_heigan.cpp) is not touched.
// The fight is fully periodic:
//   * slow dance (starts on pull, 90s): eruptions at 15s then every 10s (8 eruptions);
//   * fast dance (45s): Heigan teleports onto his platform and channels Plague Cloud (29350, 45s aura, 1s after
//     the teleport), eruptions at 7s then every 4s (10 eruptions); then back to the slow dance.
// The eruption itself is cast by the floor gameobjects (never by the boss), so it cannot be seen as a boss cast;
// instead the safe spot is computed from the phase clock. In each phase the safe section walks
// waypoint 0 -> 1 -> 2 -> 3 -> 2 -> 1 -> 0 -> ... (the core's sections 3,2,1,0,1,2,3,...).
// The phase clock is exact during the fast dance (Plague Cloud remaining duration) and derived from that for the
// following slow dances; only the very first slow dance is anchored on the moment the raid saw the pull.
// The clock is shared between all bots fighting the same Heigan so a bot that died and got resurrected mid-fight
// (or joined late) picks the schedule up again.
class HeiganBossHelper : public AiObject
{
public:
    static constexpr uint32 WaypointCount = 4;
    // Section centres, index 0 is the section that is safe at the first eruption of every phase.
    static constexpr std::array<float, WaypointCount> WaypointX = {2794.88f, 2775.49f, 2762.30f, 2755.99f};
    static constexpr std::array<float, WaypointCount> WaypointY = {-3668.12f, -3674.43f, -3684.59f, -3703.96f};
    // Heigan's platform: ranged/healers stand here during the slow dance, nobody may be near it during the fast one.
    static constexpr float PlatformX = 2794.26f;
    static constexpr float PlatformY = -3706.67f;
    static constexpr float PlatformZ = 276.54f;
    static constexpr float PlatformRadius = 10.0f;

    static constexpr uint32 SlowFirstEruptionMs = 15000;
    static constexpr uint32 SlowEruptionIntervalMs = 10000;
    static constexpr uint32 SlowEruptionCount = 8;
    static constexpr uint32 SlowPhaseDurationMs = 90000;
    static constexpr uint32 FastFirstEruptionMs = 7000;
    static constexpr uint32 FastEruptionIntervalMs = 4000;
    static constexpr uint32 FastEruptionCount = 10;
    static constexpr uint32 FastPhaseDurationMs = 45000;
    static constexpr uint32 PlagueCloudDelayMs = 1000;    // Plague Cloud is cast 1s after the teleport
    // Do not leave the current safe spot until this long after the computed eruption time (scheduler jitter).
    static constexpr uint32 EruptionSafetyMarginMs = 300;
    static constexpr float SafeSpotTolerance = 4.0f;

    struct FightState
    {
        bool fastPhase = false;
        uint32 phaseStartMs = 0;
        bool exact = false;   // phaseStartMs derived from the Plague Cloud aura (directly or via the phase length)
        uint32 lastSeenMs = 0;
    };

    explicit HeiganBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    // Finds Heigan and refreshes the shared fight clock. False when the bot is not fighting Heigan.
    bool UpdateBossAI();
    Unit* GetBoss() const { return _unit; }
    bool IsFastPhase() const { return _fastPhase; }
    uint32 GetPhaseElapsedMs() const { return _phaseElapsedMs; }
    // Number of eruptions of the current phase that already happened (safety margin applied).
    uint32 GetEruptionIndex() const;
    // Waypoint (0..3) the bot has to stand at right now.
    uint32 GetSafeWaypoint() const;
    uint32 GetMsUntilNextEruption() const;
    bool IsAtWaypoint(uint32 index, float tolerance = SafeSpotTolerance) const;
    // Ranged bots wait on the platform during the slow dance until its last eruption is over, then head to the
    // first fast-dance spot.
    bool ShouldRangedHoldPlatform() const { return !_fastPhase && GetEruptionIndex() < SlowEruptionCount; }
    // Melee always dance, ranged only during the fast dance. A main tank without aggro is free to go get it.
    bool ShouldDance();
    // Safe to stand still (e.g. cast) for the given time without missing the next move.
    bool CanStandStillFor(uint32 ms);

private:
    void Reset()
    {
        _unit = nullptr;
        _fastPhase = false;
        _phaseElapsedMs = 0;
    }

    // Decide from the boss state whether the fast dance is on right now.
    bool DetectFastPhase(FightState const& state, bool plagueCloudUp, bool idleOnPlatform, bool firstObservation,
                         uint32 elapsed) const;
    // Move the shared clock to the phase we just observed.
    static void UpdateFightState(FightState& state, bool fast, Aura const* plagueCloud, bool firstObservation,
                                 uint32 now);

    Unit* _unit = nullptr;
    bool _fastPhase = false;
    uint32 _phaseElapsedMs = 0;
};

class KelthuzadBossHelper : public AiObject
{
public:
    KelthuzadBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    const std::pair<float, float> center = {3716.19f, -5106.58f};
    const std::pair<float, float> tank_pos = {3709.19f, -5104.86f};
    const std::pair<float, float> assist_tank_pos = {3746.05f, -5112.74f};
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "kel'thuzad");

        return _unit != nullptr;
    }
    bool IsPhaseOne() { return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    bool IsPhaseTwo() { return _unit && !_unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    Unit* GetAnyShadowFissure()
    {
        Unit* shadow_fissure = nullptr;
        GuidVector units = *context->GetValue<GuidVector>("nearest triggers");
        for (auto i = units.begin(); i != units.end(); i++)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (!unit)
                continue;
            if (botAI->EqualLowercaseName(unit->GetName(), "shadow fissure"))
                shadow_fissure = unit;
        }
        return shadow_fissure;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class RazuviousBossHelper : public AiObject
{
public:
    RazuviousBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "instructor razuvious");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class SapphironBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {3512.07f, -5274.06f};
    const std::pair<float, float> center = {3517.31f, -5253.74f};
    const float GENERIC_HEIGHT = 137.29f;
    SapphironBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "sapphiron");
            if (!_unit)
                return false;
        }
        bool now_flying = _unit->IsFlying();
        if (_was_flying && !now_flying)
            _last_land_ms = getMSTime();

        _was_flying = now_flying;
        return true;
    }
    bool IsPhaseGround() { return _unit && !_unit->IsFlying(); }
    bool IsPhaseFlight() { return _unit && _unit->IsFlying(); }
    bool JustLanded()
    {
        if (!_last_land_ms)
            return false;

        return getMSTime() - _last_land_ms <= POSITION_TIME_AFTER_LANDED;
    }
    // The ice blocks themselves (GO 181247) are the cover: Frost Breath spares a target when a block sits on
    // the line from Sapphiron to it and the target is within 10 yd of that block (boss_sapphiron_40.cpp
    // IsValidExplosionTarget). Hiding only while a member still carried the Icebolt aura let the raid walk
    // off before the breath landed (Naxx40 focus runs: 16 Frost Breath deaths over two runs).
    GameObject* NearestIceBlock() { return bot->FindNearestGameObject(181247, 80.0f); }
    // Hide only once the last block is down. Icebolt splashes everyone near its target, and gathering behind
    // the FIRST block while the 2nd and 3rd were still being cast lost 5 bots to one bolt (9 Icebolt deaths
    // in tr-20261120-090913-1). The script summons the third block and starts Frost Breath on the same
    // event; the explosion lands 8.5 s later. A raid too small for three targets breathes after fewer.
    bool AllIceBlocksDown()
    {
        std::list<GameObject*> blocks;
        bot->GetGameObjectListWithEntryInGrid(blocks, 181247, 80.0f);
        uint32 targets = 0;
        if (Group* group = bot->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource())
                    if (member->IsAlive() && member->GetMapId() == bot->GetMapId())
                        ++targets;
        return !blocks.empty() && blocks.size() >= std::min<uint32>(3, targets);
    }
    bool WaitForExplosion()
    {
        if (!IsPhaseFlight())
            return false;

        // Not the Icebolt aura on a member: that sent the raid to the first frozen player while more bolts
        // were still coming.
        return AllIceBlocksDown();
    }
    bool FindPosToAvoidChill(std::vector<float>& dest)
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::Chill10, NaxxSpellIds::Chill25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("chill", bot);
        }
        if (!aura)
            return false;

        DynamicObject* dyn_obj = aura->GetDynobjOwner();
        if (!dyn_obj)
            return false;

        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        float angle = 0;
        uint32 index = botAI->GetGroupSlotIndex(bot);
        if (currentTarget)
        {
            if (botAI->IsRanged(bot))
            {
                if (bot->GetExactDist2d(currentTarget) <= 45.0f)
                    angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;
                else
                {
                    if (index % 2 == 0)
                        angle = bot->GetAngle(currentTarget) + M_PI / 2;
                    else
                        angle = bot->GetAngle(currentTarget) - M_PI / 2;
                }
            }
            else
            {
                if (index % 3 == 0)
                    angle = bot->GetAngle(currentTarget);
                else if (index % 3 == 1)
                    angle = bot->GetAngle(currentTarget) + M_PI / 2;
                else
                    angle = bot->GetAngle(currentTarget) - M_PI / 2;
            }
        }
        else
            angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;

        dest = {bot->GetPositionX() + cos(angle) * 5.0f, bot->GetPositionY() + sin(angle) * 5.0f, bot->GetPositionZ()};
        return true;
    }

private:
    void Reset()
    {
        _unit = nullptr;
        _was_flying = false;
        _last_land_ms = 0;
    }

    const uint32 POSITION_TIME_AFTER_LANDED = 5000;
    Unit* _unit = nullptr;
    bool _was_flying = false;
    uint32 _last_land_ms = 0;
};

class GluthBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos25 = {3331.48f, -3109.06f};
    const std::pair<float, float> mainTankPos10 = {3278.29f, -3162.06f};
    const std::pair<float, float> beforeDecimatePos = {3267.34f, -3175.68f};
    const std::pair<float, float> leftSlowDownPos = {3290.68f, -3141.65f};
    const std::pair<float, float> rightSlowDownPos = {3300.78f, -3151.98f};
    const std::pair<float, float> rangedPos = {3301.45f, -3139.29f};
    const std::pair<float, float> healPos = {3303.09f, -3135.24f};

    const float decimatedZombiePct = 10.0f;
    GluthBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "gluth");
            if (!_unit)
                return false;
        }
        if (_unit->IsInCombat())
        {
            if (_combat_start_ms == 0)
                _combat_start_ms = getMSTime();
        }
        else
            _combat_start_ms = 0;

        return true;
    }
    bool BeforeDecimate()
    {
        if (!_unit || !_unit->HasUnitState(UNIT_STATE_CASTING))
            return false;

        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (!spell)
            return false;

        SpellInfo const* info = spell->GetSpellInfo();
        if (!info)
            return false;

        if (NaxxSpellIds::MatchesAnySpellId(
                info, {NaxxSpellIds::Decimate10, NaxxSpellIds::Decimate25, NaxxSpellIds::Decimate25Alt}))
            return true;

        // Fallback to name for custom spell data.
        return info->SpellName[LOCALE_enUS] && botAI->EqualLowercaseName(info->SpellName[LOCALE_enUS], "decimate");
    }
    bool JustStartCombat() const { return _combat_start_ms != 0 && getMSTime() - _combat_start_ms < 10000; }
    bool IsZombieChow(Unit* unit) const { return unit && botAI->EqualLowercaseName(unit->GetName(), "zombie chow"); }

private:
    void Reset()
    {
        _unit = nullptr;
        _combat_start_ms = 0;
    }

    Unit* _unit = nullptr;
    uint32 _combat_start_ms = 0;
};

class LoathebBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {2877.57f, -3967.00f};
    const std::pair<float, float> rangePos = {2896.96f, -3980.61f};
    LoathebBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "loatheb");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class FourHorsemenBossHelper : public AiObject
{
public:
    const float posZ = 241.27f;
    const std::pair<float, float> attractPos[2] = {{2502.03f, -2910.90f},
                                                   {2484.61f, -2947.07f}};  // left (sir zeliek), right (lady blaumeux)
    FourHorsemenBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        else if (_combat_start_ms == 0)
            _combat_start_ms = getMSTime();

        if (_sir && (!_sir->IsInWorld() || !_sir->IsAlive()))
            Reset();

        if (!_sir)
        {
            _sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
            if (!_sir)
                return false;
        }
        // Naxx40 (Individual Progression) shares map 533 and the horsemen's names; its own strategy
        // (Naxx40Horsemen) owns that fight, so the WotLK one stays inert there.
        if (_sir->GetEntry() == NAXX40_ZELIEK_ENTRY)
            return false;
        _lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
        return true;
    }
    void Reset()
    {
        _sir = nullptr;
        _lady = nullptr;
        _combat_start_ms = 0;
        posToGo = 0;
    }
    bool IsAttracter(Player* bot)
    {
        if (NaxxHelpers::IsLargeNaxx(bot))
        {
            return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0) ||
                   botAI->IsAssistHealOfIndex(bot, 1) || botAI->IsAssistHealOfIndex(bot, 2);
        }
        return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0);
    }
    void CalculatePosToGo(Player* bot)
    {
        bool raid25 = NaxxHelpers::IsLargeNaxx(bot);
        Unit* lady = _lady;
        if (!lady)
            posToGo = 0;
        else
        {
            uint32 elapsed_ms = _combat_start_ms ? getMSTime() - _combat_start_ms : 0;
            // Interval: 24s - 15s - 15s - ...
            posToGo = !(elapsed_ms <= 9000 || ((elapsed_ms - 9000) / 67500) % 2 == 0);
            if (botAI->IsAssistRangedDpsOfIndex(bot, 0) || (raid25 && botAI->IsAssistHealOfIndex(bot, 1)))
                posToGo = 1 - posToGo;
        }
    }
    std::pair<float, float> CurrentAttractPos()
    {
        bool raid25 = NaxxHelpers::IsLargeNaxx(bot);
        float posX = attractPos[posToGo].first, posY = attractPos[posToGo].second;
        if (posToGo == 1)
        {
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float bias = 4.5f;
            if (raid25)
            {
                offset_x = -bias;
                offset_y = bias;
            }
            posX += offset_x;
            posY += offset_y;
        }
        return {posX, posY};
    }
    Unit* CurrentAttackTarget()
    {
        if (posToGo == 0)
            return _sir;

        return _lady;
    }

protected:
    Unit* _sir = nullptr;
    Unit* _lady = nullptr;
    uint32 _combat_start_ms = 0;
    int posToGo = 0;
};
class ThaddiusBossHelper : public AiObject
{
public:
    const std::pair<float, float> tankPosFeugen = {3522.94f, -3002.60f};
    const std::pair<float, float> tankPosStalagg = {3436.14f, -2919.98f};
    const std::pair<float, float> rangedPosFeugen = {3500.45f, -2997.92f};
    const std::pair<float, float> rangedPosStalagg = {3441.01f, -2942.04f};
    const float tankPosZ = 312.61f;
    ThaddiusBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "thaddius");
            if (!_unit)
                return false;
        }
        feugen = AI_VALUE2(Unit*, "find target", "feugen");
        stalagg = AI_VALUE2(Unit*, "find target", "stalagg");
        return true;
    }
    // Stalagg and Feugen only feign death: they stay alive at 1 HP, flagged NOT_SELECTABLE, until
    // Thaddius' tesla overload finishes them off or ACTION_RESTORE revives them. Testing IsAlive()
    // alone would latch the pet phase on forever.
    static bool IsPetActive(Unit* pet) { return pet && pet->IsAlive() && !pet->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE); }
    bool IsPhasePet() { return IsPetActive(feugen) || IsPetActive(stalagg); }
    bool IsPhaseTransition()
    {
        if (IsPhasePet())
            return false;

        return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }
    bool IsPhaseThaddius() { return !IsPhasePet() && !IsPhaseTransition(); }
    Unit* GetNearestPet()
    {
        Unit* unit = nullptr;
        if (IsPetActive(feugen))
            unit = feugen;

        if (IsPetActive(stalagg) &&
            (!IsPetActive(feugen) || bot->GetDistance(stalagg) < bot->GetDistance(feugen)))
            unit = stalagg;

        return unit;
    }
    // Each pet is fought on its own platform by its own half of the raid. A pet dragged more than 28 yd from
    // home makes its Tesla Coil shock the raid (boss_thaddius_40.cpp), and sending everyone to the nearest pet
    // left the other one chasing a target across the room: Naxx40 boss-focus runs lost 46 bots to Shock
    // (tp-20261120-065855-2). Main tank and even raid slots take Feugen; the first assist tank and odd slots
    // take Stalagg. Once one pet is down, everyone takes the other.
    Unit* GetAssignedPet()
    {
        bool const feugenUp = IsPetActive(feugen);
        bool const stalaggUp = IsPetActive(stalagg);
        if (!feugenUp || !stalaggUp)
            return feugenUp ? feugen : (stalaggUp ? stalagg : nullptr);

        bool toStalagg;
        if (PlayerbotAI::IsMainTank(bot))
            toStalagg = false;
        else if (PlayerbotAI::IsAssistTankOfIndex(bot, 0))
            toStalagg = true;
        else
        {
            int32 const slot = botAI->GetGroupSlotIndex(bot);
            toStalagg = slot >= 0 && (slot % 2) == 1;
        }
        return toStalagg ? stalagg : feugen;
    }
    std::pair<float, float> PetPhaseGetPosForTank()
    {
        if (GetAssignedPet() == feugen)
            return tankPosFeugen;

        return tankPosStalagg;
    }
    std::pair<float, float> PetPhaseGetPosForRanged()
    {
        if (GetAssignedPet() == feugen)
            return rangedPosFeugen;

        return rangedPosStalagg;
    }

protected:
    void Reset()
    {
        _unit = nullptr;
        feugen = nullptr;
        stalagg = nullptr;
    }

    Unit* _unit = nullptr;
    Unit* feugen = nullptr;
    Unit* stalagg = nullptr;
};

#endif
