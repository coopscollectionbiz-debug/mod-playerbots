/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PVPVALUES_H
#define PLAYERBOTS_PVPVALUES_H

#include "NamedObjectContext.h"
#include "SharedDefines.h"
#include "Value.h"

class PlayerbotAI;
class Unit;

class BgTypeValue : public ManualSetValue<uint32>
{
public:
    BgTypeValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "bg type") {}
};

class ArenaTypeValue : public ManualSetValue<uint32>
{
public:
    ArenaTypeValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "arena type") {}
};

class BgRoleValue : public ManualSetValue<uint32>
{
public:
    BgRoleValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "bg role") {}
};

// mod-battleground-bots (overlay patch 0019): getMSTime() of the last director-forced dismount
// (BGTactics::moveToObjective). 0 = never. CheckMountStateAction reads it to hold off the stock
// remount for a few seconds, so a bot walking into a contested objective does not mount/dismount
// in a loop. Mirrors BgRoleValue exactly: a manual-set uint32 defaulting to 0.
class BgDismountTimeValue : public ManualSetValue<uint32>
{
public:
    BgDismountTimeValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "bg dismount time") {}
};

class BgMastersValue : public SingleCalculatedValue<std::vector<CreatureData const*>>, public Qualified
{
public:
    BgMastersValue(PlayerbotAI* botAI) : SingleCalculatedValue<std::vector<CreatureData const*>>(botAI, "bg masters") {}

    std::vector<CreatureData const*> Calculate() override;
};

class BgMasterValue : public CDPairCalculatedValue, public Qualified
{
public:
    BgMasterValue(PlayerbotAI* botAI) : CDPairCalculatedValue(botAI, "bg master", 60) {}

    CreatureData const* Calculate() override;
    CreatureData const* NearestBm(bool allowDead = true);
};

class RpgBgTypeValue : public CalculatedValue<BattlegroundTypeId>
{
public:
    RpgBgTypeValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "rpg bg type") {}

    BattlegroundTypeId Calculate() override;
};

class FlagCarrierValue : public UnitCalculatedValue
{
public:
    FlagCarrierValue(PlayerbotAI* botAI, bool sameTeam = false, bool ignoreRange = false)
        : UnitCalculatedValue(botAI), sameTeam(sameTeam), ignoreRange(ignoreRange)
    {
    }

    Unit* Calculate() override;

private:
    bool sameTeam;
    bool ignoreRange;
};

#endif
