/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCHELPERS_H
#define PLAYERBOTS_TOTCHELPERS_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// Trial of the Crusader, from the WotLK guides and the core's TrialOfTheCrusader scripts. The core
// casts the base spell ids below and swaps in the 25-player and heroic versions by difficulty.
namespace TotcHelpers
{

enum TotcNpcs : uint32
{
    // The Beasts of Northrend
    NPC_GORMOK = 34796,
    NPC_SNOBOLD_VASSAL = 34800,
    NPC_DREADSCALE = 34799,
    NPC_ACIDMAW = 35144,
    NPC_SLIME_POOL = 35176,
    MODEL_ACIDMAW_MOBILE = 29816,
    MODEL_DREADSCALE_MOBILE = 24564,
    NPC_ICEHOWL = 34797,
    NPC_FURIOUS_CHARGE_STALKER = 35062,

    // Lord Jaraxxus
    NPC_JARAXXUS = 34780,
    NPC_INFERNAL_VOLCANO = 34813,
    NPC_NETHER_PORTAL = 34825,
    NPC_MISTRESS_OF_PAIN = 34826,

    // The Faction Champions' healers
    NPC_ALLIANCE_DRUID_RESTORATION = 34469,
    NPC_ALLIANCE_PALADIN_HOLY = 34465,
    NPC_ALLIANCE_PRIEST_DISCIPLINE = 34466,
    NPC_ALLIANCE_SHAMAN_RESTORATION = 34470,
    NPC_HORDE_DRUID_RESTORATION = 34459,
    NPC_HORDE_PALADIN_HOLY = 34445,
    NPC_HORDE_PRIEST_DISCIPLINE = 34447,
    NPC_HORDE_SHAMAN_RESTORATION = 34444,

    // The Twin Val'kyr
    NPC_FJOLA_LIGHTBANE = 34497,
    NPC_EYDIS_DARKBANE = 34496,
    NPC_DARK_ESSENCE = 34567,
    NPC_LIGHT_ESSENCE = 34568,
    NPC_CONCENTRATED_DARKNESS = 34628,
    NPC_CONCENTRATED_LIGHT = 34630,

    // Anub'arak
    NPC_ANUBARAK = 34564,
    NPC_SCARAB = 34605,
    NPC_FROST_SPHERE = 34606,
    NPC_NERUBIAN_BURROWER = 34607,
    NPC_ANUBARAK_SPIKE = 34660,
};

enum TotcSpells : uint32
{
    // The Beasts of Northrend
    SPELL_IMPALE = 66331,
    SPELL_PARALYTIC_TOXIN = 66823,
    SPELL_BURNING_BILE = 66869,

    // Lord Jaraxxus
    SPELL_FEL_FIREBALL = 66532,
    SPELL_LEGION_FLAME = 66197,

    // The Twin Val'kyr
    SPELL_DARK_ESSENCE = 65684,
    SPELL_LIGHT_ESSENCE = 65686,
    SPELL_LIGHT_VORTEX = 66046,
    SPELL_DARK_VORTEX = 66058,
    SPELL_DARK_TWIN_PACT = 65875,
    SPELL_LIGHT_TWIN_PACT = 65876,

    // Anub'arak
    SPELL_SUBMERGE = 65981,
    SPELL_PURSUED = 67574,
    SPELL_PERMAFROST = 66193,
    SPELL_SHADOW_STRIKE = 66134,
};

constexpr uint32 TOTC_MAP_ID = 649;
constexpr float TOTC_SEARCH_DISTANCE = 100.0f;
// Guides taunt Gormok off a tank at 3-4 Impale stacks.
constexpr uint32 IMPALE_SWAP_STACKS = 3;
// Icehowl's Trample hits whoever is within this distance of his charge line; ours.
constexpr float TRAMPLE_DODGE_WIDTH = 10.0f;
// Legion Flame and Burning Bile carriers keep this far from other players; ours.
constexpr float TOTC_SPREAD_DISTANCE = 8.0f;
// Bots keep this far from an essence orb of the other color, which explodes on touch; ours.
constexpr float TWINS_ORB_AVOID_DISTANCE = 5.0f;
// Bots keep this far from an Anub'arak spike that isn't chasing them; ours.
constexpr float SPIKE_AVOID_DISTANCE = 10.0f;

bool IsTotcEncounter(Player* bot);
Unit* GetNearestNpc(PlayerbotAI* botAI, uint32 entry, float maxDistance, bool attackableOnly = false);
// An npc of that entry attacking anyone but a tank.
Unit* GetUntankedNpc(PlayerbotAI* botAI, uint32 entry);
// The caster is casting the spell or the version the core swapped in for this difficulty.
bool IsCastingDifficultySpell(Unit* caster, uint32 baseSpellId);
// The unit has the aura or the version the core swapped in for this difficulty.
bool HasDifficultyAura(Unit const* unit, uint32 baseSpellId);
uint32 GetDifficultyAuraStacks(Unit const* unit, uint32 baseSpellId);
float GetSpellAreaRadius(uint32 spellId, float fallback);
// A caster guides interrupt: Jaraxxus' Fel Fireball, a twin's Twin's Pact, a Burrower's Shadow Strike.
Unit* GetTotcInterruptTarget(PlayerbotAI* botAI);

// The Beasts of Northrend
// A group member Paralytic Toxin is slowing, the nearest first; Burning Bile next to them cures it.
Player* GetParalyzedGroupMember(Player* bot);
// Where to step out of Icehowl's charge line while he is about to Trample, or false when clear.
bool GetTrampleDodgePosition(PlayerbotAI* botAI, Position& destination);
// A Jormungar is mobile while it shows its mobile model; the stationary one is rooted and spits.
bool IsJormungarMobile(Unit const* worm);
// Guides burn Acidmaw and off-tank Dreadscale: the main tank holds Acidmaw, the first off-tank Dreadscale.
Unit* GetAssignedJormungar(PlayerbotAI* botAI);
// Melee stand behind the mobile worm (its Spew is in front) and beside the stationary one (it sweeps
// behind); where to stand, or false when already there.
bool GetJormungarMeleePosition(PlayerbotAI* botAI, Unit* worm, Position& destination);

// Lord Jaraxxus: on heroic the portals and volcanoes can be attacked and are killed first, then
// the Mistresses of Pain.
Unit* GetJaraxxusAddToKill(PlayerbotAI* botAI);

// The Faction Champions: guides kill the healers first, the lowest health first.
bool IsFactionChampionHealer(uint32 entry);
Unit* GetFactionChampionKillTarget(PlayerbotAI* botAI);
// The crowd-control spell a dps bot of this class uses on the champions, or nullptr.
char const* GetChampionCrowdControlSpell(Player* bot);
// Each crowd-control bot takes its own champion other than the skull, in group order.
Unit* GetAssignedChampionToControl(PlayerbotAI* botAI);

// The Twin Val'kyr: the raid shares one essence, Dark until a Vortex says otherwise, and attacks the
// twin of the other color, or the twin casting Twin's Pact.
bool IsTwinsLightPhase(uint32 instanceId);
void SetTwinsLightPhase(uint32 instanceId, bool light);
// The essence the raid wants: a twin casting her Vortex switches it to her color, which protects from it.
bool WantsLightEssence(PlayerbotAI* botAI);
bool HasEssence(Player* bot, bool light);
Unit* GetTwinCastingPact(PlayerbotAI* botAI);
Unit* GetTwinToAttack(PlayerbotAI* botAI);
// An orb of the other color than the bot's essence, close enough to touch.
Unit* GetOppositeOrbInRange(PlayerbotAI* botAI);

// Anub'arak
bool IsAnubarakSubmerged(Unit* anubarak);
// A dead Frost Sphere's Permafrost, which stops a Pursuing Spike.
Unit* GetNearestPermafrost(PlayerbotAI* botAI);
}

#endif
