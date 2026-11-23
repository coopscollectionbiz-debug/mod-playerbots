/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Raid-only resistance buff for bots (the BWL Onyxia Scale Cloak pattern): bots carry no resistance gear, and the
// fights below were built round it (WHE research 11-21, reports/Vanilla raid boss bot strategies.md): Sapphiron's
// raid ~150 frost, Ouro's Quake ~200 nature, Huhuran's Poison Bolt Volley nature, the Twins' warlock tanks 150+
// shadow, the Four Horsemen's Mograine-side tanks and melee 100-150 fire. A bot within RANGE of the living boss gets
// the matching +140 resistance aura (27053-27056: infinite, classless pet-style passives) for DURATION_MS, refreshed
// while it stays; it runs out on its own once the bot leaves, so nothing outlasts the raid.
// Only Individual Progression's level-60 entries for Naxxramas: the WotLK 80 versions are untouched.

#ifndef PLAYERBOTS_RAIDRESISTANCEBUFF_H
#define PLAYERBOTS_RAIDRESISTANCEBUFF_H

#include "Action.h"
#include "Ai/Raid/RaidDiag.h"
#include "Log.h"
#include "Creature.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "SpellAuras.h"
#include "Trigger.h"

namespace RaidResistanceBuff
{
    constexpr uint32 SPELL_FIRE = 27053;
    constexpr uint32 SPELL_FROST = 27054;
    constexpr uint32 SPELL_NATURE = 27055;
    constexpr uint32 SPELL_SHADOW = 27056;
    constexpr uint32 ALL_SPELLS[] = {SPELL_FIRE, SPELL_FROST, SPELL_NATURE, SPELL_SHADOW};

    constexpr float RANGE = 150.0f;
    constexpr int32 DURATION_MS = 60000;
    constexpr int32 REFRESH_BELOW_MS = 30000;

    struct Rule
    {
        uint32 map;
        uint32 boss;
        uint32 spell;
        bool tanksAndMeleeOnly;
    };

    constexpr Rule RULES[] = {
        {533, 351018, SPELL_FROST, false},  // Naxx40 Sapphiron
        // Naxx40 Four Horsemen: IP's Mograine casts Unholy Shadow (shadow, boss_four_horsemen_40.cpp), not vanilla's
        // Righteous Fire, so his side's tanks and melee get shadow, not the guides' fire.
        {533, 351037, SPELL_SHADOW, true},
        {531, 15517, SPELL_NATURE, false},  // AQ40 Ouro
        {531, 15509, SPELL_NATURE, false},  // AQ40 Princess Huhuran
        {531, 15276, SPELL_SHADOW, false},  // AQ40 Twin Emperors (Vek'lor)
        // Sunwell Eredar Twins: Grand Warlock Alythess's Pyrogenics/Blaze/Conflagration fire killed the raid
        // (11-21 ledger). User-approved 11-22. Hosted by the header-only "sunwell" strategy (SunwellStrategy.h).
        {580, 25166, SPELL_FIRE, false},
    };

    inline uint32 WantedSpell(Player* bot, PlayerbotAI* botAI)
    {
        if (!bot || !bot->IsAlive() || !bot->GetMap() || !bot->GetMap()->IsRaid())
            return 0;
        for (Rule const& r : RULES)
        {
            if (bot->GetMapId() != r.map)
                continue;
            if (r.tanksAndMeleeOnly && botAI->IsRanged(bot) && !botAI->IsTank(bot))
                continue;
            if (bot->FindNearestCreature(r.boss, RANGE, true))
                return r.spell;
        }
        return 0;
    }

    // True when the bot's resistance buffs differ from what it should carry now (missing, about to run out, or a
    // different school's still on it).
    inline bool NeedsUpdate(Player* bot, PlayerbotAI* botAI)
    {
        uint32 const wanted = WantedSpell(bot, botAI);
        for (uint32 spell : ALL_SPELLS)
        {
            Aura* aura = bot->GetAura(spell);
            if (spell == wanted)
            {
                if (!aura || aura->GetDuration() < REFRESH_BELOW_MS)
                    return true;
            }
            else if (aura)
                return true;
        }
        return false;
    }
}

class RaidResistanceBuffTrigger : public Trigger
{
public:
    RaidResistanceBuffTrigger(PlayerbotAI* botAI) : Trigger(botAI, "raid resistance buff", 2) {}
    bool IsActive() override { return RaidResistanceBuff::NeedsUpdate(bot, botAI); }
};

class RaidResistanceBuffAction : public Action
{
public:
    RaidResistanceBuffAction(PlayerbotAI* botAI) : Action(botAI, "raid resistance buff") {}

    bool Execute(Event /*event*/) override
    {
        uint32 const wanted = RaidResistanceBuff::WantedSpell(bot, botAI);
        for (uint32 spell : RaidResistanceBuff::ALL_SPELLS)
            if (spell != wanted && bot->HasAura(spell))
                bot->RemoveAurasDueToSpell(spell);
        if (!wanted)
            return true;
        Aura* aura = bot->GetAura(wanted);
        if (!aura)
        {
            uint32 const school = wanted == RaidResistanceBuff::SPELL_FIRE    ? SPELL_SCHOOL_FIRE
                                  : wanted == RaidResistanceBuff::SPELL_FROST  ? SPELL_SCHOOL_FROST
                                  : wanted == RaidResistanceBuff::SPELL_NATURE ? SPELL_SCHOOL_NATURE
                                                                               : SPELL_SCHOOL_SHADOW;
            int32 const before = bot->GetResistance(SpellSchools(school));
            aura = bot->AddAura(wanted, bot);
            RAID_DIAG("playerbots", "[RaidResistanceBuff] {} +{}: resistance {} -> {}{}", bot->GetName(), wanted, before,
                     bot->GetResistance(SpellSchools(school)), aura ? "" : " (AddAura FAILED)");
        }
        if (!aura)
            return false;
        aura->SetMaxDuration(RaidResistanceBuff::DURATION_MS);
        aura->SetDuration(RaidResistanceBuff::DURATION_MS);
        return true;
    }
};

#endif
