/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "Ai/Raid/RaidDiag.h"
#include "NaxxSpellIds.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "StringFormat.h"

#include <mutex>
#include <unordered_map>

bool ThaddiusAttackNearestPetAction::isUseful()
{
    if (!helper.UpdateBossAI())
        return false;

    if (!helper.IsPhasePet())
        return false;

    // The assigned pet can be the far one (the platforms are ~80 yd apart).
    Unit* target = helper.GetAssignedPet();
    if (!target || !bot->IsWithinDistInMap(target, 150.0f))
        return false;

    return true;
}

// Pet-phase trace (AiPlayerbot.RaidDiagnostics): every 2 s
// per instance, each pet's distance from home (the 28 yd Tesla leash) and who it is on.
static void ThaddiusPetTrace(Player* bot, Unit* feugen, Unit* stalagg)
{
    if (!RaidDiagEnabled())
        return;
    static std::mutex m;
    static std::unordered_map<uint32, uint32> last;
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> lock(m);
        uint32& l = last[bot->GetInstanceId()];
        if (l && getMSTimeDiff(l, now) < 2000)
            return;
        l = now;
    }
    auto line = [](Unit* pet) -> std::string
    {
        if (!pet)
            return "gone";
        Creature* c = pet->ToCreature();
        Unit* v = pet->GetVictim();
        return Acore::StringFormat("{:.1f} yd from home {:.0f}% on {}{}", c ? c->GetHomePosition().GetExactDist(pet) : -1.0f,
                                   pet->GetHealthPct(), v ? v->GetName() : "none",
                                   v && v->IsPlayer() && PlayerbotAI::IsTank(v->ToPlayer()) ? " (tank)" : "");
    };
    RAID_DIAG("playerbots.thaddius", "[Naxx40 Thaddius] instance {} | Feugen {} | Stalagg {}", bot->GetInstanceId(),
              line(feugen), line(stalagg));
}

bool ThaddiusAttackNearestPetAction::Execute(Event /*event*/)
{
    Unit* target = helper.GetAssignedPet();
    if (!target)
        return false;
    ThaddiusPetTrace(bot, helper.GetFeugen(), helper.GetStalagg());

    // The two pet tanks hold their platform spot whether or not they have aggro yet: a pet follows its
    // victim, and one drawn down to the low floor leaves its 28 yd leash and the coils shock the raid
    // (focus runs landed on the low floor; 20 Shock deaths with both pets still at 99-100%).
    // In combat only: out of combat the tank spots are inside the pets' aggro range (Feugen pulled at
    // 21.7 yd), and the pull itself is DungeonClear's (both pets at once, each onto its own tank).
    // The bot a pet is on always holds it (DungeonClear's normal pull hands the pets to two non-leader tanks, and
    // Magnetic Pull swaps them); the fixed pet tanks step in only while the pet is on someone who is not a tank.
    Unit* const petVictim = target->GetVictim();
    bool const petHolder = petVictim == bot ||
                           ((PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0)) &&
                            !(petVictim && petVictim->IsPlayer() && botAI->IsTank(petVictim->ToPlayer())));
    if (petHolder && bot->IsInCombat())
    {
        std::pair<float, float> const posForTank = helper.PetPhaseGetPosForTank();
        if (bot->GetExactDist2d(posForTank.first, posForTank.second) > 4.0f)
        {
            if (AI_VALUE(Unit*, "current target") != target && bot->IsWithinLOSInMap(target))
                Attack(target);
            return MoveTo(533, posForTank.first, posForTank.second, helper.tankPosZ, false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
    }

    if (!bot->IsWithinLOSInMap(target))
        return MoveTo(target, 0, MovementPriority::MOVEMENT_COMBAT);

    if (AI_VALUE(Unit*, "current target") != target)
        return Attack(target);

    if (botAI->IsTank(bot) && AI_VALUE2(bool, "has aggro", "current target"))
    {
        std::pair<float, float> posForTank = helper.PetPhaseGetPosForTank();
        return MoveTo(533, posForTank.first, posForTank.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    if (botAI->IsRanged(bot))
    {
        std::pair<float, float> posForRanged = helper.PetPhaseGetPosForRanged();
        return MoveTo(533, posForRanged.first, posForRanged.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool ThaddiusMoveToPlatformAction::isUseful() { return true; }

bool ThaddiusMoveToPlatformAction::Execute(Event /*event*/)
{
    std::vector<std::pair<float, float>> position = {
        // high left
        {3462.99f, -2918.90f},
        // high right
        {3520.65f, -2976.51f},
        // low left
        {3471.36f, -2910.65f},
        // low right
        {3528.80f, -2967.04f},
        // center
        {3512.19f, -2928.58f},
    };
    float high_z = 312.00f, low_z = 304.02f;
    bool is_left = bot->GetDistance2d(position[0].first, position[0].second) <
                   bot->GetDistance2d(position[1].first, position[1].second);
    if (bot->GetPositionZ() >= (high_z - 3.0f))
    {
        if (is_left)
        {
            if (!MoveTo(bot->GetMapId(), position[0].first, position[0].second, high_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            {
                float distance = bot->GetExactDist2d(position[0].first, position[0].second);
                if (distance < sPlayerbotAIConfig.contactDistance)
                    JumpTo(bot->GetMapId(), position[2].first, position[2].second, low_z, MovementPriority::MOVEMENT_COMBAT);
                    // bot->TeleportTo(bot->GetMapId(), position[2].first, position[2].second, low_z, bot->GetOrientation());
            }
        }
        else
        {
            if (!MoveTo(bot->GetMapId(), position[1].first, position[1].second, high_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            {
                float distance = bot->GetExactDist2d(position[1].first, position[1].second);
                if (distance < sPlayerbotAIConfig.contactDistance)
                    JumpTo(bot->GetMapId(), position[3].first, position[3].second, low_z, MovementPriority::MOVEMENT_COMBAT);
                    // bot->TeleportTo(bot->GetMapId(), position[3].first, position[3].second, low_z, bot->GetOrientation());
            }
        }
    }
    else
        return MoveTo(bot->GetMapId(), position[4].first, position[4].second, low_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    return true;
}

bool ThaddiusMovePolarityAction::isUseful()
{
    return !botAI->IsMainTank(bot) || AI_VALUE2(bool, "has aggro", "current target");
}

bool ThaddiusMovePolarityAction::Execute(Event /*event*/)
{
    std::vector<std::pair<float, float>> position = {
        // left melee
        {3508.29f, -2920.12f},
        // left ranged
        {3501.72f, -2913.36f},
        // right melee
        {3519.74f, -2931.69f},
        // right ranged
        {3524.32f, -2936.26f},
        // center melee
        {3512.19f, -2928.58f},
        // center ranged
        {3504.68f, -2936.68f},
    };
    uint32 idx;
    if (NaxxSpellIds::HasAnyAura(bot,
            {NaxxSpellIds::NegativeCharge10, NaxxSpellIds::NegativeCharge25, NaxxSpellIds::NegativeChargeStack}) ||
        botAI->HasAura("negative charge", bot, false, false, -1, true))
    {
        idx = 0;
    }
    else if (NaxxSpellIds::HasAnyAura(bot,
                 {NaxxSpellIds::PositiveCharge10, NaxxSpellIds::PositiveCharge25, NaxxSpellIds::PositiveChargeStack}) ||
             botAI->HasAura("positive charge", bot, false, false, -1, true))
    {
        idx = 1;
    }
    else
    {
        idx = 2;
    }
    idx = idx * 2 + botAI->IsRanged(bot);
    // Charges hit opposite-charge allies within 13 yd (28062/28085), and the left/right melee spots above are only 16 yd
    // apart: Naxx40 kills lost 11-23 bots to Positive/Negative Charge (11-22). Thaddius is rooted with a 10 yd combat
    // reach, so keep the sides on the same axis but measured from him: melee 11 yd out (22 yd apart, still in melee
    // reach), ranged 20 yd out. The no-charge (centre) spots are unchanged.
    if (idx < 4)
        if (Unit* thaddius = AI_VALUE2(Unit*, "find target", "thaddius"))
        {
            float const cx = thaddius->GetPositionX(), cy = thaddius->GetPositionY();
            // Side axis from the original left melee spot, relative to Thaddius' home (3513.84, -2926.55).
            float const lx = -0.653f, ly = 0.757f;
            float const side = idx < 2 ? 1.0f : -1.0f;
            float const r = botAI->IsRanged(bot) ? 20.0f : 11.0f;
            return MoveTo(bot->GetMapId(), cx + side * lx * r, cy + side * ly * r, bot->GetPositionZ(), false, false,
                          false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    return MoveTo(bot->GetMapId(), position[idx].first, position[idx].second, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
