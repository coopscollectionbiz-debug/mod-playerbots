/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXHELPERS_H
#define PLAYERBOTS_NAXXHELPERS_H

#include "AreaDefines.h"
#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// WotLK (10/25-player) Naxxramas only; the core script is the reference for every number here.
namespace NaxxHelpers
{

enum NaxxNPCs
{
    NPC_GRAND_WIDOW_FAERLINA = 15953,
    NPC_NAXXRAMAS_FOLLOWER = 16505,
    NPC_NAXXRAMAS_WORSHIPPER = 16506,
    NPC_WEB_WRAP = 16486,
    NPC_NOTH = 15954,
    NPC_PLAGUED_CHAMPION = 16983,
    NPC_GOTHIK = 16060,
    NPC_PATCHWERK = 16028,
};

enum NaxxSpells
{
    // Grand Widow Faerlina; the core swaps in the 25-player versions by difficulty.
    SPELL_FAERLINA_FRENZY = 28798,
    SPELL_WIDOWS_EMBRACE = 28732,

    // Kel'Thuzad
    SPELL_DETONATE_MANA = 27819,
    SPELL_MANA_DETONATION_DAMAGE = 27820,
    SPELL_CHAINS_OF_KELTHUZAD = 28410,
};

constexpr uint32 GO_GOTHIK_INNER_GATE = 181170;

// Noth's Plagued Champions cast Shadow Shock on everyone within 25y.
constexpr float PLAGUED_CHAMPION_SHOCK_DISTANCE = 26.0f;
// Thane Korth'azz's Meteor splits between everyone within 8y of its target.
constexpr float THANE_METEOR_STACK_DISTANCE = 6.0f;
constexpr float NAXX_ADD_SEARCH_DISTANCE = 60.0f;

bool Is25Player(Player const* bot);
Unit* GetNearestNpc(PlayerbotAI* botAI, uint32 entry, float maxDistance);
bool IsFaerlinaFrenzied(Unit const* faerlina);
// How close a Worshipper must be to Faerlina for its Widow's Embrace to reach her.
float GetWidowsEmbraceRadius(Unit const* caster);
// The raid's one Mind Control priest for Faerlina in 25-player: the first living priest by GUID.
Player* GetFaerlinaPriest(Player* bot);
Unit* GetCharmedWorshipper(Player* bot);
// Skull: Faerlina's Followers, a Worshipper beside a frenzied Faerlina in 10-player, then Faerlina.
Unit* GetKillOrderMarkTarget(PlayerbotAI* botAI);

// Gothik: guides split the raid half and half; the main tank takes the living side, the first
// off-tank the dead side, and healers and DPS alternate by GUID within their role.
bool IsGothikDeadSideMember(Player* bot);
bool IsOnGothikDeadSide(Unit const* unit);
bool IsInGothikRoom(Unit const* unit);
Position GetGothikSideSpot(bool deadSide);
bool IsGothikGateOpen(Player* bot);

// Patchwerk's Hateful Strike hits the healthier of his top 1 (10-player) or 2 (25-player) threat
// targets in melee besides his tank; those are the off-tanks.
bool IsHatefulStrikeSoaker(Player* bot);
float GetManaDetonationRadius();
// A raider under Chains of Kel'Thuzad for the moon icon, or null when moon is already on one.
Unit* GetChainsCcTarget(PlayerbotAI* botAI);
}

#endif
