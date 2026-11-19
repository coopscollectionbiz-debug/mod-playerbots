/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPACTIONS_H
#define PLAYERBOTS_SWPACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// General

class SunwellEraseTimersAndTrackersAction : public Action
{
public:
    SunwellEraseTimersAndTrackersAction(PlayerbotAI* botAI) : Action(botAI, "sunwell erase timers and trackers") {}
    bool Execute(Event event) override;
};

// Kalecgos

class KalecgosMeleeFlankBossAction : public MovementAction
{
public:
    KalecgosMeleeFlankBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos melee flank boss") {}
    bool Execute(Event event) override;
};

class KalecgosTankPositionBossAction : public MovementAction
{
public:
    KalecgosTankPositionBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos tank position boss") {}
    bool Execute(Event event) override;
};

class KalecgosDisperseRangedAction : public MovementAction
{
public:
    KalecgosDisperseRangedAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos disperse ranged") {}
    bool Execute(Event event) override;
};

class KalecgosEnterSpectralRiftAction : public MovementAction
{
public:
    KalecgosEnterSpectralRiftAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos enter spectral rift") {}
    bool Execute(Event event) override;
};

class KalecgosReturnToPlatformAction : public MovementAction
{
public:
    KalecgosReturnToPlatformAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos return to platform") {}
    bool Execute(Event event) override;
};

class KalecgosTankPickUpSathrovarrAction : public AttackAction
{
public:
    KalecgosTankPickUpSathrovarrAction(PlayerbotAI* botAI) : AttackAction(botAI, "kalecgos tank pick up sathrovarr") {}
    bool Execute(Event event) override;
};

class KalecgosAttackSathrovarrAction : public AttackAction
{
public:
    KalecgosAttackSathrovarrAction(PlayerbotAI* botAI) : AttackAction(botAI, "kalecgos attack sathrovarr") {}
    bool Execute(Event event) override;
};

class KalecgosHealKalecAction : public MovementAction
{
public:
    KalecgosHealKalecAction(PlayerbotAI* botAI) : MovementAction(botAI, "kalecgos heal kalec") {}
    bool Execute(Event event) override;
};

// Brutallus

// Self-clear Burn with a school-immunity ability, so the bot never has to leave the cone.
class BrutallusPurgeBurnAction : public Action
{
public:
    BrutallusPurgeBurnAction(
        PlayerbotAI* botAI) : Action(botAI, "brutallus purge burn") {}
    bool Execute(Event event) override;
};

class BrutallusTauntBossAction : public AttackAction
{
public:
    BrutallusTauntBossAction(PlayerbotAI* botAI) : AttackAction(botAI, "brutallus taunt boss") {}
    bool Execute(Event event) override;
};

class BrutallusMoveToStationAction : public MovementAction
{
public:
    BrutallusMoveToStationAction(PlayerbotAI* botAI) : MovementAction(botAI, "brutallus move to station") {}
    bool Execute(Event event) override;
};

// Felmyst

class FelmystLeaveFogLaneAction : public MovementAction
{
public:
    FelmystLeaveFogLaneAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "felmyst leave fog lane") {}
    bool Execute(Event event) override;
};

class FelmystFleeEncapsulateAction : public MovementAction
{
public:
    FelmystFleeEncapsulateAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "felmyst flee encapsulate") {}
    bool Execute(Event event) override;
};

class FelmystAttackCharmedRaiderAction : public AttackAction
{
public:
    FelmystAttackCharmedRaiderAction(
        PlayerbotAI* botAI) : AttackAction(botAI, "felmyst attack charmed raider") {}
    bool Execute(Event event) override;
};

class FelmystMassDispelGasNovaAction : public Action
{
public:
    FelmystMassDispelGasNovaAction(
        PlayerbotAI* botAI) : Action(botAI, "felmyst mass dispel gas nova") {}
    bool Execute(Event event) override;
};

// Walk a stranded bot back to the raid. Exists because the vapor kite deliberately sends bots away and
// the flight-phase movement veto removes every generic way back.
class FelmystRegroupAction : public MovementAction
{
public:
    FelmystRegroupAction(PlayerbotAI* botAI) : MovementAction(botAI, "felmyst regroup") {}
    bool Execute(Event event) override;
};

class FelmystKiteVaporAction : public MovementAction
{
public:
    FelmystKiteVaporAction(PlayerbotAI* botAI) : MovementAction(botAI, "felmyst kite vapor") {}
    bool Execute(Event event) override;
};

// Eredar Twins

class TwinsFleeConflagrationAction : public MovementAction
{
public:
    TwinsFleeConflagrationAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "twins flee conflagration") {}
    bool Execute(Event event) override;
};

class TwinsClearConflagrationAction : public MovementAction
{
public:
    TwinsClearConflagrationAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "twins clear conflagration") {}
    bool Execute(Event event) override;
};

class TwinsFocusSacrolashAction : public AttackAction
{
public:
    TwinsFocusSacrolashAction(PlayerbotAI* botAI) : AttackAction(botAI, "twins focus sacrolash") {}
    bool Execute(Event event) override;
};

class TwinsTankAlythessAction : public AttackAction
{
public:
    TwinsTankAlythessAction(PlayerbotAI* botAI) : AttackAction(botAI, "twins tank alythess") {}
    bool Execute(Event event) override;
};

// Serves BOTH directions - into a patch to cleanse Dark Touched, out of one to stop burning. One action,
// because GetTwinsBlazeStation is the one place allowed to decide which.
class TwinsBlazeFootworkAction : public MovementAction
{
public:
    TwinsBlazeFootworkAction(PlayerbotAI* botAI) : MovementAction(botAI, "twins blaze footwork") {}
    bool Execute(Event event) override;
};

class TwinsSeekShadowBladesAction : public MovementAction
{
public:
    TwinsSeekShadowBladesAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "twins seek shadow blades") {}
    bool Execute(Event event) override;
};

class TwinsDispelPyrogenicsAction : public Action
{
public:
    TwinsDispelPyrogenicsAction(PlayerbotAI* botAI) : Action(botAI, "twins dispel pyrogenics") {}
    bool Execute(Event event) override;
};

// -------------------------------------------------------------------------------------------------
// M'uru / Entropius

class MuruClearDarknessAction : public MovementAction
{
public:
    MuruClearDarknessAction(PlayerbotAI* botAI) : MovementAction(botAI, "muru clear darkness") {}
    bool Execute(Event event) override;
};

// Strip 45934 off the fiend assigned to THIS bot. Not a damage action - damage is meaningless here,
// npc_dark_fiend::DamageTaken clamps every lethal hit so it can be ground to 1% and never die.
class MuruDispelDarkFiendAction : public Action
{
public:
    MuruDispelDarkFiendAction(PlayerbotAI* botAI) : Action(botAI, "muru dispel dark fiend") {}
    bool Execute(Event event) override;
};

class MuruClearVoidZoneAction : public MovementAction
{
public:
    MuruClearVoidZoneAction(PlayerbotAI* botAI) : MovementAction(botAI, "muru clear void zone") {}
    bool Execute(Event event) override;
};

class MuruFleeSingularityAction : public MovementAction
{
public:
    MuruFleeSingularityAction(PlayerbotAI* botAI) : MovementAction(botAI, "muru flee singularity") {}
    bool Execute(Event event) override;
};

class MuruLeaveShadowPulseAction : public MovementAction
{
public:
    MuruLeaveShadowPulseAction(PlayerbotAI* botAI) : MovementAction(botAI, "muru leave shadow pulse") {}
    bool Execute(Event event) override;
};

// Taunt-then-attack, in that order. A fresh add on a healer has nobody above it on the threat list, so
// the taunt does redirect - unlike the tank-to-tank handoffs on Brutallus and the Twins, which need the
// outgoing tank's threat cut FIRST because a bot taunt only matches the current top.
class MuruTankAddAction : public AttackAction
{
public:
    MuruTankAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "muru tank add") {}
    bool Execute(Event event) override;
};

class MuruFocusTargetAction : public AttackAction
{
public:
    MuruFocusTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "muru focus target") {}
    bool Execute(Event event) override;
};

// Take Sacrolash off a co-tank who has been confused. Cut-then-taunt, for the reasons spelled out on
// BrutallusTauntBossAction - a bot taunt alone cannot change a boss's victim.
class TwinsReliefTauntSacrolashAction : public AttackAction
{
public:
    TwinsReliefTauntSacrolashAction(
        PlayerbotAI* botAI) : AttackAction(botAI, "twins relief taunt sacrolash") {}
    bool Execute(Event event) override;
};

class TwinsStackOnAlythessAction : public MovementAction
{
public:
    TwinsStackOnAlythessAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "twins stack on alythess") {}
    bool Execute(Event event) override;
};

class TwinsPullSacrolashBackAction : public MovementAction
{
public:
    TwinsPullSacrolashBackAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "twins pull sacrolash back") {}
    bool Execute(Event event) override;
};

// Kil'jaeden

class KjClearArmageddonAction : public MovementAction
{
public:
    KjClearArmageddonAction(PlayerbotAI* botAI) : MovementAction(botAI, "kj clear armageddon") {}
    bool Execute(Event event) override;
};

class KjQuarantineFireBloomAction : public MovementAction
{
public:
    KjQuarantineFireBloomAction(
        PlayerbotAI* botAI) : MovementAction(botAI, "kj quarantine fire bloom") {}
    bool Execute(Event event) override;
};

class KjStackForDarknessAction : public MovementAction
{
public:
    KjStackForDarknessAction(PlayerbotAI* botAI) : MovementAction(botAI, "kj stack for darkness") {}
    bool Execute(Event event) override;
};

class KjHoldAnchorAction : public MovementAction
{
public:
    KjHoldAnchorAction(PlayerbotAI* botAI) : MovementAction(botAI, "kj hold anchor") {}
    bool Execute(Event event) override;
};

class KjHoldStationAction : public MovementAction
{
public:
    KjHoldStationAction(PlayerbotAI* botAI) : MovementAction(botAI, "kj hold station") {}
    bool Execute(Event event) override;
};

// Walk to an empowered orb and click it. A MovementAction rather than a plain Action because the walk is
// most of the work: the orbs sit 48y out from the platform centre.
class KjClaimOrbAction : public MovementAction
{
public:
    KjClaimOrbAction(PlayerbotAI* botAI) : MovementAction(botAI, "kj claim orb") {}
    bool Execute(Event event) override;
};

// Drive the possessed drake: park it on the anchor, shield the raid through Darkness, breathe on
// cooldown. Cooldowns are tracked by hand because a charmed creature observes none of its own.
class KjDriveDrakeAction : public Action
{
public:
    KjDriveDrakeAction(PlayerbotAI* botAI) : Action(botAI, "kj drive drake") {}
    bool Execute(Event event) override;
};

class KjFocusTargetAction : public AttackAction
{
public:
    KjFocusTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "kj focus target") {}
    bool Execute(Event event) override;
};

#endif
