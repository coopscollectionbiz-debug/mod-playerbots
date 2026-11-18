/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERS_H
#define PLAYERBOTS_VOATRIGGERS_H

#include "GenericTriggers.h"
#include "Trigger.h"

enum VoAIDs
{
    // Emalon the Storm Watcher
    AURA_OVERCHARGE = 64217,
    BOSS_EMALON = 33993,
    NPC_TEMPEST_MINION = 33998,
    SPELL_LIGHTNING_NOVA_10_MAN = 64216,
    SPELL_LIGHTNING_NOVA_25_MAN = 65279,

    // Archavon the Stone Watcher
    SPELL_ARCHAVON_ROCK_SHARDS = 58678,

    // Toravon the Ice Watcher
    NPC_TORAVON_FROZEN_ORB = 38456,
    SPELL_TORAVON_FROSTBITE = 72004,
};

//
// Emalon the Storm Watcher
//
class EmalonMarkBossTrigger : public Trigger
{
public:
    EmalonMarkBossTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon mark boss trigger") {}
    bool IsActive() override;
};

class EmalonLightingNovaTrigger : public Trigger
{
public:
    EmalonLightingNovaTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon lighting nova trigger") {}
    bool IsActive() override;
};

class EmalonOverchargeTrigger : public Trigger
{
public:
    EmalonOverchargeTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon overcharge trigger") {}
    bool IsActive() override;
};

class EmalonFallFromFloorTrigger : public Trigger
{
public:
    EmalonFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon fall from floor trigger") {}
    bool IsActive() override;
};

//
// Archavon the Stone Watcher
//
constexpr float ARCHAVON_ROCK_SHARDS_SPREAD = 10.0f;

class ArchavonNotTankedTrigger : public Trigger
{
public:
    ArchavonNotTankedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "archavon not tanked trigger") {}
    bool IsActive() override;
};

class ArchavonRockShardsTrigger : public Trigger
{
public:
    ArchavonRockShardsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "archavon rock shards trigger") {}
    bool IsActive() override;
};

//
// Toravon the Ice Watcher
//
constexpr uint32 TORAVON_FROSTBITE_SWAP_STACKS = 4;

class ToravonFrostbiteSwapTrigger : public Trigger
{
public:
    ToravonFrostbiteSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toravon frostbite swap trigger") {}
    bool IsActive() override;
};

class ToravonFrozenOrbTrigger : public Trigger
{
public:
    ToravonFrozenOrbTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toravon frozen orb trigger") {}
    bool IsActive() override;
};

Unit* GetNearestToravonOrb(PlayerbotAI* botAI, float maxDistance);

#endif
