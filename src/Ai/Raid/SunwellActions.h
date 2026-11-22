/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Sunwell Plateau Eredar Twins: Grand Warlock Alythess's fire (WHE 11-22). Even with +140 fire resistance the raid died
// to it (Conflagration 27, Burn 14 of 49 deaths in one wipe, tp-20261122-074621-2):
//  - Conflagration (45342): an 8 yd fire area with a 1 s damage tick on a random target, so bots stacked together all
//    burn. Ranged and healers near Alythess keep more than 8 yd from every other group member.
//  - Blaze (45235/45236): leaves a fire patch (gameobject 187366, 5 yd) under the tank, which is Burn (46394).
//    Anybody within 6 yd of a patch steps 8 yd out of it.

#ifndef PLAYERBOTS_SUNWELLACTIONS_H
#define PLAYERBOTS_SUNWELLACTIONS_H

#include <cmath>
#include <list>

#include "Action.h"
#include "GameObject.h"
#include "Group.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Random.h"
#include "Trigger.h"

namespace SunwellEredar
{
    constexpr uint32 MAP = 580;
    constexpr uint32 NPC_ALYTHESS = 25166;
    constexpr uint32 GO_BLAZE = 187366;
    constexpr float BLAZE_AVOID = 6.0f;
    constexpr float BLAZE_STEP = 8.0f;
    constexpr float SPREAD = 8.5f;
    constexpr float SPREAD_STEP = 4.0f;
    // Spreading from every neighbour kept 25 bots moving and the fight ran into the 6-minute enrage (wipes at 5% and 3%
    // after 406/465 s, tp-20261122-100907-1). Move only out of a real clump.
    constexpr uint32 SPREAD_MIN_NEIGHBOURS = 3;

    inline Creature* FightingAlythess(Player* bot)
    {
        if (!bot || !bot->IsAlive() || bot->GetMapId() != MAP || !bot->IsInCombat())
            return nullptr;
        Creature* a = bot->FindNearestCreature(NPC_ALYTHESS, 100.0f, true);
        return a && a->IsInCombat() ? a : nullptr;
    }

    constexpr uint32 NPC_SACROLASH = 25165;
    constexpr float LEASH_SAFE = 35.0f;

    // Either twin dragged 50 yd from home casts Fireblast on the whole raid (CheckInRoom): an avoidance step that ended
    // far out walked a tank and her twin past it (tr-20261122-095342-2, Fireblast x14). Keep every step within 35 yd
    // of both twins' homes.
    inline bool InsideLeash(Player* bot, float x, float y)
    {
        for (uint32 entry : {NPC_ALYTHESS, NPC_SACROLASH})
            if (Creature* twin = bot->FindNearestCreature(entry, 150.0f, true))
                if (twin->GetHomePosition().GetExactDist2d(x, y) > LEASH_SAFE)
                    return false;
        return true;
    }

    inline bool InBlaze(WorldObject* from, float x, float y, float radius)
    {
        std::list<GameObject*> fires;
        from->GetGameObjectListWithEntryInGrid(fires, GO_BLAZE, 40.0f);
        for (GameObject* go : fires)
            if (go && go->isSpawned() && go->GetDistance2d(x, y) < radius)
                return true;
        return false;
    }

    inline GameObject* NearestBlaze(Player* bot)
    {
        std::list<GameObject*> fires;
        bot->GetGameObjectListWithEntryInGrid(fires, GO_BLAZE, BLAZE_AVOID);
        GameObject* best = nullptr;
        for (GameObject* go : fires)
            if (go && go->isSpawned() && (!best || bot->GetDistance2d(go) < bot->GetDistance2d(best)))
                best = go;
        return best;
    }

    // Centre of the group members closer than SPREAD, or false if nobody is.
    inline bool CrowdCentre(Player* bot, float& cx, float& cy)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        float sx = 0.0f, sy = 0.0f;
        uint32 n = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || m == bot || !m->IsAlive() || m->GetMapId() != bot->GetMapId() || bot->GetDistance2d(m) >= SPREAD)
                continue;
            sx += m->GetPositionX();
            sy += m->GetPositionY();
            ++n;
        }
        if (n < SPREAD_MIN_NEIGHBOURS)
            return false;
        cx = sx / n;
        cy = sy / n;
        return true;
    }

    inline bool Spreads(PlayerbotAI* botAI, Player* bot)
    {
        return !botAI->IsTank(bot) && (botAI->IsRanged(bot) || botAI->IsHeal(bot));
    }
}

class EredarBlazeTrigger : public Trigger
{
public:
    EredarBlazeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eredar blaze") {}
    bool IsActive() override { return SunwellEredar::FightingAlythess(bot) && SunwellEredar::NearestBlaze(bot); }
};

class EredarConflagrationSpreadTrigger : public Trigger
{
public:
    EredarConflagrationSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eredar conflagration spread") {}
    bool IsActive() override
    {
        float cx, cy;
        return SunwellEredar::FightingAlythess(bot) && SunwellEredar::Spreads(botAI, bot) &&
               SunwellEredar::CrowdCentre(bot, cx, cy);
    }
};

class EredarMoveOutOfBlazeAction : public MovementAction
{
public:
    EredarMoveOutOfBlazeAction(PlayerbotAI* botAI) : MovementAction(botAI, "eredar move out of blaze") {}
    bool Execute(Event /*event*/) override
    {
        GameObject* fire = SunwellEredar::NearestBlaze(bot);
        if (!fire)
            return false;
        float const base = std::atan2(bot->GetPositionY() - fire->GetPositionY(), bot->GetPositionX() - fire->GetPositionX());
        // Straight away from the patch first, then fan out so the step does not land in the next patch.
        for (float const turn : {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.8f, -1.8f})
        {
            float const x = bot->GetPositionX() + std::cos(base + turn) * SunwellEredar::BLAZE_STEP;
            float const y = bot->GetPositionY() + std::sin(base + turn) * SunwellEredar::BLAZE_STEP;
            if (SunwellEredar::InBlaze(bot, x, y, SunwellEredar::BLAZE_AVOID) || !SunwellEredar::InsideLeash(bot, x, y))
                continue;
            return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }
};

class EredarConflagrationSpreadAction : public MovementAction
{
public:
    EredarConflagrationSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "eredar conflagration spread") {}
    bool Execute(Event /*event*/) override
    {
        Creature* alythess = SunwellEredar::FightingAlythess(bot);
        float cx, cy;
        if (!alythess || !SunwellEredar::CrowdCentre(bot, cx, cy))
            return false;
        float base = std::atan2(bot->GetPositionY() - cy, bot->GetPositionX() - cx);
        if (bot->GetDistance2d(cx, cy) < 0.5f)
            base = frand(0.0f, 2.0f * float(M_PI));
        for (float const turn : {0.0f, 0.7f, -0.7f, 1.4f, -1.4f})
        {
            float const x = bot->GetPositionX() + std::cos(base + turn) * SunwellEredar::SPREAD_STEP;
            float const y = bot->GetPositionY() + std::sin(base + turn) * SunwellEredar::SPREAD_STEP;
            // Stay in casting range of Alythess and out of the fire patches.
            if (alythess->GetDistance2d(x, y) > 32.0f || SunwellEredar::InBlaze(bot, x, y, SunwellEredar::BLAZE_AVOID) ||
                !SunwellEredar::InsideLeash(bot, x, y))
                continue;
            return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }
};

class RaidSunwellTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidSunwellTriggerContext()
    {
        creators["eredar blaze"] = &RaidSunwellTriggerContext::blaze;
        creators["eredar conflagration spread"] = &RaidSunwellTriggerContext::spread;
    }

private:
    static Trigger* blaze(PlayerbotAI* ai) { return new EredarBlazeTrigger(ai); }
    static Trigger* spread(PlayerbotAI* ai) { return new EredarConflagrationSpreadTrigger(ai); }
};

class RaidSunwellActionContext : public NamedObjectContext<Action>
{
public:
    RaidSunwellActionContext()
    {
        creators["eredar move out of blaze"] = &RaidSunwellActionContext::blaze;
        creators["eredar conflagration spread"] = &RaidSunwellActionContext::spread;
    }

private:
    static Action* blaze(PlayerbotAI* ai) { return new EredarMoveOutOfBlazeAction(ai); }
    static Action* spread(PlayerbotAI* ai) { return new EredarConflagrationSpreadAction(ai); }
};

#endif
