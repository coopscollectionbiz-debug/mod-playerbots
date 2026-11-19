/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Patchwerk (Individual Progression's level-60 Naxxramas): encounter gate, per-instance state and the view
// every Patchwerk trigger, action and multiplier reads. Pure rules: Naxx40PatchwerkRules.h. Spec (overlay repo):
// docs/superpowers/specs/2026-11-01-playerbot-naxx40-patchwerk-design.md.

#ifndef PLAYERBOTS_NAXX40PATCHWERK_H
#define PLAYERBOTS_NAXX40PATCHWERK_H

#include <cstdint>
#include <memory>
#include <vector>

#include "Naxx40PatchwerkRules.h"

class Creature;
class Player;
class Spell;
class Unit;

namespace Naxx40Patchwerk
{
    constexpr uint32_t NAXX40_MAP_ID = 533;

    // A view is valid for the current tick only: boss and any Player* taken from it must never be stored across
    // ticks. Copied out of the per-instance state under the mutex; re-read it each tick instead.
    struct PatchwerkView
    {
        Creature* boss = nullptr;  // alive, non-null whenever GetView returned true
        // The tank priority and the healer split are latched (the first refresh where a latched-eligible role tank is
        // his victim). Before that, roles / tanks / healers are empty and nobody is capped or assigned.
        bool latched = false;
        // Non-tanks hold fire: true until the latch, failing open OPENER_FAIL_OPEN_MS after the gate first opened.
        bool openerHold = true;
        Naxx40PatchwerkRules::Roles roles;
        // Hateful Strike's current targets (StrikeCandidates over his sorted threat list). Either may be a pet or 0.
        Naxx40PatchwerkRules::StrikePair pair;
        // One entry per latched tank, in priority order, as of the last refresh (alive, threat, max health, melee).
        std::vector<Naxx40PatchwerkRules::TankInfo> tanks;
        // The latched healer split (bots only), shared and immutable; null before the latch.
        std::shared_ptr<Naxx40PatchwerkRules::HealerSplit const> healers;
    };

    // false = strategy inert: not map 533, not RAID_DIFFICULTY_10MAN_HEROIC (IP's Naxx40), the instance's
    // BOSS_PATCHWERK (0) not IN_PROGRESS, or no living Patchwerk (351028). On a failed gate the instance's state
    // is dropped, so the next pull re-latches from scratch; so does an instance entry not refreshed for 10 s.
    // Refreshes the shared state (threat list, strike pair, roles, damage samples, heal claims, diag) at most every
    // REFRESH_INTERVAL_MS. True before the latch too (view.latched false), so the opener hold can run.
    bool GetView(Player* bot, PatchwerkView& view);

    // None for a human or self bot, before the latch, for a healer (PlayerbotAI::IsHeal, not a latched tank) and for
    // the MT; Soaker / Spare for the latched tanks in those roles; otherwise MeleeDps or RangedDps
    // (PlayerbotAI::IsRanged).
    Naxx40PatchwerkRules::CapRole CapRoleOf(Player* bot, PatchwerkView const& view);

    // The bot's cap (Naxx40PatchwerkRules::CapFor), with the latched tanks' threat re-read live from the boss so the
    // reference is this tick's, not the last refresh's.
    Naxx40PatchwerkRules::Cap CapOf(Player* bot, PatchwerkView const& view);

    enum class HealerJob : uint8_t
    {
        None,
        Soaker,   // heals the current strike pair
        MainTank  // heals the MT
    };

    // The bot's job in the latched healer split; None for a human, a self bot, a stock healer or before the latch.
    HealerJob HealerJobOf(Player* bot, PatchwerkView const& view);

    // The strike-pair member this soaker healer should heal now (Naxx40PatchwerkRules::PickSoakerTarget: fewer claimed
    // in-flight heals, the bot's own claim excluded, then lower health %). Living players on the bot's map count, full
    // health included (pull 1: soaker healers pre-cast; KeepPrecastHeal stops the engine interrupting the heal); null
    // when the pair holds none. Read-only: the claim is taken by NoteHealCast once the cast starts.
    Player* SoakerHealTarget(Player* bot, PatchwerkView const& view);

    // PlayerbotAI::UpdateAI asks before interrupting a single-target heal whose target is at full health: true keeps a
    // soaker healer's heal on a strike-pair member (the strike lands within the cast). Cheap: map id first, then the
    // cached view (latched only), the bot's healer job and the pair's raw guids.
    bool KeepPrecastHeal(Player* bot, Unit* target);

    // PlayerbotAI::UpdateAI asks every react tick while a single-target heal is casting: true (and counted in the
    // diag's `cancels=`) when this soaker healer's heal is about to land on a target still at PRECAST_CANCEL_PCT health
    // or more (Naxx40PatchwerkRules::ShouldCancelPrecast). Mana is only spent at cast completion, so the interrupt is
    // free. Any target, pair member or not. Cheap: map id first, then the cached view (latched only) and the job.
    bool CancelPrecastHeal(Player* bot, Spell* spell, Unit* target);

    // A heal cast started: claims (bot, target) until a refresh finds the bot no longer casting at that target.
    void NoteHealCast(Player* bot, Player* target);

    // The threat the `raid` cheat should add to this bot now (Naxx40PatchwerkRules::ThreatBoostFor over the latched
    // tanks' threat re-read live from the boss, as CapOf does); 0 when none is due or the attempt is not latched.
    float ThreatBoostOf(Player* bot, PatchwerkView const& view);

    // Counts one threat boost in the current attempt (the diag's `boosts=`). Under the state mutex.
    void NoteThreatBoost(Player* bot);

    // True, and recorded, the first time this bot asks in the current attempt; false afterwards or when no attempt is
    // latched. Under the state mutex; dropped with the instance state.
    bool ClaimManaRefill(Player* bot);

    // Read-only, for the trigger: this bot's refill was already claimed this attempt (true when none is latched, so the
    // trigger stays quiet).
    bool ManaRefillClaimed(Player* bot);
}

#endif
