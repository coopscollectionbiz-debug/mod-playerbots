/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Twin Emperors v2 actions (overlay patch 0016). Spec (overlay repo):
// docs/superpowers/specs/2026-10-22-playerbot-aq40-twin-emperors-v2-design.md, §5.3-§5.8. The decisions live in
// Aq40Helpers.cpp; every action re-fetches its destination at execution, because the trigger's may be a tick old.
// Revision 4 §13.3: every MoveTo the strategy issues (hold spot, stage, hold station, landing clear, victim hold) goes
// through TwinsBerthed, which routes it round a 14 y circle around Vek'lor; arrival tests stay on the real destination.

#include "Aq40Actions.h"

#include <vector>

#include "Aq40Helpers.h"
#include "CharmInfo.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "Pet.h"
#include "PetDefines.h"
#include "Playerbots.h"
#include "ThreatManager.h"

using namespace TempleOfAhnQirajHelpers;

namespace
{
    // A parked pet within this of its park spot (2-D) is left alone.
    constexpr float TWINS_PET_PARK_ARRIVE = 3.0f;
    // The holding melee tank turns in place when Vek'nilash is outside this frontal arc.
    constexpr float TWINS_MT_FACE_ARC = float(M_PI) / 2.0f;

    // The bot's pet plus every non-totem controlled creature (guardians), the pet first and once.
    std::vector<Creature*> TwinsPetsOf(Player* bot)
    {
        std::vector<Creature*> pets;
        if (Pet* pet = bot->GetPet())
            pets.push_back(pet);
        for (Unit* controlled : bot->m_Controlled)
        {
            Creature* creature = controlled ? controlled->ToCreature() : nullptr;
            if (!creature || creature->IsTotem())
                continue;
            if (!pets.empty() && creature == pets.front())
                continue;
            pets.push_back(creature);
        }
        return pets;
    }

    // Passive and not attacking. True when anything changed.
    bool TwinsPetStop(Creature* pet)
    {
        bool changed = false;
        if (pet->GetReactState() != REACT_PASSIVE)
        {
            pet->SetReactState(REACT_PASSIVE);
            if (CharmInfo* ci = pet->GetCharmInfo())
                ci->SetPlayerReactState(REACT_PASSIVE);
            changed = true;
        }
        if (pet->GetVictim())
        {
            pet->AttackStop();
            changed = true;
        }
        return changed;
    }

    // Follow the owner (PlayerbotAI::PetFollow, per creature), unless it already follows. True when it was ordered.
    bool TwinsPetFollow(Player* bot, Creature* pet)
    {
        CharmInfo* const ci = pet->GetCharmInfo();
        if (!ci)
            return false;
        if (ci->HasCommandState(COMMAND_FOLLOW) && (ci->IsFollowing() || ci->IsReturning()) && !pet->GetVictim())
            return false;
        if (pet->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != NULL_MOTION_TYPE)
            return false;  // feared, charging, ...: the core owns its movement

        pet->AttackStop();
        pet->CastStop();
        pet->ClearInPetCombat();
        pet->GetMotionMaster()->MoveFollow(bot, PET_FOLLOW_DIST, pet->GetFollowAngle());
        if (Pet* asPet = pet->ToPet())
            asPet->ClearCastWhenWillAvailable();
        ci->SetCommandState(COMMAND_FOLLOW);
        ci->SetIsCommandAttack(false);
        ci->SetIsAtStay(false);
        ci->SetIsReturning(true);
        ci->SetIsCommandFollow(true);
        ci->SetIsFollowing(false);
        ci->RemoveStayPosition();
        ci->SetForcedSpell(0);
        ci->SetForcedTargetGUID();
        return true;
    }

    // Ruling R9: park the pet at `spot` on a stay command. The PetAI's own stay handling then keeps it there:
    // MovementInform marks it at stay on arrival, and HandleReturnMovement walks it back to the saved stay position
    // (the MovePoint's destination) whenever it strays. Re-issued only when it is neither there nor walking there.
    bool TwinsPetPark(Creature* pet, Position const& spot)
    {
        bool changed = TwinsPetStop(pet);
        CharmInfo* const ci = pet->GetCharmInfo();
        if (!ci || pet->GetExactDist2d(spot) <= TWINS_PET_PARK_ARRIVE)
            return changed;
        if (ci->HasCommandState(COMMAND_STAY) && ci->IsReturning() && !pet->movespline->Finalized())
            return changed;  // on its way
        if (pet->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != NULL_MOTION_TYPE)
            return changed;

        pet->GetMotionMaster()->Clear(false);
        pet->GetMotionMaster()->MovePoint(pet->GetGUID().GetCounter(), spot.GetPositionX(), spot.GetPositionY(),
                                          spot.GetPositionZ());
        ci->SetCommandState(COMMAND_STAY);
        ci->SetIsCommandAttack(false);
        ci->SetIsCommandFollow(false);
        ci->SetIsFollowing(false);
        ci->SetIsAtStay(false);
        ci->SetIsReturning(true);
        ci->SaveStayPosition(false);  // the spline's final destination = the park spot
        ci->SetForcedSpell(0);
        ci->SetForcedTargetGUID();
        return true;
    }

    // Final-review I2: a pet pass that changed anything records the owner, so the pets are given back once the
    // encounter is over (TwinsRestoreTouchedPets). Returns `changed`.
    bool TwinsRecordPets(Player* bot, bool changed)
    {
        if (changed)
            TwinsMarkPetsTouched(bot);
        return changed;
    }

    // The side whose warlock slot the bot holds (0 = A, 1 = B), or -1.
    int32 TwinsWarlockSlotOf(TwinsView const& view, Player* bot)
    {
        for (int32 i = 0; i < 2; ++i)
            if (!view.activeWarlock[i].IsEmpty() && view.activeWarlock[i] == bot->GetGUID())
                return i;
        return -1;
    }
}  // namespace

// Final-review I2 (declared in Aq40Helpers.h): give one owner's pets back after the encounter. The stance mapping is
// SetPetStanceAction's (PetsAction.cpp) for Playerbots.DefaultPetStance.
bool TempleOfAhnQirajHelpers::TwinsRestorePets(Player* owner)
{
    ReactStates react = REACT_DEFENSIVE;
    switch (sPlayerbotAIConfig.DefaultPetStance)
    {
        case 0:
            react = REACT_PASSIVE;
            break;
        case 2:
            react = REACT_AGGRESSIVE;
            break;
        default:
            break;  // 1, and any other value: defensive, as the playerbots stance command falls back
    }

    bool any = false;
    bool allFollowing = true;
    for (Creature* pet : TwinsPetsOf(owner))
    {
        if (!pet->IsAlive())
            continue;
        any = true;
        CharmInfo* const ci = pet->GetCharmInfo();
        if (pet->GetReactState() != react)
        {
            pet->SetReactState(react);
            if (ci)
                ci->SetPlayerReactState(react);
        }
        TwinsPetFollow(owner, pet);
        if (ci && !ci->HasCommandState(COMMAND_FOLLOW))
            allFollowing = false;  // under a controlled motion (fear, charge): try again on the next pass
    }
    return any && allFollowing;
}

bool Aq40TwinsEraseTrackersAction::Execute(Event /*event*/)
{
    TwinsEraseTrackers(bot);
    return false;
}

bool Aq40TwinsClearExplodeAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    // Try up to four snapped bearings and take the first the pathfinder accepts.
    for (uint32 attempt = 0; attempt < 4; ++attempt)
    {
        Position spot;
        if (!GetTwinsExplodeClearSpot(bot, spot, attempt))
            return false;
        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                   false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        {
            return true;
        }
    }
    return false;
}

bool Aq40TwinsClearBlizzardAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    for (uint32 attempt = 0; attempt < 4; ++attempt)
    {
        Position spot;
        if (!GetTwinsBlizzardClearSpot(bot, spot, attempt))
            return false;
        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                   false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        {
            return true;
        }
    }
    return false;
}

// Revision 7 §16.1, the universal burst escape: the Explode dodge's shape, round Vek'lor at BURST_ESCAPE_TO. Every role
// (the active warlock then casts from 16 y, ruling R13 F1). Re-checks the trigger: it may be a tick old.
bool Aq40TwinsClearBurstAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove() || !TwinsInBurstRange(bot))
        return false;

    for (uint32 attempt = 0; attempt < 4; ++attempt)
    {
        Position spot;
        if (!GetTwinsBurstClearSpot(bot, spot, attempt))
            return false;
        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                   false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        {
            return true;
        }
    }
    return false;
}

bool Aq40TwinsFocusTargetAction::Execute(Event /*event*/)
{
    Unit* target = GetTwinsAttackTarget(bot, botAI);
    return target && Attack(target);
}

// Spec §5.4 step 1: leave the landing zone on the stable sector bearing the predicate built, and recall the pet,
// which counts for the nearest-target roll too (§5.8). A designated warlock's pets go back to their park spot
// instead (ruling R9): following a waiting warlock would put them inside Vek'lor's 13 y zone. The pickup tank
// itself is never inside (its `escape` is its own position), so it holds still.
bool Aq40TwinsLandingClearAction::Execute(Event /*event*/)
{
    Position escape;
    if (!TwinsInLandingZone(bot, botAI, escape))
        return false;

    TwinsView view;
    int32 const slot = GetTwinsView(bot, view) ? TwinsWarlockSlotOf(view, bot) : -1;
    bool recalled = false;
    for (Creature* pet : TwinsPetsOf(bot))
    {
        if (!pet->IsAlive())
            continue;
        recalled |= slot >= 0 ? TwinsPetPark(pet, TWINS_STATIONS[slot].warlockCast) : TwinsPetFollow(bot, pet);
    }
    TwinsRecordPets(bot, recalled);

    bool moved = false;
    if (botAI->CanMove() && bot->GetExactDist2d(escape) > TWINS_SPOT_ARRIVE)
    {
        // The berth is inactive inside the landing window, where this runs; kept so every Twins mover shares it.
        Position const to = TwinsBerthed(bot, escape);
        moved = MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                       false, MovementPriority::MOVEMENT_COMBAT);
    }
    return moved || recalled;
}

// Revision 2 §11.2, the no-drag rule: a wrong victim (TwinsWrongVictimOf) never drags a twin across the room. Vek'lor:
// hold 12-40 y from him (escape to 14 y inside 12, walk to 38 y beyond 40). Vek'nilash: walk to his active melee tank.
// This, and the Explode / Blizzard dodges, are the only movement the multiplier lets a wrong victim make. Holding in
// place returns false, so the bot keeps casting while the veto keeps every other mover off it.
// Revision 11 §20.1: Vek'nilash's victim kites him instead (twins under SEPARATION_GUARD, or no living active melee
// tank): to the drag station by the hold station's route step (TwinsStationStep), walking backwards when the step
// leads away from him and turning in place to face him when it does not move (hold station's face rule).
bool Aq40TwinsVictimHoldAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position spot;
    bool kite = false;
    if (!TwinsWrongVictim(bot, botAI, spot, &kite))
        return false;
    if (!kite)
    {
        if (bot->GetExactDist2d(spot) <= TWINS_VICTIM_ARRIVE)
            return false;
        Position const to = TwinsBerthed(bot, spot);
        return MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                      false, MovementPriority::MOVEMENT_COMBAT);
    }

    Creature* const veknilash = GetTwin(bot, AQT_DATA_VEKNILASH);
    bool moved = false;
    if (bot->GetExactDist2d(spot) > TWINS_VICTIM_ARRIVE)
    {
        Position step;
        TwinsStationStep(bot, spot, step);
        Position const to = TwinsBerthed(bot, step);
        bool backwards = false;
        if (veknilash)
        {
            float const toStepX = to.GetPositionX() - bot->GetPositionX();
            float const toStepY = to.GetPositionY() - bot->GetPositionY();
            float const toBossX = veknilash->GetPositionX() - bot->GetPositionX();
            float const toBossY = veknilash->GetPositionY() - bot->GetPositionY();
            backwards = toStepX * toBossX + toStepY * toBossY < 0.0f;
        }
        moved = MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                       false, MovementPriority::MOVEMENT_COMBAT, true, backwards);
    }
    // On the station (or the duplicate guard refused the step): face him, as the hold station does. The wrong-victim
    // veto keeps every generic mover (tank face included) off this bot, so nothing else would turn it.
    if (!moved && veknilash && !bot->isMoving() && !bot->HasInArc(TWINS_MT_FACE_ARC, veknilash))
        bot->SetFacingToObject(veknilash);
    return moved;
}

// Spec §5.1: the active melee tank brings Vek'nilash back onto a station, one route step at a time. Revision 3 §12.2:
// only while the twins are closer than SEPARATION_GUARD, and toward the AUTHORED station farther from Vek'lor (kite
// him away from the other boss); otherwise his anchor follows him.
// Walk backwards when the step leads away from him (BWL BwlNefarianMtStationAction's rule), so the tank keeps
// facing him and keeps swinging. Returns false on purpose: MoveTo's duplicate guard reports false for a held
// position, and the movement multiplier is what keeps generic movement (tank face included) off the tick.
bool Aq40TwinsHoldStationAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position step;
    if (!TwinsShouldHoldStation(bot, botAI, step))
        return false;

    Creature* const veknilash = GetTwin(bot, AQT_DATA_VEKNILASH);
    Position const to = TwinsBerthed(bot, step);
    bool backwards = false;
    if (veknilash)
    {
        float const toStepX = to.GetPositionX() - bot->GetPositionX();
        float const toStepY = to.GetPositionY() - bot->GetPositionY();
        float const toBossX = veknilash->GetPositionX() - bot->GetPositionX();
        float const toBossY = veknilash->GetPositionY() - bot->GetPositionY();
        backwards = toStepX * toBossX + toStepY * toBossY < 0.0f;
    }
    bool moved = false;
    if (bot->GetExactDist2d(step) > TWINS_MT_STEP_MIN)
        moved = MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                       false, MovementPriority::MOVEMENT_COMBAT, true, backwards);

    // On the station (or the duplicate guard refused the step): turn in place to face him (BWL NefMtStep::Face,
    // Huhuran's parked MT). The movement veto keeps stock tank face off this tank while he is off station, so
    // nothing else would turn it.
    if (!moved && veknilash && !bot->isMoving() && !bot->HasInArc(TWINS_MT_FACE_ARC, veknilash))
        bot->SetFacingToObject(veknilash);
    return false;
}

// Spec §5.3: healers, the waiting tanks, the active warlock and (Revision 3 §12.1) melee in the pre-swap move walk
// to their owned (anchored) spot (Revision 9 §18.1: the ranged arcs are gone); Revision 4 §13.2: a healer's spot
// may be its step-in point.
// Returns false on purpose (see hold station). Revision 3 §12.2: every MoveTo is reported, and a bot that twice fails
// to walk to its spot parks where it stands for the epoch.
bool Aq40TwinsHoldSpotAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position spot;
    bool parkable = true;
    if (!GetTwinsSpot(bot, botAI, spot, &parkable))
        return false;
    float const dist = bot->GetExactDist2d(spot);
    if (dist <= TWINS_SPOT_ARRIVE)
        return false;
    Position const to = TwinsBerthed(bot, spot);
    bool const moved = MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false,
                              false, false, MovementPriority::MOVEMENT_COMBAT);
    // Fix round 1 (R22): the warlock's escape, the melee pre-swap point and the healer step-in point are never
    // parked, so never reported.
    if (parkable)
        TwinsReportSpotMove(bot, moved, dist);
    return false;
}

// Spec §5.4 step 3 / §5.5: Searing Pain (double threat) by name on Vek'lor; Shadow Ward when it is ready and his
// Shadow Bolts are on me. A fill-in warlock (a mage, a shadow priest) knows neither spell, so both casts fail and
// the tick falls through to its own rotation on him (focus target keeps it on Vek'lor).
bool Aq40TwinsWarlockTankAction::Execute(Event /*event*/)
{
    if (!TwinsShouldWarlockTank(bot, botAI))
        return false;
    Creature* const veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    if (!veklor || !veklor->IsAlive())
        return false;

    if (veklor->GetVictim() == bot && !botAI->HasAura("shadow ward", bot) &&
        botAI->CanCastSpell("shadow ward", bot) && botAI->CastSpell("shadow ward", bot))
        return true;
    return botAI->CastSpell("searing pain", veklor);
}

// BWL BwlThreatHoldAction: stop the swing / auto-repeat, keep the target.
bool Aq40TwinsThreatHoldAction::Execute(Event /*event*/)
{
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    return true;
}

// Spec §5.8 + ruling R9. Pets count for the nearest-target roll (SelectNearestTarget counts pets), so:
//   * the two designated warlocks' pets are passive and parked at their side's casting spot (24 y, outside both
//     clear radii) for the whole encounter;
//   * every other warlock pet, and the pet of any other magic-school owner, is passive at its owner (a felhunter
//     sent 95 y across is pointless and arrives as one more body in the roll);
//   * physical owners' pets (hunter pets) attack Vek'nilash only - the only twin they can hurt - and get no attack
//     order during the landing window or while their owner holds fire (ruling R8), when they are passive instead;
//     a pet over the DPS threat cap on him (spec §5.6) is passive too until the ratio drops.
// Unlatched owners (late joiners) get no opinion. Returns true only when something changed, so the always-on
// trigger costs nothing once the pets are where they belong.
bool Aq40TwinsDirectPetsAction::Execute(Event /*event*/)
{
    // Form-up (spec §5.5): every pet passive and following its owner to the staging spot until the pull. A pet
    // that strays toward a twin would pull early (aggro ~23 y at level 60: see the STAGE_A_DIST note), and
    // counts for nothing before the pull anyway.
    if (IsTwinsFormUp(bot))
    {
        bool changed = false;
        for (Creature* pet : TwinsPetsOf(bot))
        {
            if (!pet->IsAlive())
                continue;
            changed |= TwinsPetStop(pet);
            changed |= TwinsPetFollow(bot, pet);
        }
        return TwinsRecordPets(bot, changed);
    }

    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    std::vector<Creature*> const pets = TwinsPetsOf(bot);
    if (pets.empty())
        return false;

    bool changed = false;
    int32 const slot = TwinsWarlockSlotOf(view, bot);
    if (slot >= 0)
    {
        for (Creature* pet : pets)
            if (pet->IsAlive())
                changed |= TwinsPetPark(pet, TWINS_STATIONS[slot].warlockCast);
        return TwinsRecordPets(bot, changed);
    }

    TwinsMe const me = GetTwinsMe(bot);
    if (me.role == Aq40TwinsRules::Role::None)
        return false;

    bool const physical = me.role == Aq40TwinsRules::Role::MeleeTank || IsTwinsPhysicalGroup(bot, botAI);
    if (bot->getClass() == CLASS_WARLOCK || !physical)
    {
        for (Creature* pet : pets)
        {
            if (!pet->IsAlive())
                continue;
            changed |= TwinsPetStop(pet);
            changed |= TwinsPetFollow(bot, pet);
        }
        return TwinsRecordPets(bot, changed);
    }

    if (view.landingWindow || IsTwinsHoldingFire(bot, botAI))
    {
        for (Creature* pet : pets)
            if (pet->IsAlive())
                changed |= TwinsPetStop(pet);
        return TwinsRecordPets(bot, changed);
    }

    Creature* const veknilash = view.veknilash;
    if (!bot->IsValidAttackTarget(veknilash))
        return false;

    // Spec §5.6: no pet may become his top threat over his designated active melee tank. The DPS cap
    // (Aq40TwinsRules::ThreatVetoed: own + headroom (0) >= 0.80 of the tank's threat; no tank threat = no cap): a pet
    // over it is stopped (passive), and the next pass re-orders the attack once the ratio drops.
    Player* const vnTank = TwinsActiveTankFor(view, bot, AQT_DATA_VEKNILASH);
    float const tankThreat = vnTank ? veknilash->GetThreatMgr().GetThreat(vnTank) : 0.0f;

    for (Creature* pet : pets)
    {
        CharmInfo* charmInfo = pet->GetCharmInfo();
        if (!pet->IsAlive() || !charmInfo || !pet->IsAIEnabled)
            continue;
        if (Aq40TwinsRules::ThreatVetoed(veknilash->GetThreatMgr().GetThreat(pet), tankThreat))
        {
            changed |= TwinsPetStop(pet);
            continue;
        }
        if (pet->GetVictim() == veknilash && charmInfo->IsCommandAttack())
            continue;  // already correct: do not restart the chase every tick

        if (pet->GetReactState() == REACT_PASSIVE)
        {
            pet->SetReactState(REACT_DEFENSIVE);
            charmInfo->SetPlayerReactState(REACT_DEFENSIVE);
        }
        if (pet->GetVictim())
            pet->AttackStop();

        pet->ClearUnitState(UNIT_STATE_FOLLOW);
        pet->SetTarget(veknilash->GetGUID());
        charmInfo->SetIsCommandAttack(true);
        charmInfo->SetIsAtStay(false);
        charmInfo->SetIsFollowing(false);
        charmInfo->SetIsCommandFollow(false);
        charmInfo->SetIsReturning(false);
        pet->AI()->AttackStart(veknilash);
        changed = true;
    }
    return TwinsRecordPets(bot, changed);
}

// Spec §5.5 form-up (ruling R11): walk to the staging spot and hold it (the hold spot shape). Returns false on
// purpose: MoveTo's duplicate guard reports false for a held point, and the movement multiplier keeps follow and
// the other generic movers off the bot. Never walks out of a cast: warlock A's pull is a 1.5 s cast from here.
bool Aq40TwinsStageAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetTwinsStagingSpot(bot, spot))
        return false;
    TwinsMaybeAnnounceFormedUp(bot);

    float const dist = bot->GetExactDist2d(spot);
    if (!botAI->CanMove() || bot->IsNonMeleeSpellCast(false) || dist <= TWINS_SPOT_ARRIVE)
        return false;
    Position const to = TwinsBerthed(bot, spot);
    bool const moved = MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false,
                              false, false, MovementPriority::MOVEMENT_COMBAT);
    // Revision 3 §12.2: two failed walks park the bot where it stands (formed up there) for the episode.
    TwinsReportStageMove(bot, moved, dist);
    return false;
}

// Spec §5.5 "twins pull" (the owner's command). The trigger is a cached ChatCommandTrigger: only an engine that owns
// it resets it, so a "twins pull" said while the bot was outside AQ40 is still pending when it arrives here (a
// summon into the Twins room fires it on the first tick). Hence every check is redone, and warlock A also has to
// stand within STAGE_ARRIVE of its staging spot, A's casting spot (ruling R16 H2; S_A before it), which a stale event
// after a summon fails. After the pull it already stands where it casts from. Only warlock A casts; everyone else
// returns false.
bool Aq40TwinsPullAction::Execute(Event /*event*/)
{
    if (!IsTwinsFormUp(bot))
        return false;
    TwinsMe const me = GetTwinsStagingMe(bot);
    if (me.role != Aq40TwinsRules::Role::WarlockTank || me.side != Aq40TwinsRules::Side::A)
        return false;
    Creature* const veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    Position spot;
    if (!veklor || !veklor->IsAlive() || !GetTwinsStagingSpot(bot, spot))
        return false;
    // A fill-in warlock A (a mage, a shadow priest) has no Searing Pain: say so rather than "try again".
    if (!AI_VALUE2(uint32, "spell id", "searing pain"))
    {
        LOG_DEBUG("playerbots", "[Aq40Twins] pull refused: {} does not know searing pain", bot->GetName());
        botAI->TellMaster("Twins: I don't know Searing Pain - pull Vek'lor manually.");
        return false;
    }

    float const d = bot->GetExactDist2d(spot);
    if (d > Aq40TwinsRules::STAGE_ARRIVE)
    {
        LOG_DEBUG("playerbots", "[Aq40Twins] pull refused: {} is {:.1f} y off its pull spot", bot->GetName(), d);
        botAI->TellMaster("Twins: not on my pull spot yet - say \"twins pull\" again once formed up.");
        return false;
    }
    // A cast-time spell is refused while moving (PlayerbotAI::CastSpell); the last step onto the spot may still be
    // running.
    if (bot->isMoving())
        bot->StopMoving();

    bool const cast = botAI->CastSpell("searing pain", veklor);
    LOG_DEBUG("playerbots", "[Aq40Twins] pull: {} searing pain on Vek'lor at {:.1f} y -> {}", bot->GetName(),
              bot->GetExactDist2d(veklor), cast ? "cast" : "FAILED");
    if (!cast)
        botAI->TellMaster("Twins: the pull failed - say \"twins pull\" again.");
    return cast;
}

// Revision 6 §15.1: re-resolved at execution (the trigger's pick may be a tick old). False = stock healing runs.
bool Aq40TwinsTankHealAction::Execute(Event /*event*/)
{
    Player* const tank = TwinsTankHealTarget(bot, botAI);
    char const* const spell = tank ? Aq40TankHealSpellName(bot) : nullptr;
    if (!spell || !botAI->CastSpell(spell, tank))
        return false;
    TwinsCountTankHeal(bot);
    return true;
}
