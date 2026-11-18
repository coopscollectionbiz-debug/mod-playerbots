/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldExtraTriggers.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "UldExtraHelpers.h"

using namespace EncounterHelpers;
using namespace UlduarExtraHelpers;

bool UldKillOrderMarkTrigger::IsActive()
{
    // One marker per raid; encounter-only, so trash keeps normal targeting.
    return bot->IsInCombat() && IsEncounterInProgress(bot, MAP_ULDUAR) && IsMechanicTrackerBot(bot, MAP_ULDUAR) &&
           GetKillOrderMarkTarget(botAI);
}

bool UldOffTankAddTrigger::IsActive()
{
    return PlayerbotAI::IsAssistTankOfIndex(bot, 0, true) && IsEncounterInProgress(bot, MAP_ULDUAR) &&
           GetUntankedOffTankAdd(botAI);
}

bool UldIgnisConstructTrigger::IsActive()
{
    // The off-tank drags its constructs into Scorch, then to the water.
    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true) || !AI_VALUE2(Unit*, "find target", "ignis the furnace master"))
        return false;

    constexpr float ARRIVAL_DISTANCE = 4.0f;
    Position destination;
    return GetIgnisConstructDestination(botAI, destination) && bot->GetExactDist2d(destination) > ARRIVAL_DISTANCE;
}

bool UldIgnisMeleeTrigger::IsActive()
{
    // Scorch hits his front: melee stand behind him.
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot))
        return false;

    Unit* ignis = AI_VALUE2(Unit*, "find target", "ignis the furnace master");
    return ignis && bot->GetVictim() == ignis;
}

bool UldXtDebuffTrigger::IsActive()
{
    // Searing Light and Gravity Bomb hurt everyone near their target, who leaves the raid.
    float const radius = GetXtDebuffRadius(bot);
    return radius > 0.0f && GetNearestPlayerInRadius(bot, radius) != nullptr;
}

bool UldXtBotAddTrigger::IsActive()
{
    // Ranged kill Scrapbots before they reach XT-002 to heal him, and Boombots away from the raid.
    if (!PlayerbotAI::IsRangedDps(bot) || !AI_VALUE2(Unit*, "find target", "xt-002 deconstructor"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() &&
        (victim->GetEntry() == NPC_XE321_BOOMBOT || victim->GetEntry() == NPC_XS013_SCRAPBOT))
        return false;

    float const range = botAI->GetRange("spell");
    return GetNearestNpc(botAI, NPC_XE321_BOOMBOT, range) || GetNearestNpc(botAI, NPC_XS013_SCRAPBOT, range);
}

bool UldAuriayaStackTrigger::IsActive()
{
    // Sonic Screech splits between everyone in front of her, and Feral Rush only picks distant players.
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* auriaya = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!auriaya || auriaya->GetVictim() == bot)
        return false;

    constexpr float STACK_TOLERANCE = 5.0f;
    return bot->GetExactDist2d(GetAuriayaStackSpot(auriaya)) > STACK_TOLERANCE;
}

bool UldInterruptTrigger::IsActive()
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_SHAMAN:
        case CLASS_MAGE:
        case CLASS_DEATH_KNIGHT:
        case CLASS_HUNTER:
            break;
        default:
            return false;
    }

    return GetInterruptTarget(botAI) != nullptr;
}

bool UldCouncilTankTrigger::IsActive()
{
    Unit* member = GetAssignedCouncilMember(botAI);
    return member && (bot->GetVictim() != member || member->GetVictim() != bot);
}

bool UldAlgalonPhasePunchTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* algalon = AI_VALUE2(Unit*, "find target", "algalon the observer");
    Unit* victim = algalon ? algalon->GetVictim() : nullptr;
    if (!victim || victim == bot || GetAuraStackAmount(victim, SPELL_PHASE_PUNCH) < PHASE_PUNCH_SWAP_STACKS)
        return false;

    return GetTakeoverTank(bot, victim, [](Player const* tank)
        { return GetAuraStackAmount(tank, SPELL_PHASE_PUNCH) >= PHASE_PUNCH_SWAP_STACKS; }) == bot;
}

bool UldAlgalonBigBangTrigger::IsActive()
{
    // Everyone but the main tank waits out Big Bang inside a Black Hole; the core phases them back after.
    if (PlayerbotAI::IsMainTank(bot) || bot->HasAuraType(SPELL_AURA_PHASE))
        return false;

    Unit* algalon = AI_VALUE2(Unit*, "find target", "algalon the observer");
    int32 remaining = 0;
    if (!IsCastingDifficultySpell(algalon, SPELL_BIG_BANG, &remaining) || remaining > BIG_BANG_ENTER_LEAD_MS)
        return false;

    return GetNearestNpc(botAI, NPC_BLACK_HOLE, ULDUAR_EXTRA_SEARCH_DISTANCE) != nullptr;
}

bool UldAlgalonStarTrigger::IsActive()
{
    // Ranged DPS kill the Collapsing Stars that make the Black Holes.
    if (!PlayerbotAI::IsRangedDps(bot) || !AI_VALUE2(Unit*, "find target", "algalon the observer"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() && victim->GetEntry() == NPC_COLLAPSING_STAR)
        return false;

    return GetNearestNpc(botAI, NPC_COLLAPSING_STAR, botAI->GetRange("spell")) != nullptr;
}

bool UldRazorscaleMeleeTrigger::IsActive()
{
    // Grounded, she breathes fire in front: melee stand behind her.
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot))
        return false;

    constexpr float RAZORSCALE_GROUNDED_Z = 440.0f;
    Unit* razorscale = AI_VALUE2(Unit*, "find target", "razorscale");
    return razorscale && razorscale->GetPositionZ() < RAZORSCALE_GROUNDED_Z && bot->GetVictim() == razorscale;
}

bool UldXtVoidZoneTrigger::IsActive()
{
    // Hard mode: Gravity Bomb leaves a Void Zone that guides get out of at once.
    return AI_VALUE2(Unit*, "find target", "xt-002 deconstructor") &&
           GetNearestNpc(botAI, NPC_XT_VOID_ZONE, GetXtVoidZoneAvoidDistance());
}

bool UldKologarnCrunchArmorTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* kologarn = AI_VALUE2(Unit*, "find target", "kologarn");
    Unit* victim = kologarn ? kologarn->GetVictim() : nullptr;
    if (!victim || victim == bot || GetCrunchArmorStacks(victim) < CRUNCH_ARMOR_SWAP_STACKS)
        return false;

    return GetTakeoverTank(bot, victim, [](Player const* tank)
        { return GetCrunchArmorStacks(tank) >= CRUNCH_ARMOR_SWAP_STACKS; }) == bot;
}

bool UldThorimUnbalancingStrikeTrigger::IsActive()
{
    // Guides: the other tank taunts Thorim until Unbalancing Strike falls off his tank.
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* thorim = AI_VALUE2(Unit*, "find target", "thorim");
    Unit* victim = thorim && thorim->IsHostileTo(bot) ? thorim->GetVictim() : nullptr;
    if (!victim || victim == bot || !victim->HasAura(SPELL_UNBALANCING_STRIKE))
        return false;

    return GetTakeoverTank(bot, victim, [](Player const* tank) { return tank->HasAura(SPELL_UNBALANCING_STRIKE); }) ==
           bot;
}

bool UldThorimSifFrostNovaTrigger::IsActive()
{
    // Hard mode: guides get away from Sif before her Frost Nova lands.
    float safeDistance = 0.0f;
    return AI_VALUE2(Unit*, "find target", "thorim") && GetSifInNovaRange(botAI, safeDistance);
}

bool UldThorimSifBlizzardTrigger::IsActive()
{
    // Hard mode: guides stay out of Sif's Blizzard as it moves around the arena.
    float safeDistance = 0.0f;
    return AI_VALUE2(Unit*, "find target", "thorim") && GetSifBlizzardInRange(botAI, safeDistance);
}

bool UldMimironFlamesTrigger::IsActive()
{
    // Hard mode: the fire patches burn whoever stands in them; they exist only in this encounter.
    float safeDistance = 0.0f;
    return bot->IsInCombat() && GetMimironFlamesInRange(botAI, safeDistance);
}

bool UldMimironFrostBombTrigger::IsActive()
{
    // Hard mode: guides get clear of a Frost Bomb before it goes off.
    float safeDistance = 0.0f;
    return bot->IsInCombat() && GetMimironFrostBombInRange(botAI, safeDistance);
}

bool UldVezaxSaroniteVaporsTrigger::IsActive()
{
    // Normal mode: guides kill each Saronite Vapor, so six never live at once to form the Animus. The
    // vapors never join the fight, so nothing else targets them.
    if (!PlayerbotAI::IsRangedDps(bot) || IsUlduarHardMode(botAI) || !AI_VALUE2(Unit*, "find target", "general vezax"))
        return false;

    Unit* vapors = GetNearestNpc(botAI, NPC_SARONITE_VAPORS, botAI->GetRange("spell"));
    return vapors && bot->GetVictim() != vapors && bot->IsValidAttackTarget(vapors);
}

bool UldHodirToastyFireTrigger::IsActive()
{
    // A Toasty Fire negates Biting Cold; ranged and healers wait out their stacks beside one.
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsTank(bot) || botAI->HasCheat(BotCheatMask::raid))
        return false;

    if (GetAuraStackAmount(bot, SPELL_BITING_COLD) < BITING_COLD_FIRE_STACKS ||
        !AI_VALUE2(Unit*, "find target", "hodir"))
        return false;

    constexpr float FIRE_STAND_DISTANCE = 3.0f;
    Unit* fire = GetNearestNpc(botAI, NPC_TOASTY_FIRE, ULDUAR_EXTRA_SEARCH_DISTANCE);
    return fire && bot->GetExactDist2d(fire) > FIRE_STAND_DISTANCE;
}

bool UldHodirBitingColdTrigger::IsActive()
{
    // Moving or jumping sheds Biting Cold; the raid cheat strips it instead.
    if (GetAuraStackAmount(bot, SPELL_BITING_COLD) < BITING_COLD_MOVE_STACKS ||
        botAI->HasCheat(BotCheatMask::raid) || !AI_VALUE2(Unit*, "find target", "hodir"))
        return false;

    // Ranged and healers with a Toasty Fire go to it instead.
    return !PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsTank(bot) ||
           !GetNearestNpc(botAI, NPC_TOASTY_FIRE, ULDUAR_EXTRA_SEARCH_DISTANCE);
}

bool UldHodirIceBlockTrigger::IsActive()
{
    // Ranged DPS break the Flash Freeze ice around frozen players and Hodir's helpers.
    if (!PlayerbotAI::IsRangedDps(bot) || !AI_VALUE2(Unit*, "find target", "hodir"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() &&
        (victim->GetEntry() == NPC_FLASH_FREEZE_PLAYER || victim->GetEntry() == NPC_FLASH_FREEZE_HELPER))
        return false;

    return GetHodirIceBlock(botAI) != nullptr;
}

bool UldHodirFrozenBlowsTrigger::IsActive()
{
    // Frozen Blows turns his hits to Frost: the frost-resistance off-tank takes him while it lasts,
    // then the main tank takes him back.
    Unit* hodir = AI_VALUE2(Unit*, "find target", "hodir");
    Player* offTank = GetGroupAssistTank(bot, 0);
    Player* mainTank = GetGroupMainTank(bot);
    if (!hodir || !offTank || !mainTank)
        return false;

    bool const frozenBlows = HasDifficultyAura(hodir, SPELL_FROZEN_BLOWS, hodir);
    Player* wanted = frozenBlows ? offTank : mainTank;
    return wanted == bot && hodir->GetVictim() != bot;
}

bool UldFreyaIronRootsTrigger::IsActive()
{
    // Guides free a rooted player by killing the roots; DPS break them.
    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    if (!AI_VALUE2(Unit*, "find target", "freya") && !AI_VALUE2(Unit*, "find target", "elder ironbranch"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() &&
        (victim->GetEntry() == NPC_IRON_ROOTS || victim->GetEntry() == NPC_STRENGTHENED_IRON_ROOTS))
        return false;

    return GetFreyaIronRoots(botAI) != nullptr;
}

bool UldFreyaNaturesFuryTrigger::IsActive()
{
    // Nature's Fury hurts the allies around its target, so the target leaves the raid.
    float const radius = GetNaturesFuryRadius(bot);
    return radius > 0.0f && GetNearestPlayerInRadius(bot, radius) != nullptr;
}

bool UldFreyaSunBeamTrigger::IsActive()
{
    if (!AI_VALUE2(Unit*, "find target", "freya") && !AI_VALUE2(Unit*, "find target", "elder brightleaf"))
        return false;

    float safeDistance = 0.0f;
    return GetFreyaSunBeamInRange(botAI, safeDistance) != nullptr;
}

bool UldMimironProximityMineTrigger::IsActive()
{
    // Guides keep everyone off the mines Leviathan Mk II drops; the raid cheat clears them instead.
    if (botAI->HasCheat(BotCheatMask::raid) || !AI_VALUE2(Unit*, "find target", "leviathan mk ii"))
        return false;

    return GetNearestNpc(botAI, NPC_MIMIRON_PROXIMITY_MINE, MIMIRON_MINE_AVOID_DISTANCE) != nullptr;
}

bool UldYoggOminousCloudTrigger::IsActive()
{
    // Touching a cloud adds a Guardian on top of Sara's own; guides keep clear of them in phase one.
    // The raid cheat has its own cloud handling. Sara can't be attacked in phase one, so she may be
    // missing from the threat list "find target" reads; the clouds alone mark the encounter.
    if (botAI->HasCheat(BotCheatMask::raid) || !bot->IsInCombat())
        return false;

    return GetNearestNpc(botAI, NPC_YOGG_OMINOUS_CLOUD, YOGG_CLOUD_AVOID_DISTANCE) != nullptr;
}
