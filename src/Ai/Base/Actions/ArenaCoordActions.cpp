/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ArenaCoordActions.h"

#include <algorithm>

#include "ArenaCoordValues.h"
#include "Battleground.h"
#include "Event.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Opcodes.h"
#include "Player.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "WorldPacket.h"

namespace
{
// "PvP Trinket" — the on-use spell shared by every season's Medallion of the Alliance/Horde
// in this DB (verified: all 28 medallion item_template rows carry spellid_1 = 42292).
constexpr uint32 PVP_TRINKET_SPELL = 42292;

// The equipped medallion, or nullptr if none is worn or the effect is on cooldown.
Item* FindPvpTrinket(Player* bot)
{
    for (uint8 slot : {EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2})
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& itemSpell = item->GetTemplate()->Spells[i];
            if (itemSpell.SpellId == int32(PVP_TRINKET_SPELL) && itemSpell.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                return bot->HasSpellCooldown(PVP_TRINKET_SPELL) ? nullptr : item;
        }
    }

    return nullptr;
}

// Loss-of-control effects the medallion (42292) removes and that are worth breaking.
// Polymorph is matched via MECHANIC_POLYMORPH, NOT a bare HasAuraType(SPELL_AURA_TRANSFORM):
// TRANSFORM also matches non-CC self-forms (Warlock Metamorphosis, novelty transforms), which
// would burn the 2-min medallion for nothing. UNIT_STATE_LOST_CONTROL was considered and
// rejected as the umbrella — it is CONTROLLED|JUMPING|CHARGING (UnitDefines.h:219, CONTROLLED
// = confused|stunned|fleeing), so it false-positives on jumps/charges and misses charm.
bool IsCCd(Player* p)
{
    return p->HasAuraType(SPELL_AURA_MOD_FEAR) || p->HasAuraType(SPELL_AURA_MOD_STUN) ||
           p->HasAuraType(SPELL_AURA_MOD_CONFUSE) || p->HasAuraType(SPELL_AURA_MOD_CHARM) ||
           p->HasAuraWithMechanic(1ULL << MECHANIC_POLYMORPH);
}
}  // namespace

bool ArenaPvpTrinketAction::isUseful()
{
    // Cheap-first: arena gate, then aura-type lookups, then the equipment scan.
    return bot->InArena() && IsCCd(bot) && FindPvpTrinket(bot) != nullptr;
}

bool ArenaPvpTrinketAction::Execute(Event /*event*/)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg || !bg->isArena())
        return false;

    // "Matters" check: don't burn the 2-min cooldown on throwaway CC. Break when the bot is
    // in danger (< 50% HP) or the team's healer is locked down alongside it (classic
    // cross-CC kill window).
    bool matters = bot->GetHealthPct() < 50.0f;
    if (!matters)
    {
        for (auto const& itr : bg->GetPlayers())
        {
            Player* mate = itr.second;
            if (!mate || mate == bot || !mate->IsAlive() || mate->GetBgTeamId() != bot->GetBgTeamId())
                continue;

            if (ArenaKillTargetValue::IsHealerSpec(mate) && IsCCd(mate))
            {
                matters = true;
                break;
            }
        }
    }
    if (!matters)
        return false;

    // Sharpness reaction delay: this action is evaluated at ~1s cadence while the bot sits in
    // CC, so holding with probability (reactMs - 1000)/reactMs per evaluation makes the
    // EXPECTED wait ≈ reactMs — a probabilistic hold, not a scheduled timer. Full-sharp bots
    // (band 4, 300ms <= 1s) break instantly. This is the ReactMs mechanism Task 10 reserved;
    // it does NOT stack with the tunnel-chance decay. Latched at first use (ParseBands) —
    // restart the worldserver to apply conf edits.
    uint8 sharp = ArenaKillTargetValue::SharpnessFor(bot, bg->GetArenaType());
    static std::vector<uint32> const reactBands =
        ArenaKillTargetValue::ParseBands("ArenaCoord.ReactMs", {3000, 1800, 900, 300});
    uint32 reactMs = reactBands[std::min<size_t>(sharp >= 1 ? sharp - 1 : 0, reactBands.size() - 1)];
    if (reactMs > 1000 && urand(0, reactMs) > 1000)
        return false;

    Item* item = FindPvpTrinket(bot);
    if (!item)
        return false;

    // Use the item exactly the way a real client click does (mirrors UseTrinketAction's cast
    // path): a CMSG_USE_ITEM through HandleUseItemOpcode. The core's CheckCast honors the
    // medallion's usable-while-stunned/feared/confused spell attributes, so this is the one
    // path that provably works from inside CC — a plain CastSpell-style gate would refuse.
    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    uint8 castCount = 1;
    ObjectGuid itemGuid = item->GetGUID();
    uint32 glyphIndex = 0;
    uint8 castFlags = 0;
    uint32 targetFlag = TARGET_FLAG_NONE;

    WorldPacket packet(CMSG_USE_ITEM);
    packet << bagIndex << slot << castCount << PVP_TRINKET_SPELL << itemGuid << glyphIndex << castFlags;
    packet << targetFlag << bot->GetPackGUID();

    bot->GetSession()->HandleUseItemOpcode(packet);

    LOG_DEBUG("playerbots", "[ArenaCoord] {} trinkets out of CC (sharp={})", bot->GetName(), sharp);
    return true;
}
