/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Loatheb (Individual Progression's level-60 Naxxramas): encounter gate, per-instance state and the view
// every Loatheb trigger, action and multiplier reads. Pure rules: Naxx40LoathebRules.h. Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-loatheb-design.md.

#ifndef PLAYERBOTS_NAXX40LOATHEB_H
#define PLAYERBOTS_NAXX40LOATHEB_H

#include <cstdint>
#include <memory>
#include <vector>

#include "Naxx40LoathebRules.h"

class Creature;
class Player;
class Unit;

namespace Naxx40Loatheb
{
    constexpr uint32_t NAXX40_MAP_ID = 533;

    // A live spore at the last refresh (spec §4.9 item 3): its raw GUID and when a refresh first saw it (getMSTime
    // clock, never 0), so the stray fallback can tell its age.
    struct SporeSeen
    {
        uint64_t guid;
        uint32_t firstSeenMs;
    };

    // A view is valid for the current tick only. loatheb, mainTank and any Creature*/Player* taken from it (SporeToKill)
    // must never be stored across ticks: the unit can despawn or log out. Re-read the view each tick instead.
    struct LoathebView
    {
        Creature* loatheb = nullptr;  // alive, non-null whenever GetView returned true
        // loatheb->GetVictim() when it is a player, else null (the strategy adds no taunts: whoever stock tank
        // behaviour put on him is the main tank). A non-tank victim (threat neglected, the tank died) is still the
        // rotation and emergency heal target; mainTankIsTank (PlayerbotAI::IsTank) gates the tank cooldowns.
        Player* mainTank = nullptr;
        bool mainTankIsTank = false;
        // The attempt's healer split, latched once per attempt. Shared and immutable, so reading it is free: use
        // view.plan->. Non-null whenever GetView returned true.
        std::shared_ptr<Naxx40LoathebRules::HealerPlan const> plan;
        uint8_t doomCount = 0;                   // Inevitable Doom casts resolved this attempt
        uint32_t msSinceDoomResolved = UINT32_MAX;  // UINT32_MAX before the first Doom resolved
        uint32_t msToDoomResolve = UINT32_MAX;      // UINT32_MAX when no Doom is ticking
        // The spores alive at the last refresh (spec §4.9 item 3), shared and immutable like plan; null before the
        // first grouped refresh. Each one must be resolved again (it may have died since).
        std::shared_ptr<std::vector<SporeSeen> const> spores;
    };

    // false = strategy inert: not map 533, not RAID_DIFFICULTY_10MAN_HEROIC (IP's Naxx40), the instance's
    // BOSS_LOATHEB (5) not IN_PROGRESS, or no living Loatheb (351020). On a failed gate the instance's state is
    // dropped, so the next pull re-latches from scratch; so does an instance entry not refreshed for 10 s (a new
    // attempt, or a reused instance id). Refreshes the shared state (Doom clock, live spore list and kill count, diag)
    // at most every REFRESH_INTERVAL_MS.
    bool GetView(Player* bot, LoathebView& view);

    enum class HealerJob : uint8_t
    {
        None,
        Rotation,
        Group  // a raid healer (spec §4.7 items 3-5; HealerPlan::group): heals the raid right after each Doom
    };

    // The bot's job in the latched plan; None for a non-healer, a human, or a healer class the plan does not use.
    HealerJob MyHealerJob(Player* bot, LoathebView const& view);

    // Remaining ms of whichever Corrupted Mind lockout aura (CM_LOCKOUT) the unit carries, -1 if none.
    int32_t LockoutRemainingMs(Player* bot);

    // The class big heal the tank rotation casts by name (priest greater heal, paladin holy light, druid healing
    // touch, shaman healing wave); null for any other class. RotationMayCast checks a slot's mana against this spell.
    char const* BigHealSpellName(Player* bot);

    // Spec §4.2: true when this rotation healer is the slot up (RotationUp over the latched rotation) and
    // RotationCastNow holds for view.mainTank; false without a main tank. A slot is ready when it is alive, in the
    // world, has no lockout aura and has the mana for BigHealSpellName. msUntilDue is this bot's estimated wait in ms
    // (0 when it may cast now, UINT32_MAX when it is not in the rotation): one spacing per living slot between the last
    // caster and it, less the time since the last tank heal, never below its own lockout. Under the state mutex.
    bool RotationMayCast(Player* bot, LoathebView const& view, uint32_t& msUntilDue);

    // A rotation heal landed on the main tank: records the rotation clock (last tank heal time + last caster). Ignored
    // for a bot not in the rotation and when no attempt is latched.
    void NoteRotationHeal(Player* bot);

    // A tank heal (rotation or emergency) went off. Feeds the diag's casts=<all> and emerg=<emergency>.
    void NoteTankHealCast(Player* bot, bool emergency);

    // A raid heal (spec §4.7 item 4) went off: feeds the diag's rheals=<prayer>/<chain>/<big>, and topup=<n> when it
    // was the main-tank top-up (spec §4.9 item 1).
    void NoteRaidHealCast(Player* bot, Naxx40LoathebRules::RaidHeal kind, bool topUp);

    // Spec §4.7 item 4: another priest has claimed Prayer of Healing on this raid subgroup for the Doom that resolved
    // last (the bot's own claim does not count). False when no attempt is latched. Read-only, for the trigger.
    bool PartyHealClaimed(Player* bot, uint8_t subgroup);

    // Claims Prayer of Healing on the subgroup for the Doom that resolved last: true when unclaimed or already the
    // bot's own claim, false when another priest holds it or no attempt is latched. Taken right before the cast.
    bool ClaimPartyHeal(Player* bot, uint8_t subgroup);

    // Spec §4.9 item 1: the main-tank top-up for the Doom that resolved last is not this bot's to take: another raid
    // healer has claimed it (the bot's own claim does not count), or this bot's top-up cast was refused during this
    // Doom (ReleaseTankTopUp). False when no attempt is latched. Read-only, for the trigger.
    bool TankTopUpClaimed(Player* bot);

    // Claims the main-tank top-up for the Doom that resolved last: true when unclaimed or already the bot's own claim,
    // false when another raid healer holds it, this bot's top-up was refused during this Doom, or no attempt is
    // latched. Taken right before the cast.
    bool ClaimTankTopUp(Player* bot);

    // The bot's top-up cast was refused: gives back its own claim for the current Doom, so another raid healer can take
    // it, and records the refusal, so this bot skips the top-up for the rest of that Doom's window instead of
    // re-claiming and retrying every tick.
    void ReleaseTankTopUp(Player* bot);

    // Spec §4.9 item 3: the spore the bot should be attacking, else null. engaged = the spore the bot is on now (its
    // victim or "current target"; null or any non-spore unit = none): kept while it is alive and still passes
    // SporeEngages with engaged = true, so two spores reaching the same bots never swap its target mid-kill. Otherwise
    // the nearest live spore (view.spores, 2-D centre to centre) that SporeEngages accepts: within SPORE_STACK_RADIUS
    // of RANGE_SPOT, or alive SPORE_STRAY_MS and within SPORE_ENGAGE of the bot; never within SPORE_TANK_EXCLUDE of
    // Loatheb's current victim. No role test: the caller (Naxx40LoathebAi::SporeToKill) limits it to DPS bots.
    // Resolved live, so the distances are this tick's.
    Creature* EngageSpore(Player* bot, LoathebView const& view, Unit* engaged);

    // Spec §4.3f / §4.6: true, and recorded, the first time this bot asks in the current attempt; false afterwards or
    // when no attempt is latched. Under the state mutex; dropped with the instance state.
    bool ClaimManaRefill(Player* bot);
    bool ClaimHealthstone(Player* bot);

    // The bot's healthstone claim (ClaimHealthstone) was taken this attempt. Read-only.
    bool HealthstoneClaimed(Player* bot);

    // Spec §4.7 item 7 debug: true once per bot per Doom (the latched doom count), so a "healthstone not ready" line is
    // logged at most once per bot per after-Doom window. False when no attempt is latched.
    bool ClaimStoneNote(Player* bot);

    // Counts a used potion / healthstone / bandage for the diag line (None and StepOut are ignored). Call it only once
    // the core accepted the use. A potion also records this bot's potion time for MsSinceLastPotion.
    void NoteConsumable(Player* bot, Naxx40LoathebRules::Consumable c);

    // Ms since this bot's last Shadow Protection Potion this attempt (NoteConsumable(Potion)); UINT32_MAX when none or
    // when no attempt is latched. In combat the core records no potion cooldown, only Player::SetLastPotionId (it
    // refuses every later potion until combat ends), so the strategy times the spec's 2-min potion cooldown itself.
    uint32_t MsSinceLastPotion(Player* bot);

    // Spec §4.2 emergency heal, single-flight: true for one bot per ROTATION_MIN_SPACING_MS per instance (and
    // recorded); false otherwise or when no attempt is latched. Taken right before the cast, never in a trigger.
    bool ClaimEmergencyHeal(Player* bot);
}

#endif
