/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Four Horsemen tank pairs, DPS halves and healer groups (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-27-playerbot-naxx40-four-horsemen-design.md §4.1-§4.3.
//
// Return-value rules (memory: an action returning false hands the tick to the default actions; one returning true
// stops the engine for the tick, so a hold that never ends starves the rotation):
//   - true while the bot is travelling to its spot (it issued a leg, or MoveTo refused a duplicate of the leg it is
//     still running) and on the tick a taunt / Disarm / engage lands;
//   - false once it stands on its spot, so the tank rotation (Sunder, Revenge, buffs) keeps running. The strategy's
//     Naxx40HorsemenMultiplier vetoes tank face, follow, formation, reach melee / charge and the generic target
//     choosers for every latched bot (NaxxMultipliers.cpp), so falling through never drags a bot off its spot.

#include <array>
#include <cmath>
#include <list>
#include <string>
#include <vector>

#include "Creature.h"
#include "Log.h"
#include "Naxx40Horsemen.h"
#include "Naxx40HorsemenStations.h"
#include "NaxxActions.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotTextMgr.h"
#include "ThreatManager.h"

using namespace Naxx40HorsemenRules;

namespace
{
    constexpr float SPOT_ARRIVE = 5.0f;      // a spot is reached within this (bots settle a couple of yards off)
    constexpr float BLAU_ARRIVE = 2.5f;      // Blaumeux's points are 5 y apart (tools/naxx40-horsemen-stations.py)
    constexpr float TAUNT_TRY_RANGE = 30.0f; // try the class taunt from here (Taunt / Growl / HoR / Dark Command)
    constexpr float ENGAGE_APPROACH = 4.0f;  // walk in to this far from the horseman while the taunt fails
    constexpr float FORCED_LEAVE_RANGE = 10.0f;  // the relief counts as "on him" within this of the horseman
    constexpr uint8_t FORCED_LEAVE_ANYWAY = LEAVE_AT_STACKS + 3;  // a relief never next on threat: leave at this
    constexpr float VOID_STANDING_IN = 3.5f;     // Consumption's 2 y around a zone, plus the player's 1.5 y reach
    constexpr float FACE_ARC = static_cast<float>(M_PI) / 2.0f;
    constexpr float MELEE_ARRIVE = 2.5f;     // melee follow a spot MELEE_BEHIND (3 y) behind the boss: a tight fit
    constexpr float MELEE_REPATH = 2.0f;     // re-issue the melee leg once its spot moved this far (boss dragged)
    constexpr float ESCORT_REPATH = 4.0f;    // re-issue the escort leg once the escorted tank moved this far
    constexpr float VOID_SEARCH = 12.0f;     // Void Zones within this of Blaumeux can reach a melee spot (3 y + 5 y)
    constexpr float VOID_SEARCH_FROM = 20.0f;  // search only once the bot is within this of Blaumeux
    constexpr uint8 EDGE_READY = 1;
    constexpr uint8 EDGE_RELIEVE = 2;

    char const* DutyName(TankDuty d)
    {
        switch (d)
        {
            case TankDuty::Hold: return "hold";
            case TankDuty::Ready: return "ready";
            case TankDuty::Relieve: return "relieve";
            case TankDuty::Leave: return "leave";
            case TankDuty::Rest: return "rest";
        }
        return "?";
    }

    char const* HorseTag(Horse h)
    {
        static char const* const TAG[4] = {"K", "M", "B", "Z"};
        return h == Horse::None ? "-" : TAG[Idx(h)];
    }

    // A live Void Zone (Blaumeux) within VOID_CLEAR of (x, y). Searched from the bot, which stands near the point
    // (the tank walks Blaumeux around her own corner), so the grid search is well inside its 533 y clamp.
    bool VoidZoneNear(Player* bot, float x, float y, float radius = VOID_CLEAR)
    {
        std::list<Creature*> zones;
        bot->GetCreatureListWithEntryInGrid(zones, VOID_ZONE_ENTRY, bot->GetExactDist2d(x, y) + radius);
        for (Creature* c : zones)
            if (c && c->IsInWorld() && c->IsAlive() && c->GetExactDist2d(x, y) < radius)
                return true;
        return false;
    }

    // Blaumeux's current tank point (shared per instance), read-only.
    Naxx40HorsemenStations::Spot const& BlaumeuxPoint(Player* bot)
    {
        return Naxx40HorsemenStations::BLAU_POINTS[Naxx40Horsemen::BlauCircleIndex(bot) %
                                                   Naxx40HorsemenStations::BLAU_POINT_COUNT];
    }

    // Blaumeux's tanking spot: the current point, moved (shared per instance) by PickBlauPoint when a Void Zone blocks
    // it. She casts the zone on her victim, so it lands under the tank: a zone near the tank while he stands on the
    // current point blocks it too (pull 2). One grid search from the bot covers every point (the relief at the ready
    // spot is ~75 y from the farthest, well inside the 533 y clamp).
    Naxx40HorsemenStations::Spot BlaumeuxSpot(Player* bot)
    {
        using Naxx40HorsemenStations::BLAU_POINTS;
        using Naxx40HorsemenStations::BLAU_POINT_COUNT;
        std::vector<Pt> points;
        float reach = 0.0f;
        for (Naxx40HorsemenStations::Spot const& s : BLAU_POINTS)
        {
            points.push_back(Pt{s.x, s.y});
            reach = std::max(reach, bot->GetExactDist2d(s.x, s.y));
        }
        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, VOID_ZONE_ENTRY, reach + VOID_CLEAR);
        std::vector<Pt> zones;
        for (Creature* c : found)
            if (c && c->IsInWorld() && c->IsAlive())
                zones.push_back(Pt{c->GetPositionX(), c->GetPositionY()});

        size_t const cur = Naxx40Horsemen::BlauCircleIndex(bot) % BLAU_POINT_COUNT;
        bool const onCur = bot->GetExactDist2d(BLAU_POINTS[cur].x, BLAU_POINTS[cur].y) <= SPOT_ARRIVE;
        size_t const idx =
            PickBlauPoint(points, cur, Pt{bot->GetPositionX(), bot->GetPositionY()}, onCur, zones);
        if (idx != cur)
        {
            Naxx40Horsemen::SetBlauCircleIndex(bot, static_cast<uint8>(idx));
            LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} walks Blaumeux to point {} (void zone on {}, {} zones)",
                      bot->GetName(), idx, cur, zones.size());
        }
        return BLAU_POINTS[idx];
    }

    // Live Void Zones within VOID_SEARCH of the horseman. Searched from the bot (melee, near the boss), well inside the
    // grid search's 533 y clamp.
    std::vector<Pt> VoidZonesNear(Player* bot, Creature* horse)
    {
        std::vector<Pt> out;
        std::list<Creature*> zones;
        bot->GetCreatureListWithEntryInGrid(zones, VOID_ZONE_ENTRY, bot->GetExactDist2d(horse) + VOID_SEARCH);
        for (Creature* c : zones)
            if (c && c->IsInWorld() && c->IsAlive() && c->GetExactDist2d(horse) <= VOID_SEARCH)
                out.push_back(Pt{c->GetPositionX(), c->GetPositionY()});
        return out;
    }

    // Stop swinging / auto-repeat and clear the target; a pet still attacking is called back to follow (it would
    // otherwise keep hitting the horseman this bot left, and its threat / marks are the owner's problem). The
    // multiplier vetoes stock "pet attack" meanwhile. True when there was something to stop.
    bool StopAttacking(Player* bot, PlayerbotAI* botAI, AiObjectContext* context)
    {
        Unit* const current = context->GetValue<Unit*>("current target")->Get();
        Pet* const pet = bot->GetPet();
        bool const petBusy = pet && pet->IsAlive() && pet->GetVictim();
        if (!bot->GetVictim() && !current && !petBusy)
            return false;
        if (bot->GetVictim() || current)
        {
            bot->AttackStop();
            bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            context->GetValue<Unit*>("current target")->Set(nullptr);
        }
        if (petBusy)
            botAI->PetFollow();
        return true;
    }

    // Go to (x, y, z) and stand there. True only while travelling; within SPOT_ARRIVE it turns to faceTo (if any) and
    // falls through. `leg` issues one MOVEMENT_COMBAT MoveTo (MovementAction::MoveTo is protected, so the action
    // passes it in as a lambda).
    template <typename LegFn>
    bool StandAt(Player* bot, PlayerbotAI* botAI, Naxx40HorsemenStations::Spot const& s, Unit* faceTo, LegFn leg)
    {
        if (bot->GetExactDist2d(s.x, s.y) > SPOT_ARRIVE)
        {
            if (!botAI->CanMove())
                return false;
            return leg(s.x, s.y, s.z) || bot->isMoving();
        }
        if (faceTo && !bot->isMoving() && !bot->HasInArc(FACE_ARC, faceTo))
            bot->SetFacingToObject(faceTo);
        return false;
    }

    // A latched druid tank stays in bear form: the multiplier vetoes "caster form", and this puts back a druid
    // that is out of it (a form lost to death, a druid seated from a healer spec) so its Growl works on its turn.
    bool KeepBearForm(Player* bot, PlayerbotAI* botAI, Event event)
    {
        if (bot->getClass() != CLASS_DRUID || botAI->HasAnyAuraOf(bot, "dire bear form", "bear form", nullptr))
            return false;
        return botAI->DoSpecificAction("dire bear form", event, true) ||
               botAI->DoSpecificAction("bear form", event, true);
    }

    Player* PartnerOf(Player* bot, Naxx40Horsemen::HorsemenView const& view, Horse h, bool iAmActive)
    {
        uint64 const raw = iAmActive ? view.assign->resting[Idx(h)] : view.assign->active[Idx(h)];
        if (!raw)
            return nullptr;
        Player* p = ObjectAccessor::GetPlayer(bot->GetMap(), ObjectGuid(raw));
        return p && p->IsInWorld() && p->GetMap() == bot->GetMap() && p->IsAlive() ? p : nullptr;
    }
}

namespace
{
    // Final review I1: a forced leave wipes the leaving tank's threat, and the horseman then takes the next on his
    // threat list. True when that is the relief: it holds threat and nobody else (DPS and pets included) has more.
    bool ReliefIsNext(Creature* horse, Unit* leaving, Unit* relief)
    {
        ThreatManager& mgr = horse->GetThreatMgr();
        float const reliefThreat = mgr.GetThreat(relief);
        if (reliefThreat <= 0.0f)
            return false;
        for (ThreatReference const* ref : mgr.GetUnsortedThreatList())
        {
            Unit* const who = ref->GetVictim();
            if (who != leaving && who != relief && ref->GetThreat() > reliefThreat)
                return false;
        }
        return true;
    }
}

bool Naxx40HorsemenTankAction::Execute(Event event)
{
    Naxx40Horsemen::HorsemenView view;
    if (!Naxx40Horsemen::GetView(bot, view))
        return false;
    bool iAmActive = false;
    Horse const h = Naxx40Horsemen::MyPair(bot, view, iAmActive);
    auto const leg = [this](float x, float y, float z)
    {
        return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    };

    // A spare tank (beyond 8, or freed by a dead horseman) waits on the safe spot, off every horseman; it steps in
    // only when RefillPairs seats it in a pair.
    if (h == Horse::None)
    {
        if (!Naxx40Horsemen::IsSpareTank(bot, view))
            return false;
        if (!lastSpare)
        {
            LOG_DEBUG("playerbots", "[Naxx40Horsemen] tank {} spare -> safe spot", bot->GetName());
            lastSpare = true;
            lastDutyValid = false;  // the next pair seat logs its first duty
        }
        if (KeepBearForm(bot, botAI, event))
            return true;
        bool const stopped = StopAttacking(bot, botAI, context);
        return StandAt(bot, botAI, Naxx40HorsemenStations::SAFE_SPOT, nullptr, leg) || stopped;
    }
    if (!view.alive[Idx(h)])
        return false;
    lastSpare = false;
    if (KeepBearForm(bot, botAI, event))
        return true;
    Creature* const horse = view.horse[Idx(h)];

    Player* const partner = PartnerOf(bot, view, h, iAmActive);
    uint8 const myStacks = Naxx40Horsemen::MarkStacks(bot, h);
    uint8 const partnerStacks = partner ? Naxx40Horsemen::MarkStacks(partner, h) : 0;
    // TankDutyFor's victim slot: for the ACTIVE tank it means "still mine" = anyone but the partner has him, so a DPS
    // who ripped aggro (or no victim at all) keeps Hold and Engage taunts him back; only the partner's taunt is a
    // handover. For the resting tank it is the plain victim test.
    // A partner that may not take him (TakesOver: 3+ of his marks) holding him is not a handover either.
    bool const victimSlot = iAmActive ? horse->GetVictim() != partner || !TakesOver(partnerStacks)
                                      : horse->GetVictim() == bot;
    TankDuty duty = TankDutyFor(iAmActive, myStacks, victimSlot, partner != nullptr, partnerStacks);

    // A relief without a taunt (cooldown, a Vanilla paladin) never takes him by taunt: once the 4th mark has landed
    // and the partner is on the horseman (within FORCED_LEAVE_RANGE), the active tank leaves anyway and the threat
    // passes to the relief that has been hitting him. Only once the relief is next on his threat list, else a DPS
    // inherits him and runs him across the room (final review I1); at FORCED_LEAVE_ANYWAY marks it leaves regardless.
    bool forcedLeave = false;
    if (duty == TankDuty::Hold && iAmActive && myStacks >= LEAVE_AT_STACKS + 1 && partner &&
        partner->GetExactDist2d(horse) <= FORCED_LEAVE_RANGE &&
        (myStacks >= FORCED_LEAVE_ANYWAY || ReliefIsNext(horse, bot, partner)))
    {
        duty = TankDuty::Leave;
        forcedLeave = true;
    }

    if (!lastDutyValid || duty != lastDuty)
    {
        LOG_DEBUG("playerbots", "[Naxx40Horsemen] tank {} {} {} -> {}{} (stacks {}, partner {} stacks {}, victim {})",
                  bot->GetName(), HorseTag(h), lastDutyValid ? DutyName(lastDuty) : "none", DutyName(duty),
                  forcedLeave ? " (forced: 4th mark, relief on him)" : "", myStacks,
                  partner ? partner->GetName() : "none", partnerStacks,
                  horse->GetVictim() ? horse->GetVictim()->GetName() : "none");
        lastDuty = duty;
        lastDutyValid = true;
    }

    // Spec §4.1: the bot in a human's paired slot tells them when it is their turn. Only the active bot does it, and
    // only to a real player (no PlayerbotAI at all); a bot partner reads the same state itself.
    if (iAmActive && partner && IsRealPlayer(partner))
        WhisperPartner(h, horse, partner, myStacks);

    switch (duty)
    {
        case TankDuty::Hold:
            return Hold(event, h, horse, partner);
        case TankDuty::Relieve:
        {
            // Walk onto the tank spot beside the active tank first (ReliefTauntsNow), so the taunt turns him in place.
            Naxx40HorsemenStations::Spot const spot = h == Horse::Blaumeux ? BlaumeuxSpot(bot)
                                                                          : Naxx40HorsemenStations::HORSE[Idx(h)].tankSpot;
            if (horse->GetVictim() != bot &&
                !ReliefTauntsNow(bot->GetExactDist2d(spot.x, spot.y), horse->GetExactDist2d(spot.x, spot.y),
                                 SPOT_ARRIVE))
                return StandAt(bot, botAI, spot, horse, leg);
            return Engage(event, horse);
        }
        case TankDuty::Ready:
        {
            StopAttacking(bot, botAI, context);
            DropThreat(horse);
            return StandAt(bot, botAI, Naxx40HorsemenStations::HORSE[Idx(h)].readySpot, horse, leg);
        }
        case TankDuty::Leave:
        case TankDuty::Rest:
        {
            // Leave: stop attacking and drop the target so nothing re-acquires the horseman and his threat cannot
            // bounce back onto this tank (memory: a taunt only matches top threat). Rest: the same, held at the spot.
            bool const stopped = StopAttacking(bot, botAI, context);
            DropThreat(horse, forcedLeave);
            return StandAt(bot, botAI, Naxx40HorsemenStations::SAFE_SPOT, nullptr, leg) || stopped;
        }
    }
    return false;
}

// Active tank: keep the horseman and walk it to its tanking spot; taunt it back when something else has it.
bool Naxx40HorsemenTankAction::Hold(Event event, Horse h, Creature* horse, Player* partner)
{
    if (horse->GetVictim() != bot)
    {
        // The resting partner already took him (a human taunting early): never contest it. The pair flips on the
        // next view refresh and this bot becomes the resting tank. A partner still carrying 3+ of his marks got him
        // back on unhalved old threat (TakesOver), and is taunted off.
        if (partner && horse->GetVictim() == partner && TakesOver(Naxx40Horsemen::MarkStacks(partner, h)))
            return false;
        return Engage(event, horse);
    }

    // Start the swing once; never re-issue Attack() every tick (memory: Attack cancels movement / re-targets). Attack
    // sets "current target" even when it waits to swing, so that counts as accepted too.
    if (bot->GetVictim() != horse && AI_VALUE(Unit*, "current target") != horse)
        Attack(horse);

    Naxx40HorsemenStations::Spot spot = h == Horse::Blaumeux ? BlaumeuxSpot(bot)
                                                              : Naxx40HorsemenStations::HORSE[Idx(h)].tankSpot;
    // Pull 5 diagnostic: Blaumeux's tank died twice at 2 marks with two healers on him. Log the edge of standing in
    // a Void Zone's Consumption (2 y, plus the player's reach).
    if (h == Horse::Blaumeux)
    {
        bool const nowInVoid = VoidZoneNear(bot, bot->GetPositionX(), bot->GetPositionY(), VOID_STANDING_IN);
        if (nowInVoid && !inVoid)
            LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} stands in a Void Zone ({:.1f} y from point {}, {:.0f}% hp)",
                      bot->GetName(), bot->GetExactDist2d(spot.x, spot.y),
                      Naxx40Horsemen::BlauCircleIndex(bot) % Naxx40HorsemenStations::BLAU_POINT_COUNT,
                      bot->GetHealthPct());
        inVoid = nowInVoid;
    }
    if (bot->GetExactDist2d(spot.x, spot.y) > (h == Horse::Blaumeux ? BLAU_ARRIVE : SPOT_ARRIVE))
        return MoveToStation(spot.x, spot.y, spot.z, horse);

    // On the station. Warriors keep Disarm on him (needs Defensive Stance under stock rules; CanCastSpell is false
    // otherwise, or on cooldown, and the tick falls through to the rotation).
    if (bot->getClass() == CLASS_WARRIOR && bot->IsWithinMeleeRange(horse) && botAI->CanCastSpell("disarm", horse) &&
        botAI->CastSpell("disarm", horse))
        return true;

    // Tank face is vetoed for paired tanks, so turn in place (the AQ40 / BWL station rule).
    if (!bot->isMoving() && !bot->HasInArc(FACE_ARC, horse))
        bot->SetFacingToObject(horse);
    return false;
}

// Under the raid cheat, wipe this off-duty tank's threat on its horseman (the upstream ICC taunt precedent). Every mark
// halves the threat of the units it hits and this tank stands outside it, so its old threat would otherwise pull the
// horseman back across the room (TakesOver). Never while he is on this tank, except on a forced leave (the relief is
// on him): in pull 5 Korth'azz followed his 4-mark tank toward the safe spot and took a DPS on the way.
void Naxx40HorsemenTankAction::DropThreat(Creature* horse, bool evenIfVictim)
{
    if (!botAI->HasCheat(BotCheatMask::raid) || (horse->GetVictim() == bot && !evenIfVictim))
        return;
    ThreatManager& mgr = horse->GetThreatMgr();
    if (mgr.GetThreat(bot) <= 0.0f)
        return;
    LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} drops its threat on {} ({:.0f})", bot->GetName(), horse->GetName(),
              mgr.GetThreat(bot));
    mgr.ResetThreat(bot);
}

// Get the horseman onto this tank: the class taunt ("taunt spell": Taunt / Growl / Hand of Reckoning / Dark Command,
// whatever the bot's tank strategy registers) from TAUNT_TRY_RANGE, walking in while it fails; in melee without a
// taunt (cooldown, or a Vanilla paladin who has none) it simply attacks and builds threat.
bool Naxx40HorsemenTankAction::Engage(Event event, Creature* horse)
{
    if (horse->GetVictim() == bot)
    {
        if (bot->GetVictim() != horse)
            Attack(horse);
        return false;  // the pair flips on the next refresh; meanwhile the rotation runs
    }

    // "taunt spell" casts on the current target.
    if (AI_VALUE(Unit*, "current target") != horse)
        context->GetValue<Unit*>("current target")->Set(horse);

    float const dist = bot->GetExactDist2d(horse);
    if (dist <= TAUNT_TRY_RANGE && botAI->DoSpecificAction("taunt spell", event, true))
    {
        LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} taunts {} (victim was {})", bot->GetName(), horse->GetName(),
                  horse->GetVictim() ? horse->GetVictim()->GetName() : "none");
        if (bot->GetVictim() != horse)
            Attack(horse);
        return true;
    }

    if (dist > ENGAGE_APPROACH + 1.0f && botAI->CanMove())
    {
        // A point on the horseman-to-bot line, ENGAGE_APPROACH out from him.
        float const k = ENGAGE_APPROACH / dist;
        float const x = horse->GetPositionX() + (bot->GetPositionX() - horse->GetPositionX()) * k;
        float const y = horse->GetPositionY() + (bot->GetPositionY() - horse->GetPositionY()) * k;
        bool const moved = MoveTo(bot->GetMapId(), x, y, horse->GetPositionZ(), false, false, false, false,
                                  MovementPriority::MOVEMENT_COMBAT, true);
        if (moved || bot->isMoving())
            return true;
    }

    // In melee (or the walk was refused): attack once to build threat, then let the rotation run.
    if (bot->GetVictim() != horse)
        return Attack(horse);
    return false;
}

// Walk the held horseman to its station: backwards when the step leads away from him, so the tank keeps facing him
// (the BWL Nefarian / AQ40 twins station rule).
bool Naxx40HorsemenTankAction::MoveToStation(float x, float y, float z, Creature* horse)
{
    if (!botAI->CanMove())
        return false;
    float const toStepX = x - bot->GetPositionX();
    float const toStepY = y - bot->GetPositionY();
    float const toBossX = horse->GetPositionX() - bot->GetPositionX();
    float const toBossY = horse->GetPositionY() - bot->GetPositionY();
    bool const backwards = toStepX * toBossX + toStepY * toBossY < 0.0f;
    bool const moved = MoveTo(bot->GetMapId(), x, y, z, false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true, backwards);
    // A refused duplicate of the leg still running is still travelling: keep the tick (see the file comment).
    return moved || bot->isMoving();
}

// Spec §4.1: whisper the human resting partner on the edges of THEIR duty (computed exactly as their bot would):
// Ready -> "Get ready for <Horse> — next mark", Relieve -> "Taunt <Horse> now". Once per swap each
// (ClaimWhisperEdge is keyed on the current active tank, so a flip starts over).
void Naxx40HorsemenTankAction::WhisperPartner(Horse h, Creature* horse, Player* partner, uint8_t myStacks)
{
    TankDuty const theirs = TankDutyFor(false, Naxx40Horsemen::MarkStacks(partner, h), horse->GetVictim() == partner,
                                        true, myStacks);
    uint8 edge = 0;
    if (theirs == TankDuty::Relieve)
        edge = EDGE_RELIEVE;
    else if (theirs == TankDuty::Ready)
        edge = EDGE_READY;
    if (!edge || !Naxx40Horsemen::ClaimWhisperEdge(bot, h, bot->GetGUID().GetRawValue(), edge))
        return;
    // Translatable bot text (module rule); no bot-text row ships, so the default wording is what is sent.
    std::string const msg = edge == EDGE_RELIEVE
        ? PlayerbotTextMgr::instance().GetBotTextOrDefault("naxx40_horsemen_taunt_now", "Taunt %horse now",
                                                           {{"%horse", horse->GetName()}})
        : PlayerbotTextMgr::instance().GetBotTextOrDefault("naxx40_horsemen_get_ready",
                                                           "Get ready for %horse \xE2\x80\x94 next mark",
                                                           {{"%horse", horse->GetName()}});
    botAI->Whisper(msg, partner->GetName());
    LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} whispers {}: {}", bot->GetName(), partner->GetName(), msg);
}

// Spec §4.2-§4.3: each DPS half and healer group works the horseman Choose picks from its order and this bot's own mark
// stacks (nobody takes a 4th). None = the safe spot (DPS stop attacking; healers just stand there). DPS attack only
// that horseman: melee from MeleeSpot (behind him, off his active tank; Korth'azz's melee all share it, so they stack
// for Meteor), turned out of Blaumeux's Void Zones, and never on Zeliek; ranged from rangedSpot. Healers stand on
// healerSpot; their tank heal is the separate "naxx40 horsemen tank heal" action, the rest is stock healing. True only
// while travelling (and on the tick Attack starts); false on the spot so the rotation / heals run.
bool Naxx40HorsemenRaidAction::Execute(Event /*event*/)
{
    Naxx40Horsemen::HorsemenView view;
    if (!Naxx40Horsemen::GetView(bot, view))
        return false;
    uint8 const half = Naxx40Horsemen::MyDpsHalf(bot, view);
    uint8 const group = half == 0xFF ? Naxx40Horsemen::MyHealerGroup(bot, view) : 0xFF;
    if (half == 0xFF && group == 0xFF)
        return false;
    bool const healer = half == 0xFF;

    // Spec §4.3f: Mograine's Unholy Shadow (~7.2k every 15 s on his tank, undispellable) and Korth'azz's Meteor drain
    // the front healers in a fight they cannot drink in. Each healer bot gets ONE full refill below 25 % under the
    // `raid` cheat, per attempt (a continuous regen was tried on Nefarian and gave effectively infinite mana).
    if (healer && bot->IsAlive() && !IsSelfBot(bot) && botAI->HasCheat(BotCheatMask::raid))
    {
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        if (maxMana && 100.0f * float(bot->GetPower(POWER_MANA)) < MANA_REFILL_BELOW_PCT * float(maxMana) &&
            Naxx40Horsemen::ClaimManaRefill(bot))
        {
            bot->SetPower(POWER_MANA, maxMana);
            LOG_DEBUG("playerbots", "[Naxx40Horsemen] mana refill {} (healer group {})", bot->GetName(), group);
        }
    }

    // The front gate keeps DPS and the group-4 floater (which follows DPS_ORDER[0]) off the phase-2 pair until a front
    // horseman dies; healer groups 0-3 work Blaumeux / Zeliek from the pull.
    // Melee DPS never work Zeliek (Holy Wrath's 10 y chain reaches every melee in his reach): WithoutZeliek.
    Order const order = healer ? HealerOrder(group, view.alive)
                       : PlayerbotAI::IsRanged(bot) ? DPS_ORDER[half] : WithoutZeliek(DPS_ORDER[half]);
    bool const frontGate = !healer || group == 4;
    std::array<uint8_t, 4> stacks{};
    for (uint8 i = 0; i < 4; ++i)
        stacks[i] = Naxx40Horsemen::MarkStacks(bot, static_cast<Horse>(i));
    // Spec §4.2a: hold a switch until the horseman being left and the one being joined are both settled on a fresh
    // active tank (HoldSwitch), so healers keep healing the 3-stack tank and nobody crosses the room mid-swap.
    std::array<bool, 4> settled{};
    for (uint8 i = 0; i < 4; ++i)
    {
        // A dead horseman's pair stays in the assignment until the next refresh, so a tank can exist while horse[i] is
        // null (pull 9: the first horseman kill crashed the world here).
        Player* const tank = view.alive[i] ? Naxx40Horsemen::ActiveTank(bot, view, static_cast<Horse>(i)) : nullptr;
        settled[i] = HorseSettled(view.alive[i], tank != nullptr, tank && view.horse[i]->GetVictim() == tank,
                                  tank ? Naxx40Horsemen::MarkStacks(tank, static_cast<Horse>(i)) : 0);
    }
    Horse const old = Naxx40Horsemen::CurrentHorse(bot);
    // Spec §4.3d: the two healers of a group leave their first horseman on marks 1 and 2, then stay one mark apart.
    // Spec §4.2d: a DPS half's seats leave theirs on marks 2 and 3, so Korth'azz always keeps melee for Meteor.
    uint8_t const leaveAt = healer
        ? (group < 4 ? HealerLeaveAt(Naxx40Horsemen::MyHealerSeat(bot, view), !Naxx40Horsemen::HasMoved(bot))
                     : LEAVE_AT_STACKS)
        : DpsLeaveAt(Naxx40Horsemen::MyDpsSeat(bot, view), !Naxx40Horsemen::HasMoved(bot));
    // Spec §4.2f: with IP's weak marks, the last front horseman keeps the whole front until he dies.
    Horse const uncapped = Naxx40Horsemen::DoableMarks() ? LastFront(view.alive) : Horse::None;
    Horse const pick = Choose(order, old, stacks, view.alive, frontGate, leaveAt, uncapped);
    Horse h = HoldSwitch(old, pick, stacks, settled);
    if (h != pick && !switchHeld)
        LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} holds {} -> {} until settled (stacks K{} M{} B{} Z{})",
                  bot->GetName(), HorseTag(old), HorseTag(pick), stacks[0], stacks[1], stacks[2], stacks[3]);
    switchHeld = h != pick;
    // Spec §4.3a: the last healer on a corner stays until another healer stands in reach of its tank.
    bool coverHold = false;
    if (healer && h != old && old != Horse::None)
    {
        Player* const tank = Naxx40Horsemen::ActiveTank(bot, view, old);
        coverHold = HealerMustStay(view.alive[Idx(old)], tank != nullptr,
                                   Naxx40Horsemen::OtherHealerCovers(bot, view, old, tank), stacks[Idx(old)]);
        if (coverHold)
        {
            if (!healerHeld)
                LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} stays on {} until another healer reaches {}",
                          bot->GetName(), HorseTag(old), tank->GetName());
            h = old;
        }
    }
    healerHeld = coverHold;
    if (h != old)
    {
        Naxx40Horsemen::SetCurrentHorse(bot, h);
        LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} {} {} {} -> {} (stacks K{} M{} B{} Z{})", bot->GetName(),
                  healer ? "heal" : "dps", healer ? group : half, HorseTag(old), HorseTag(h), stacks[0], stacks[1],
                  stacks[2], stacks[3]);
    }

    auto const leg = [this](float x, float y, float z)
    {
        return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    };

    // Spec §4.2b: a horseman on this bot (it pulled threat, or his tank died) is brought back to his tank: the bot stops
    // attacking and runs to his active tank (his tank spot when he has none), so the relief closes the taunt range from
    // both sides instead of chasing a victim across the room (pull 1: 40-75 s loose).
    for (uint8 i = 0; i < 4; ++i)
    {
        if (!view.alive[i] || view.horse[i]->GetVictim() != bot)
            continue;
        Player* const tank = Naxx40Horsemen::ActiveTank(bot, view, static_cast<Horse>(i));
        Naxx40HorsemenStations::Spot const dest = tank && tank != bot
            ? Naxx40HorsemenStations::Spot{tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ()}
            : Naxx40HorsemenStations::HORSE[i].tankSpot;
        if (!victimRun)
            LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} has {}: runs to {}", bot->GetName(),
                      view.horse[i]->GetName(), tank ? tank->GetName() : "his tank spot");
        victimRun = true;
        meleeLegValid = false;
        bool const stopped = !healer && StopAttacking(bot, botAI, context);
        return StandAt(bot, botAI, dest, nullptr, leg) || stopped;
    }
    victimRun = false;
    Creature* const horse = h == Horse::None ? nullptr : view.horse[Idx(h)];
    if (!horse || healer || PlayerbotAI::IsRanged(bot))
        meleeLegValid = false;  // the melee leg memory only spans one melee stint on one horseman

    if (h == Horse::None)
    {
        bool const stopped = !healer && StopAttacking(bot, botAI, context);
        return StandAt(bot, botAI, Naxx40HorsemenStations::SAFE_SPOT, nullptr, leg) || stopped;
    }
    if (!horse)
        return false;

    if (healer)
    {
        Naxx40HorsemenStations::HorseSpots const& spots = Naxx40HorsemenStations::HORSE[Idx(h)];
        Player* const tank = Naxx40Horsemen::ActiveTank(bot, view, h);
        bool const tankAlive = tank && tank != bot && tank->IsAlive();
        std::array<float, 4> dist{};
        for (uint8 i = 0; i < 4; ++i)
            dist[i] = view.alive[i] ? bot->GetExactDist2d(view.horse[i]) : 0.0f;
        bool const markSafe = MarkSafe(h, dist, view.alive, stacks);

        // Spec §4.3b: escort the tank while he drags an unparked horseman; stand once within ESCORT_RANGE and sight.
        // Blaumeux is parked on her current tank point (all of them are in reach of her healer spot).
        Naxx40HorsemenStations::Spot const& park = h == Horse::Blaumeux ? BlaumeuxPoint(bot) : spots.tankSpot;
        if (tankAlive && markSafe && HealerEscorts(horse->GetExactDist2d(park.x, park.y)))
        {
            if (!escorting)
                LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} escorts {} ({} not parked)", bot->GetName(),
                          tank->GetName(), horse->GetName());
            escorting = true;
            healStop = false;
            if (bot->GetExactDist2d(tank) <= ESCORT_RANGE && bot->IsWithinLOSInMap(tank))
            {
                if (bot->isMoving())
                    bot->StopMoving();
                escortLegValid = false;
                return false;
            }
            if (!botAI->CanMove())
                return false;
            Pt const at{tank->GetPositionX(), tank->GetPositionY()};
            bool moved = false;
            if (!escortLegValid || Dist(at, escortLeg) > ESCORT_REPATH)
            {
                moved = leg(at.x, at.y, tank->GetPositionZ());
                if (moved)
                {
                    escortLeg = at;
                    escortLegValid = true;
                }
            }
            return moved || bot->isMoving();
        }
        escorting = false;
        escortLegValid = false;

        // Spec §4.3c: on the way to the spot, stop and heal the tank once he is low and in reach (cast-time heals
        // never fire while moving; the tank heal runs on the next tick, when the bot stands).
        if (bot->GetExactDist2d(spots.healerSpot.x, spots.healerSpot.y) > SPOT_ARRIVE &&
            HealerStopsToHeal(tankAlive, tankAlive ? tank->GetHealthPct() : 100.0f,
                              tankAlive ? bot->GetExactDist2d(tank) : 1000.0f, markSafe) &&
            bot->IsWithinLOSInMap(tank))
        {
            if (!healStop)
                LOG_DEBUG("playerbots", "[Naxx40Horsemen] raid {} stops to heal {} ({:.0f}%)", bot->GetName(),
                          tank->GetName(), tank->GetHealthPct());
            healStop = true;
            if (bot->isMoving())
                bot->StopMoving();
            return false;
        }
        healStop = false;
        return StandAt(bot, botAI, spots.healerSpot, horse, leg);
    }

    // DPS attack only this horseman. Start the swing / cast target once: never re-issue Attack() every tick (memory:
    // Attack cancels movement); it sets "current target" even when it waits, so that counts as accepted too.
    bool attacked = false;
    if (bot->GetVictim() != horse && AI_VALUE(Unit*, "current target") != horse)
        attacked = Attack(horse);

    if (PlayerbotAI::IsRanged(bot))
        return StandAt(bot, botAI, Naxx40HorsemenStations::HORSE[Idx(h)].rangedSpot, horse, leg) || attacked;

    // Melee (never on Zeliek: WithoutZeliek above): behind him, on the far side from his active tank; no living active
    // tank -> behind his facing (MeleeSpot against a point 1 y in front of him).
    Pt const boss{horse->GetPositionX(), horse->GetPositionY()};
    Player* const tank = Naxx40Horsemen::ActiveTank(bot, view, h);
    Pt const front = tank && tank != bot
        ? Pt{tank->GetPositionX(), tank->GetPositionY()}
        : Pt{boss.x + std::cos(horse->GetOrientation()), boss.y + std::sin(horse->GetOrientation())};
    Pt spot = MeleeSpot(h, boss, front);
    // The Void Zone search only matters near her; farther out the raw spot is a fine heading.
    if (h == Horse::Blaumeux && bot->GetExactDist2d(horse) <= VOID_SEARCH_FROM)
        spot = AvoidVoid(spot, boss, VoidZonesNear(bot, horse));

    if (bot->GetExactDist2d(spot.x, spot.y) <= MELEE_ARRIVE)
    {
        if (!bot->isMoving() && !bot->HasInArc(FACE_ARC, horse))
            bot->SetFacingToObject(horse);
        return attacked;
    }

    // While the tank drags him the spot moves every tick: re-issue the leg only when it moved more than MELEE_REPATH
    // from the last one issued, or the bot is out of melee range. In melee range the leg (MOVEMENT_COMBAT, so lower
    // priority moves wait) runs on while the tick falls through to the rotation.
    bool const inReach = bot->IsWithinMeleeRange(horse);
    bool moved = false;
    if (botAI->CanMove() && (!meleeLegValid || meleeLegHorse != h || !inReach ||
                             Dist(spot, meleeLeg) > MELEE_REPATH))
    {
        float z = horse->GetPositionZ();
        bot->UpdateAllowedPositionZ(spot.x, spot.y, z);
        moved = leg(spot.x, spot.y, z);
        if (moved)
        {
            meleeLeg = spot;
            meleeLegHorse = h;
            meleeLegValid = true;
        }
    }
    if (inReach)
        return attacked;
    return moved || bot->isMoving() || attacked;
}

// Re-resolved at execution (the trigger's pick may be a tick old). False = stock healing runs.
bool Naxx40HorsemenTankHealAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Player* const tank = Naxx40Horsemen::TankHealTarget(bot, botAI, spell);
    return tank && spell && botAI->CastSpell(spell, tank);
}

// The DPS hard stop (the trigger checks ThreatStop on the plain ratio): stop the running auto-attack / auto-shot and
// keep "current target", so the attack restarts on him once the multiplier's pre-cast veto lifts.
bool Naxx40HorsemenThreatHoldAction::Execute(Event /*event*/)
{
    LOG_DEBUG("playerbots", "[Naxx40Horsemen] {} threat hold on {}", bot->GetName(),
              bot->GetVictim() ? bot->GetVictim()->GetName() : "autorepeat");
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    return true;
}
