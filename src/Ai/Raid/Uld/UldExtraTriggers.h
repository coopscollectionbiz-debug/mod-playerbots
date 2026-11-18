/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDEXTRATRIGGERS_H
#define PLAYERBOTS_ULDEXTRATRIGGERS_H

#include "Trigger.h"

// Every boss below
class UldKillOrderMarkTrigger : public Trigger
{
public:
    UldKillOrderMarkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld kill order mark") {}
    bool IsActive() override;
};

class UldOffTankAddTrigger : public Trigger
{
public:
    UldOffTankAddTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld off-tank add") {}
    bool IsActive() override;
};

class UldInterruptTrigger : public Trigger
{
public:
    UldInterruptTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld interrupt") {}
    bool IsActive() override;
};

// The Iron Council
class UldCouncilTankTrigger : public Trigger
{
public:
    UldCouncilTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld council tank") {}
    bool IsActive() override;
};

class UldXtVoidZoneTrigger : public Trigger
{
public:
    UldXtVoidZoneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld xt void zone") {}
    bool IsActive() override;
};

// Kologarn
class UldKologarnCrunchArmorTrigger : public Trigger
{
public:
    UldKologarnCrunchArmorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld kologarn crunch armor") {}
    bool IsActive() override;
};

// Thorim
class UldThorimUnbalancingStrikeTrigger : public Trigger
{
public:
    UldThorimUnbalancingStrikeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld thorim unbalancing strike") {}
    bool IsActive() override;
};

class UldThorimSifFrostNovaTrigger : public Trigger
{
public:
    UldThorimSifFrostNovaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld thorim sif frost nova") {}
    bool IsActive() override;
};

class UldThorimSifBlizzardTrigger : public Trigger
{
public:
    UldThorimSifBlizzardTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld thorim sif blizzard") {}
    bool IsActive() override;
};

// Mimiron
class UldMimironFlamesTrigger : public Trigger
{
public:
    UldMimironFlamesTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld mimiron flames") {}
    bool IsActive() override;
};

class UldMimironFrostBombTrigger : public Trigger
{
public:
    UldMimironFrostBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld mimiron frost bomb") {}
    bool IsActive() override;
};

// General Vezax
class UldVezaxSaroniteVaporsTrigger : public Trigger
{
public:
    UldVezaxSaroniteVaporsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld vezax saronite vapors") {}
    bool IsActive() override;
};

// Hodir
class UldHodirToastyFireTrigger : public Trigger
{
public:
    UldHodirToastyFireTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld hodir toasty fire") {}
    bool IsActive() override;
};

class UldHodirBitingColdTrigger : public Trigger
{
public:
    UldHodirBitingColdTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld hodir biting cold") {}
    bool IsActive() override;
};

class UldHodirIceBlockTrigger : public Trigger
{
public:
    UldHodirIceBlockTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld hodir ice block") {}
    bool IsActive() override;
};

class UldHodirFrozenBlowsTrigger : public Trigger
{
public:
    UldHodirFrozenBlowsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld hodir frozen blows") {}
    bool IsActive() override;
};

// Freya
class UldFreyaIronRootsTrigger : public Trigger
{
public:
    UldFreyaIronRootsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld freya iron roots") {}
    bool IsActive() override;
};

class UldFreyaNaturesFuryTrigger : public Trigger
{
public:
    UldFreyaNaturesFuryTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld freya nature's fury") {}
    bool IsActive() override;
};

class UldFreyaSunBeamTrigger : public Trigger
{
public:
    UldFreyaSunBeamTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld freya sun beam") {}
    bool IsActive() override;
};

// Mimiron
class UldMimironProximityMineTrigger : public Trigger
{
public:
    UldMimironProximityMineTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld mimiron proximity mine") {}
    bool IsActive() override;
};

// Yogg-Saron
class UldYoggOminousCloudTrigger : public Trigger
{
public:
    UldYoggOminousCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld yogg ominous cloud") {}
    bool IsActive() override;
};

// Razorscale
class UldRazorscaleMeleeTrigger : public Trigger
{
public:
    UldRazorscaleMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld razorscale melee") {}
    bool IsActive() override;
};

// Ignis the Furnace Master
class UldIgnisConstructTrigger : public Trigger
{
public:
    UldIgnisConstructTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld ignis construct") {}
    bool IsActive() override;
};

class UldIgnisMeleeTrigger : public Trigger
{
public:
    UldIgnisMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld ignis melee") {}
    bool IsActive() override;
};

// XT-002 Deconstructor
class UldXtDebuffTrigger : public Trigger
{
public:
    UldXtDebuffTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld xt debuff") {}
    bool IsActive() override;
};

class UldXtBotAddTrigger : public Trigger
{
public:
    UldXtBotAddTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld xt bot add") {}
    bool IsActive() override;
};

// Auriaya
class UldAuriayaStackTrigger : public Trigger
{
public:
    UldAuriayaStackTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld auriaya stack") {}
    bool IsActive() override;
};

// Algalon the Observer
class UldAlgalonPhasePunchTrigger : public Trigger
{
public:
    UldAlgalonPhasePunchTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld algalon phase punch") {}
    bool IsActive() override;
};

class UldAlgalonBigBangTrigger : public Trigger
{
public:
    UldAlgalonBigBangTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld algalon big bang") {}
    bool IsActive() override;
};

class UldAlgalonStarTrigger : public Trigger
{
public:
    UldAlgalonStarTrigger(PlayerbotAI* botAI) : Trigger(botAI, "uld algalon star") {}
    bool IsActive() override;
};

#endif
