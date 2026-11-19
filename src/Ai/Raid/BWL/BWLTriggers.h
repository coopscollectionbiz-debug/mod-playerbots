/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLTRIGGERS_H
#define PLAYERBOTS_BWLTRIGGERS_H

#include "Trigger.h"

// General

class BwlSuppressionDeviceTrigger : public Trigger
{
public:
    BwlSuppressionDeviceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl suppression device") {}
    bool IsActive() override;
};

// Razorgore the Untamed

class BwlRazorgoreNotMindControlledTrigger : public Trigger
{
public:
    BwlRazorgoreNotMindControlledTrigger(PlayerbotAI* botAI, std::string const& name = "bwl razorgore not mind controlled") : Trigger(botAI, name) {}
    bool IsActive() override;
};

// Vaelastrasz the Corrupt

class BwlVaelastraszPositioningTrigger : public Trigger
{
public:
    BwlVaelastraszPositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl vaelastrasz positioning") {}
    bool IsActive() override;
};

class BwlVaelastraszBurningAdrenalineTrigger : public Trigger
{
public:
    BwlVaelastraszBurningAdrenalineTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl vaelastrasz burning adrenaline") {}
    bool IsActive() override;
};

// Broodlord Lashlayer

class BwlBroodlordRangedTooCloseTrigger : public Trigger
{
public:
    BwlBroodlordRangedTooCloseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl broodlord ranged too close") {}
    bool IsActive() override;
};

// Firemaw / Ebonroc / Flamegor

class BwlBlackDrakeNotVictimTrigger : public Trigger
{
public:
    BwlBlackDrakeNotVictimTrigger(PlayerbotAI* botAI, std::string bossName)
        : Trigger(botAI, "bwl " + bossName + " not victim"),
          bossName(std::move(bossName)) {}
    bool IsActive() override;

private:
    const std::string bossName;
};

// Chromaggus

class BwlAfflictionBronzeTrigger : public Trigger
{
public:
    BwlAfflictionBronzeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl affliction bronze") {}
    bool IsActive() override;
};

// Nefarian

class BwlNefarianPositioningTrigger : public Trigger
{
public:
    BwlNefarianPositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian positioning") {}
    bool IsActive() override;
};

class BwlNefarianWildMagicTrigger : public Trigger
{
public:
    BwlNefarianWildMagicTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian wild magic") {}
    bool IsActive() override;
};

class BwlNefarianCorruptedTotemTrigger : public Trigger
{
public:
    BwlNefarianCorruptedTotemTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "bwl nefarian corrupted totem", 1 * IN_MILLISECONDS)
    {
    }
    bool IsActive() override;
};

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07)

class BwlNefarianMtStationTrigger : public Trigger
{
public:
    BwlNefarianMtStationTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian mt station") {}
    bool IsActive() override;
};

class BwlNefarianPositionTrigger : public Trigger
{
public:
    BwlNefarianPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian position") {}
    bool IsActive() override;
};

class BwlNefarianDpsTargetTrigger : public Trigger
{
public:
    BwlNefarianDpsTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian dps target") {}
    bool IsActive() override;
};

class BwlNefarianOtTargetTrigger : public Trigger
{
public:
    BwlNefarianOtTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian ot target") {}
    bool IsActive() override;
};

class BwlNefarianTremorTrigger : public Trigger
{
public:
    BwlNefarianTremorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian tremor") {}
    bool IsActive() override;
};

class BwlNefarianDecurseTankTrigger : public Trigger
{
public:
    BwlNefarianDecurseTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian decurse tank") {}
    bool IsActive() override;
};

class BwlNefarianDispelPolymorphTrigger : public Trigger
{
public:
    BwlNefarianDispelPolymorphTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian dispel polymorph") {}
    bool IsActive() override;
};

class BwlNefarianPriestTankOnlyTrigger : public Trigger
{
public:
    BwlNefarianPriestTankOnlyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian priest tank only") {}
    bool IsActive() override;
};

class BwlNefarianHunterRearmTrigger : public Trigger
{
public:
    BwlNefarianHunterRearmTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian hunter rearm") {}
    bool IsActive() override;
};

class BwlNefarianRogueSurviveTrigger : public Trigger
{
public:
    BwlNefarianRogueSurviveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian rogue survive") {}
    bool IsActive() override;
};

class BwlNefarianMtSunderTrigger : public Trigger
{
public:
    BwlNefarianMtSunderTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl nefarian mt sunder") {}
    bool IsActive() override;
};

// Trash

class BwlDeathTalonWyrmguardTankTrigger : public Trigger
{
public:
    BwlDeathTalonWyrmguardTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl death talon wyrmguard tank") {}
    bool IsActive() override;
};

class BwlDeathTalonWyrmguardRangedTrigger : public Trigger
{
public:
    BwlDeathTalonWyrmguardRangedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl death talon wyrmguard ranged") {}
    bool IsActive() override;
};

// Death Talon Wyrmguard packs (overlay patch 0020)

class BwlWyrmguardTankTrigger : public Trigger
{
public:
    BwlWyrmguardTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard tank") {}
    bool IsActive() override;
};

class BwlWyrmguardMarkSkullTrigger : public Trigger
{
public:
    BwlWyrmguardMarkSkullTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard mark skull") {}
    bool IsActive() override;

private:
    uint32 lastNoSkullLogMs = 0;  // throttles the "[BWLWyrmguard] no skull" debug line (per bot)
};

class BwlWyrmguardDpsTargetTrigger : public Trigger
{
public:
    BwlWyrmguardDpsTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard dps target") {}
    bool IsActive() override;
};

class BwlWyrmguardMeleePositionTrigger : public Trigger
{
public:
    BwlWyrmguardMeleePositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard melee position") {}
    bool IsActive() override;
};

class BwlWyrmguardMeleeBehindTrigger : public Trigger
{
public:
    BwlWyrmguardMeleeBehindTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard melee behind") {}
    bool IsActive() override;
};

class BwlWyrmguardEbonrocKeepOutTrigger : public Trigger
{
public:
    BwlWyrmguardEbonrocKeepOutTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard ebonroc keep out") {}
    bool IsActive() override;
};

class BwlWyrmguardHealerPositionTrigger : public Trigger
{
public:
    BwlWyrmguardHealerPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl wyrmguard healer position") {}
    bool IsActive() override;
};

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
class BwlThreatHoldTrigger : public Trigger
{
public:
    BwlThreatHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bwl threat hold") {}
    bool IsActive() override;
};

#endif
