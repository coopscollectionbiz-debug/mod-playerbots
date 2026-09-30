/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WarriorTriggers.h"
#include "Ai/Base/Util/GenericBuffUtils.h"
#include "Playerbots.h"

namespace
{
constexpr uint32 SPELL_VIGILANCE = 50720;
constexpr uint32 SPELL_SHATTERING_THROW = 64382;
constexpr uint32 SPELL_DIVINE_SHIELD = 642;
constexpr uint32 SPELL_ICE_BLOCK = 45438;
constexpr uint32 SPELL_BLESSING_OF_PROTECTION = 41450;
constexpr uint32 SPELL_COMMANDING_PRESENCE_RANKS[] = { 12318, 12857, 12858, 12860, 12861 };
}

bool BloodrageBuffTrigger::IsActive()
{
    return AI_VALUE2(uint8, "health", "self target") >= sPlayerbotAIConfig.mediumHealth &&
           AI_VALUE2(uint8, "rage", "self target") < 20;
}

bool VigilanceTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_VIGILANCE))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        // The action casts Vigilance on dps, but the trigger fails if any party member has
        // Vigilance from the bot (so the player can have a bot cast Vigilance on any group member).
        if (member->HasAura(SPELL_VIGILANCE, bot->GetGUID()))
            return false;
    }

    return true;
}

bool ShatteringThrowTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_SHATTERING_THROW) || bot->HasSpellCooldown(SPELL_SHATTERING_THROW))
        return false;

    GuidVector enemies = AI_VALUE(GuidVector, "possible targets");

    for (ObjectGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || !enemy->IsAlive() || enemy->IsFriendlyTo(bot))
            continue;

        if (bot->IsWithinDistInMap(enemy, 25.0f) &&
            (enemy->HasAura(SPELL_DIVINE_SHIELD) ||
             enemy->HasAura(SPELL_ICE_BLOCK) ||
             enemy->HasAura(SPELL_BLESSING_OF_PROTECTION)))
        {
            return true;
        }
    }

    return false;
}

bool BattleShoutTrigger::IsActive()
{
    uint32 battleShoutSpellId = AI_VALUE2(uint32, "spell id", "battle shout");
    if (!battleShoutSpellId)
        return false;

    SpellInfo const* bsInfo = sSpellMgr->GetSpellInfo(battleShoutSpellId);
    if (!bsInfo)
        return false;

    int32 bsApValue = 0;
    for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
    {
        if (bsInfo->Effects[eff].ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER)
        {
            bsApValue = bsInfo->Effects[eff].BasePoints + 1;
            break;
        }
    }

    if (!bsApValue)
        return false;

    static const float commandingPresenceBonus[] = {
        0.05f, 0.10f, 0.15f, 0.20f, 0.25f
    };

    float cpBonus = 0.0f;
    for (int rank = 4; rank >= 0; --rank)
    {
        if (bot->HasAura(SPELL_COMMANDING_PRESENCE_RANKS[rank]))
        {
            cpBonus = commandingPresenceBonus[rank];
            break;
        }
    }

    int32 effectiveBsAp = int32(bsApValue * (1.0f + cpBonus));

    static char const* blessingNames[] = {
        "blessing of might", "greater blessing of might", nullptr
    };

    // Returns true when this unit would benefit from the warrior refreshing
    // Battle Shout. An equal-or-stronger Blessing of Might means there is no
    // reason to refresh Battle Shout for this unit.
    auto needsBattleShout = [&](Unit* target) -> bool
    {
        if (!target || !target->IsAlive())
            return false;

        Aura* battleShout = botAI->GetAura("battle shout", target);
        if (!ai::buff::BuffBelowRefreshTarget(botAI, battleShout, 0))
            return false;

        for (int i = 0; blessingNames[i] != nullptr; ++i)
        {
            Aura* bom = botAI->GetAura(blessingNames[i], target);
            if (!bom)
                continue;

            SpellInfo const* bomInfo = bom->GetSpellInfo();
            if (!bomInfo)
                continue;

            for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
            {
                if (bomInfo->Effects[eff].ApplyAuraName != SPELL_AURA_MOD_ATTACK_POWER)
                    continue;

                int32 bomApValue = bomInfo->Effects[eff].BasePoints + 1;
                if (bomApValue >= effectiveBsAp)
                    return false;

                break;
            }
        }

        return true;
    };

    // Preserve the original solo/self-refresh behavior.
    if (needsBattleShout(bot))
        return true;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Battle Shout is an area buff centered on the warrior. If any living
    // nearby group member would benefit, refresh it even when the warrior's
    // own long-duration Battle Shout is still active.
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member == bot)
            continue;

        if (!member->IsInWorld())
            continue;

        if (member->GetMap() != bot->GetMap() ||
            bot->GetDistance(member) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }

        if (needsBattleShout(member))
            return true;
    }

    return false;
}
