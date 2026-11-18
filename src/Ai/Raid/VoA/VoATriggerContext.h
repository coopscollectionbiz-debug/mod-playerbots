/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERCONTEXT_H
#define PLAYERBOTS_VOATRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "VoATriggers.h"

class RaidVoATriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidVoATriggerContext()
    {
        creators["emalon mark boss trigger"] = &RaidVoATriggerContext::emalon_mark_boss_trigger;
        creators["emalon lighting nova trigger"] = &RaidVoATriggerContext::emalon_lighting_nova_trigger;
        creators["emalon overcharge trigger"] = &RaidVoATriggerContext::emalon_overcharge_trigger;
        creators["emalon fall from floor trigger"] = &RaidVoATriggerContext::emalon_fall_from_floor_trigger;
        creators["emalon nature resistance trigger"] = &RaidVoATriggerContext::emalon_nature_resistance_trigger;
        creators["koralon fire resistance trigger"] = &RaidVoATriggerContext::koralon_fire_resistance_trigger;
        creators["archavon not tanked trigger"] = &RaidVoATriggerContext::archavon_not_tanked;
        creators["archavon rock shards trigger"] = &RaidVoATriggerContext::archavon_rock_shards;
        creators["toravon frostbite swap trigger"] = &RaidVoATriggerContext::toravon_frostbite_swap;
        creators["toravon frozen orb trigger"] = &RaidVoATriggerContext::toravon_frozen_orb;
    }

private:
    static Trigger* archavon_not_tanked(PlayerbotAI* botAI) { return new ArchavonNotTankedTrigger(botAI); }
    static Trigger* archavon_rock_shards(PlayerbotAI* botAI) { return new ArchavonRockShardsTrigger(botAI); }
    static Trigger* toravon_frostbite_swap(PlayerbotAI* botAI) { return new ToravonFrostbiteSwapTrigger(botAI); }
    static Trigger* toravon_frozen_orb(PlayerbotAI* botAI) { return new ToravonFrozenOrbTrigger(botAI); }
    static Trigger* emalon_mark_boss_trigger(PlayerbotAI* botAI) { return new EmalonMarkBossTrigger(botAI); }
    static Trigger* emalon_lighting_nova_trigger(PlayerbotAI* botAI) { return new EmalonLightingNovaTrigger(botAI); }
    static Trigger* emalon_overcharge_trigger(PlayerbotAI* botAI) { return new EmalonOverchargeTrigger(botAI); }
    static Trigger* emalon_fall_from_floor_trigger(PlayerbotAI* botAI) { return new EmalonFallFromFloorTrigger(botAI); }
    static Trigger* emalon_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "emalon the storm watcher"); }
    static Trigger* koralon_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "koralon the flame watcher"); }
};

#endif
