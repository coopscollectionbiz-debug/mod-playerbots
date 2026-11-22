/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXTRIGGERS_H
#define PLAYERBOTS_NAXXTRIGGERS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "NaxxBossHelper.h"
#include "PlayerbotAIConfig.h"
#include "Trigger.h"

class MutatingInjectionTrigger : public HasAuraTrigger
{
public:
    MutatingInjectionTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "mutating injection", 1) {}
};

class MutatingInjectionMeleeTrigger : public MutatingInjectionTrigger
{
public:
    MutatingInjectionMeleeTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class MutatingInjectionRangedTrigger : public MutatingInjectionTrigger
{
public:
    MutatingInjectionRangedTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class AuraRemovedTrigger : public Trigger
{
public:
    AuraRemovedTrigger(PlayerbotAI* botAI, std::string name) : Trigger(botAI, name, 1)
    {
        this->prev_check = false;
    }
    virtual bool IsActive() override;

protected:
    bool prev_check;
};

class MutatingInjectionRemovedTrigger : public HasNoAuraTrigger
{
public:
    MutatingInjectionRemovedTrigger(PlayerbotAI* ai) : HasNoAuraTrigger(ai, "mutating injection") {}
    virtual bool IsActive();
};

namespace NaxxGrobbulusSpray
{
    constexpr float Radius = 48.0f;          // spell 90008 radius index 11 (45 yd) plus margin
    constexpr float Arc = static_cast<float>(M_PI) / 2.0f;  // cone is 60 degrees; 90 for margin
}

class GrobbulusTankFacingTrigger : public Trigger
{
public:
    GrobbulusTankFacingTrigger(PlayerbotAI* ai) : Trigger(ai, "grobbulus tank facing") {}
    bool IsActive() override;
};

class GrobbulusPoisonCloudTrigger : public Trigger
{
public:
    GrobbulusPoisonCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "grobbulus poison cloud") {}
    bool IsActive() override;
};

class GrobbulusSprayConeTrigger : public Trigger
{
public:
    GrobbulusSprayConeTrigger(PlayerbotAI* ai) : Trigger(ai, "grobbulus spray cone") {}
    bool IsActive() override;
};

class GrobbulusCloudTrigger : public Trigger
{
public:
    GrobbulusCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "grobbulus cloud event"), last_cloud_ms(0) {}
    bool IsActive() override;

private:
    uint32 last_cloud_ms;
    static constexpr uint32 CloudRotationDelayMs = 15000;
};

class HeiganMeleeTrigger : public Trigger
{
public:
    explicit HeiganMeleeTrigger(PlayerbotAI* ai) : Trigger(ai, "heigan melee"), helper(ai) {}
    bool IsActive() override;

private:
    HeiganBossHelper helper;
};

class HeiganRangedTrigger : public Trigger
{
public:
    explicit HeiganRangedTrigger(PlayerbotAI* ai) : Trigger(ai, "heigan ranged"), helper(ai) {}
    bool IsActive() override;

private:
    HeiganBossHelper helper;
};

class RazuviousTankTrigger : public Trigger
{
public:
    RazuviousTankTrigger(PlayerbotAI* ai) : Trigger(ai, "instructor razuvious tank"), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class RazuviousNontankTrigger : public Trigger
{
public:
    RazuviousNontankTrigger(PlayerbotAI* ai) : Trigger(ai, "instructor razuvious non-tank"), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class RazuviousShoutRangeTrigger : public Trigger
{
public:
    RazuviousShoutRangeTrigger(PlayerbotAI* ai) : Trigger(ai, "razuvious shout range") {}
    bool IsActive() override;
};

class KelthuzadTrigger : public Trigger
{
public:
    KelthuzadTrigger(PlayerbotAI* ai) : Trigger(ai, "kel'thuzad trigger"), helper(ai) {}
    bool IsActive() override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanTrigger : public Trigger
{
public:
    AnubrekhanTrigger(PlayerbotAI* ai) : Trigger(ai, "anub'rekhan") {}
    bool IsActive() override;
};

 class FaerlinaTrigger : public Trigger
 {
 public:
     FaerlinaTrigger(PlayerbotAI* ai) : Trigger(ai, "faerlina") {}
     bool IsActive() override;
 };

class MaexxnaTrigger : public Trigger
{
public:
    MaexxnaTrigger(PlayerbotAI* ai) : Trigger(ai, "maexxna") {}
    bool IsActive() override;
};

//class PatchwerkTankTrigger : public Trigger
//{
//public:
//    PatchwerkTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk tank") {}
//    bool IsActive() override;
//};
//
//class PatchwerkNonTankTrigger : public Trigger
//{
//public:
//    PatchwerkNonTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk non-tank") {}
//    bool IsActive() override;
//};
//
//class PatchwerkRangedTrigger : public Trigger
//{
//public:
//    PatchwerkRangedTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk ranged") {}
//    bool IsActive() override;
//};

class ThaddiusPhasePetTrigger : public Trigger
{
public:
    ThaddiusPhasePetTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase pet"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhasePetLoseAggroTrigger : public ThaddiusPhasePetTrigger
{
public:
    ThaddiusPhasePetLoseAggroTrigger(PlayerbotAI* ai) : ThaddiusPhasePetTrigger(ai) {}
    virtual bool IsActive()
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        return ThaddiusPhasePetTrigger::IsActive() && botAI->IsTank(bot) && target && target->GetVictim() != bot;
    }
};

class ThaddiusPhaseTransitionTrigger : public Trigger
{
public:
    ThaddiusPhaseTransitionTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase transition"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhaseThaddiusTrigger : public Trigger
{
public:
    ThaddiusPhaseThaddiusTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase thaddius"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class FourHorsemenAttractorsTrigger : public Trigger
{
public:
    FourHorsemenAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen attractors"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

class FourHorsemenExceptAttractorsTrigger : public Trigger
{
public:
    FourHorsemenExceptAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen except attractors"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Naxx40 Four Horsemen (overlay patch 0022): the bot holds a slot in a live horseman's tank pair, or is a spare tank.
class Naxx40HorsemenTankTrigger : public Trigger
{
public:
    Naxx40HorsemenTankTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 horsemen tank") {}
    bool IsActive() override;
};

// Naxx40 Four Horsemen (overlay patch 0022): the bot is a latched DPS (a half) or healer (a group) in the view.
class Naxx40HorsemenRaidTrigger : public Trigger
{
public:
    Naxx40HorsemenRaidTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 horsemen raid") {}
    bool IsActive() override;
};

// Naxx40 Four Horsemen (overlay patch 0022): a latched healer can heal its horseman's active tank now
// (Naxx40Horsemen::TankHealTarget).
class Naxx40HorsemenTankHealTrigger : public Trigger
{
public:
    Naxx40HorsemenTankHealTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 horsemen tank heal") {}
    bool IsActive() override;
};

// Naxx40 Four Horsemen (overlay patch 0022): a latched DPS still attacking a horseman has passed THREAT_HOLD of his
// active tank's threat (Naxx40HorsemenRules::ThreatStop, plain ratio). The `bwl threat hold` shape.
class Naxx40HorsemenThreatHoldTrigger : public Trigger
{
public:
    Naxx40HorsemenThreatHoldTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 horsemen threat hold") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (overlay patch 0022; spec 2026-10-30 §4.1): a consumable is due (Naxx40LoathebAi::ConsumableFor), or
// the bot is channelling its bandage.
class Naxx40LoathebConsumableTrigger : public Trigger
{
public:
    Naxx40LoathebConsumableTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb consumable") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.3): the main tank is low enough for a castable defensive cooldown.
class Naxx40LoathebTankCooldownTrigger : public Trigger
{
public:
    Naxx40LoathebTankCooldownTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb tank cooldown") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.2): the bot's rotation slot is up, or the main tank needs an emergency heal.
class Naxx40LoathebTankHealTrigger : public Trigger
{
public:
    Naxx40LoathebTankHealTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb tank heal") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.7 item 4): a raid healer's saved heal is due right after a Doom.
class Naxx40LoathebGroupHealTrigger : public Trigger
{
public:
    Naxx40LoathebGroupHealTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb group heal") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.8 item 3): a DPS bot has a spore to attack (within SPORE_ENGAGE, clear of
// Loatheb's victim) that it is not on yet, or is still on a spore that is dead or out of that rule.
class Naxx40LoathebSporeTrigger : public Trigger
{
public:
    Naxx40LoathebSporeTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb spore") {}
    bool IsActive() override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.2 / §4.8 item 4): a locked-out healer has a damage spell it can cast at Loatheb now,
// or a healer's mana refill is pending.
class Naxx40LoathebHealerDpsTrigger : public Trigger
{
public:
    Naxx40LoathebHealerDpsTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 loatheb healer dps") {}
    bool IsActive() override;
};

// Naxx40 Anub'Rekhan (overlay patch 0022; spec 2026-10-30-playerbot-naxx40-anubrekhan §3): the bot has a spot this tick
// (Naxx40Anub::SpotFor) and is off it by more than its tolerance.
class Naxx40AnubSpreadTrigger : public Trigger
{
public:
    Naxx40AnubSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 anub spread") {}
    bool IsActive() override;
};

// Naxx40 Patchwerk (overlay patch 0022; spec 2026-11-01 §3.3): a soaker healer has a strike-pair member to heal
// (Naxx40PatchwerkAi::HealTarget).
class Naxx40PatchwerkSoakerHealTrigger : public Trigger
{
public:
    Naxx40PatchwerkSoakerHealTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 patchwerk soaker heal") {}
    bool IsActive() override;
};

// Naxx40 Patchwerk (spec 2026-11-01 §3.3): an MT healer and the MT is alive.
class Naxx40PatchwerkMtHealTrigger : public Trigger
{
public:
    Naxx40PatchwerkMtHealTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 patchwerk mt heal") {}
    bool IsActive() override;
};

// Naxx40 Patchwerk (spec 2026-11-01 §3.2): a capped bot auto-attacking him passed its stop ratio
// (Naxx40PatchwerkAi::ThreatHoldDue).
class Naxx40PatchwerkThreatHoldTrigger : public Trigger
{
public:
    Naxx40PatchwerkThreatHoldTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 patchwerk threat hold") {}
    bool IsActive() override;
};

// Naxx40 Patchwerk (pull 1): a latched soaker / spare bot is owed a threat boost under the `raid` cheat
// (Naxx40PatchwerkAi::ThreatBoostDue).
class Naxx40PatchwerkThreatBoostTrigger : public Trigger
{
public:
    Naxx40PatchwerkThreatBoostTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 patchwerk threat boost") {}
    bool IsActive() override;
};

// Naxx40 Patchwerk (spec 2026-11-01 §3.3): a role healer bot's one mana refill is due
// (Naxx40PatchwerkAi::ManaRefillDue).
class Naxx40PatchwerkManaRefillTrigger : public Trigger
{
public:
    Naxx40PatchwerkManaRefillTrigger(PlayerbotAI* ai) : Trigger(ai, "naxx40 patchwerk mana refill") {}
    bool IsActive() override;
};

class SapphironGroundTrigger : public Trigger
{
public:
    SapphironGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron ground"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class SapphironFlightTrigger : public Trigger
{
public:
    SapphironFlightTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron flight"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class GluthTrigger : public Trigger
{
public:
    GluthTrigger(PlayerbotAI* ai) : Trigger(ai, "gluth trigger"), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthMainTankMortalWoundTrigger : public Trigger
{
public:
    GluthMainTankMortalWoundTrigger(PlayerbotAI* ai) : Trigger(ai, "gluth main tank mortal wound trigger"), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class LoathebTrigger : public Trigger
{
public:
    LoathebTrigger(PlayerbotAI* ai) : Trigger(ai, "loatheb"), helper(ai) {}
    bool IsActive() override;

private:
    LoathebBossHelper helper;
};

// Skull for Faerlina's adds and Widow's Embrace

class NaxxKillOrderMarkTrigger : public Trigger
{
public:
    NaxxKillOrderMarkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx kill order mark") {}
    bool IsActive() override;
};

// Grand Widow Faerlina

class NaxxFaerlinaMindControlTrigger : public Trigger
{
public:
    NaxxFaerlinaMindControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx faerlina mind control") {}
    bool IsActive() override;
};

class NaxxFaerlinaEmbraceTrigger : public Trigger
{
public:
    NaxxFaerlinaEmbraceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx faerlina embrace") {}
    bool IsActive() override;
};

class NaxxFaerlinaBringWorshipperTrigger : public Trigger
{
public:
    NaxxFaerlinaBringWorshipperTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx faerlina bring worshipper") {}
    bool IsActive() override;
};

// Maexxna

class NaxxMaexxnaWebWrapTrigger : public Trigger
{
public:
    NaxxMaexxnaWebWrapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx maexxna web wrap") {}
    bool IsActive() override;
};

// Noth the Plaguebringer

class NaxxNothNotTankedTrigger : public Trigger
{
public:
    NaxxNothNotTankedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx noth not tanked") {}
    bool IsActive() override;
};

class NaxxPlaguedChampionRangeTrigger : public Trigger
{
public:
    NaxxPlaguedChampionRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx plagued champion range") {}
    bool IsActive() override;
};

// Gothik the Harvester

class NaxxGothikSideTrigger : public Trigger
{
public:
    NaxxGothikSideTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx gothik side") {}
    bool IsActive() override;
};

class NaxxGothikNotTankedTrigger : public Trigger
{
public:
    NaxxGothikNotTankedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx gothik not tanked") {}
    bool IsActive() override;
};

// Patchwerk

class NaxxPatchwerkSoakerTrigger : public Trigger
{
public:
    NaxxPatchwerkSoakerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx patchwerk soaker") {}
    bool IsActive() override;
};

class NaxxPatchwerkMeleeTrigger : public Trigger
{
public:
    NaxxPatchwerkMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx patchwerk melee") {}
    bool IsActive() override;
};

// The Four Horsemen

class NaxxThaneMeteorStackTrigger : public Trigger
{
public:
    NaxxThaneMeteorStackTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx thane meteor stack"), helper(botAI) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Kel'Thuzad

class NaxxDetonateManaTrigger : public Trigger
{
public:
    NaxxDetonateManaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx detonate mana") {}
    bool IsActive() override;
};

class NaxxChainsMarkTrigger : public Trigger
{
public:
    NaxxChainsMarkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "naxx chains mark") {}
    bool IsActive() override;
};

#endif
