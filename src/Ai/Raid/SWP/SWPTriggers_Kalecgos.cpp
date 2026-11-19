/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPTriggers.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

// General

bool SunwellBotIsNotInCombatTrigger::IsActive()
{
    return !bot->IsInCombat() && bot->GetMapId() == SUNWELL_MAP_ID;
}

// Kalecgos

// Shared gate: the encounter is running and the bot is on the dragon-realm floor.
static bool KalecgosActiveInDragonRealm(Player* bot, Creature** dragonOut = nullptr)
{
    if (!IsInSunwell(bot) || IsInSpectralRealm(bot) || !bot->IsInCombat())
        return false;

    Creature* dragon = FindKalecgosDragon(bot);
    if (!dragon || !dragon->IsInCombat())
        return false;

    if (dragonOut)
        *dragonOut = dragon;
    return true;
}

bool KalecgosMeleeInBreathOrTailArcTrigger::IsActive()
{
    if (botAI->IsTank(bot) || botAI->IsRanged(bot) || botAI->IsHeal(bot))
        return false;

    Creature* dragon = nullptr;
    if (!KalecgosActiveInDragonRealm(bot, &dragon))
        return false;

    // Only correct bots actually at the boss; parked/dead-zone bots are generic-movement's
    // problem. Frost Breath owns the front cone, Tail Lash the rear cone.
    if (bot->GetExactDist2d(dragon) > 15.0f)
        return false;

    return dragon->isInFront(bot, float(M_PI) / 3) || dragon->isInBack(bot, float(M_PI) / 3);
}

bool KalecgosTankPositionBossTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot))
        return false;

    Creature* dragon = nullptr;
    if (!KalecgosActiveInDragonRealm(bot, &dragon))
        return false;

    if (dragon->GetVictim() != bot)
        return false;  // reposition only while actually holding him

    return bot->GetExactDist2d(KALECGOS_TANK_POSITION.GetPositionX(),
                               KALECGOS_TANK_POSITION.GetPositionY()) > 5.0f;
}

bool KalecgosRangedClumpedTrigger::IsActive()
{
    if (botAI->IsTank(bot) || botAI->IsMelee(bot))
        return false;  // ranged dps + healers only

    Creature* dragon = nullptr;
    if (!KalecgosActiveInDragonRealm(bot, &dragon))
        return false;

    if (bot->GetExactDist2d(dragon) < KALECGOS_RANGED_MIN_RANGE)
        return true;

    return GetNearestPlayerInRadius(bot, KALECGOS_RANGED_SPACING) != nullptr;
}

bool KalecgosSpectralRealmNeedsTankTrigger::IsActive()
{
    if (!botAI->IsTank(bot))
        return false;
    if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
        return false;
    if (!KalecgosActiveInDragonRealm(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Player* mainTank = GetGroupMainTank(bot);
    if (bot == mainTank)
        return false;  // the main tank holds the dragon topside

    // Deterministic pick among eligible topside tank BOTS (lowest GUID), mirroring the
    // healer trigger, so exhaustion or death rotates coverage instead of pinning one slot.
    Player* pick = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !botAI->IsTank(member))
            continue;
        if (IsInSpectralRealm(member))
            return false;  // the spectral realm already has a tank
        if (member == mainTank)
            continue;
        if (!GET_PLAYERBOT_AI(member))
            continue;  // never pick a human
        if (member->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
            continue;
        if (!pick || member->GetGUID() < pick->GetGUID())
            pick = member;
    }

    if (pick != bot)
        return false;

    return bot->FindNearestGameObject(static_cast<uint32>(SunwellObjects::GO_SPECTRAL_RIFT), 100.0f) != nullptr;
}

bool KalecgosSpectralRealmNeedsHealerTrigger::IsActive()
{
    if (!botAI->IsHeal(bot))
        return false;
    if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
        return false;
    if (!KalecgosActiveInDragonRealm(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Fire only for the deterministic pick (lowest GUID among eligible topside healers)
    // so exactly one healer walks to the rift at a time - no shared claim state needed.
    Player* pick = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !botAI->IsHeal(member))
            continue;
        if (IsInSpectralRealm(member))
            return false;  // the spectral realm already has a healer
        if (!GET_PLAYERBOT_AI(member))
            continue;  // never pick a human - they can't be steered
        if (member->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
            continue;
        if (!pick || member->GetGUID() < pick->GetGUID())
            pick = member;
    }

    if (pick != bot)
        return false;

    return bot->FindNearestGameObject(static_cast<uint32>(SunwellObjects::GO_SPECTRAL_RIFT), 100.0f) != nullptr;
}

bool KalecgosStrayOffPlatformTrigger::IsActive()
{
    // A bot below the platform WITHOUT the spectral aura is stranded on the room's
    // lower level - both bosses are invisible and unreachable from there.
    if (!IsInSunwell(bot) || IsInSpectralRealm(bot) || !bot->IsInCombat())
        return false;

    if (bot->GetPositionZ() >= KALECGOS_PLATFORM_Z)
        return false;

    Creature* dragon = FindKalecgosDragon(bot);
    return dragon && dragon->IsInCombat();
}

bool KalecgosSpectralRealmNeedsDpsTrigger::IsActive()
{
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;
    if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
        return false;
    if (!KalecgosActiveInDragonRealm(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Deterministic multi-pick (healer-trigger pattern, extended to N): keep
    // KALECGOS_SPECTRAL_DPS_TARGET dps below, chosen as the lowest GUIDs among
    // eligible topside dps BOTS. Humans below still count toward the quota.
    uint32 below = 0;
    std::vector<ObjectGuid> eligible;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || botAI->IsTank(member) || botAI->IsHeal(member))
            continue;
        if (IsInSpectralRealm(member))
        {
            ++below;
            continue;
        }
        if (!GET_PLAYERBOT_AI(member))
            continue;  // never pick a human
        if (member->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_EXHAUSTION)))
            continue;
        eligible.push_back(member->GetGUID());
    }

    if (below >= KALECGOS_SPECTRAL_DPS_TARGET)
        return false;

    size_t const needed = KALECGOS_SPECTRAL_DPS_TARGET - below;
    std::sort(eligible.begin(), eligible.end());
    if (eligible.size() > needed)
        eligible.resize(needed);
    if (std::find(eligible.begin(), eligible.end(), bot->GetGUID()) == eligible.end())
        return false;

    return bot->FindNearestGameObject(static_cast<uint32>(SunwellObjects::GO_SPECTRAL_RIFT), 100.0f) != nullptr;
}

bool KalecgosSathrovarrLooseTrigger::IsActive()
{
    if (!botAI->IsTank(bot) || !IsInSpectralRealm(bot))
        return false;

    Creature* demon = FindSathrovarr(bot);
    if (!demon || !demon->IsAlive())
        return false;

    if (demon->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BANISH)))
        return false;

    Unit* victim = demon->GetVictim();
    if (victim == bot)
        return false;

    // Loose = beating on nobody, on a non-player (Kalec!), or on a non-tank player.
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    return !victimPlayer || !botAI->IsTank(victimPlayer);
}

bool KalecgosSpectralDpsWrongTargetTrigger::IsActive()
{
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !IsInSpectralRealm(bot))
        return false;

    Creature* demon = FindSathrovarr(bot);
    if (!demon || !demon->IsAlive())
        return false;

    if (demon->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BANISH)))
        return false;

    return AI_VALUE(Unit*, "current target") != demon;
}

bool KalecgosKalecNeedsHealingTrigger::IsActive()
{
    if (!botAI->IsHeal(bot) || !IsInSpectralRealm(bot))
        return false;

    Creature* kalec = FindKalecFriendly(bot);
    return kalec && kalec->IsAlive() && kalec->GetHealthPct() < 85.0f;
}
