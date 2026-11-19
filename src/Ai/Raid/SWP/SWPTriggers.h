/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPTRIGGERS_H
#define PLAYERBOTS_SWPTRIGGERS_H

#include "Trigger.h"

// General

class SunwellBotIsNotInCombatTrigger : public Trigger
{
public:
    SunwellBotIsNotInCombatTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "sunwell bot is not in combat") {}
    bool IsActive() override;
};

// Kalecgos

class KalecgosMeleeInBreathOrTailArcTrigger : public Trigger
{
public:
    KalecgosMeleeInBreathOrTailArcTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos melee in breath or tail arc") {}
    bool IsActive() override;
};

class KalecgosTankPositionBossTrigger : public Trigger
{
public:
    KalecgosTankPositionBossTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos tank position boss") {}
    bool IsActive() override;
};

class KalecgosRangedClumpedTrigger : public Trigger
{
public:
    KalecgosRangedClumpedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos ranged clumped") {}
    bool IsActive() override;
};

class KalecgosSpectralRealmNeedsTankTrigger : public Trigger
{
public:
    KalecgosSpectralRealmNeedsTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos spectral realm needs tank") {}
    bool IsActive() override;
};

class KalecgosSpectralRealmNeedsHealerTrigger : public Trigger
{
public:
    KalecgosSpectralRealmNeedsHealerTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos spectral realm needs healer") {}
    bool IsActive() override;
};

class KalecgosStrayOffPlatformTrigger : public Trigger
{
public:
    KalecgosStrayOffPlatformTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos stray off platform") {}
    bool IsActive() override;
};

class KalecgosSpectralRealmNeedsDpsTrigger : public Trigger
{
public:
    KalecgosSpectralRealmNeedsDpsTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos spectral realm needs dps") {}
    bool IsActive() override;
};

class KalecgosSathrovarrLooseTrigger : public Trigger
{
public:
    KalecgosSathrovarrLooseTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos sathrovarr loose in spectral realm") {}
    bool IsActive() override;
};

class KalecgosSpectralDpsWrongTargetTrigger : public Trigger
{
public:
    KalecgosSpectralDpsWrongTargetTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos spectral dps wrong target") {}
    bool IsActive() override;
};

class KalecgosKalecNeedsHealingTrigger : public Trigger
{
public:
    KalecgosKalecNeedsHealingTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "kalecgos kalec needs healing") {}
    bool IsActive() override;
};

// Brutallus

class BrutallusOutOfStationTrigger : public Trigger
{
public:
    BrutallusOutOfStationTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "brutallus out of station") {}
    bool IsActive() override;
};

class BrutallusTankSwapTrigger : public Trigger
{
public:
    BrutallusTankSwapTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "brutallus tank swap") {}
    bool IsActive() override;
};

class BrutallusBurningTrigger : public Trigger
{
public:
    BrutallusBurningTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "brutallus burning") {}
    bool IsActive() override;
};

// This bot carries Burn AND has a self-clear that actually strips it, so it can stay in the cone
// and keep contributing instead of walking out. See BrutallusPurgeBurnAction for which abilities
// qualify - it is a shorter list than it looks.
class BrutallusCanPurgeBurnTrigger : public Trigger
{
public:
    BrutallusCanPurgeBurnTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "brutallus can purge burn") {}
    bool IsActive() override;
};

// Felmyst

// She is airborne, a fog lane has been identified, and this bot is standing in it. Only one lane is
// ever fogged at a time and it disperses before the next strafe, so this is a short sideways hop and
// the bot holds position the rest of the flight phase - see the FELMYST_* block in SWPHelpers.h.
class FelmystInFogLaneTrigger : public Trigger
{
public:
    FelmystInFogLaneTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst in fog lane") {}
    bool IsActive() override;
};

// A raid member is Encapsulated and this bot is inside the 20y blast. The victim itself is pacified
// and rooted, so clearing out is entirely the job of the bots around it.
class FelmystNearEncapsulateTrigger : public Trigger
{
public:
    FelmystNearEncapsulateTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst near encapsulate") {}
    bool IsActive() override;
};

// Gas Nova is on enough of the raid nearby for a Mass Dispel to be worth its cooldown. Priest-only,
// and stands down whenever the bot owes a repositioning move - see FelmystShouldMassDispel.
// A raid member has been mind-controlled and is now fighting the raid. They cannot be saved (see
// FindFelmystCharmedRaider), so the only answer is to kill them - which is the retail mechanic too.
class FelmystCharmedRaiderLooseTrigger : public Trigger
{
public:
    FelmystCharmedRaiderLooseTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst charmed raider loose") {}
    bool IsActive() override;
};

class FelmystGasNovaDispellableTrigger : public Trigger
{
public:
    FelmystGasNovaDispellableTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst gas nova dispellable") {}
    bool IsActive() override;
};

// This bot is stranded away from the raid and has no live reason to be - typically a vapor carrier whose
// vapor has despawned. Stands down while any fog lane is live so the walk back never crosses one.
class FelmystStrayFromRaidTrigger : public Trigger
{
public:
    FelmystStrayFromRaidTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst stray from raid") {}
    bool IsActive() override;
};

class FelmystChasedByVaporTrigger : public Trigger
{
public:
    FelmystChasedByVaporTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "felmyst chased by vapor") {}
    bool IsActive() override;
};

// Eredar Twins

// A Conflagration is being cast AT THIS BOT and the cast has not landed yet. That window is the whole
// mechanic: the destination is resolved when the cast completes, so running now moves an 8y chain bomb
// out of the raid, and once it lands the bot is confused and can do nothing at all.
class TwinsConflagrationOnMeTrigger : public Trigger
{
public:
    TwinsConflagrationOnMeTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins conflagration on me") {}
    bool IsActive() override;
};

// Somebody else nearby is a Conflagration bomb - either being cast at, or already pulsing 1600 into
// everything within 8y of itself.
class TwinsNearConflagrationTrigger : public Trigger
{
public:
    TwinsNearConflagrationTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins near conflagration") {}
    bool IsActive() override;
};

// This bot is damaging Alythess while Sacrolash is still alive. Every point of it is thrown away -
// Empower fully heals whichever twin survives - and killing Alythess first is strictly worse besides,
// because the survivor then inherits Conflagration.
class TwinsWrongDpsTargetTrigger : public Trigger
{
public:
    TwinsWrongDpsTargetTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins wrong dps target") {}
    bool IsActive() override;
};

// The raid's off tank is not holding Alythess. She never melees, so her entire threat to the raid is
// Blaze landing on whoever holds her - which needs to be somebody with a health pool and a healer.
class TwinsAlythessUntankedTrigger : public Trigger
{
public:
    TwinsAlythessUntankedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins alythess untanked") {}
    bool IsActive() override;
};

// One trigger for BOTH directions of blaze footwork - stepping out of a patch, and stepping into one to
// convert away a stack of Dark Touched. Splitting them into two triggers would recreate the in-and-out
// oscillator; see GetTwinsBlazeStation, which is the single position authority.
class TwinsBlazeFootworkTrigger : public Trigger
{
public:
    TwinsBlazeFootworkTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins blaze footwork") {}
    bool IsActive() override;
};

// Buried in Flame Touched, and out of range of the Shadow Blades that would strip it.
class TwinsNeedsShadowCleanseTrigger : public Trigger
{
public:
    TwinsNeedsShadowCleanseTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins needs shadow cleanse") {}
    bool IsActive() override;
};

// Pyrogenics is up on Alythess and this bot can strip a magic buff off an enemy.
class TwinsPyrogenicsUpTrigger : public Trigger
{
public:
    TwinsPyrogenicsUpTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins pyrogenics up") {}
    bool IsActive() override;
};

// This bot's co-tank on Sacrolash has been hit by Confounding Blow and is confused, so it cannot steer
// her for the next 6s. Symmetric between the two Sacrolash tanks - whoever taunts keeps her until the
// next Confounding Blow lands on them.
class TwinsCotankConfoundedTrigger : public Trigger
{
public:
    TwinsCotankConfoundedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins cotank confounded") {}
    bool IsActive() override;
};

// Phase 2: Sacrolash is dead and this bot is out of the clump. Shadow Nova is by then the only shadow in
// the fight, so standing together is what strips Flame Touched off the raid - see TWINS_STACK_RANGE.
// -------------------------------------------------------------------------------------------------
// M'uru / Entropius

// Standing in - or about to be standing in - M'uru's 15y Darkness. Ranked above even the Dark Fiend
// dispel: 3000 shadow per second with healing received at -100% kills a level-70 bot in four ticks and
// no healer can answer it, so a bot in there is worth nothing to the raid until it is out.
class MuruInDarknessTrigger : public Trigger
{
public:
    MuruInDarknessTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru in darkness") {}
    bool IsActive() override;
};

// A Dark Fiend this bot has been assigned still carries 45934. This is THE wipe mechanic of the fight:
// 45944 is 5000 plus 2000 per second for ten seconds to every player within FIFTY yards, uncapped, and
// damage cannot kill a fiend at all - only a MAGIC dispel removes it.
class MuruDarkFiendUpTrigger : public Trigger
{
public:
    MuruDarkFiendUpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru dark fiend up") {}
    bool IsActive() override;
};

class MuruInVoidZoneTrigger : public Trigger
{
public:
    MuruInVoidZoneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru in void zone") {}
    bool IsActive() override;
};

// An ARMED singularity is within pull range. Armed matters: it spends the first 8 of its 18 seconds
// without 46228 and cannot grab anybody, and it is chasing someone the whole time.
class MuruNearSingularityTrigger : public Trigger
{
public:
    MuruNearSingularityTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru near singularity") {}
    bool IsActive() override;
};

// A melee DPS is inside a Void Sentinel's 10y Shadow Pulse. The Sentinel's own tank is exempt.
class MuruInShadowPulseTrigger : public Trigger
{
public:
    MuruInShadowPulseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru in shadow pulse") {}
    bool IsActive() override;
};

// A tank's assigned add is loose on somebody who is not a tank.
class MuruAddUntankedTrigger : public Trigger
{
public:
    MuruAddUntankedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru add untanked") {}
    bool IsActive() override;
};

// This bot is on the wrong thing for its lane - including the handoff window, where M'uru is clamped to
// 1 HP and NOT_SELECTABLE for a full 7 seconds before Entropius exists.
class MuruWrongTargetTrigger : public Trigger
{
public:
    MuruWrongTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "muru wrong target") {}
    bool IsActive() override;
};

class TwinsOutOfAlythessStackTrigger : public Trigger
{
public:
    TwinsOutOfAlythessStackTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins out of alythess stack") {}
    bool IsActive() override;
};

// Sacrolash has drifted toward the 50y mark where CheckInRoom fires raid-wide Fireblast. Only her
// current tank sees this, because only her current tank can walk her back.
class TwinsSacrolashOverextendedTrigger : public Trigger
{
public:
    TwinsSacrolashOverextendedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twins sacrolash overextended") {}
    bool IsActive() override;
};

// Kil'jaeden

// An Armageddon marker is close enough that the meteor beneath it would land on this bot. 9999 fire in a
// 9y circle, six seconds after the marker appears - instant death at level 70.
class KjInArmageddonTrigger : public Trigger
{
public:
    KjInArmageddonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj in armageddon") {}
    bool IsActive() override;
};

// This bot is possessing a blue drake. Stays true for the whole possession: the drake is the only thing
// standing between the raid and 47,499 unmitigated shadow, and the bot's own body is pacified, silenced
// and movement-locked, so it has nothing else it could be doing.
class KjPilotingDrakeTrigger : public Trigger
{
public:
    KjPilotingDrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj piloting drake") {}
    bool IsActive() override;
};

// Kil'jaeden is carrying the 8s Darkness aura. Its expiry deals 47,499 at radius 50000 - the whole map -
// so this is not a dodge, it is a deadline to be inside the drake's 12y shield.
class KjDarknessIncomingTrigger : public Trigger
{
public:
    KjDarknessIncomingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj darkness incoming") {}
    bool IsActive() override;
};

// An orb has been empowered, this bot is the designated pilot, and nobody in the raid holds a drake.
class KjOrbAvailableTrigger : public Trigger
{
public:
    KjOrbAvailableTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj orb available") {}
    bool IsActive() override;
};

// This bot is carrying Fire Bloom - ten separate 1618 hits into 10y of allies, one every two seconds.
// Deliberately inactive inside a Darkness window, because the shield strips the DoT outright.
class KjFireBloomOnMeTrigger : public Trigger
{
public:
    KjFireBloomOnMeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj fire bloom on me") {}
    bool IsActive() override;
};

// The main tank has drifted off the authored anchor, and the boss chases his victim - so the boss has
// drifted with him, and every fixed position in the fight is measured from that anchor.
class KjBossOffAnchorTrigger : public Trigger
{
public:
    KjBossOffAnchorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj boss off anchor") {}
    bool IsActive() override;
};

// A ranged bot or healer is off its slot on the spread ring.
class KjOutOfStationTrigger : public Trigger
{
public:
    KjOutOfStationTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj out of station") {}
    bool IsActive() override;
};

// This bot is on the wrong thing for its lane - including the two windows where Kil'jaeden exists but
// cannot legally be attacked (the 11s rebirth, and after the killing blow).
class KjWrongTargetTrigger : public Trigger
{
public:
    KjWrongTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kj wrong target") {}
    bool IsActive() override;
};

#endif
