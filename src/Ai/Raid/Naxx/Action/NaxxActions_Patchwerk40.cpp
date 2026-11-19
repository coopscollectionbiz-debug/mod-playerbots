/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Patchwerk heals, threat hold, tank threat boost and mana refill (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-11-01-playerbot-naxx40-patchwerk-design.md §3.
//
// Return-value rules (memory: an action returning false hands the tick to the default actions): true while the action
// owns the tick (stepping in, stopping to cast, the tick a cast / stop / refill went off); false when it has nothing to
// do or the cast was refused, so stock heals and the rotation run.

#include <algorithm>

#include "Log.h"
#include "Naxx40Loatheb.h"
#include "Naxx40Patchwerk.h"
#include "NaxxActions.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"

using namespace Naxx40PatchwerkRules;

namespace
{
    // Out of HEAL_RANGE the healer walks to the point this far from its target, on the line toward itself.
    constexpr float STEP_IN_DIST = 25.0f;
}

namespace Naxx40PatchwerkAi
{
    Player* HealTarget(Player* bot, Naxx40Patchwerk::PatchwerkView const& view, Naxx40Patchwerk::HealerJob job)
    {
        if (!bot->IsAlive() || job == Naxx40Patchwerk::HealerJob::None ||
            Naxx40Patchwerk::HealerJobOf(bot, view) != job || !Naxx40Loatheb::BigHealSpellName(bot))
            return nullptr;
        if (job == Naxx40Patchwerk::HealerJob::Soaker)
            return Naxx40Patchwerk::SoakerHealTarget(bot, view);
        if (!view.roles.mt)
            return nullptr;
        Player* mt = ObjectAccessor::GetPlayer(bot->GetMap(), ObjectGuid(view.roles.mt));
        // Fix round 1: quiet while the MT is full (PlayerbotAI::UpdateAI interrupts a heal on a full-health target).
        if (!mt || !mt->IsInWorld() || mt->GetMap() != bot->GetMap() || !mt->IsAlive() || mt->IsFullHealth())
            return nullptr;
        return mt;
    }

    bool ThreatHoldDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view)
    {
        if (!bot->IsAlive() || !view.boss || (!bot->GetVictim() && !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)))
            return false;
        AiObjectContext* const context = botAI->GetAiObjectContext();
        if (context->GetValue<bool>("neglect threat")->Get())
            return false;
        if (context->GetValue<Unit*>("current target")->Get() != view.boss)
            return false;
        // Fix round 1: during the opener hold a non-tank already swinging at him stops too (nobody is capped yet).
        if (view.openerHold)
            return !PlayerbotAI::IsTank(bot) && !IsSelfBot(bot);
        Cap const cap = Naxx40Patchwerk::CapOf(bot, view);
        return cap.capped && ThreatStopped(view.boss->GetThreatMgr().GetThreat(bot), cap);
    }

    float ThreatBoostDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view)
    {
        // Bots only, never a self bot, under the `raid` cheat; a living latched role tank (ThreatBoostFor returns 0 for
        // the MT, non-tanks and the dead).
        if (!bot->IsAlive() || IsSelfBot(bot) || !botAI->HasCheat(BotCheatMask::raid) || !view.latched)
            return 0.0f;
        return Naxx40Patchwerk::ThreatBoostOf(bot, view);
    }

    bool ManaRefillDue(Player* bot, PlayerbotAI* botAI, Naxx40Patchwerk::PatchwerkView const& view)
    {
        // Every role healer bot, assigned or stock (spec §3.3: one refill per healer). A latched attempt is required:
        // ClaimManaRefill / ManaRefillClaimed hold no state before it.
        if (!bot->IsAlive() || IsSelfBot(bot) || !botAI->HasCheat(BotCheatMask::raid) || !view.latched ||
            !PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsTank(bot))
            return false;
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        if (!maxMana || 100.0f * float(bot->GetPower(POWER_MANA)) >= MANA_REFILL_BELOW_PCT * float(maxMana))
            return false;
        return !Naxx40Patchwerk::ManaRefillClaimed(bot);
    }
}

// Re-decided at execution (the trigger's pick may be a tick old). Steps in until within HEAL_RANGE and line of sight:
// out of range to STEP_IN_DIST from the target, in range without sight halfway in (re-issued until sight is back).
// Every refusal logs one "heal refused" line (pull 1: a shaman's 565 of 597 executions failed silently).
bool Naxx40PatchwerkHealAction::Execute(Event /*event*/)
{
    Naxx40Patchwerk::PatchwerkView view;
    if (!Naxx40Patchwerk::GetView(bot, view))
    {
        LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: no view", bot->GetName());
        return false;
    }
    Player* const target = Naxx40PatchwerkAi::HealTarget(bot, view, job);
    if (!target)
    {
        LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: no target", bot->GetName());
        return false;
    }
    char const* const spell = Naxx40Loatheb::BigHealSpellName(bot);
    if (!spell)
    {
        LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: no spell name", bot->GetName());
        return false;
    }

    float const d = bot->GetExactDist2d(target);
    if (target != bot)
    {
        if (d > HEAL_RANGE || !bot->IsWithinLOSInMap(target))
        {
            if (!botAI->CanMove())
            {
                LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: can't move (out of range or LOS) to {} "
                          "dist={:.1f}", bot->GetName(), target->GetName(), d);
                return false;
            }
            float const standOff = d > HEAL_RANGE ? STEP_IN_DIST : std::min(STEP_IN_DIST, d * 0.5f);
            float const len = std::max(d, 0.1f);
            float const x = target->GetPositionX() + (bot->GetPositionX() - target->GetPositionX()) / len * standOff;
            float const y = target->GetPositionY() + (bot->GetPositionY() - target->GetPositionY()) / len * standOff;
            bool const moved = MoveTo(bot->GetMapId(), x, y, target->GetPositionZ(), false, false, false, false,
                                      MovementPriority::MOVEMENT_COMBAT, true);
            if (moved || bot->isMoving())
                return true;
            LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: move refused (out of range or LOS) to {} "
                      "dist={:.1f}", bot->GetName(), target->GetName(), d);
            return false;
        }
    }
    // Cast-time heals never start while moving (PlayerbotAI::CanCastSpell): stop now, cast on the next tick.
    if (bot->isMoving())
    {
        bot->StopMoving();
        return true;
    }
    if (!botAI->CastSpell(spell, target))
    {
        // PlayerbotAI::CastSpell drops Spell::prepare's result without a log line; mana is the usual cause.
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} heal refused: CastSpell refused {} on {} dist={:.1f} mana={}%",
                  bot->GetName(), spell, target->GetName(), d,
                  maxMana ? 100 * bot->GetPower(POWER_MANA) / maxMana : 0);
        return false;
    }
    Naxx40Patchwerk::NoteHealCast(bot, target);
    return true;
}

bool Naxx40PatchwerkThreatHoldAction::Execute(Event /*event*/)
{
    LOG_DEBUG("playerbots", "[Naxx40Patchwerk] {} threat hold on {}", bot->GetName(),
              bot->GetVictim() ? bot->GetVictim()->GetName() : "autorepeat");
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    return true;
}

// Pull 1 (spec §3.1): under the `raid` cheat a soaker is lifted to SOAKER_FLOOR x the MT's threat and a spare to
// SPARE_FLOOR x the lower soaker's, raw (no modifiers, no redirects), so a revived or never-struck tank re-enters the
// order and the caps keyed to it hold.
bool Naxx40PatchwerkThreatBoostAction::Execute(Event /*event*/)
{
    Naxx40Patchwerk::PatchwerkView view;
    if (!Naxx40Patchwerk::GetView(bot, view))
        return false;
    float const amount = Naxx40PatchwerkAi::ThreatBoostDue(bot, botAI, view);
    if (amount <= 0.0f)
        return false;
    view.boss->GetThreatMgr().AddThreat(bot, amount, nullptr, true, true);
    Naxx40Patchwerk::NoteThreatBoost(bot);
    LOG_DEBUG("playerbots", "[Naxx40Patchwerk] threat boost {} +{:.0f} ({})", bot->GetName(), amount,
              Naxx40Patchwerk::CapRoleOf(bot, view) == CapRole::Soaker ? "soaker" : "spare");
    return true;
}

// Spec §3.3 (the Huhuran / Horsemen / Loatheb pattern): ONE full refill per role healer bot per attempt.
bool Naxx40PatchwerkManaRefillAction::Execute(Event /*event*/)
{
    Naxx40Patchwerk::PatchwerkView view;
    if (!Naxx40Patchwerk::GetView(bot, view) || !Naxx40PatchwerkAi::ManaRefillDue(bot, botAI, view) ||
        !Naxx40Patchwerk::ClaimManaRefill(bot))
        return false;
    bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    LOG_DEBUG("playerbots", "[Naxx40Patchwerk] mana refill {}", bot->GetName());
    return true;
}
