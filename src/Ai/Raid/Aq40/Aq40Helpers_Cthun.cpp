/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40CthunHelpers.h"

#include <algorithm>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Aq40Helpers.h"
#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"

namespace TempleOfAhnQirajHelpers
{

namespace
{
    struct CthunOpening
    {
        ObjectGuid puller;
        bool over = false;
        uint32 pullerBeams = 0;               // Green Beams the Eye has started at the puller
        bool eyeWasCastingAtPuller = false;   // edge detector for the count
    };

    struct CthunBotState
    {
        uint32 exitHoldStartMs = 0;  // 0 = not holding
        uint32 lastStomachDiagMs = 0;  // stomach diagnostic throttle
    };

    // Different instances update on parallel map threads: two AQ40 raids would race on an unlocked
    // container and rehash under each other (the Aug 25-26 prod crashes). One lock, held briefly.
    std::mutex cthunMutex;
    std::unordered_map<uint32, CthunSnapshot> cthunSnapshotByInstance;
    std::unordered_map<ObjectGuid, CthunBotState> cthunBotState;
    // Latched once per encounter: bearing from the Eye to the raid's centroid when first needed.
    std::unordered_map<uint32, float> cthunRingRefByInstance;
    std::unordered_map<uint32, CthunOpening> cthunOpeningByInstance;
    // Spot ranks, latched per instance and role the first time any bot of that role needs one (see
    // GetCthunLatchedRank). A live bearing rank re-derived every tick would reshuffle bots standing
    // on full rings of fixed spots.
    std::unordered_map<uint32, std::unordered_map<ObjectGuid, uint32>> cthunMeleeRankByInstance;
    std::unordered_map<uint32, std::unordered_map<ObjectGuid, uint32>> cthunRangedRankByInstance;

    CthunBotState GetBotState(Player* bot)
    {
        std::lock_guard<std::mutex> lock(cthunMutex);
        auto it = cthunBotState.find(bot->GetGUID());
        return it != cthunBotState.end() ? it->second : CthunBotState();
    }

    // Nearest living attackable add of the given entries within maxDist (2D) of the bot, on the
    // stomach side (z < 0) or the room side (z > 0). Keeps the bot's CURRENT victim if it already
    // qualifies (range included): a per-tick nearest pick flickers, and every retarget's Attack()
    // cancels movement.
    Creature* PickCthunAdd(Player* bot, std::vector<uint32> const& entries, bool stomachSide,
                           float maxDist = CTHUN_SEARCH_RANGE)
    {
        if (Unit* victim = bot->GetVictim())
        {
            if (Creature* current = victim->ToCreature())
            {
                if (current->IsAlive() && bot->IsValidAttackTarget(current) &&
                    (current->GetPositionZ() < 0.0f) == stomachSide &&
                    bot->GetExactDist2d(current) <= maxDist &&
                    std::find(entries.begin(), entries.end(), current->GetEntry()) != entries.end())
                {
                    return current;
                }
            }
        }

        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entries, maxDist);

        Creature* best = nullptr;
        float bestDist = 0.0f;
        for (Creature* c : found)
        {
            if (!c || !c->IsAlive() || !bot->IsValidAttackTarget(c))
                continue;
            if ((c->GetPositionZ() < 0.0f) != stomachSide)
                continue;
            float const d = bot->GetExactDist2d(c);
            if (d > maxDist)
                continue;
            if (!best || d < bestDist)
            {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    // Nearest living (z < 0) Flesh Tentacle to the bot, searched from the C'Thun BODY. requireValid
    // additionally filters on bot->IsValidAttackTarget (healers don't need it). Counts are for logs.
    Creature* SearchCthunFleshTentacle(Player* bot, bool requireValid, uint32& found, uint32& alive,
                                       uint32& valid)
    {
        found = alive = valid = 0;
        Creature* body = GetTwin(bot, AQT_DATA_CTHUN);
        std::list<Creature*> list;
        if (body)
            body->GetCreatureListWithEntryInGrid(list, NPC_CTHUN_FLESH_TENTACLE, CTHUN_FLESH_SEARCH_RANGE);
        found = uint32(list.size());

        Creature* best = nullptr;
        float bestDist = 0.0f;
        for (Creature* c : list)
        {
            if (!c || !c->IsAlive() || c->GetPositionZ() >= 0.0f)
                continue;
            ++alive;
            bool const isValid = bot->IsValidAttackTarget(c);
            if (isValid)
                ++valid;
            if (requireValid && !isValid)
                continue;
            float const d = bot->GetExactDist(c);
            if (!best || d < bestDist)
            {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    // Stomach target. Searched from the C'Thun BODY, the way the snapshot counts them: from the
    // stomach floor the tentacles can sit beyond a bot-anchored search. Keeps the current victim if
    // it is a living Flesh Tentacle; else the nearest to the bot among living, valid, z < 0 ones.
    Creature* PickCthunFleshTentacle(Player* bot)
    {
        if (Unit* victim = bot->GetVictim())
            if (Creature* current = victim->ToCreature())
                if (current->GetEntry() == NPC_CTHUN_FLESH_TENTACLE && current->IsAlive() &&
                    bot->IsValidAttackTarget(current))
                    return current;

        uint32 found = 0;
        uint32 alive = 0;
        uint32 valid = 0;
        Creature* best = SearchCthunFleshTentacle(bot, true, found, alive, valid);
        if (!best)
            LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach no target: found {} alive {} valid {} botZ {:.1f}",
                      bot->GetName(), found, alive, valid, bot->GetPositionZ());
        return best;
    }

    bool CthunExitOwed(Player* bot)
    {
        return !CthunExitReason(bot).empty();
    }

    float CthunBearing(float cx, float cy, WorldObject const* obj)
    {
        return std::atan2(obj->GetPositionY() - cy, obj->GetPositionX() - cx);
    }

    // Per-instance ring reference angle, latched on first need: bearing from the Eye to the centroid
    // of the group's living room-side members at that moment (one pass). Erased with the snapshot.
    float GetCthunRingRef(Player* bot, CthunSnapshot const& snap)
    {
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            auto it = cthunRingRefByInstance.find(instanceId);
            if (it != cthunRingRefByInstance.end())
                return it->second;
        }

        float sumX = 0.0f;
        float sumY = 0.0f;
        uint32 n = 0;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
                    member->GetPositionZ() <= 0.0f)
                    continue;
                sumX += member->GetPositionX();
                sumY += member->GetPositionY();
                ++n;
            }
        }
        float const refAngle = n ? std::atan2(sumY / n - snap.centerY, sumX / n - snap.centerX)
                                 : CthunBearing(snap.centerX, snap.centerY, bot);

        std::lock_guard<std::mutex> lock(cthunMutex);
        // First writer wins, so two bots computing it on the same tick agree.
        return cthunRingRefByInstance.try_emplace(instanceId, refAngle).first->second;
    }

    // Rank over the FULL 360 deg by bearing from the cut opposite the pack (ringRef + pi), GUID
    // tie-break - computed ONCE per instance and role for every living bot of that role in one pass
    // and latched, so spots stay fixed (deaths don't reshuffle). A bot missing from the latched
    // table (late arrival) takes the next free rank. count = the table's size.
    uint32 GetCthunLatchedRank(Player* bot, bool ranged, float ringRef, CthunSnapshot const& snap,
                               uint32& count)
    {
        auto& byInstance = ranged ? cthunRangedRankByInstance : cthunMeleeRankByInstance;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        bool built = false;
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            auto it = byInstance.find(instanceId);
            if (it != byInstance.end())
            {
                built = true;
                auto r = it->second.find(bot->GetGUID());
                if (r != it->second.end())
                {
                    count = uint32(it->second.size());
                    return r->second;
                }
            }
        }

        std::vector<std::pair<float, ObjectGuid>> order;
        if (!built)
        {
            auto relOf = [&](Player* p)
            {
                return Position::NormalizeOrientation(CthunBearing(snap.centerX, snap.centerY, p) -
                                                      ringRef + float(M_PI));
            };
            order.emplace_back(relOf(bot), bot->GetGUID());
            if (Group* group = bot->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (!member || member == bot || !GET_PLAYERBOT_AI(member) || !member->IsAlive() ||
                        member->GetMap() != bot->GetMap() || IsCthunRangedRole(member) != ranged)
                        continue;
                    order.emplace_back(relOf(member), member->GetGUID());
                }
            }
            std::sort(order.begin(), order.end());
        }

        std::lock_guard<std::mutex> lock(cthunMutex);
        auto& table = byInstance[instanceId];
        if (table.empty())  // first writer builds the whole table
            for (uint32 i = 0; i < order.size(); ++i)
                table.try_emplace(order[i].second, i);
        uint32 const rank = table.try_emplace(bot->GetGUID(), uint32(table.size())).first->second;
        count = uint32(table.size());
        return rank;
    }

    // The bot's FIXED spot (spacing >= 12y by construction). Ranged: two interleaved full circles.
    // Melee (Eye phase): 6 paired ring spots + the centre - or, while the opening runs, a staging
    // point off the puller's lane.
    Position ComputeCthunSlot(Player* bot, CthunSnapshot const& snap)
    {
        bool const ranged = IsCthunRangedRole(bot);
        float const ringRef = GetCthunRingRef(bot, snap);
        uint32 count = 1;
        uint32 const rank = GetCthunLatchedRank(bot, ranged, ringRef, snap, count);
        float x = snap.centerX;
        float y = snap.centerY;

        if (ranged)
        {
            uint32 const ring = rank % 2;  // A for even ranks
            uint32 const slot = rank / 2;
            uint32 const nRing = std::max<uint32>(1, ring == 0 ? (count + 1) / 2 : count / 2);
            float angle = ringRef + 2.0f * float(M_PI) * (float(slot) + 0.5f) / float(nRing);
            if (ring == 1)
                angle += float(M_PI) / float(nRing);  // interleave with ring A
            float const radius = ring == 0 ? CTHUN_RANGED_RING_A : CTHUN_RANGED_RING_B;
            x += std::cos(angle) * radius;
            y += std::sin(angle) * radius;
        }
        else if (IsCthunOpeningActive(bot))
        {
            // Staging off the puller's lane: even ranks on one side, odd on the other.
            float const side = rank % 2 == 0 ? 1.0f : -1.0f;
            float const angle = ringRef + side * (float(M_PI) / 2.0f + float(rank / 2) *
                                                                     CTHUN_MELEE_STAGE_SPACING /
                                                                     CTHUN_MELEE_STAGE_RADIUS);
            x += std::cos(angle) * CTHUN_MELEE_STAGE_RADIUS;
            y += std::sin(angle) * CTHUN_MELEE_STAGE_RADIUS;
        }
        else
        {
            uint32 const paired = CTHUN_MELEE_RING_SPOTS * CTHUN_MELEE_PER_SPOT;  // 12 on the ring
            float const step = 2.0f * float(M_PI) / float(CTHUN_MELEE_RING_SPOTS);
            float tangent = ringRef + float(M_PI) / 2.0f;
            bool partner = false;

            if (rank < paired)
            {
                float const angle = ringRef + step * float(rank / CTHUN_MELEE_PER_SPOT);
                x += std::cos(angle) * CTHUN_MELEE_RING;
                y += std::sin(angle) * CTHUN_MELEE_RING;
                tangent = angle + float(M_PI) / 2.0f;
                partner = rank % CTHUN_MELEE_PER_SPOT != 0;
            }
            else if (rank < paired + CTHUN_MELEE_PER_SPOT)
                partner = rank % CTHUN_MELEE_PER_SPOT != 0;  // the centre spot
            else
            {
                float const angle = ringRef + step * (float(rank - paired - CTHUN_MELEE_PER_SPOT) + 0.5f);
                x += std::cos(angle) * CTHUN_MELEE_RING;
                y += std::sin(angle) * CTHUN_MELEE_RING;
            }

            if (partner)
            {
                x += std::cos(tangent) * CTHUN_MELEE_PAIR_OFFSET;
                y += std::sin(tangent) * CTHUN_MELEE_PAIR_OFFSET;
            }
        }

        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);
        return Position(x, y, z);
    }

    // Nearest living raid member (humans included) in the room within maxDist.
    Player* FindCthunNeighbor(Player* bot, float maxDist)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;

        Player* best = nullptr;
        float bestDist = maxDist;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap())
                continue;
            if (member->GetPositionZ() < 0.0f)
                continue;  // in the stomach
            float const d = bot->GetExactDist2d(member);
            if (d < bestDist)
            {
                best = member;
                bestDist = d;
            }
        }
        return best;
    }

    // Unit targets of every Green Beam currently being cast (the Eye in P1, Giant Eye Tentacles in
    // P2). Empty in the stomach or with no valid snapshot. One grid scan.
    std::vector<Unit*> CollectCthunBeamTargets(Player* bot)
    {
        std::vector<Unit*> targets;
        if (IsInCthunStomach(bot))
            return targets;

        CthunSnapshot const snap = GetCthunSnapshot(bot);
        if (!snap.valid)
            return targets;

        std::list<Creature*> casters;
        bot->GetCreatureListWithEntryInGrid(casters, NPC_CTHUN_GIANT_EYE_TENTACLE, CTHUN_SEARCH_RANGE);
        if (snap.phase == CthunPhase::Eye)
            if (Creature* eye = GetTwin(bot, AQT_DATA_EYE_OF_CTHUN))
                casters.push_back(eye);

        for (Creature* caster : casters)
        {
            if (!caster || !caster->IsAlive())
                continue;
            Spell* spell = caster->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (!spell || !spell->m_spellInfo || spell->m_spellInfo->Id != SPELL_CTHUN_GREEN_BEAM)
                continue;
            if (Unit* target = spell->m_targets.GetUnitTarget())
                targets.push_back(target);
        }
        return targets;
    }

    // State-driven, no timers. The opening starts when the Eye first has a player victim (the
    // engager, boss_cthun's _beamTarget) and ends once the puller's 4th Green Beam has landed (the
    // script fires exactly 4 at the engager). Fallbacks: the Eye phase ending, the puller dying, or
    // a Green Beam at anyone else. World reads happen outside the lock; the lock only guards the map.
    void UpdateCthunOpening(Player* bot, CthunSnapshot const& snap)
    {
        uint32 const instanceId = bot->GetMap()->GetInstanceId();

        bool exists = false;
        CthunOpening opening;
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            auto it = cthunOpeningByInstance.find(instanceId);
            if (it != cthunOpeningByInstance.end())
            {
                exists = true;
                opening = it->second;
            }
        }

        Creature* eye = snap.phase == CthunPhase::Eye ? GetTwin(bot, AQT_DATA_EYE_OF_CTHUN) : nullptr;

        if (!exists)
        {
            if (!eye)
                return;
            Unit* victim = eye->GetVictim();
            Player* puller = victim ? victim->ToPlayer() : nullptr;
            if (!puller)
                return;

            std::lock_guard<std::mutex> lock(cthunMutex);
            CthunOpening fresh;
            fresh.puller = puller->GetGUID();
            cthunOpeningByInstance.try_emplace(instanceId, fresh);  // first writer wins
            return;
        }

        if (opening.over)
            return;

        char const* reason = nullptr;
        bool castingAtPuller = false;
        if (!eye)
            reason = "eye phase ended";
        else
        {
            Player* puller = ObjectAccessor::GetPlayer(bot->GetMap(), opening.puller);
            if (!puller || !puller->IsAlive())
                reason = "puller dead";
            else
            {
                Spell* spell = eye->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                if (spell && spell->m_spellInfo && spell->m_spellInfo->Id == SPELL_CTHUN_GREEN_BEAM)
                {
                    Unit* target = spell->m_targets.GetUnitTarget();
                    if (target && target->GetGUID() == opening.puller)
                        castingAtPuller = true;
                    else if (target)
                        reason = "beam at another";
                }
            }
        }

        uint32 countedBeam = 0;
        bool endedNow = false;
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            auto it = cthunOpeningByInstance.find(instanceId);
            if (it == cthunOpeningByInstance.end() || it->second.over)
                return;
            CthunOpening& state = it->second;
            if (castingAtPuller && !state.eyeWasCastingAtPuller)
                countedBeam = ++state.pullerBeams;
            state.eyeWasCastingAtPuller = castingAtPuller;
            if (!reason && state.pullerBeams >= CTHUN_OPENING_PULLER_BEAMS && !castingAtPuller)
                reason = "4th puller beam landed";
            if (reason)
            {
                state.over = true;
                endedNow = true;
            }
        }

        if (countedBeam)
            LOG_DEBUG("playerbots", "[Aq40Cthun] opening puller beam {}", countedBeam);
        if (endedNow)
            LOG_DEBUG("playerbots", "[Aq40Cthun] opening over ({})", reason);
    }

    // The opening puller as seen by another bot: entry exists, not over, alive on this map, and not
    // the bot itself. Same gating and state update as IsCthunOpeningPuller.
    Player* GetCthunOpeningPuller(Player* bot)
    {
        if (!IsCthunEncounterActive(bot) || IsInCthunStomach(bot))
            return nullptr;

        CthunSnapshot const snap = GetCthunSnapshot(bot);
        if (!snap.valid)
            return nullptr;

        UpdateCthunOpening(bot, snap);

        ObjectGuid pullerGuid;
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            auto it = cthunOpeningByInstance.find(bot->GetMap()->GetInstanceId());
            if (it == cthunOpeningByInstance.end() || it->second.over)
                return nullptr;
            pullerGuid = it->second.puller;
        }

        if (pullerGuid == bot->GetGUID())
            return nullptr;
        Player* puller = ObjectAccessor::GetPlayer(bot->GetMap(), pullerGuid);
        return puller && puller->IsAlive() ? puller : nullptr;
    }
}  // namespace

// ---------------------------------------------------------------------------------------------
// ENCOUNTER STATE
// ---------------------------------------------------------------------------------------------
bool IsCthunEncounterActive(Player* bot)
{
    if (!IsInAq40(bot))
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->GetBossState(AQT_DATA_CTHUN) == IN_PROGRESS;
}

CthunSnapshot GetCthunSnapshot(Player* bot)
{
    if (!IsCthunEncounterActive(bot))
        return CthunSnapshot();

    uint32 const instanceId = bot->GetMap()->GetInstanceId();
    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> lock(cthunMutex);
        auto it = cthunSnapshotByInstance.find(instanceId);
        if (it != cthunSnapshotByInstance.end() &&
            getMSTimeDiff(it->second.takenAtMs, now) < CTHUN_SNAPSHOT_TTL_MS)
        {
            return it->second;
        }
    }

    // GetTwin is just InstanceScript::GetCreature (no grid search) despite its name.
    Creature* body = GetTwin(bot, AQT_DATA_CTHUN);
    Creature* eye = GetTwin(bot, AQT_DATA_EYE_OF_CTHUN);
    if (!body)
        return CthunSnapshot();

    CthunSnapshot snap;
    snap.takenAtMs = now;
    snap.valid = true;
    snap.centerX = body->GetPositionX();
    snap.centerY = body->GetPositionY();
    snap.centerZ = body->GetPositionZ();

    // The Eye FAKE-dies (health 0 + NOT_SELECTABLE, still "alive"), so test selectability.
    bool const eyeUp = eye && eye->IsAlive() && eye->GetHealth() > 0 &&
                       !eye->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    bool const bodyUp = body->IsAlive() && !body->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    if (eyeUp)
        snap.phase = CthunPhase::Eye;
    else if (bodyUp)
        snap.phase = CthunPhase::Body;

    // Weakened BY TENTACLE COUNT, exactly the script's rule (both Flesh Tentacles dead -> weakened;
    // new ones spawn when it ends). Carapace is read for the diagnostic line only.
    std::list<Creature*> flesh;
    body->GetCreatureListWithEntryInGrid(flesh, NPC_CTHUN_FLESH_TENTACLE, CTHUN_FLESH_SEARCH_RANGE);
    for (Creature* c : flesh)
        if (c && c->IsAlive())
            ++snap.fleshAlive;

    snap.weakened = snap.phase == CthunPhase::Body && snap.fleshAlive == 0;
    snap.glare = snap.phase == CthunPhase::Eye && eye->HasAura(SPELL_CTHUN_FREEZE_ANIM);
    if (snap.glare)
        snap.beamAngle = eye->GetOrientation();
    bool const carapace = body->HasAura(SPELL_CTHUN_CARAPACE);

    bool changed = true;
    {
        std::lock_guard<std::mutex> lock(cthunMutex);
        auto it = cthunSnapshotByInstance.find(instanceId);
        if (it != cthunSnapshotByInstance.end())
        {
            CthunSnapshot const& prev = it->second;
            changed = prev.phase != snap.phase || prev.weakened != snap.weakened ||
                      prev.fleshAlive != snap.fleshAlive || prev.glare != snap.glare;
        }
        cthunSnapshotByInstance[instanceId] = snap;
    }

    if (changed)
        LOG_DEBUG("playerbots", "[Aq40Cthun] state phase={} weakened={} flesh={} glare={} carapace={}",
                  uint32(snap.phase), snap.weakened ? 1 : 0, snap.fleshAlive, snap.glare ? 1 : 0,
                  carapace ? 1 : 0);

    // Hazard interrupts. A casting bot evaluates no trigger until its cast ends (WaitForSpellCast
    // sets the AI delay to the cast time), so the bot that refreshes the snapshot interrupts any
    // endangered caster for it. The fresh snapshot is already cached, so the nested snapshot reads
    // below hit the cache (no recursion). No lock is held during the sweep.
    if (snap.phase != CthunPhase::None)
    {
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member->GetMap() != bot->GetMap() || !member->IsAlive())
                    continue;
                PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
                if (!memberAI || IsSelfBot(member) || !member->IsNonMeleeSpellCast(false))
                    continue;

                bool const beam = CthunShouldClearBeam(member);
                if (!beam && CthunGlareSecondsToHit(member) >= CTHUN_GLARE_MOVE_SECONDS)
                    continue;

                memberAI->RequestSpellInterrupt();
                LOG_DEBUG("playerbots", "[Aq40Cthun] {} interrupt cast ({})", member->GetName(),
                          beam ? "beam" : "glare");
            }
        }
    }

    return snap;
}

bool IsInCthunStomach(Player* bot)
{
    if (!IsInAq40(bot))
        return false;
    if (bot->HasAura(SPELL_CTHUN_DIGESTIVE_ACID))
        return true;
    // The script's own "not in stomach" test is z > 0, but a trash hall at z ~-3 to 0 lies right above
    // the stomach, so the height test is the stomach ceiling. The Twins room is also deep below 0,
    // ~900y away, hence the proximity term.
    return bot->GetPositionZ() < CTHUN_STOMACH_CEILING_Z &&
           bot->GetExactDist2d(CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y) < CTHUN_STOMACH_RADIUS;
}

void CthunEraseTrackers(Player* bot)
{
    if (!bot || !bot->GetMap())
        return;

    std::lock_guard<std::mutex> lock(cthunMutex);
    cthunBotState.erase(bot->GetGUID());
    cthunSnapshotByInstance.erase(bot->GetMap()->GetInstanceId());
    cthunRingRefByInstance.erase(bot->GetMap()->GetInstanceId());
    cthunOpeningByInstance.erase(bot->GetMap()->GetInstanceId());
    cthunMeleeRankByInstance.erase(bot->GetMap()->GetInstanceId());
    cthunRangedRankByInstance.erase(bot->GetMap()->GetInstanceId());
}

// ---------------------------------------------------------------------------------------------
// STOMACH
// ---------------------------------------------------------------------------------------------
bool IsCthunExitHolding(Player* bot)
{
    uint32 const start = GetBotState(bot).exitHoldStartMs;
    return start != 0 && getMSTimeDiff(start, getMSTime()) < CTHUN_EXIT_HOLD_MS;
}

void CthunStartExitHold(Player* bot)
{
    std::lock_guard<std::mutex> lock(cthunMutex);
    cthunBotState[bot->GetGUID()].exitHoldStartMs = std::max<uint32>(1, getMSTime());
}

std::string CthunExitReason(Player* bot)
{
    if (bot->GetHealthPct() < CTHUN_STOMACH_EXIT_HEALTH_PCT)
        return "low health";

    if (Aura* acid = bot->GetAura(SPELL_CTHUN_DIGESTIVE_ACID))
    {
        // A healer in here with us (or, for a healer, anyone to heal) buys a longer stay.
        bool healed = false;
        if (Group* group = bot->GetGroup())
        {
            bool const selfHealer = PlayerbotAI::IsHeal(bot);
            for (GroupReference* ref = group->GetFirstMember(); ref && !healed; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
                    !IsInCthunStomach(member))
                    continue;
                healed = selfHealer || PlayerbotAI::IsHeal(member);
            }
        }
        uint32 const limit = healed ? CTHUN_ACID_EXIT_STACKS_HEALED : CTHUN_ACID_EXIT_STACKS;
        if (acid->GetStackAmount() >= limit)
            return "acid " + std::to_string(uint32(acid->GetStackAmount()));
    }

    // Only a valid Body-phase snapshot may declare the tentacle term (weakened == no flesh alive,
    // and the damage window is outside).
    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (snap.valid && snap.phase == CthunPhase::Body && snap.fleshAlive == 0)
        return "no flesh (weakened)";

    return std::string();
}

namespace
{
    // Once a second per bot while in the stomach: why can't it act? (cast/move state dump)
    void CthunStomachDiag(Player* bot, PlayerbotAI* botAI)
    {
        uint32 const now = getMSTime();
        {
            std::lock_guard<std::mutex> lock(cthunMutex);
            CthunBotState& state = cthunBotState[bot->GetGUID()];
            if (state.lastStomachDiagMs && getMSTimeDiff(state.lastStomachDiagMs, now) < 1000)
                return;
            state.lastStomachDiagMs = std::max<uint32>(1, now);
        }

        Unit* target = GetCthunAttackTarget(bot, botAI);
        Creature* targetCreature = target ? target->ToCreature() : nullptr;
        Unit* victim = bot->GetVictim();
        Creature* victimCreature = victim ? victim->ToCreature() : nullptr;
        Aura* acid = bot->GetAura(SPELL_CTHUN_DIGESTIVE_ACID);
        LOG_DEBUG("playerbots",
                  "[Aq40Cthun] {} stomach diag pos {:.1f},{:.1f},{:.1f} tgt {} dist {:.1f} moving {} "
                  "flags 0x{:X} lostCtl {} stunned {} rooted {} mounted {} canMove {} victim {} acid {}",
                  bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                  targetCreature ? targetCreature->GetEntry() : 0u, target ? bot->GetExactDist(target) : 0.0f,
                  bot->isMoving() ? 1 : 0, uint32(bot->GetUnitMovementFlags()),
                  bot->HasUnitState(UNIT_STATE_LOST_CONTROL) ? 1 : 0,
                  bot->HasUnitState(UNIT_STATE_STUNNED) ? 1 : 0, bot->IsRooted() ? 1 : 0,
                  bot->IsMounted() ? 1 : 0, botAI->CanMove() ? 1 : 0,
                  victimCreature ? victimCreature->GetEntry() : 0u,
                  acid ? uint32(acid->GetStackAmount()) : 0u);
    }
}  // namespace

CthunAirborne GetCthunAirborneState(Player* bot, PlayerbotAI* botAI, float& groundZ)
{
    groundZ = INVALID_HEIGHT;
    if (bot->GetMapId() != AQ40_MAP_ID || (!IsCthunEncounterActive(bot) && !IsInCthunStomach(bot)))
        return CthunAirborne::None;
    // Runs every tick for every bot (the airborne trigger), so the stomach diagnostic rides here:
    // it must fire even when the close-in trigger is inactive.
    if (IsInCthunStomach(bot))
        CthunStomachDiag(bot, botAI);
    if (!botAI->CanMove() || !bot->movespline->Finalized() || bot->IsBeingTeleported())
        return CthunAirborne::None;

    // Hover teleport ONLY for the stomach landing (script drops swallowed players at z -70 over a
    // floor near -97). In the main room GetHeight finds a surface ~5y BELOW the real floor (108.9
    // read as 103.3), and a generic "z - ground > 3" rule teleported bots under the floor over and
    // over - they vanished and reappeared.
    if (IsInCthunStomach(bot) && bot->GetPositionZ() > CTHUN_STOMACH_HOVER_Z)
    {
        groundZ = bot->GetMap()->GetHeight(bot->GetPhaseMask(), bot->GetPositionX(), bot->GetPositionY(),
                                           bot->GetPositionZ(), true, CTHUN_GROUND_SEARCH);
        // Only trust a height that lands on the stomach floor; else use the known floor level.
        if (groundZ < CTHUN_STOMACH_FLOOR_MIN_Z || groundZ > CTHUN_STOMACH_HOVER_Z)
            groundZ = CTHUN_STOMACH_EXIT_Z;
        return CthunAirborne::Hovering;
    }
    if (bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR))
        return CthunAirborne::StaleFall;
    return CthunAirborne::None;
}

bool GetCthunStomachApproachSpot(Player* bot, PlayerbotAI* botAI, Position& spot)
{
    if (!IsInCthunStomach(bot) || IsCthunExitHolding(bot) || CthunExitOwed(bot) || !botAI->CanMove())
        return false;

    Creature* tentacle = nullptr;
    if (Unit* target = GetCthunAttackTarget(bot, botAI))
        if (Creature* c = target->ToCreature(); c && c->GetEntry() == NPC_CTHUN_FLESH_TENTACLE)
            tentacle = c;
    if (!tentacle && PlayerbotAI::IsHeal(bot))
    {
        uint32 found = 0;
        uint32 alive = 0;
        uint32 valid = 0;
        tentacle = SearchCthunFleshTentacle(bot, false, found, alive, valid);
    }
    if (!tentacle)
        return false;

    float const want = IsCthunRangedRole(bot) ? CTHUN_STOMACH_RANGED_RANGE : CTHUN_STOMACH_MELEE_RANGE;
    float const dx = bot->GetPositionX() - tentacle->GetPositionX();
    float const dy = bot->GetPositionY() - tentacle->GetPositionY();
    float const dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= want)
        return false;

    float const k = (want - 2.0f) / dist;
    float x = tentacle->GetPositionX() + dx * k;
    float y = tentacle->GetPositionY() + dy * k;
    float z = tentacle->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    spot.Relocate(x, y, z);
    return true;
}

bool CthunShouldExitStomach(Player* bot)
{
    if (!IsCthunEncounterActive(bot) || !IsInCthunStomach(bot))
        return false;
    if (IsCthunExitHolding(bot))
        return false;  // trigger sent; stand still while the script checks us
    return CthunExitOwed(bot);
}

// ---------------------------------------------------------------------------------------------
// DARK GLARE - a 5y-half-width ray from the Eye's facing, unlimited range, sweeping 180 deg over
// 35s in a random direction. TIME-TO-HIT model: with no direction tracking, assume the worst (the
// beam is sweeping toward the bot) and turn the bot's angular distance from the beam edge (half-
// width + pad, as an angle at its radius) into seconds. Bots move ~4s before the beam could reach
// them, rotating away from it on the side they are already on by ~9s of sweep from their OWN
// bearing (spacing kept). The wind-up (facing set, not yet firing) is covered because the freeze
// aura is already on.
// ---------------------------------------------------------------------------------------------
float CthunGlareSecondsToHit(Player* bot)
{
    if (IsInCthunStomach(bot))
        return 999.0f;

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (!snap.valid || !snap.glare)
        return 999.0f;

    float const r = bot->GetExactDist2d(snap.centerX, snap.centerY);
    if (r > CTHUN_GLARE_MAX_RANGE)
        return 999.0f;  // not in the room

    float const edge =
        std::asin(std::min(1.0f, (CTHUN_GLARE_HALF_WIDTH + CTHUN_GLARE_PAD) / std::max(r, 0.1f)));
    float const botAngle =
        std::atan2(bot->GetPositionY() - snap.centerY, bot->GetPositionX() - snap.centerX);
    float const rel = Position::NormalizeOrientation(botAngle - snap.beamAngle);
    float const dev = rel <= float(M_PI) ? rel : 2.0f * float(M_PI) - rel;  // [0, pi]
    return std::max(0.0f, dev - edge) / CTHUN_GLARE_SWEEP_RATE;
}

bool GetCthunGlareDodgeSpot(Player* bot, Position& spot)
{
    if (CthunGlareSecondsToHit(bot) >= CTHUN_GLARE_MOVE_SECONDS)
        return false;

    CthunSnapshot const snap = GetCthunSnapshot(bot);  // cached: valid + glare checked above
    float const r = bot->GetExactDist2d(snap.centerX, snap.centerY);
    float const botAngle =
        std::atan2(bot->GetPositionY() - snap.centerY, bot->GetPositionX() - snap.centerX);
    // [0, 2pi): below pi = counter-clockwise of the beam.
    float const rel = Position::NormalizeOrientation(botAngle - snap.beamAngle);
    float const side = rel <= float(M_PI) ? 1.0f : -1.0f;

    float const radius = std::max(r, 15.0f);
    float const edgeAtRadius =
        std::asin(std::min(1.0f, (CTHUN_GLARE_HALF_WIDTH + CTHUN_GLARE_PAD) / radius));
    float const sector = 2.0f * float(M_PI) / float(CTHUN_GLARE_SECTORS);
    // Rotate from the bot's OWN bearing, not to a common absolute angle: everyone in danger turns
    // the same way by about the same amount, so relative spacing (the melee pairs) is kept - a
    // shared target stacked the whole ring into one point for the next beam to chain. Snapped so
    // the destination is stable tick to tick (MoveTo refuses a creeping spot).
    float const step = edgeAtRadius + CTHUN_GLARE_SAFE_SECONDS * CTHUN_GLARE_SWEEP_RATE;
    float const want = std::round((botAngle + side * step) / sector) * sector;

    float x = snap.centerX + std::cos(want) * radius;
    float y = snap.centerY + std::sin(want) * radius;
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    spot.Relocate(x, y, z);
    return true;
}

bool CthunShouldDodgeGlare(Player* bot)
{
    Position spot;
    return GetCthunGlareDodgeSpot(bot, spot);
}

// ---------------------------------------------------------------------------------------------
// GREEN BEAM - clear the chain at its source: while the Eye (P1) or any Giant Eye Tentacle (P2)
// is casting it, every RANGED bot within 12y of the target steps away, AND a ranged target steps
// away from its own neighbours (it may have walked back into the pack, or been knocked into it by a
// tentacle). Melee ignore beams: they stand in fixed pairs ~12y apart, so a chain on a pair jumps
// once to the partner and stops.
// This is also the entire opening solution (first beam lands >= 5s after engage).
// ---------------------------------------------------------------------------------------------
bool IsCthunBeamTarget(Player* bot)
{
    for (Unit* target : CollectCthunBeamTargets(bot))
        if (target == bot)
            return true;
    return false;
}

Unit* GetCthunBeamThreat(Player* bot)
{
    // Opening no-go zone first: the puller draws the first beams, and in the gaps between casts
    // slot walks / melee reach would otherwise close back in on it.
    if (Player* puller = GetCthunOpeningPullerWithin(bot, CTHUN_PULLER_AVOID_RANGE))
        return puller;

    // Melee never react to Green Beam, as target or neighbour: they stand in fixed pairs ~12y apart
    // (ranged row 1 >= 12y out), so a chain on a pair jumps once to the partner and stops.
    if (!IsCthunRangedRole(bot))
        return nullptr;

    std::vector<Unit*> const targets = CollectCthunBeamTargets(bot);
    if (targets.empty())
        return nullptr;

    if (std::find(targets.begin(), targets.end(), static_cast<Unit*>(bot)) != targets.end())
        return FindCthunNeighbor(bot, CTHUN_BEAM_CLEAR_RANGE);  // the victim clears its neighbours

    Unit* nearest = nullptr;
    float nearestDist = CTHUN_BEAM_CLEAR_RANGE;
    for (Unit* target : targets)
    {
        float const d = bot->GetExactDist2d(target);
        if (d < nearestDist)
        {
            nearest = target;
            nearestDist = d;
        }
    }
    return nearest;
}

bool CthunShouldClearBeam(Player* bot)
{
    return GetCthunBeamThreat(bot) != nullptr;
}

bool GetCthunBeamClearSpot(Player* bot, Position& spot, uint32 attempt)
{
    Unit* threat = GetCthunBeamThreat(bot);
    if (!threat)
        return false;

    // Twins' sector-snapped escape builder is generic: clearance + 4y, attempt rotates 90 deg. The
    // puller zone flees to ~30y - outside the hold range, so issuing it also replaces any long
    // in-flight form-up path (walkers trail the puller at 25-30y); a beam chain to ~16y.
    float const clearance = threat == GetCthunOpeningPullerWithin(bot, CTHUN_PULLER_AVOID_RANGE)
                                ? CTHUN_PULLER_AVOID_RANGE + 1.0f
                                : CTHUN_BEAM_CLEAR_RANGE;
    return BuildTwinsEscapeSpot(bot, threat->GetPositionX(), threat->GetPositionY(), clearance,
                                attempt, spot);
}

// ---------------------------------------------------------------------------------------------
// OPENING - the Eye's first four Green Beams all go to the puller. It runs to the side of the room
// opposite the raid (ring reference + pi) so the beams land far from the pack while it spreads.
// ---------------------------------------------------------------------------------------------
bool IsCthunOpeningPuller(Player* bot)
{
    if (!IsCthunEncounterActive(bot) || IsInCthunStomach(bot))
        return false;

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (!snap.valid)
        return false;

    UpdateCthunOpening(bot, snap);

    std::lock_guard<std::mutex> lock(cthunMutex);
    auto it = cthunOpeningByInstance.find(bot->GetMap()->GetInstanceId());
    return it != cthunOpeningByInstance.end() && !it->second.over && it->second.puller == bot->GetGUID();
}

bool IsCthunOpeningActive(Player* bot)
{
    if (!IsCthunEncounterActive(bot))
        return false;

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (!snap.valid)
        return false;

    UpdateCthunOpening(bot, snap);

    std::lock_guard<std::mutex> lock(cthunMutex);
    auto it = cthunOpeningByInstance.find(bot->GetMap()->GetInstanceId());
    return it != cthunOpeningByInstance.end() && !it->second.over;
}

Player* GetCthunOpeningPullerWithin(Player* bot, float range)
{
    Player* puller = GetCthunOpeningPuller(bot);
    return puller && bot->GetExactDist2d(puller) < range ? puller : nullptr;
}

bool GetCthunPullerSpot(Player* bot, Position& spot)
{
    if (!IsCthunOpeningPuller(bot))
        return false;

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    // The ring reference is the bearing to the raid's centroid, so +pi is the far side.
    float const angle = GetCthunRingRef(bot, snap) + float(M_PI);
    float x = snap.centerX + std::cos(angle) * CTHUN_PULLER_RADIUS;
    float y = snap.centerY + std::sin(angle) * CTHUN_PULLER_RADIUS;
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    if (bot->GetExactDist2d(x, y) <= CTHUN_SLOT_TOLERANCE)
        return false;  // arrived: hold

    spot.Relocate(x, y, z);
    return true;
}

// ---------------------------------------------------------------------------------------------
// FIXED SPOTS for everyone, spaced past the ~13y effective chain jump by construction; no disperse.
// Every bot returns to its OWN spot whenever off it - Eye phase only; in the Body phase native reach
// puts melee on the body and walks ranged into range. Ranged stand on two interleaved circles and
// cast from there - all inside Eye casting range. Melee (tanks included) have paired
// spots in the Eye's reach (6 on a 15.5y ring + the centre, 2 per spot) and ignore Green Beam: a
// chain on a pair jumps once to the partner and stops. While the opening runs (until the puller's
// 4th beam has landed) melee stage 35y out at +-90 deg off the puller's lane instead.
// ---------------------------------------------------------------------------------------------
bool IsCthunRangedRole(Player* member)
{
    return !PlayerbotAI::IsTank(member) && (PlayerbotAI::IsHeal(member) || PlayerbotAI::IsRanged(member));
}

bool CthunShouldFormUp(Player* bot, PlayerbotAI* /*botAI*/)
{
    if (!IsCthunEncounterActive(bot) || IsInCthunStomach(bot))
        return false;

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (!snap.valid || snap.phase == CthunPhase::None || snap.glare)
        return false;  // the glare dodge owns movement; everyone walks back after it

    // Spots are an Eye-phase thing: in the Body phase native reach puts melee on the body and walks
    // ranged into range of the tentacles.
    if (snap.phase != CthunPhase::Eye)
        return false;
    bool const ranged = IsCthunRangedRole(bot);

    Position const spot = ComputeCthunSlot(bot, snap);
    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= CTHUN_SLOT_TOLERANCE)
        return false;

    if (CthunHazardMoveOwed(bot) || IsCthunOpeningPuller(bot) ||
        GetCthunOpeningPullerWithin(bot, CTHUN_PULLER_HOLD_RANGE))
        return false;

    // Ranged: don't walk near a beam in flight - or while being its target. Melee ignore beams.
    if (ranged)
        for (Unit* target : CollectCthunBeamTargets(bot))
            if (target == bot || bot->GetExactDist2d(target) < CTHUN_BEAM_HOLD_RANGE)
                return false;

    return true;
}

bool GetCthunFormUpSpot(Player* bot, PlayerbotAI* botAI, Position& spot)
{
    if (!CthunShouldFormUp(bot, botAI))
        return false;
    spot = ComputeCthunSlot(bot, GetCthunSnapshot(bot));
    return true;
}

// ---------------------------------------------------------------------------------------------
// TARGETING
// ---------------------------------------------------------------------------------------------
Unit* GetCthunAttackTarget(Player* bot, PlayerbotAI* /*botAI*/)
{
    if (!IsCthunEncounterActive(bot) || PlayerbotAI::IsHeal(bot))
        return nullptr;  // healers: no opinion

    if (IsInCthunStomach(bot))
    {
        if (IsCthunExitHolding(bot) || CthunExitOwed(bot))
            return nullptr;  // leaving: see IsCthunHoldingFire
        return PickCthunFleshTentacle(bot);
    }

    CthunSnapshot const snap = GetCthunSnapshot(bot);
    if (!snap.valid)
        return nullptr;

    Creature* body = GetTwin(bot, AQT_DATA_CTHUN);
    if (snap.weakened && body && body->IsAlive() && bot->IsValidAttackTarget(body))
        return body;

    bool const tank = PlayerbotAI::IsTank(bot);
    bool const ranged = !tank && PlayerbotAI::IsRanged(bot);

    if (snap.phase == CthunPhase::Eye)
    {
        if (tank)
        {
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_CLAW_TENTACLE}, false))
                return add;
        }
        else if (ranged)
        {
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_EYE_TENTACLE}, false, CTHUN_RANGED_ADD_RANGE))
                return add;
            if (Creature* add =
                    PickCthunAdd(bot, {NPC_CTHUN_CLAW_TENTACLE}, false, CTHUN_RANGED_CLAW_RANGE))
                return add;
        }
        else
        {
            // Only a claw that spawned on me: no cross-room claw chasing.
            if (Creature* add =
                    PickCthunAdd(bot, {NPC_CTHUN_CLAW_TENTACLE}, false, CTHUN_MELEE_ADD_RANGE))
                return add;
        }

        Creature* eye = GetTwin(bot, AQT_DATA_EYE_OF_CTHUN);
        if (eye && eye->IsAlive() && bot->IsValidAttackTarget(eye))
            return eye;
        return nullptr;
    }

    if (snap.phase == CthunPhase::Body)
    {
        // The Giant Eye Tentacle first for every non-tank damage dealer (melee included). P2 has no
        // ranged spots, so the picks use the full search range; native reach walks them in.
        if (!tank)
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_GIANT_EYE_TENTACLE}, false))
                return add;

        if (ranged)
        {
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_EYE_TENTACLE}, false))
                return add;
        }
        else
        {
            // A Giant Claw submerges when nobody is in its melee range; holding it is the tank's job.
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_GIANT_CLAW_TENTACLE}, false))
                return add;
            if (Creature* add = PickCthunAdd(bot, {NPC_CTHUN_CLAW_TENTACLE}, false,
                                             tank ? CTHUN_SEARCH_RANGE : CTHUN_MELEE_ADD_RANGE))
                return add;
            if (tank)
                return nullptr;  // no opinion: stock tank logic
        }

        // Under Carapace this is harmless and keeps rage up.
        if (body && body->IsAlive() && bot->IsValidAttackTarget(body))
            return body;
    }

    return nullptr;  // Eye -> body transition: no opinion
}

bool IsCthunHoldingFire(Player* bot, PlayerbotAI* /*botAI*/)
{
    // A WITHHOLDING decision needs its own predicate: a nullptr target reads as "no opinion" to the
    // target-hold multiplier, which would then let dps assist walk the bot back to a tentacle.
    if (!IsCthunEncounterActive(bot) || !IsInCthunStomach(bot))
        return false;
    return IsCthunExitHolding(bot) || CthunExitOwed(bot);
}

bool CthunShouldRetarget(Player* bot, PlayerbotAI* botAI)
{
    Unit* want = GetCthunAttackTarget(bot, botAI);
    return want && bot->GetVictim() != want;
}

bool CthunHazardMoveOwed(Player* bot)
{
    return CthunShouldExitStomach(bot) || IsCthunExitHolding(bot) || CthunShouldDodgeGlare(bot) ||
           CthunShouldClearBeam(bot);
}

bool CthunShouldDismount(Player* bot, PlayerbotAI* botAI)
{
    if (!bot->IsMounted() || !IsCthunEncounterActive(bot) || bot->HasAura(SPELL_CTHUN_MOUTH_TENTACLE))
        return false;
    // Riding in is fine (faster): stay mounted while a move is owed, dismount once in place.
    Position spot;
    if (CthunShouldFormUp(bot, botAI) || GetCthunPullerSpot(bot, spot) || CthunHazardMoveOwed(bot))
        return false;
    return true;
}

bool CthunShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI)
{
    if (!IsCthunEncounterActive(bot))
        return false;
    // The puller too, so reach/formation/follow can't drag it back while it holds the far side;
    // anyone near the opening puller, so reach/formation/follow can't close on it; and everyone on
    // or walking to a FIXED spot in the Eye phase (spots are in reach/casting range and the Eye
    // doesn't move) - so nothing chases off it. The Body phase has no spots: native reach works.
    bool const onFixedSpot =
        !IsInCthunStomach(bot) && GetCthunSnapshot(bot).phase == CthunPhase::Eye;
    return CthunHazardMoveOwed(bot) || IsCthunOpeningPuller(bot) ||
           GetCthunOpeningPullerWithin(bot, CTHUN_PULLER_HOLD_RANGE) != nullptr || onFixedSpot ||
           CthunShouldFormUp(bot, botAI);
}

}  // namespace TempleOfAhnQirajHelpers
