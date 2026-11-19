/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ArenaCoordValues.h"

#include <tuple>

#include "AiFactory.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "Config.h"
#include "Group.h"
#include "Playerbots.h"
#include "StringConvert.h"
#include "Tokenize.h"

namespace
{
// WotLK-canonical "turtle" defensives: a target under one of these is a wasted swap.
constexpr uint32 TURTLE_AURAS[] = {
    642,    // Divine Shield
    45438,  // Ice Block
    47585,  // Dispersion
    1022,   // Hand of Protection (rank 1)
    5599,   // Hand of Protection (rank 2)
    10278,  // Hand of Protection (rank 3)
    19263,  // Deterrence
    31224,  // Cloak of Shadows
};
}

std::vector<uint32> ArenaKillTargetValue::ParseBands(char const* key, std::vector<uint32> const& defaults)
{
    std::string csv = sConfigMgr->GetOption<std::string>(key, "");
    if (csv.empty())
        return defaults;

    std::vector<uint32> out;
    bool malformed = false;
    for (std::string_view tok : Acore::Tokenize(csv, ',', false))
    {
        if (auto v = Acore::StringTo<uint32>(tok))
            out.push_back(*v);
        else
            malformed = true;
    }

    if (malformed || out.size() < defaults.size())
    {
        LOG_WARN("playerbots", "[ArenaCoord] malformed csv for {} (\"{}\") — using defaults.", key, csv);
        return defaults;
    }

    return out;
}

bool ArenaKillTargetValue::IsHealerSpec(Player* p)
{
    if (!p)
        return false;

    uint8 tab = AiFactory::GetPlayerSpecTab(p);
    switch (p->getClass())
    {
        case CLASS_PRIEST:
            return tab == PRIEST_TAB_DISCIPLINE || tab == PRIEST_TAB_HOLY;
        case CLASS_PALADIN:
            return tab == PALADIN_TAB_HOLY;
        case CLASS_DRUID:
            return tab == DRUID_TAB_RESTORATION;
        case CLASS_SHAMAN:
            return tab == SHAMAN_TAB_RESTORATION;
        default:
            return false;
    }
}

uint8 ArenaKillTargetValue::SquishRank(uint8 cls)
{
    switch (cls)
    {
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
            return 0;  // cloth
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return 1;  // leather
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return 2;  // mail
        default:
            return 3;  // plate (warrior, paladin, death knight)
    }
}

bool ArenaKillTargetValue::HasTurtleAura(Unit* u)
{
    if (!u)
        return false;

    for (uint32 spellId : TURTLE_AURAS)
        if (u->HasAura(spellId))
            return true;

    return false;
}

uint8 ArenaKillTargetValue::SharpnessFor(Player* bot, uint8 arenaType)
{
    if (!bot)
        return 4;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return 4;  // a real player plays at full sharpness

    // upstream 17214b3 renamed HasRealPlayerMaster() -> HasGameClientMaster() (same meaning:
    // the master is a human at a client, i.e. a real player or a selfbot).
    if (botAI->HasGameClientMaster())
        return 4;

    // Grouped with any real player (no playerbot AI attached) => full sharpness.
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && !GET_PLAYERBOT_AI(member))
                return 4;
        }
    }

    // Solo-bot team: band by the bot's own arena team rating.
    uint32 rating = 1500;  // missing team => mid rating
    uint8 slot = 0xFF;
    switch (arenaType)
    {
        case ARENA_TYPE_2v2: slot = 0; break;
        case ARENA_TYPE_3v3: slot = 1; break;
        case ARENA_TYPE_5v5: slot = 2; break;
        default: break;
    }
    if (slot < 3)
        if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(bot->GetArenaTeamId(slot)))
            rating = team->GetRating();

    // Ceilings shared with mod-arena-roster's tier bands (one config store at runtime).
    // Latched at first use (see ParseBands) — restart the worldserver to apply conf edits.
    static std::vector<uint32> const ceilings = ParseBands("ArenaRoster.TierCeilings", {800, 1400, 1800});

    if (rating <= ceilings[0])
        return 1;
    if (rating <= ceilings[1])
        return 2;
    if (rating <= ceilings[2])
        return 3;
    return 4;
}

Unit* ArenaKillTargetValue::Calculate()
{
    if (!bot->InBattleground())
        return nullptr;

    Battleground* bg = bot->GetBattleground();
    if (!bg || !bg->isArena())
        return nullptr;

    // Rank key: non-turtled first, healers first, squishiest first, lowest GUID first.
    // Every team member computes the same ordering => shared focus target with no messaging.
    using RankKey = std::tuple<bool, uint8, uint8, ObjectGuid>;

    Player* best = nullptr;
    RankKey bestKey;

    for (auto const& itr : bg->GetPlayers())
    {
        Player* enemy = itr.second;
        if (!enemy || !enemy->IsAlive())
            continue;

        // GetBgTeamId, not GetTeamId: rated arena teams can share a faction.
        if (enemy->GetBgTeamId() == bot->GetBgTeamId())
            continue;

        RankKey key{HasTurtleAura(enemy), IsHealerSpec(enemy) ? 0 : 1, SquishRank(enemy->getClass()),
                    enemy->GetGUID()};
        if (!best || key < bestKey)
        {
            best = enemy;
            bestKey = key;
        }
    }

    // If every enemy is turtled the best turtled one is still returned; nullptr if no enemies.
    return best;
}
