/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPHELPERS_H
#define PLAYERBOTS_SWPHELPERS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"

class GameObject;
class Player;
class PlayerbotAI;
class Unit;

namespace SunwellPlateauHelpers
{
constexpr uint32 MAP_SUNWELL_PLATEAU = 580;

enum SunwellNPCs
{
    // Kalecgos
    NPC_KALECGOS = 24850,
    NPC_KALEC = 24891,
    NPC_SATHROVARR = 24892,

    // Brutallus
    NPC_BRUTALLUS = 24882,

    // Felmyst
    NPC_FELMYST = 25038,
    NPC_DEMONIC_VAPOR = 25265,
    NPC_DEMONIC_VAPOR_TRAIL = 25267,

    // Eredar Twins
    NPC_LADY_SACROLASH = 25165,
    NPC_GRAND_WARLOCK_ALYTHESS = 25166,

    // M'uru
    NPC_MURU = 25741,
    NPC_DARK_FIEND = 25744,
    NPC_VOID_SENTINEL = 25772,
    NPC_SHADOWSWORD_BERSERKER = 25798,
    NPC_SHADOWSWORD_FURY_MAGE = 25799,
    NPC_ENTROPIUS = 25840,
    NPC_SINGULARITY = 25855,
    NPC_DARKNESS = 25879,

    // Kil'jaeden
    NPC_KILJAEDEN = 25315,
    NPC_SHIELD_ORB = 25502,
    NPC_HAND_OF_THE_DECEIVER = 25588,
    NPC_VOLATILE_FELFIRE_FIEND = 25598,
    NPC_BLUE_DRAKE = 25653,
    NPC_SINISTER_REFLECTION = 25708,
    NPC_ARMAGEDDON_TARGET = 25735,
};

enum SunwellGameObjects
{
    GO_SPECTRAL_RIFT = 187055,
    GO_ORB_OF_THE_BLUE_FLIGHT_1 = 187869,
    GO_ORB_OF_THE_BLUE_FLIGHT_2 = 188114,
    GO_ORB_OF_THE_BLUE_FLIGHT_3 = 188115,
    GO_ORB_OF_THE_BLUE_FLIGHT_4 = 188116,
};

enum SunwellSpells
{
    // Kalecgos
    SPELL_SPECTRAL_EXHAUSTION = 44867,
    SPELL_SPECTRAL_REALM = 46021,
    SPELL_CURSE_OF_BOUNDLESS_AGONY = 45032,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR = 45034,
    SPELL_KALECGOS_BANISH = 44836,

    // Brutallus
    SPELL_METEOR_SLASH = 45150,
    SPELL_BRUTALLUS_STOMP = 45185,
    SPELL_BURN = 46394,

    // Felmyst
    SPELL_FELMYST_SPEED_BURST = 45495,

    // Kil'jaeden
    SPELL_FIRE_BLOOM = 45641,
    SPELL_DARKNESS_OF_A_THOUSAND_SOULS = 46605,
    SPELL_ARMAGEDDON_MISSILE = 45909,
    SPELL_DRAKE_BLINK = 45862,
    SPELL_DRAKE_BREATH_HASTE = 45856,
    SPELL_DRAKE_BREATH_REVITALIZE = 45860,
    SPELL_DRAKE_SHIELD_OF_THE_BLUE = 45848,
};

// Kalecgos: guides' portal group for a 25-player raid, and how far the two dragons may drift apart.
constexpr uint32 KALECGOS_PORTAL_HEALERS = 2;
constexpr uint32 KALECGOS_PORTAL_DPS = 4;
constexpr float KALECGOS_HEALTH_SYNC_PCT = 5.0f;
// Below this, Kalecgos waits for Sathrovarr's banish: guides take Sathrovarr to 1% first.
constexpr float KALECGOS_HOLD_PCT = 12.0f;
constexpr uint32 KALECGOS_CURSE_KEEP_MS = 15000;
constexpr float KALECGOS_PORTAL_SEARCH_DISTANCE = 80.0f;

// Brutallus: guides' soak groups take 3 Meteor Slashes before the other tank taunts.
constexpr uint32 BRUTALLUS_SWAP_STACKS = 3;

// Eredar Twins: Shadow Nova reaches 10y, Conflagration 8y.
constexpr float TWINS_SPREAD_DISTANCE = 10.0f;

// Kil'jaeden: Fire Bloom hurts everyone within 10y of its target.
constexpr float FIRE_BLOOM_SPREAD_DISTANCE = 12.0f;
// Shield of the Blue covers 12y; the raid stacks well inside that.
constexpr float DRAKE_SHIELD_STACK_DISTANCE = 8.0f;
// Darkness is an 8s cast; the 5s shield goes up with this much of it left, so it is up on impact.
constexpr int32 DRAKE_SHIELD_CAST_LEAD_MS = 2500;
// Guides keep the raid out of the Hands' 30y Shadow Bolt Volley.
constexpr float HAND_VOLLEY_DISTANCE = 30.0f;

constexpr float SWP_ADD_SEARCH_DISTANCE = 80.0f;
// Kalec dies if no one heals him in the realm.
constexpr float KALEC_HEAL_BELOW_PCT = 90.0f;
// Guides spread for Demonic Vapor and lead it away from the raid; the distance is ours.
constexpr float FELMYST_VAPOR_LEAD_DISTANCE = 20.0f;
// Guides tank the Hands "far apart"; the distance is ours.
constexpr float HAND_SEPARATION_DISTANCE = 20.0f;

// The add or boss the raid should kill next, or null when the skull is already right.
Unit* GetKillOrderMarkTarget(PlayerbotAI* botAI);
// A Shadowsword Berserker for the sheep, or null when moon is already right.
Unit* GetCcMarkTarget(PlayerbotAI* botAI);
Unit* GetNearestNpc(PlayerbotAI* botAI, uint32 entry, float maxDistance);
// Finds a creature whether or not this bot can see it (Kalecgos and Sathrovarr are in different realms).
Unit* FindCreatureAnyRealm(Player* bot, uint32 entry);
bool IsInSpectralRealm(Unit const* unit);
GameObject* GetSpectralRift(Player* bot);
// The Kil'jaeden dragon is taken by the raid's extra tank: the third, or the second without one.
Player* GetDragonRider(Player* bot);
GameObject* GetUsableBlueFlightOrb(Player* bot);
Unit* GetPossessedDrake(Player* bot);
// The largest damage radius among the spells this hazard creature's periodic auras trigger.
float GetHazardRadius(Unit const* hazard, float fallback);
// How close Burn spreads to others, read from the spell data.
float GetBurnSpreadRadius(Player const* bot);
bool IsHazardNpc(uint32 entry);
// Felmyst's Fog of Corruption lanes run north-south at these X positions.
int8 GetFelmystBreathLane(Unit const* felmyst);
float GetFelmystLaneX(int8 lane);
bool IsInFelmystLane(Player const* bot, int8 lane);
bool IsUsingDarkness(Unit const* kiljaeden, int32* remainingMs = nullptr);
Unit* GetNearestHazard(PlayerbotAI* botAI, float* radius = nullptr);
// Each portal takes the nearest bots not exhausted: a tank when the realm has none for long, the
// guides' healers, and DPS, counting whoever just went through.
bool IsKalecgosPortalTeamMember(PlayerbotAI* botAI);
// Soak groups are the ranged and healers in the raid subgroup of the tank Brutallus is hitting.
bool IsBrutallusSoaker(Player* bot, Unit* brutallus);
bool IsInBrutallusSoakSpot(Player* bot, Unit* brutallus);
Position GetBrutallusSoakSpot(Unit* brutallus);
bool IsDemonicVaporTarget(Unit* vapor, Player* bot);
// Sacrolash gets the main tank and first off-tank; Alythess the third tank, or the second without one.
Player* GetAlythessTank(Player* bot);
bool IsSacrolashTank(Player* bot);
// Each of the first three tanks takes one Hand of the Deceiver, in GUID order.
Unit* GetAssignedHand(PlayerbotAI* botAI);
bool IsTooCloseToOtherHand(PlayerbotAI* botAI, Unit* hand);
Unit* GetRangedAddTarget(PlayerbotAI* botAI);
// The dragon the raid stacks on for Shield of the Blue: one ridden by a raid member.
Unit* GetShieldDrake(PlayerbotAI* botAI);
}

#endif
