/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PartyMemberWithoutAuraValue.h"
#include "GenericBuffUtils.h"
#include "Playerbots.h"

extern std::vector<std::string> split(std::string const s, char delim);

class PlayerWithoutAuraPredicate : public FindPlayerPredicate, public PlayerbotAIAware
{
public:
    PlayerWithoutAuraPredicate(PlayerbotAI* botAI, std::string const aura)
        : FindPlayerPredicate(), PlayerbotAIAware(botAI), auras(split(aura, ','))
    {
    }

public:
    bool Check(Unit* unit) override
    {
        if (!unit->IsAlive())
            return false;

        for (std::vector<std::string>::iterator i = auras.begin(); i != auras.end(); ++i)
        {
            if (!ai::buff::BuffBelowRefreshTarget(botAI, botAI->GetAura(*i, unit), 0))
                return false;
        }

        return true;
    }

private:
    std::vector<std::string> auras;
};

Unit* PartyMemberWithoutAuraValue::Calculate()
{
    PlayerWithoutAuraPredicate predicate(botAI, qualifier);

    ObjectGuid driveByGuid =
        context
            ->GetValue<ObjectGuid>(
                "drive by party target")
            ->Get();

    time_t driveByUntil =
        context
            ->GetValue<time_t>(
                "drive by party target until")
            ->Get();

    if (driveByGuid)
    {
        time_t now = time(nullptr);

        if (!driveByUntil || now >= driveByUntil)
        {
            context
                ->GetValue<ObjectGuid>(
                    "drive by party target")
                ->Set(ObjectGuid::Empty);

            context
                ->GetValue<time_t>(
                    "drive by party target until")
                ->Set(0);
        }
        else
        {
            Player* player =
                botAI->GetPlayer(driveByGuid);

            bool valid =
                player &&
                player != bot &&
                player->IsInWorld() &&
                player->IsAlive() &&
                !player->IsGameMaster() &&
                !GET_PLAYERBOT_AI(player) &&
                player->IsFriendlyTo(bot) &&
                bot->GetDistance(player) <= 15.0f &&
                bot->IsWithinLOS(
                    player->GetPositionX(),
                    player->GetPositionY(),
                    player->GetPositionZ());

            if (!valid)
            {
                context
                    ->GetValue<ObjectGuid>(
                        "drive by party target")
                    ->Set(ObjectGuid::Empty);

                context
                    ->GetValue<time_t>(
                        "drive by party target until")
                    ->Set(0);
            }
            else if (predicate.Check(player))
            {
                LOG_DEBUG(
                    "playerbots",
                    "[DRIVEBY-BUFF] override-hit "
                    "bot={} target={} qualifier={}",
                    bot->GetName(),
                    player->GetName(),
                    qualifier);

                // For buff targeting only, this player now behaves
                // like the party member selected by the normal
                // Playerbots buff system.
                return player;
            }
            else
            {
                LOG_DEBUG(
                    "playerbots",
                    "[DRIVEBY-BUFF] override-reject "
                    "bot={} target={} qualifier={}",
                    bot->GetName(),
                    player->GetName(),
                    qualifier);
            }
        }
    }

    // Normal real-party behavior is completely unchanged.
    return FindPartyMember(predicate);
}
