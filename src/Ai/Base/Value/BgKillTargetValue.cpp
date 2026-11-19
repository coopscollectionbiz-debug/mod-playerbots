// Ai/Base/Value/BgKillTargetValue.cpp
#include "BgKillTargetValue.h"
#include "BattleGroundTactics.h"   // SPELL_CAPTURE_BANNER (21651)
#include "BattlegroundBotsApi.h"
#include "BgKillRank.h"
#include "Battleground.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include <cmath>

namespace
{
    constexpr float kKillRange = 40.0f;
    constexpr float kPeelRange = 15.0f;
    bool IsMeleePlayer(Player* p) { return !PlayerbotAI::IsRangedDps(p, true) && !PlayerbotAI::IsHeal(p, true); }
}

// In AB/AV/EY towers/IC a node is taken by CASTING SPELL_CAPTURE_BANNER on the banner, and ANY
// damage interrupts it. A guard that ignores the channeller loses the node to a single unpunished
// cast, which is exactly what was seen live — so a capturer ranks with the flag carrier (tier 1).
// NB: WS has no capture cast (the flag is a click), and the module API exposes only a bot's own
// jittered GUARD post — not the flag position — so WS gets no equivalent here (a post can sit
// 20y+ and a storey above the flag, so "enemy near my post" is not "enemy on our flag").
static bool IsCapturingNode(Player* p)
{
    Spell* s = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return s && s->m_spellInfo && s->m_spellInfo->Id == SPELL_CAPTURE_BANNER;
}

bool BgKillTargetValue::IsHealerLike(Player* p) const
{
    if (Spell* s = p->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (s->m_spellInfo &&
            PlayerbotAI::IsHealingSpell(s->m_spellInfo->SpellFamilyName, s->m_spellInfo->SpellFamilyFlags))
            return true;
    return PlayerbotAI::IsHeal(p, true);
}

bool BgKillTargetValue::Unreachable(Player* target, std::vector<Player*> const& enemies) const
{
    // No LoS raycast here: LoS is inherited from the source value — "nearest enemy players"
    // (NearestEnemyPlayersValue -> NearestUnitsValue::Calculate) already drops anything the bot
    // cannot see, with ignoreLos defaulted false the whole way down.
    if (std::fabs(bot->GetPositionZ() - target->GetPositionZ()) > 8.0f) return true;
    if (IsMeleePlayer(bot))
    {
        uint32 screen = 0;
        for (Player* e : enemies)
            if (e != target && IsMeleePlayer(e) && e->GetDistance(target) <= 5.0f) ++screen;
        if (screen > 2) return true;
    }
    return false;
}

Unit* BgKillTargetValue::Calculate()
{
    if (!InLiveOpenWorldBg(bot) || !BattlegroundBotsApi::Enabled()) return nullptr;
    BattlegroundBotsApi::BgKnobs const knobs = BattlegroundBotsApi::Knobs();

    // Players only: this vector also feeds the melee-screen test in Unreachable().
    std::vector<Player*> enemies;
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest enemy players"))
    {
        Unit* u = botAI->GetUnit(guid);
        Player* p = u ? u->ToPlayer() : nullptr;
        if (!p || !p->IsAlive() || bot->GetDistance(p) > kKillRange) continue;
        // A player riding a vehicle is NOT targetable — the vehicle is, and the scan below adds it.
        if (p->GetVehicle()) continue;
        enemies.push_back(p);
    }

    // Ranking context (about the BOT, not the candidates). Both halves answer the same live
    // report — bots walking 30y to a healer while a pack beats on them: a bot that is being hit
    // (or already low) stops shopping for healers, and a melee bot's healer radius is short enough
    // that the "kill" is not really a cross-field walk.
    BgKillRank::RankCtx ctx;
    ctx.healerPriority = knobs.healerPriority;
    ctx.underPressure  = bot->getAttackers().size() >= knobs.pressureAttackers ||
                         bot->GetHealthPct() < static_cast<float>(knobs.pressureHpPct);
    ctx.healerMaxDist  = (PlayerbotAI::IsMelee(bot, true) || PlayerbotAI::IsTank(bot, true))
                         ? knobs.healerRangeMelee : knobs.healerRangeRanged;

    Unit* enemyFc = AI_VALUE(Unit*, "enemy flag carrier");
    Unit* teamFc  = AI_VALUE(Unit*, "team flag carrier");
    Unit* victim  = bot->GetVictim();
    ObjectGuid hint = BattlegroundBotsApi::GetTeamKillHint(bot);

    // ONE units vector for both candidate kinds, index-aligned with `cands`: the pick is a Unit*,
    // and AttackBgPriorityTargetAction::Attack() takes any Unit.
    std::vector<Unit*> units;
    std::vector<BgKillRank::Cand> cands;
    units.reserve(enemies.size() + 4);
    cands.reserve(enemies.size() + 4);
    for (Player* p : enemies)
    {
        BgKillRank::Cand c;
        c.isFc = enemyFc && p == enemyFc;
        c.capturing = IsCapturingNode(p);
        c.healer = IsHealerLike(p);
        c.dist = bot->GetDistance(p);
        // Short-circuit on everything that already disqualifies the healer tier (knob off, out of
        // reach, under pressure): the screening scan inside Unreachable() is O(enemies) per healer
        // and must not run on a react tick when its answer cannot change the pick.
        c.unreachable = knobs.healerPriority && c.healer && !ctx.underPressure &&
                        c.dist <= ctx.healerMaxDist && Unreachable(p, enemies);
        c.hpPct = p->GetHealthPct();
        Unit* v = p->GetVictim();
        c.attackingMe = v == bot;
        Player* vp = v ? v->ToPlayer() : nullptr;
        c.attackingProtectee = vp && vp != bot && vp->GetBgTeamId() == bot->GetBgTeamId()
                               && ((teamFc && vp == teamFc) || PlayerbotAI::IsHeal(vp, true));
        c.isHint = !hint.IsEmpty() && p->GetGUID() == hint;
        c.isVictim = victim == p;
        units.push_back(p);
        cands.push_back(c);
    }

    // Hostile siege vehicles (IC demolishers/siege engines/catapults/glaive throwers, and the keep
    // cannons). They are CREATURES and the driver inside is untargetable, so a ranking built from
    // "nearest enemy players" alone cannot see one at all — which is exactly how a demolisher parked
    // on a keep gate went unanswered live. "possible targets" is the engine's own shared, cached
    // unfriendly-unit value (LoS-filtered, AttackersValue::IsPossibleTarget), so this adds no new
    // world query on the react tick.
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "possible targets"))
    {
        Unit* u = botAI->GetUnit(guid);
        // IsCreature() also keeps a player who happens to carry a vehicle kit out of the list twice.
        if (!u || !u->IsCreature() || !u->IsVehicle() || !u->IsAlive()) continue;
        if (!bot->IsValidAttackTarget(u)) continue;
        float const dist = bot->GetDistance(u);
        if (dist > kKillRange) continue;
        BgKillRank::Cand c;
        c.vehicle = true;
        c.attackingMe = u->GetVictim() == bot;   // tier 1 already, but keep the flag honest
        c.hpPct = u->GetHealthPct();
        c.dist = dist;
        c.isHint = !hint.IsEmpty() && u->GetGUID() == hint;
        c.isVictim = victim == u;
        units.push_back(u);
        cands.push_back(c);
    }
    if (cands.empty()) return nullptr;

    int best = BgKillRank::Best(cands, ctx);
    if (best < 0) return nullptr;
    Unit* pick = units[static_cast<std::size_t>(best)];
    BattlegroundBotsApi::ReportKillPick(bot, pick->GetGUID());
    return pick;
}

Unit* BgPeelTargetValue::Calculate()
{
    if (!InLiveOpenWorldBg(bot) || !BattlegroundBotsApi::Enabled()) return nullptr;
    Unit* teamFc = AI_VALUE(Unit*, "team flag carrier");
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest enemy players"))
    {
        Unit* u = botAI->GetUnit(guid);
        Player* p = u ? u->ToPlayer() : nullptr;
        if (!p || !p->IsAlive() || bot->GetDistance(p) > kPeelRange || !IsMeleePlayer(p)) continue;
        Unit* v = p->GetVictim();
        Player* vp = v ? v->ToPlayer() : nullptr;
        if (!vp || vp == bot || vp->GetBgTeamId() != bot->GetBgTeamId()) continue;
        if ((teamFc && vp == teamFc) || PlayerbotAI::IsHeal(vp, true)) return p;
    }
    return nullptr;
}
