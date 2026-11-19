/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONS_H
#define PLAYERBOTS_NAXXACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "Naxx40Anub.h"
#include "Naxx40Horsemen.h"
#include "Naxx40Loatheb.h"
#include "Naxx40Patchwerk.h"
#include "NaxxBossHelper.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

class GrobbulusGoBehindAction : public MovementAction
{
public:
    GrobbulusGoBehindAction(PlayerbotAI* ai, float distance = 24.0f, float delta_angle = M_PI / 8)
        : MovementAction(ai, "grobbulus go behind")
    {
        this->distance = distance;
        this->delta_angle = delta_angle;
    }
    virtual bool Execute(Event event);

protected:
    float distance, delta_angle;
};

class GrobbulusRotateAction : public RotateAroundTheCenterPointAction
{
public:
    GrobbulusRotateAction(PlayerbotAI* botAI)
        : RotateAroundTheCenterPointAction(botAI, "rotate grobbulus", 3281.23f, -3310.38f, 35.0f, 8, true, M_PI) {}
    virtual bool isUseful() override
    {
        return RotateAroundTheCenterPointAction::isUseful() && botAI->IsMainTank(bot) &&
               AI_VALUE2(bool, "has aggro", "boss target");
    }
    uint32 GetCurrWaypoint() override;
};

class GrobbulusMoveCenterAction : public MoveInsideAction
{
public:
    GrobbulusMoveCenterAction(PlayerbotAI* ai) : MoveInsideAction(ai, 3281.23f, -3310.38f, 5.0f) {}
};

class GrobbulusMoveAwayAction : public MovementAction
{
public:
    GrobbulusMoveAwayAction(PlayerbotAI* ai, float distance = 18.0f)
        : MovementAction(ai, "grobbulus move away"), distance(distance)
    {
    }
    bool Execute(Event event) override;

private:
    float distance;
};

// One action for both roles: melee (and tanks) dance every phase, ranged/healers wait on the platform during the
// slow dance and only dance the fast one.
class HeiganDanceAction : public MovementAction
{
public:
    HeiganDanceAction(PlayerbotAI* ai, bool ranged)
        : MovementAction(ai, ranged ? "heigan dance ranged" : "heigan dance melee"), helper(ai), ranged(ranged)
    {
    }
    bool Execute(Event event) override;

private:
    // Move to (near) the given dance waypoint. Returns true while a move had to be issued.
    bool MoveToWaypoint(uint32 index, float distance);
    bool MoveToPlatform(float distance);

    HeiganBossHelper helper;
    bool ranged;
    int32 lastWaypoint = -1;
};

class ThaddiusAttackNearestPetAction : public AttackAction
{
public:
    ThaddiusAttackNearestPetAction(PlayerbotAI* ai) : AttackAction(ai, "thaddius attack nearest pet"), helper(ai) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();

private:
    ThaddiusBossHelper helper;
};

// class ThaddiusMeleeToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusMeleeToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius melee to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

// class ThaddiusRangedToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusRangedToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius ranged to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

class ThaddiusMoveToPlatformAction : public MovementAction
{
public:
    ThaddiusMoveToPlatformAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move to platform") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class ThaddiusMovePolarityAction : public MovementAction
{
public:
    ThaddiusMovePolarityAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move polarity") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class RazuviousUseObedienceCrystalAction : public MovementAction
{
public:
    RazuviousUseObedienceCrystalAction(PlayerbotAI* ai)
        : MovementAction(ai, "razuvious use obedience crystal"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class RazuviousTargetAction : public AttackAction
{
public:
    RazuviousTargetAction(PlayerbotAI* ai) : AttackAction(ai, "razuvious target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class FourHorsemenAttractAlternativelyAction : public AttackAction
{
public:
    FourHorsemenAttractAlternativelyAction(PlayerbotAI* ai) : AttackAction(ai, "four horsemen attract alternatively"), helper(ai)
    {
    }
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

class FourHorsemenAttackInOrderAction : public AttackAction
{
public:
    FourHorsemenAttackInOrderAction(PlayerbotAI* ai) : AttackAction(ai, "four horsemen attack in order"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

// Naxx40 Four Horsemen tank pairs (overlay patch 0022; spec 2026-10-27 §4.1): Hold / Ready / Relieve / Leave / Rest
// from Naxx40HorsemenRules::TankDutyFor, plus the whispers to a human resting partner. NaxxActions_Horsemen40.cpp.
class Naxx40HorsemenTankAction : public AttackAction
{
public:
    Naxx40HorsemenTankAction(PlayerbotAI* ai) : AttackAction(ai, "naxx40 horsemen tank") {}
    bool Execute(Event event) override;

private:
    bool Hold(Event event, Naxx40HorsemenRules::Horse h, Creature* horse, Player* partner);
    bool Engage(Event event, Creature* horse);
    bool MoveToStation(float x, float y, float z, Creature* horse);
    void DropThreat(Creature* horse, bool evenIfVictim = false);
    void WhisperPartner(Naxx40HorsemenRules::Horse h, Creature* horse, Player* partner, uint8_t myStacks);

    Naxx40HorsemenRules::TankDuty lastDuty = Naxx40HorsemenRules::TankDuty::Rest;
    bool lastDutyValid = false;
    bool lastSpare = false;
    bool inVoid = false;  // Blaumeux tank standing in a Void Zone (pull 5 diagnostic edge)
};

// Naxx40 Four Horsemen DPS halves and healer groups (overlay patch 0022; spec 2026-10-27 §4.2-§4.3): which horseman
// to be on this turn (Naxx40HorsemenRules::Choose, mark stacks), where to stand, and DPS attack only that horseman.
// NaxxActions_Horsemen40.cpp.
class Naxx40HorsemenRaidAction : public AttackAction
{
public:
    Naxx40HorsemenRaidAction(PlayerbotAI* ai) : AttackAction(ai, "naxx40 horsemen raid") {}
    bool Execute(Event event) override;

private:
    // The last melee leg issued (spec review fix: re-path only when the dragged boss moved the spot > 2 y).
    Naxx40HorsemenRules::Pt meleeLeg{0.0f, 0.0f};
    Naxx40HorsemenRules::Horse meleeLegHorse = Naxx40HorsemenRules::Horse::None;
    bool meleeLegValid = false;
    bool switchHeld = false;  // log edge: a HoldSwitch wait (spec §4.2a)
    bool victimRun = false;   // log edge: running a horseman back to his tank (spec §4.2b)
    bool healerHeld = false;  // log edge: the last healer on a corner staying (spec §4.3a)
    // The last escort leg issued (spec §4.3b): re-path only when the escorted tank moved > ESCORT_REPATH from it.
    Naxx40HorsemenRules::Pt escortLeg{0.0f, 0.0f};
    bool escortLegValid = false;
    bool escorting = false;   // log edge: escorting an unparked horseman's tank (spec §4.3b)
    bool healStop = false;    // log edge: stopped on the way to heal (spec §4.3c)
};

// Naxx40 Four Horsemen (spec 2026-10-27 §4.3): the class fast heal, by name, on the healer's horseman's active tank
// (Naxx40Horsemen::TankHealTarget, re-resolved at execution). False = stock healing runs.
class Naxx40HorsemenTankHealAction : public Action
{
public:
    Naxx40HorsemenTankHealAction(PlayerbotAI* ai) : Action(ai, "naxx40 horsemen tank heal") {}
    bool Execute(Event event) override;
};

// Naxx40 Four Horsemen (spec 2026-10-27 §4.2): the DPS hard stop. Stops the running auto-attack / auto-shot and keeps
// the target; the multiplier's pre-cast veto keeps it from restarting until the ratio falls. The `bwl threat hold`
// shape. NaxxActions_Horsemen40.cpp.
class Naxx40HorsemenThreatHoldAction : public Action
{
public:
    Naxx40HorsemenThreatHoldAction(PlayerbotAI* ai) : Action(ai, "naxx40 horsemen threat hold") {}
    bool Execute(Event event) override;
};

// class SapphironGroundMainTankPositionAction : public MovementAction
// {
// public:
//     SapphironGroundMainTankPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron ground main tank
//     position") {} virtual bool Execute(Event event);
// };

class SapphironGroundPositionAction : public MovementAction
{
public:
    SapphironGroundPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron ground position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    SapphironBossHelper helper;
};

class SapphironFlightPositionAction : public MovementAction
{
public:
    SapphironFlightPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron flight position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    SapphironBossHelper helper;
    bool MoveToNearestIcebolt();
};

// class SapphironAvoidChillAction : public MovementAction
// {
// public:
//     SapphironAvoidChillAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron avoid chill") {}
//     virtual bool Execute(Event event);
// };

class KelthuzadChooseTargetAction : public AttackAction
{
public:
    KelthuzadChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "kel'thuzad choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

class KelthuzadPositionAction : public MovementAction
{
public:
    KelthuzadPositionAction(PlayerbotAI* ai) : MovementAction(ai, "kel'thuzad position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanChooseTargetAction : public AttackAction
{
public:
    AnubrekhanChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "anub'rekhan choose target") {}
    bool Execute(Event event) override;
};

class AnubrekhanPositionAction : public RotateAroundTheCenterPointAction
{
public:
    AnubrekhanPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "anub'rekhan position", 3272.49f, -3476.27f, 45.0f, 16) {}
    bool Execute(Event event) override;
};

// Naxx40 Anub'Rekhan (overlay patch 0022; spec 2026-10-30-playerbot-naxx40-anubrekhan §3): a non-tank bot walks to its
// latched spot (Naxx40Anub::SpotFor) when it is more than the slot's tolerance off it. NaxxActions_Anub40.cpp.
class Naxx40AnubSpreadAction : public MovementAction
{
public:
    Naxx40AnubSpreadAction(PlayerbotAI* ai) : MovementAction(ai, "naxx40 anub spread") {}
    bool Execute(Event event) override;
};

class GluthChooseTargetAction : public AttackAction
{
public:
    GluthChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "gluth choose target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthPositionAction : public RotateAroundTheCenterPointAction
{
public:
    GluthPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "gluth position", 3293.61f, -3149.01f, 12.0f, 12), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthSlowdownAction : public Action
{
public:
    GluthSlowdownAction(PlayerbotAI* ai) : Action(ai, "gluth slowdown"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class LoathebPositionAction : public MovementAction
{
public:
    LoathebPositionAction(PlayerbotAI* ai) : MovementAction(ai, "loatheb position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class LoathebChooseTargetAction : public AttackAction
{
public:
    LoathebChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "loatheb choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

// Naxx40 Loatheb (overlay patch 0022; spec 2026-10-30 §4): the per-bot decisions the six triggers, their actions and
// Naxx40LoathebMultiplier share, so a trigger and its action never disagree. Each takes a view GetView returned true
// for (view.loatheb non-null). NaxxActions_Loatheb40.cpp.
namespace Naxx40LoathebAi
{
    // The bot is channelling the bandage (Heavy Runecloth Bandage's use spell): the consumable action holds it still.
    bool IsBandaging(Player* bot);

    // Spec §4.1 / §4.6 / §4.7 / §4.8 item 1: ChooseConsumable over the bot's live state (tanks and rotation healers
    // never bandage). Items the bot lacks
    // count as ready when the `raid` cheat can supply them (never for a self bot); an item the bot cannot use (level,
    // First Aid skill) never does. With healthstoneClaimed false, a bot in the after-Doom window below
    // HEALTHSTONE_BELOW_PCT that gets no healthstone logs why (once per bot per Doom, LOG_DEBUG).
    Naxx40LoathebRules::Consumable ConsumableFor(Player* bot, PlayerbotAI* botAI,
                                                 Naxx40Loatheb::LoathebView const& view, bool healthstoneClaimed);

    // Spec §4.3: the main tank's defensive cooldown by health; spell = the castable class spell to use (null = None).
    Naxx40LoathebRules::TankCd TankCdFor(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view,
                                         char const*& spell);

    // Spec §4.2: this latched bot healer should land its big heal on view.mainTank now. rotation = it is the rotation
    // slot that is up (RotationMayCast); otherwise it is the emergency (main tank under EMERGENCY_TANK_PCT, its heal
    // free, the tank in reach and sight, the heal castable: while moving, known with the mana for it, so the action
    // can stop the bot and cast on the next tick).
    bool TankHealWanted(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view, bool& rotation);

    // Spec §4.7 item 4: a raid healer (HealerJob::Group) with its heal free, right after a Doom: ChooseRaidHeal over
    // the raid in reach (HEAL_REACH 2-D + line of sight, injured = below BANDAGE_BELOW_PCT). Spec §4.9 item 1 first:
    // while no other raid healer has claimed this Doom's main-tank top-up (topUpTaken forces "claimed") and the main
    // tank is below MT_TOPUP_BELOW_PCT in reach and sight, topUp = true and the heal (Chain Heal for a shaman, else the
    // big heal) goes on him. Otherwise Prayer = Prayer of Healing on the priest's own party (target = its most injured
    // member) unless another priest claimed that party for this Doom (partyTaken forces "claimed"); Chain / Big = Chain
    // Heal / the class big heal on the most injured raid member other than the main tank (the big heal skips a bot
    // channelling a bandage). Prayer counts party members within PRAYER_RADIUS of the caster. The spell must be
    // castable (while moving: known with the mana for it, so the action can stop the bot and cast on the next tick);
    // spell / target are null on None.
    Naxx40LoathebRules::RaidHeal RaidHealFor(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view,
                                             bool partyTaken, bool topUpTaken, Player*& target, char const*& spell,
                                             bool& topUp);

    // Spec §4.8 item 3: a DPS bot, the only role that attacks spores: alive, not a tank, not a healer (PlayerbotAI::
    // IsHeal or a latched job).
    bool KillsSpores(Player* bot, Naxx40Loatheb::LoathebView const& view);

    // The spore (SPORE_ENTRY, alive or dead) the bot is on: its victim, else its "current target"; null when neither is
    // a spore.
    Unit* CurrentSpore(Player* bot, PlayerbotAI* botAI);

    // Spec §4.8 item 3: the spore a DPS bot should attack (Naxx40Loatheb::EngageSpore with CurrentSpore as the engaged
    // spore, when KillsSpores holds), else null. When it is null and CurrentSpore is not, that spore is dead or out of
    // the rule and the spore action takes the bot back to Loatheb.
    Creature* SporeToKill(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view);

    // A live spore (SPORE_ENTRY) within dist of the bot, 2D surface to surface as upstream's 1 y spore chooser
    // measures.
    bool SporeWithin(Player* bot, float dist);

    // Spec §4.2 / §4.8 item 4: the damage spell a latched bot healer with its heal locked out casts at Loatheb now, else
    // null. HealerMayDps holds (raid healers time against view.msToDoomResolve and stop once Doom is on its 15 s cycle,
    // spec §4.7 item 5), Loatheb is in range and sight, and the spell is castable now (PlayerbotAI::CanCastSpell: off
    // cooldown; while moving instant only) and ends STOP_BEFORE_MS before the heal is due. The trigger and the action
    // share it, so a spell on cooldown never fires the trigger.
    char const* HealerDpsSpell(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view);

    // Spec §4.2: one mana refill per healer per attempt below MANA_REFILL_BELOW_PCT under the `raid` cheat (never a
    // self bot). NeedsManaRefill is the cheap pre-claim test; RefillMana claims and fills.
    bool NeedsManaRefill(Player* bot, PlayerbotAI* botAI);
    void RefillMana(Player* bot, PlayerbotAI* botAI);
}

// Naxx40 Loatheb (spec 2026-10-30 §4.1): Shadow Protection Potion while Doomed, healthstone / bandage right after a
// Doom resolves, stepping out of Poison Shock first. Items are supplied under the `raid` cheat and used through the
// core's item-use handler (WorldSession::HandleUseItemOpcode, the stock UseItemAction path), so real cooldowns apply.
class Naxx40LoathebConsumableAction : public MovementAction
{
public:
    Naxx40LoathebConsumableAction(PlayerbotAI* ai) : MovementAction(ai, "naxx40 loatheb consumable") {}
    bool Execute(Event event) override;

private:
    bool StepOut(Creature* loatheb);
    uint32 lastUseMs = 0;  // last use attempt: a refused use is not retried every tick
};

// Naxx40 Loatheb (spec 2026-10-30 §4.3): the main tank's defensive cooldown (Shield Wall / Last Stand and the other
// tank classes' equivalents) as its health drops.
class Naxx40LoathebTankCooldownAction : public Action
{
public:
    Naxx40LoathebTankCooldownAction(PlayerbotAI* ai) : Action(ai, "naxx40 loatheb tank cooldown") {}
    bool Execute(Event event) override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.2): the class big heal (Naxx40Loatheb::BigHealSpellName) by name on the main tank,
// standing still in reach and sight; a rotation healer out of reach or sight walks in first.
class Naxx40LoathebTankHealAction : public MovementAction
{
public:
    Naxx40LoathebTankHealAction(PlayerbotAI* ai) : MovementAction(ai, "naxx40 loatheb tank heal") {}
    bool Execute(Event event) override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.7 item 4, §4.9 item 1): the raid healer's saved heal right after a Doom
// (Naxx40LoathebAi::RaidHealFor: the window's first one tops up a main tank below 60 %, the rest are Prayer of Healing
// on its party, else Chain Heal / the big heal on the most injured raid member).
class Naxx40LoathebGroupHealAction : public Action
{
public:
    Naxx40LoathebGroupHealAction(PlayerbotAI* ai) : Action(ai, "naxx40 loatheb group heal") {}
    bool Execute(Event event) override;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.9 item 3): a DPS bot attacks a spore that reached the ranged stack (or a stray
// SPORE_STRAY_MS old within SPORE_ENGAGE of it; clear of Loatheb's victim), and returns to Loatheb once that spore is
// dead or out of the rule.
class Naxx40LoathebSporeAction : public AttackAction
{
public:
    Naxx40LoathebSporeAction(PlayerbotAI* ai) : AttackAction(ai, "naxx40 loatheb spore") {}
    bool Execute(Event event) override;

private:
    ObjectGuid lastSporeGuid;  // the last spore Attack() was issued on, with its time: a refused one is not spammed
    uint32 lastSporeMs = 0;
};

// Naxx40 Loatheb (spec 2026-10-30 §4.2): a healer whose heal is locked out casts damage spells off IP's lockout lists
// at Loatheb while its mana allows.
class Naxx40LoathebHealerDpsAction : public Action
{
public:
    Naxx40LoathebHealerDpsAction(PlayerbotAI* ai) : Action(ai, "naxx40 loatheb healer dps") {}
    bool Execute(Event event) override;
};

// Naxx40 Patchwerk (overlay patch 0022; spec 2026-11-01 §3): the per-bot decisions the four triggers and their actions
// share, so a trigger and its action never disagree. Each takes a view Naxx40Patchwerk::GetView returned true for.
// NaxxActions_Patchwerk40.cpp.
namespace Naxx40PatchwerkAi
{
    // The heal target of this bot's assigned job (Soaker: Naxx40Patchwerk::SoakerHealTarget; MainTank: the living MT),
    // or null when the bot is dead, not assigned that job, or has no class big heal (Naxx40Loatheb::BigHealSpellName).
    Player* HealTarget(Player* bot, Naxx40Patchwerk::PatchwerkView const& view, Naxx40Patchwerk::HealerJob job);

    // The hard stop: a capped bot auto-attacking / shooting Patchwerk is at its cap's stop ratio
    // (Naxx40PatchwerkRules::ThreatStopped), or a non-tank is doing so during the opener hold. Never under "neglect
    // threat".
    bool ThreatHoldDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view);

    // A role healer bot (PlayerbotAI::IsHeal, assigned or stock) below MANA_REFILL_BELOW_PCT mana under the `raid` cheat
    // whose one refill this attempt is unclaimed.
    bool ManaRefillDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view);

    // The threat a living latched soaker / spare bot (never a self bot) is owed under the `raid` cheat
    // (Naxx40Patchwerk::ThreatBoostOf); 0 when none is due.
    float ThreatBoostDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view);
}

// Naxx40 Patchwerk (spec 2026-11-01 §3.3): the class big heal by name on the job's target. Soaker healers pre-cast on a
// strike-pair member at any health (Naxx40Patchwerk::KeepPrecastHeal); MT healers only on a damaged MT (PlayerbotAI::
// UpdateAI interrupts a heal on a full target). Out of HEAL_RANGE or line of sight it steps in toward the target first.
class Naxx40PatchwerkHealAction : public MovementAction
{
public:
    Naxx40PatchwerkHealAction(PlayerbotAI* ai, std::string const name, Naxx40Patchwerk::HealerJob job)
        : MovementAction(ai, name), job(job)
    {
    }
    bool Execute(Event event) override;

private:
    Naxx40Patchwerk::HealerJob job;
};

// Soaker healers: the strike-pair member with fewer claimed heals in flight, then lower health.
class Naxx40PatchwerkSoakerHealAction : public Naxx40PatchwerkHealAction
{
public:
    Naxx40PatchwerkSoakerHealAction(PlayerbotAI* ai)
        : Naxx40PatchwerkHealAction(ai, "naxx40 patchwerk soaker heal", Naxx40Patchwerk::HealerJob::Soaker)
    {
    }
};

// MT healers: the main tank.
class Naxx40PatchwerkMtHealAction : public Naxx40PatchwerkHealAction
{
public:
    Naxx40PatchwerkMtHealAction(PlayerbotAI* ai)
        : Naxx40PatchwerkHealAction(ai, "naxx40 patchwerk mt heal", Naxx40Patchwerk::HealerJob::MainTank)
    {
    }
};

// Naxx40 Patchwerk: the capped bot's hard stop. Stops the running auto-attack / auto-shot and keeps the target; the
// multiplier's pre-cast veto keeps it from restarting until the ratio falls. The `bwl threat hold` shape.
class Naxx40PatchwerkThreatHoldAction : public Action
{
public:
    Naxx40PatchwerkThreatHoldAction(PlayerbotAI* ai) : Action(ai, "naxx40 patchwerk threat hold") {}
    bool Execute(Event event) override;
};

// Naxx40 Patchwerk (pull 1): the `raid` cheat's tank threat floor. Adds the owed threat raw to the boss's threat list.
class Naxx40PatchwerkThreatBoostAction : public Action
{
public:
    Naxx40PatchwerkThreatBoostAction(PlayerbotAI* ai) : Action(ai, "naxx40 patchwerk threat boost") {}
    bool Execute(Event event) override;
};

// Naxx40 Patchwerk: one full mana refill per role healer bot per attempt under the `raid` cheat (Loatheb pattern).
class Naxx40PatchwerkManaRefillAction : public Action
{
public:
    Naxx40PatchwerkManaRefillAction(PlayerbotAI* ai) : Action(ai, "naxx40 patchwerk mana refill") {}
    bool Execute(Event event) override;
};

//class PatchwerkRangedPositionAction : public MovementAction
//{
//public:
//    PatchwerkRangedPositionAction(PlayerbotAI* ai) : MovementAction(ai, "patchwerk ranged position") {}
//    bool Execute(Event event) override;
//};

// Shared

class NaxxKillOrderMarkAction : public Action
{
public:
    NaxxKillOrderMarkAction(PlayerbotAI* botAI) : Action(botAI, "naxx kill order mark") {}
    bool Execute(Event event) override;
};

// Attacks the named boss and taunts it off whoever has it.
class NaxxTauntBossAction : public AttackAction
{
public:
    NaxxTauntBossAction(PlayerbotAI* botAI, std::string const& name, std::string bossName)
        : AttackAction(botAI, name), bossName(std::move(bossName)) {}
    bool Execute(Event event) override;

private:
    std::string const bossName;
};

// Grand Widow Faerlina

class NaxxFaerlinaMindControlAction : public MovementAction
{
public:
    NaxxFaerlinaMindControlAction(PlayerbotAI* botAI) : MovementAction(botAI, "naxx faerlina mind control") {}
    bool Execute(Event event) override;
};

class NaxxFaerlinaEmbraceAction : public Action
{
public:
    NaxxFaerlinaEmbraceAction(PlayerbotAI* botAI) : Action(botAI, "naxx faerlina embrace") {}
    bool Execute(Event event) override;
};

class NaxxFaerlinaBringWorshipperAction : public MovementAction
{
public:
    NaxxFaerlinaBringWorshipperAction(PlayerbotAI* botAI) : MovementAction(botAI, "naxx faerlina bring worshipper") {}
    bool Execute(Event event) override;
};

// Maexxna

class NaxxAttackWebWrapAction : public AttackAction
{
public:
    NaxxAttackWebWrapAction(PlayerbotAI* botAI) : AttackAction(botAI, "naxx attack web wrap") {}
    bool Execute(Event event) override;
};

// Noth the Plaguebringer

class NaxxMoveFromPlaguedChampionAction : public MovementAction
{
public:
    NaxxMoveFromPlaguedChampionAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "naxx move from plagued champion") {}
    bool Execute(Event event) override;
};

// Gothik the Harvester

class NaxxGothikGoToSideAction : public MovementAction
{
public:
    NaxxGothikGoToSideAction(PlayerbotAI* botAI) : MovementAction(botAI, "naxx gothik go to side") {}
    bool Execute(Event event) override;
};

// Patchwerk

class NaxxPatchwerkSoakAction : public AttackAction
{
public:
    NaxxPatchwerkSoakAction(PlayerbotAI* botAI) : AttackAction(botAI, "naxx patchwerk soak") {}
    bool Execute(Event event) override;
};

// The Four Horsemen

class NaxxThaneStackAction : public MovementAction
{
public:
    NaxxThaneStackAction(PlayerbotAI* botAI) : MovementAction(botAI, "naxx thane stack") {}
    bool Execute(Event event) override;
};

// Kel'Thuzad

class NaxxDetonateManaSpreadAction : public MovementAction
{
public:
    NaxxDetonateManaSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "naxx detonate mana spread") {}
    bool Execute(Event event) override;
};

class NaxxChainsMarkAction : public Action
{
public:
    NaxxChainsMarkAction(PlayerbotAI* botAI) : Action(botAI, "naxx chains mark") {}
    bool Execute(Event event) override;
};

#endif
