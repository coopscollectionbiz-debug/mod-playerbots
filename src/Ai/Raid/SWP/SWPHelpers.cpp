/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPHelpers.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

#include "Spell.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"

namespace SunwellPlateauHelpers
{

    std::unordered_map<uint32, KalecgosSnapshot> kalecgosSnapshotByInstance;

    std::unordered_map<uint32, BrutallusSnapshot> brutallusSnapshotByInstance;

    const Position KALECGOS_PLATFORM_CENTER = { 1704.22f, 924.76f, 53.16f, 0.0f };

    const Position KALECGOS_TANK_POSITION = { 1704.22f, 934.00f, 53.16f, 0.0f };

    bool IsInSunwell(Player* bot)
    {
        return bot->GetMapId() == SUNWELL_MAP_ID;
    }

    bool IsInSpectralRealm(Player* bot)
    {
        // The aura is authoritative: the room has a lower level at z~15 UNDER the boss
        // platform, so a Z test misclassifies bots that fall or path off the platform -
        // and the spectral realm is an invisibility layer, so a misclassified bot can't
        // even see the units its triggers point it at. 46021 is applied by both entry
        // paths (spectral blast and rift click, via 46019); its removal ports the player
        // back up.
        return IsInSunwell(bot) &&
               bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_SPECTRAL_REALM));
    }

    // Cross-realm lookups must NOT use "find target" (threat-list only) nor
    // GetFirstAliveUnitByEntry (hostile "possible targets" value) - the other realm's boss
    // is ~127y above/below and never on either. A plain grid search works: 3D range,
    // LOS-free, radius safely under the ~533y Cell::Visit clamp.

    Creature* FindKalecgosDragon(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_KALECGOS_DRAGON), 250.0f);
    }

    Creature* FindSathrovarr(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_SATHROVARR), 250.0f);
    }

    Creature* FindKalecFriendly(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_KALEC_FRIENDLY), 250.0f);
    }

    Creature* FindBrutallus(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_BRUTALLUS), 250.0f);
    }

    bool CastClassTaunt(Player* bot, PlayerbotAI* botAI, Unit* target)
    {
        if (!target || !target->IsAlive())
            return false;

        switch (bot->getClass())
        {
            case CLASS_WARRIOR:
                return botAI->CastSpell("taunt", target);
            case CLASS_PALADIN:
                return botAI->CastSpell("hand of reckoning", target);
            case CLASS_DRUID:
                return botAI->CastSpell("growl", target);
            case CLASS_DEATH_KNIGHT:
                return botAI->CastSpell("dark command", target);
            default:
                return false;
        }
    }

    namespace
    {
        // The Main Assist slot flag. playerbots reads MEMBER_FLAG_MAINTANK (via
        // PlayerbotAI::GetMainTankGuid) but nothing anywhere reads MEMBER_FLAG_MAINASSIST, so the
        // off-tank half of a raid's assignments has to be looked up here. Empty when unassigned.
        ObjectGuid GetBrutallusMainAssistGuid(Group* group)
        {
            if (!group)
                return ObjectGuid::Empty;

            for (Group::member_citerator itr = group->GetMemberSlots().begin();
                 itr != group->GetMemberSlots().end(); ++itr)
            {
                if (itr->flags & MEMBER_FLAG_MAINASSIST)
                    return itr->guid;
            }

            return ObjectGuid::Empty;
        }
    }  // namespace

    // Resolve BOTH tank slots in one pass. Callers that classify every raid member (the seating
    // chart) must not re-derive this per member: GetMainTankGuid walks the group, so a per-member
    // call is O(n^2) on a path that runs for every queued action of every bot.
    static void GetBrutallusTankGuids(Group* group, PlayerbotAI* botAI, ObjectGuid& mainOut,
                                      ObjectGuid& offOut)
    {
        mainOut = PlayerbotAI::GetMainTankGuid(group);  // honours MEMBER_FLAG_MAINTANK
        offOut = GetBrutallusMainAssistGuid(group);

        // A Main Assist who is not a tank (the common case - it is a raid-marking role, not a
        // tanking one) is ignored, and the off tank falls back to the lowest-GUID alive tank bot
        // who is not the main tank. Deterministic, so every bot in the raid agrees without
        // sharing any state.
        Player* flagged = offOut ? ObjectAccessor::FindPlayer(offOut) : nullptr;
        if (flagged && botAI->IsTank(flagged) && offOut != mainOut)
            return;

        offOut = ObjectGuid::Empty;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                continue;
            if (!botAI->IsTank(member) || member->GetGUID() == mainOut)
                continue;
            if (!offOut || member->GetGUID() < offOut)
                offOut = member->GetGUID();
        }
    }

    BrutallusTankRole GetBrutallusTankRole(Player* bot, PlayerbotAI* botAI)
    {
        if (!botAI->IsTank(bot))
            return BrutallusTankRole::None;

        Group* group = bot->GetGroup();
        if (!group)
            return BrutallusTankRole::Main;  // solo tank: he is the anchor by default

        ObjectGuid mainGuid, offGuid;
        GetBrutallusTankGuids(group, botAI, mainGuid, offGuid);

        if (bot->GetGUID() == mainGuid)
            return BrutallusTankRole::Main;
        if (bot->GetGUID() == offGuid)
            return BrutallusTankRole::Off;
        return BrutallusTankRole::None;
    }

    uint32 GetBrutallusSlashStacks(Unit* unit)
    {
        if (!unit)
            return 0;

        Aura* aura = unit->GetAura(static_cast<uint32>(SunwellSpells::SPELL_METEOR_SLASH));
        return aura ? aura->GetStackAmount() : 0;
    }

    bool HasFresherBrutallusTank(Player* bot, PlayerbotAI* botAI, uint32 stacks)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                continue;
            if (!botAI->IsTank(member) || member->GetMapId() != bot->GetMapId())
                continue;
            if (GetBrutallusSlashStacks(member) < stacks)
                return true;
        }

        return false;
    }

    namespace
    {
        // ONE seating chart for the whole raid, not one pool per role.
        //
        // Per-role pools were a real defect: the cone offers 6 seats inside melee range and 4
        // outside, but a 25-man fields ~7 melee and ~13 ranged/healers. The ranged pool overflowed
        // its rings every pull and the overflow was pushed outward into a clamped pile - which is
        // both the "stacked super tight" look and a guaranteed Burn cascade, since a pile is 0y
        // apart and Burn jumps at 2y.
        //
        // So: melee sort FIRST (they need the inner rings - the only ones inside melee range) and
        // ranged/healers fill outward, and each bot takes the next seat regardless of role. Ties
        // break on GUID so every bot in the raid derives the same chart with no shared state.
        // DEAD members keep their seats: a death must not renumber everyone mid-fight.
        //
        // Returns false when the bot has no seat - not grouped, not a bot, one of the two assigned
        // tanks (they have their own fixed spots), or past the last seat in the cone. "No seat"
        // means generic behaviour, which parks a bot behind the boss and out of the cone; that is
        // a safe place to be, just not a useful one.
        bool BrutallusSeat(Player* bot, PlayerbotAI* botAI, uint32& lane, uint32& ring,
                           float& columnSign)
        {
            Group* group = bot->GetGroup();
            if (!group)
                return false;

            ObjectGuid mainGuid, offGuid;
            GetBrutallusTankGuids(group, botAI, mainGuid, offGuid);
            if (bot->GetGUID() == mainGuid || bot->GetGUID() == offGuid)
                return false;

            std::vector<std::pair<uint32, ObjectGuid>> chart;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !GET_PLAYERBOT_AI(member))
                    continue;  // humans are never assigned a station
                if (member->GetGUID() == mainGuid || member->GetGUID() == offGuid)
                    continue;
                // THREE tiers, and the middle one matters: melee innermost because only the inner
                // rings are inside melee range, then HEALERS, then ranged dps furthest out.
                //
                // Healers used to be lumped in with ranged dps, which put them on the 24-28y rings -
                // the furthest point in the formation from the Burn isolation zones, i.e. exactly
                // where they could not reach the bots who need healing most. Published strategies for
                // this fight put healers at the FRONT of each group for precisely that reason.
                uint32 order = 2u;                                    // ranged dps
                if (botAI->IsHeal(member))
                    order = 1u;                                       // healers
                else if (botAI->IsMelee(member))
                    order = 0u;                                       // melee: must be in melee range
                chart.emplace_back(order, member->GetGUID());
            }

            std::sort(chart.begin(), chart.end());
            auto it = std::find_if(chart.begin(), chart.end(),
                                   [bot](std::pair<uint32, ObjectGuid> const& e)
                                   { return e.second == bot->GetGUID(); });
            if (it == chart.end())
                return false;

            uint32 const index = uint32(it - chart.begin());
            lane = index % 2;                      // alternate lanes so both stay evenly manned
            uint32 const seat = index / 2;
            if (seat >= BRUTALLUS_SEATS_PER_LANE)
                return false;                      // cone is full

            ring = seat / 2;
            columnSign = (seat % 2 == 0) ? -1.0f : 1.0f;
            return true;
        }

    }  // namespace

    bool GetBrutallusStation(Player* bot, PlayerbotAI* botAI, Creature* boss, Position& station)
    {
        if (!boss)
            return false;

        BrutallusSnapshot const& snap = GetBrutallusSnapshot(bot);
        if (!snap.valid || !snap.anchorSet)
            return false;  // nothing to measure a formation from yet: fight normally

        // Lane 0 is the anchor axis (where the cone rests while the main tank holds); lane 1 is
        // BRUTALLUS_LANE_SEPARATION off it on the LOS-probed open side.
        float const lane1 = snap.anchorAxis + snap.laneSign * BRUTALLUS_LANE_SEPARATION;

        // ---- the two assigned tanks: fixed spots, and they NEVER quarantine ----
        // Order matters here. An earlier revision tested Burn first, so a burned OFF TANK was sent
        // 100+ degrees away - he abandoned lane 1, dropped the threat the handoff depends on, and
        // dragged his own lane's bots with him (observed). A tank's Burn is the raid's problem to
        // heal, not his to run from: the holder's is removed by his next Stomp anyway, and the main
        // tank may not move at all.
        // WHOEVER THE ANCHOR IS, he is never stationed - not just whoever holds the Main Tank flag.
        // If the main tank dies the latch releases and re-forms on the surviving tank, and that tank
        // is still role Off: sending him to lane 1 would order him 64 degrees away from the exact
        // spot the new axis was just measured from, the boss would follow, and the two would chase
        // each other around.
        BrutallusTankRole const tankRole = GetBrutallusTankRole(bot, botAI);
        if (tankRole == BrutallusTankRole::Main || snap.anchorGuid == bot->GetGUID())
            return false;  // frozen where he took the boss; he IS the anchor

        if (tankRole == BrutallusTankRole::Off)
        {
            // One lane over, and pulled COMFORTABLY inside melee range rather than mirroring the
            // main tank's radius: if both tanks sit at the edge of GetMeleeRange (~20.8y) the boss
            // steps toward whichever he is attacking, and every step invalidates the latched
            // geometry. See BRUTALLUS_OFFTANK_MAX_RADIUS.
            float const radius = std::min(snap.anchorRadius, BRUTALLUS_OFFTANK_MAX_RADIUS);
            station.Relocate(snap.anchorBossX + std::cos(lane1) * radius,
                             snap.anchorBossY + std::sin(lane1) * radius, snap.anchorZ);
            return true;
        }

        bool const burned = bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BURN_DOT));

        uint32 lane = 0;
        uint32 ring = 0;
        float columnSign = -1.0f;
        if (!BrutallusSeat(bot, botAI, lane, ring, columnSign))
        {
            // No seat in the cone. Normally that means "use generic behaviour" - but a BURNED bot
            // must still be isolated, and this was a real defect: unseated bots are the overflow,
            // generic behaviour clumps them together, and because every generic mover is vetoed
            // here nothing else could move them. They sat in a burning pile and spread it to each
            // other (observed - a stationary clump of ranged, all burning).
            if (!burned)
                return false;

            uint32 const seed = uint32(bot->GetGUID().GetCounter());
            lane = seed % 2;
            columnSign = (seed & 2u) ? 1.0f : -1.0f;
            ring = seed % 3;
        }

        float const laneBearing = (lane == 0) ? snap.anchorAxis : lane1;
        bool const shedding = GetBrutallusSlashStacks(bot) >= BRUTALLUS_BAIL_STACKS;

        float bearing;
        float radius;

        if (burned || shedding)
        {
            // Which way is "outward" for this lane, i.e. away from the other lane.
            float const outwardSign = (lane == 0) ? -snap.laneSign : snap.laneSign;
            bool const onOutwardColumn = (columnSign == outwardSign);

            // One slot per bot so nobody stacks (see BRUTALLUS_BURN_SLOT_ANGLE). The outward zones
            // are per-lane so 5 slots each suffice; the gap zone is SHARED by both lanes' inward
            // columns, so its slot index carries the lane too.
            uint32 const slot = onOutwardColumn ? ring : (lane * BRUTALLUS_RINGS + ring);
            uint32 const col = slot % BRUTALLUS_BURN_SLOT_COLUMNS;
            // Fills 0, +1, -1, +2, -2 - from the centre of the zone outward, so the first bots to
            // burn get the bearings with the most clearance.
            float const step = float((col + 1) / 2) * BRUTALLUS_BURN_SLOT_ANGLE *
                               ((col % 2 == 1) ? 1.0f : -1.0f);
            float const row = float(slot / BRUTALLUS_BURN_SLOT_COLUMNS) * BRUTALLUS_BURN_ROW_STEP;

            // The zone centre comes from the LOS-PROBED table, never recomputed here: a zone that
            // turned out to be inside the corridor wall has already been remapped, and recomputing
            // would send burned bots back into rock where their healers cannot see them.
            //   * outward column -> 90 degrees out into open ground, 78+ degrees clear of BOTH cones,
            //     and the walk crosses no seat because that side of the lane is empty;
            //   * inward column  -> the gap, because it cannot reach open ground (outward crosses its
            //     own frozen tank, further inward runs into the other lane's stack), pushed far
            //     enough out that boss drift cannot rotate it into a cone edge.
            uint32 const zone = lane * 2 + (onOutwardColumn ? 1u : 0u);
            bearing = snap.burnBearing[zone] + step;
            radius = snap.burnRange[zone] + row;
        }
        else
        {
            bearing = laneBearing + columnSign * BRUTALLUS_CONE_COLUMN_ANGLE;
            radius = BRUTALLUS_RING_RADII[ring];
        }

        // The LATCHED boss position, never the live one - see BRUTALLUS_LANE_SEPARATION. Z comes
        // from the anchor too: the tank stood there, so it is known-good ground.
        station.Relocate(snap.anchorBossX + std::cos(bearing) * radius,
                         snap.anchorBossY + std::sin(bearing) * radius, snap.anchorZ);
        return true;
    }

    // NEITHER snapshot getter may create an entry just by being asked. Using operator[] here
    // inserts one on every read, the out-of-combat "erase timers and trackers" action then
    // deletes it and reports SUCCESS - and because that action sits at ACTION_EMERGENCY + 11 and
    // a true return stops the engine for the tick, the bot silently stops doing anything else at
    // all: no follow, no stay, no commands, indistinguishable from a broken bot. That is exactly
    // what happened to the tank bots (tanks only, because the Brutallus multiplier was the one
    // caller that read a snapshot while out of combat).
    KalecgosSnapshot const& GetKalecgosSnapshot(Player* bot)
    {
        static KalecgosSnapshot const empty;
        if (!IsInSunwell(bot))
            return empty;

        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        time_t const now = std::time(nullptr);

        auto it = kalecgosSnapshotByInstance.find(instanceId);
        if (it != kalecgosSnapshotByInstance.end() && it->second.takenAt == now)
            return it->second;  // at most one refresh per second, whoever asks first pays

        Creature* dragon = FindKalecgosDragon(bot);
        Creature* demon = FindSathrovarr(bot);
        if (!dragon || !demon || !dragon->IsInCombat())
        {
            if (it != kalecgosSnapshotByInstance.end())
                kalecgosSnapshotByInstance.erase(it);  // encounter not running: keep the map empty
            return empty;
        }

        KalecgosSnapshot& snap = kalecgosSnapshotByInstance[instanceId];
        snap.takenAt = now;
        snap.valid = true;
        snap.dragonPct = dragon->GetHealthPct();
        snap.demonPct = demon->GetHealthPct();
        snap.dragonBanished = dragon->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BANISH));
        snap.demonBanished = demon->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BANISH));
        return snap;
    }

    BrutallusSnapshot const& GetBrutallusSnapshot(Player* bot)
    {
        static BrutallusSnapshot const empty;
        if (!IsInSunwell(bot))
            return empty;

        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        time_t const now = std::time(nullptr);

        auto it = brutallusSnapshotByInstance.find(instanceId);
        if (it != brutallusSnapshotByInstance.end() && it->second.takenAt == now)
            return it->second;

        Creature* boss = FindBrutallus(bot);

        if (!boss)
        {
            // THIS BOT cannot see him, which says nothing about whether the fight is running - it
            // is far away, or dead at a graveyard well past the 250y search. Keep the entry: it is
            // the whole raid's latched formation (see BRUTALLUS_SNAPSHOT_KEEP_MS).
            if (it == brutallusSnapshotByInstance.end())
                return empty;
            if (getMSTimeDiff(it->second.lastSeenMs, getMSTime()) < BRUTALLUS_SNAPSHOT_KEEP_MS)
                return it->second;
            brutallusSnapshotByInstance.erase(it);
            return empty;
        }

        if (!boss->IsAlive() || !boss->IsInCombat())
        {
            // Authoritative: a bot that CAN see him reports the encounter is over.
            if (it != brutallusSnapshotByInstance.end())
                brutallusSnapshotByInstance.erase(it);
            return empty;
        }

        BrutallusSnapshot& snap = brutallusSnapshotByInstance[instanceId];
        snap.lastSeenMs = getMSTime();
        bool const fresh = snap.takenAt == 0;
        snap.takenAt = now;
        snap.valid = true;
        snap.victimGuid = boss->GetVictim() ? boss->GetVictim()->GetGUID() : ObjectGuid::Empty;
        snap.bossX = boss->GetPositionX();
        snap.bossY = boss->GetPositionY();

        // First tick of this pull: stamp it, and start with no anchor. Everything below survives
        // the 1s refresh, and only the entry being erased (boss out of combat) clears it.
        if (fresh)
        {
            snap.combatStartMs = getMSTime();
            snap.anchorSet = false;
            snap.anchorGuid.Clear();
        }

        // A dead or vanished anchor releases the latch so the surviving tank can re-anchor - the
        // whole formation is measured from it, so a corpse must never keep defining the fight.
        if (snap.anchorSet)
        {
            Player* anchored = ObjectAccessor::FindPlayer(snap.anchorGuid);
            if (!anchored || !anchored->IsAlive() || anchored->GetMapId() != boss->GetMapId())
                snap.anchorSet = false;
        }

        if (!snap.anchorSet)
        {
            // Latch only once a TANK BOT is holding him in melee range AND HAS SETTLED there.
            // Two separate traps:
            //   * latching on aggro alone freezes him mid-approach, tens of yards short, because at
            //     pull time he and the boss are still running at each other;
            //   * latching the moment he arrives records a bearing he is about to leave - he lands
            //     moving (a warrior Charges) and then repositions, and because the boss turns to
            //     follow him the entire raid ends up lined up on a stale axis (observed twice).
            // isMoving() is only sampled once per second here, which is enough for a 2.5s window.
            Unit* victim = boss->GetVictim();
            Player* holder = victim ? victim->ToPlayer() : nullptr;
            bool const eligible = holder && holder->IsAlive() && GET_PLAYERBOT_AI(holder) &&
                                  PlayerbotAI::IsTank(holder) && holder->IsWithinMeleeRange(boss);

            if (!eligible || holder->isMoving() || snap.settleGuid != holder->GetGUID())
            {
                snap.settleGuid = eligible ? holder->GetGUID() : ObjectGuid::Empty;
                snap.settleSince = eligible ? getMSTime() : 0;
            }

            if (eligible && snap.settleSince &&
                getMSTimeDiff(snap.settleSince, getMSTime()) >= BRUTALLUS_ANCHOR_SETTLE_MS)
            {
                snap.anchorSet = true;
                snap.anchorGuid = holder->GetGUID();
                snap.anchorX = holder->GetPositionX();
                snap.anchorY = holder->GetPositionY();
                snap.anchorZ = holder->GetPositionZ();
                snap.anchorBossX = boss->GetPositionX();
                snap.anchorBossY = boss->GetPositionY();
                snap.anchorAxis = boss->GetAngle(holder);
                snap.anchorRadius = boss->GetExactDist2d(holder);

                // Which side lane 1 goes. The corridor is narrow and the anchor can be anywhere
                // in it, so this is probed rather than hardcoded: an LOS check from the boss to
                // each candidate tank spot rejects the side that is inside rock. Probed ONCE per
                // pull - it must never run per action.
                bool los[2];
                float x[2], y[2];
                for (uint32 i = 0; i < 2; ++i)
                {
                    float const sign = (i == 0) ? 1.0f : -1.0f;
                    float const bearing = snap.anchorAxis + sign * BRUTALLUS_LANE_SEPARATION;
                    x[i] = snap.anchorBossX + std::cos(bearing) * snap.anchorRadius;
                    y[i] = snap.anchorBossY + std::sin(bearing) * snap.anchorRadius;
                    los[i] = boss->IsWithinLOS(x[i], y[i], snap.anchorZ);
                }

                if (los[0] != los[1])
                    snap.laneSign = los[0] ? 1.0f : -1.0f;
                else
                    snap.laneSign = 1.0f;  // both open (or both blocked): either is as good

                // Probe all four Burn isolation zones. The laneSign probe above only established
                // that LANE 1's side is open; a zone inside the corridor wall makes the bot path to
                // wherever it can reach instead, out of its healers' line of sight, and Burn kills
                // it there. A blocked OUTWARD zone falls back to that lane's gap zone, which is open
                // by construction because both lanes are.
                for (uint32 lane = 0; lane < 2; ++lane)
                {
                    float const laneBearing =
                        snap.anchorAxis + (lane == 0 ? 0.0f : snap.laneSign * BRUTALLUS_LANE_SEPARATION);
                    float const outwardSign = (lane == 0) ? -snap.laneSign : snap.laneSign;

                    float const gapBearing = laneBearing - outwardSign * BRUTALLUS_BURN_GAP_ROTATION;
                    snap.burnBearing[lane * 2] = gapBearing;
                    snap.burnRange[lane * 2] = BRUTALLUS_BURN_GAP_RANGE;

                    float outBearing = laneBearing + outwardSign * BRUTALLUS_BURN_OUTWARD_ROTATION;
                    float outRange = BRUTALLUS_BURN_OUTWARD_RANGE;
                    float const ox = snap.anchorBossX + std::cos(outBearing) * outRange;
                    float const oy = snap.anchorBossY + std::sin(outBearing) * outRange;
                    if (!boss->IsWithinLOS(ox, oy, snap.anchorZ))
                    {
                        outBearing = gapBearing;
                        outRange = BRUTALLUS_BURN_GAP_RANGE;
                    }
                    snap.burnBearing[lane * 2 + 1] = outBearing;
                    snap.burnRange[lane * 2 + 1] = outRange;
                }

                // The quarantine bearings are logged as offsets from the anchor axis in DEGREES: a
                // Burn zones are logged as "<bearing off axis in degrees>@<radius>" in the order
                // lane0-gap, lane0-outward, lane1-gap, lane1-outward. An OUTWARD entry that reads the
                // same bearing/radius as its lane's GAP entry means it probed into rock and was
                // remapped - that is the thing to check when burned bots die without visibly being
                // hit by a slash.
                LOG_DEBUG("playerbots",
                          "[Brutallus] anchor {} at ({:.1f},{:.1f}) boss ({:.1f},{:.1f}) axis "
                          "{:.2f} r {:.1f} laneSign {:.0f} los {}/{} burn "
                          "{:.0f}@{:.0f} {:.0f}@{:.0f} {:.0f}@{:.0f} {:.0f}@{:.0f}",
                          holder->GetName(), snap.anchorX, snap.anchorY, snap.anchorBossX,
                          snap.anchorBossY, snap.anchorAxis, snap.anchorRadius, snap.laneSign,
                          los[0] ? 1 : 0, los[1] ? 1 : 0,
                          (snap.burnBearing[0] - snap.anchorAxis) * 180.0f / float(M_PI), snap.burnRange[0],
                          (snap.burnBearing[1] - snap.anchorAxis) * 180.0f / float(M_PI), snap.burnRange[1],
                          (snap.burnBearing[2] - snap.anchorAxis) * 180.0f / float(M_PI), snap.burnRange[2],
                          (snap.burnBearing[3] - snap.anchorAxis) * 180.0f / float(M_PI), snap.burnRange[3]);
            }
        }

        return snap;
    }

    // ---------------------------------------------------------------------------------------------
    // Felmyst

    Creature* FindFelmyst(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_FELMYST), 250.0f);
    }

    bool IsFelmystAirborne(Creature* boss)
    {
        return boss && boss->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
    }

    Creature* FindFelmystVaporChasingMe(Player* bot)
    {
        // Small radius on purpose: a vapor that is not near us yet is not our problem, and this runs
        // on a trigger tick.
        std::list<Creature*> vapors;
        bot->GetCreatureListWithEntryInGrid(vapors, static_cast<uint32>(SunwellNpcs::NPC_DEMONIC_VAPOR),
                                            60.0f);

        for (Creature* vapor : vapors)
        {
            if (!vapor->IsAlive())
                continue;

            // The summoner IS the chased player - npc_demonic_vapor::IsSummonedBy MoveFollows its
            // summoner, and 45391 force-casts the summon onto one random enemy. So this is an exact
            // test; there is no need to guess from proximity.
            if (TempSummon* summon = vapor->ToTempSummon())
                if (summon->GetSummonerGUID() == bot->GetGUID())
                    return vapor;
        }

        return nullptr;
    }

    int32 GetFelmystActiveLane(Creature* boss)
    {
        if (!boss)
            return -1;

        float bestDist = FELMYST_LANE_DETECT_RANGE;
        int32 bestLane = -1;

        // Nearest lane point within range...
        for (uint32 i = 0; i < 6; ++i)
        {
            float const d = boss->GetExactDist2d(FELMYST_LANE_POINT_X[i], FELMYST_LANE_POINT_Y[i]);
            if (d < bestDist)
            {
                bestDist = d;
                bestLane = int32(i % 3);
            }
        }

        if (bestLane < 0)
            return -1;

        // ...but only believed if no OTHER lane's point is nearly as close. She stages ~28y from the
        // middle lane point, so without this the middle lane would be "detected" every time she is
        // parked at a side and the raid would hop back and forth for nothing.
        for (uint32 i = 0; i < 6; ++i)
        {
            if (int32(i % 3) == bestLane)
                continue;

            float const d = boss->GetExactDist2d(FELMYST_LANE_POINT_X[i], FELMYST_LANE_POINT_Y[i]);
            if (d < bestDist + FELMYST_LANE_DETECT_MARGIN)
                return -1;  // ambiguous - hold position rather than guess
        }

        return bestLane;
    }

    bool IsInFelmystFogLane(float x, float y, int32 lane)
    {
        if (lane < 0 || lane > 2)
            return false;

        float const reach = FELMYST_FOG_RADIUS + FELMYST_FOG_MARGIN;

        // Top lane: its 17 extra triggers run east to x 1551 across the whole y range, so anything
        // east of the line is inside it.
        if (lane == 0)
            return x > FELMYST_LANE_TRIGGER_X[0] - reach && x < FELMYST_TOP_LANE_EAST_MAX + reach;

        // Middle and bottom are single line segments. Clamping y onto the segment gives the end caps
        // for free: a bot well north or south of the trigger field is clear at any x.
        float const laneX = FELMYST_LANE_TRIGGER_X[lane];
        float const clampedY = std::max(FELMYST_LANE_TRIGGER_Y_MIN,
                                        std::min(FELMYST_LANE_TRIGGER_Y_MAX, y));
        float const dx = x - laneX;
        float const dy = y - clampedY;

        return std::sqrt(dx * dx + dy * dy) < reach;
    }

    bool GetFelmystFogStation(Player* bot, PlayerbotAI* /*botAI*/, Creature* boss, Position& station,
                              uint32 attempt)
    {
        if (attempt >= FELMYST_REFUGE_CANDIDATES)
            return false;

        int32 const lane = GetFelmystActiveLane(boss);
        if (lane < 0)
            return false;

        if (!IsInFelmystFogLane(bot->GetPositionX(), bot->GetPositionY(), lane))
            return false;  // already clear - hold, and do not walk back into fading fog

        float const westEdge = FELMYST_LANE_LETHAL_WEST[lane];
        float const eastEdge = FELMYST_LANE_LETHAL_EAST[lane];

        // Is each side's refuge actually inside the room? The top lane has no east refuge (its own
        // triggers scatter east) and the bottom lane has no west one (that is outside the room).
        bool const westUsable = westEdge - FELMYST_FOG_MARGIN >= FELMYST_ROOM_X_MIN;
        bool const eastUsable = eastEdge + FELMYST_FOG_MARGIN <= FELMYST_ROOM_X_MAX;

        if (!westUsable && !eastUsable)
            return false;

        // Prefer the side the bot is already on, so the hop never routes across the live fog line.
        bool goWest = bot->GetPositionX() < FELMYST_LANE_TRIGGER_X[lane];
        if (goWest && !westUsable)
            goWest = false;
        else if (!goWest && !eastUsable)
            goWest = true;

        // The refuge, then progressively shorter steps toward it along the same axis (see
        // FELMYST_REFUGE_STEPS). Every candidate is a step in the SAFE direction, so whichever one
        // MoveTo accepts is strictly progress out of the band.
        float const refugeX = goWest ? (westEdge - FELMYST_FOG_MARGIN)
                                     : (eastEdge + FELMYST_FOG_MARGIN);

        float x;
        if (attempt == 0)
        {
            x = refugeX;
        }
        else
        {
            float const step = FELMYST_REFUGE_STEPS[attempt];
            x = goWest ? (bot->GetPositionX() - step) : (bot->GetPositionX() + step);

            // Never overshoot the refuge, and never step so short that it lands inside the kill
            // radius when a longer step was available - the keepout is the floor on usefulness.
            if (goWest)
            {
                x = std::max(x, refugeX);
                if (x > westEdge - FELMYST_FOG_KEEPOUT && x > refugeX)
                    x = westEdge - FELMYST_FOG_KEEPOUT;
            }
            else
            {
                x = std::min(x, refugeX);
                if (x < eastEdge + FELMYST_FOG_KEEPOUT && x < refugeX)
                    x = eastEdge + FELMYST_FOG_KEEPOUT;
            }

            // A step that would not actually move us is not worth an order.
            if (std::fabs(x - bot->GetPositionX()) < 1.0f)
                return false;
        }

        x = std::max(FELMYST_ROOM_X_MIN, std::min(FELMYST_ROOM_X_MAX, x));

        float const y = bot->GetPositionY();  // pure sideways hop: shortest, and keeps the raid spread

        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        station.Relocate(x, y, z);
        return true;
    }


    Player* FindFelmystEncapsulateVictim(Player* bot, Creature* boss)
    {
        if (!boss)
            return nullptr;

        // 1+2. Either Encapsulate aura on a raid member. 45665 is the one that pulses the damage;
        // 45661 is the channel. Both are checked because 45665 is applied through a spell LINK and
        // declares TARGET_UNIT_CASTER, so which unit it lands on is not obvious from the data.
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive())
                    continue;

                if (member->HasAura(static_cast<uint32>(SunwellSpells::SPELL_ENCAPSULATE_PULSE)) ||
                    member->HasAura(static_cast<uint32>(SunwellSpells::SPELL_ENCAPSULATE_CHANNEL)))
                {
                    return member;
                }
            }
        }

        // 3. Her cast/channel target. This is the EARLIEST signal - it exists before the pulse aura
        // lands - and the first 3500 tick comes one second in, so the head start is worth having.
        // Both slots are checked: the spell has a cast time, so it sits in the GENERIC slot first and
        // only becomes CHANNELED once the cast finishes.
        for (uint32 slot : { uint32(CURRENT_GENERIC_SPELL), uint32(CURRENT_CHANNELED_SPELL) })
        {
            Spell* spell = boss->GetCurrentSpell(slot);
            if (!spell || !spell->m_spellInfo)
                continue;

            if (spell->m_spellInfo->Id != static_cast<uint32>(SunwellSpells::SPELL_ENCAPSULATE_CHANNEL))
                continue;

            if (Unit* target = spell->m_targets.GetUnitTarget())
                if (Player* victim = target->ToPlayer())
                    if (victim->IsAlive())
                        return victim;
        }

        return nullptr;
    }

    bool FelmystShouldFleeEncapsulate(Player* bot, Creature* boss)
    {
        if (!boss || !boss->IsAlive() || !boss->IsInCombat())
            return false;

        // Ground phase only: the channel is cancelled at takeoff (scheduler.CancelAll), so a live
        // Encapsulate cannot coexist with the flight phase.
        if (IsFelmystAirborne(boss))
            return false;

        Player* const victim = FindFelmystEncapsulateVictim(bot, boss);
        if (!victim || victim == bot)
            return false;  // the victim is pacified and rooted - it cannot save itself

        return bot->GetExactDist2d(victim->GetPositionX(), victim->GetPositionY()) <
               FELMYST_ENCAPSULATE_FLEE;
    }

    bool GetFelmystEncapsulateFleeSpot(Player* bot, Player* victim, Position& spot, uint32 attempt)
    {
        if (!victim || attempt >= FELMYST_REFUGE_CANDIDATES)
            return false;

        // Straight away from the victim, so every bot keeps its own bearing and the raid fans out
        // instead of collapsing onto one escape point.
        float dx = bot->GetPositionX() - victim->GetPositionX();
        float dy = bot->GetPositionY() - victim->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.5f)
        {
            // Standing on top of the victim: pick a deterministic bearing from the GUID so the whole
            // stack does not pile onto one spot.
            float const bearing = float(bot->GetGUID().GetCounter() % 16u) * 2.0f * float(M_PI) / 16.0f;
            dx = std::cos(bearing);
            dy = std::sin(bearing);
            len = 1.0f;
        }

        // Ladder shortens the flee rather than freezing when the ground behind the bot is off-mesh -
        // never inside the blast, so the last candidate still clears the 20.4y radius. 2y per rung is
        // enough here: unlike the fog hop the distance is short, so the useful range is narrow.
        float const range = FELMYST_ENCAPSULATE_FLEE - float(attempt) * 2.0f;
        if (range < FELMYST_ENCAPSULATE_RADIUS + FELMYST_FOG_KEEPOUT)
            return false;

        float x = victim->GetPositionX() + dx / len * range;
        float y = victim->GetPositionY() + dy / len * range;
        x = std::max(FELMYST_ROOM_X_MIN, std::min(FELMYST_ROOM_X_MAX, x));

        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }


    uint32 CountFelmystGasNovaAfflicted(Player* bot)
    {
        uint32 count = 0;
        uint32 const gasNova = static_cast<uint32>(SunwellSpells::SPELL_GAS_NOVA);

        if (bot->HasAura(gasNova))
            ++count;

        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot || !member->IsAlive())
                    continue;

                if (!member->HasAura(gasNova))
                    continue;

                if (bot->GetExactDist2d(member->GetPositionX(), member->GetPositionY()) <=
                    FELMYST_MASS_DISPEL_RADIUS)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    bool FelmystShouldMassDispel(Player* bot, PlayerbotAI* botAI, Creature* boss)
    {
        if (bot->getClass() != CLASS_PRIEST)
            return false;

        uint32 const massDispel = static_cast<uint32>(SunwellSpells::SPELL_MASS_DISPEL);
        if (!bot->HasSpell(massDispel) || bot->HasSpellCooldown(massDispel))
            return false;

        if (!boss || !boss->IsAlive() || !boss->IsInCombat())
            return false;

        // A cast-time spell is cancelled outright if the bot is moving (playerbots' own
        // CastSpell does this), so never even try while repositioning - and never take a tick
        // away from a movement the raid cannot afford to lose.
        if (bot->isMoving())
            return false;

        if (FelmystShouldFleeEncapsulate(bot, boss))
            return false;

        Position station;
        if (GetFelmystFogStation(bot, botAI, boss, station))
            return false;  // owes a fog hop

        return CountFelmystGasNovaAfflicted(bot) >= FELMYST_GAS_NOVA_MIN_TARGETS;
    }


    Player* FindFelmystCharmedRaider(Player* bot)
    {
        uint32 const charm = static_cast<uint32>(SunwellSpells::SPELL_FOG_OF_CORRUPTION_CHARM);

        if (bot->HasAura(charm))
            return nullptr;  // charmed bots are not running this strategy for us

        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;

        Player* nearest = nullptr;
        float nearestDist = FELMYST_CHARMED_HUNT_RANGE;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;

            if (!member->HasAura(charm))
                continue;

            float const dist = bot->GetExactDist2d(member->GetPositionX(), member->GetPositionY());
            if (dist < nearestDist)
            {
                nearestDist = dist;
                nearest = member;
            }
        }

        return nearest;
    }

    bool GetFelmystRaidCentroid(Player* bot, Position& centroid)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint32 const charm = static_cast<uint32>(SunwellSpells::SPELL_FOG_OF_CORRUPTION_CHARM);
        float sumX = 0.0f;
        float sumY = 0.0f;
        uint32 count = 0;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;
            if (member->GetMapId() != bot->GetMapId())
                continue;
            if (member->HasAura(charm))
                continue;  // hostile and wandering - never follow them

            sumX += member->GetPositionX();
            sumY += member->GetPositionY();
            ++count;
        }

        if (!count)
            return false;

        float const x = sumX / float(count);
        float const y = sumY / float(count);
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        centroid.Relocate(x, y, z);
        return true;
    }

    bool FelmystShouldRegroup(Player* bot, PlayerbotAI* botAI, Creature* boss)
    {
        if (!boss || !boss->IsAlive() || !boss->IsInCombat())
            return false;

        if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_FOG_OF_CORRUPTION_CHARM)))
            return false;

        // Anything with a live reason to be where it is outranks regrouping.
        if (FindFelmystVaporChasingMe(bot))
            return false;
        if (FelmystShouldFleeEncapsulate(bot, boss))
            return false;

        Position station;
        if (GetFelmystFogStation(bot, botAI, boss, station))
            return false;  // owes a fog hop

        // NEVER walk back across a live fog lane. Holding for the few seconds a lane is up costs
        // nothing (~16s between strafes, and the entire ground phase is free), whereas crossing is
        // fatal - and it would oscillate against the fog hop, which outranks this.
        if (GetFelmystActiveLane(boss) >= 0)
            return false;

        Position centroid;
        if (!GetFelmystRaidCentroid(bot, centroid))
            return false;

        return bot->GetExactDist2d(centroid.GetPositionX(), centroid.GetPositionY()) >
               FELMYST_REGROUP_RANGE;
    }

    // ---------------------------------------------------------------------------------------------
    // Eredar Twins

    Creature* FindSacrolash(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_SACROLASH), 200.0f);
    }

    Creature* FindAlythess(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_ALYTHESS), 200.0f);
    }

    bool IsTwinsEncounterActive(Player* bot)
    {
        if (!IsInSunwell(bot))
            return false;

        // EITHER twin, because the whole fight has to keep working after one of them dies - and the
        // survivor is the half that gets harder (Empower fully heals it).
        for (Creature* twin : { FindSacrolash(bot), FindAlythess(bot) })
        {
            if (!twin || !twin->IsAlive() || !twin->IsInCombat())
                continue;

            if (bot->GetExactDist2d(twin->GetPositionX(), twin->GetPositionY()) <=
                TWINS_PARTICIPANT_RANGE)
            {
                return true;
            }
        }

        return false;
    }

    TwinsTankRole GetTwinsTankRole(Player* bot, PlayerbotAI* botAI)
    {
        if (!botAI->IsTank(bot))
            return TwinsTankRole::None;

        Group* group = bot->GetGroup();
        if (!group)
            return TwinsTankRole::Sacrolash;  // solo tank: she is the one that must be held

        // Reuse the raid-assignment reader the Brutallus formation uses - it is generic (Main Tank flag,
        // then Main Assist with a deterministic lowest-GUID fallback) and resolving both slots in one
        // pass matters here too, because this runs on a hot per-action path.
        ObjectGuid mainGuid, offGuid;
        GetBrutallusTankGuids(group, botAI, mainGuid, offGuid);

        if (bot->GetGUID() == mainGuid)
            return TwinsTankRole::Sacrolash;
        if (bot->GetGUID() == offGuid)
            return TwinsTankRole::Alythess;

        // The relief slot: next lowest-GUID alive tank BOT after the two assigned ones. Deterministic, so
        // every bot in the raid derives the same answer without sharing any state - and humans are
        // excluded (GET_PLAYERBOT_AI) because they would be "picked" and never comply.
        ObjectGuid reliefGuid;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                continue;
            if (!botAI->IsTank(member))
                continue;
            if (member->GetGUID() == mainGuid || member->GetGUID() == offGuid)
                continue;
            if (!reliefGuid || member->GetGUID() < reliefGuid)
                reliefGuid = member->GetGUID();
        }

        if (reliefGuid && bot->GetGUID() == reliefGuid)
            return TwinsTankRole::SacrolashRelief;

        return TwinsTankRole::None;
    }

    bool IsTwinsSacrolashTank(TwinsTankRole role)
    {
        return role == TwinsTankRole::Sacrolash || role == TwinsTankRole::SacrolashRelief;
    }

    bool TwinsShouldReliefTaunt(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        if (!IsTwinsSacrolashTank(GetTwinsTankRole(bot, botAI)))
            return false;

        Creature* sacrolash = FindSacrolash(bot);
        if (!sacrolash || !sacrolash->IsAlive())
            return false;

        // Never taunt a boss we already hold, and never from outside taunt range.
        Unit* victim = sacrolash->GetVictim();
        if (!victim || victim == bot)
            return false;

        if (bot->GetExactDist2d(sacrolash->GetPositionX(), sacrolash->GetPositionY()) >
            BRUTALLUS_TAUNT_RANGE)
        {
            return false;
        }

        // Only ever take her off the OTHER assigned Sacrolash tank. If a caster has somehow grabbed her
        // this stays quiet: rescuing that is ordinary aggro management, and cutting a caster's threat
        // (which the taunt action does) would just hand her to the next caster along - the Brutallus
        // lesson about never freezing whoever is currently holding.
        Player* holder = victim->ToPlayer();
        if (!holder || !IsTwinsSacrolashTank(GetTwinsTankRole(holder, botAI)))
            return false;

        // The whole trigger: the holder cannot steer. 6s of confused wandering with her in tow is what
        // walks her toward the 50y Fireblast leash.
        return holder->HasAura(static_cast<uint32>(SunwellSpells::SPELL_CONFOUNDING_BLOW));
    }

    bool TwinsShouldFixLeash(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        Creature* sacrolash = FindSacrolash(bot);
        if (!sacrolash || !sacrolash->IsAlive())
            return false;

        // Only the tank she is actually chasing can move her, and a confused one cannot move at all.
        if (sacrolash->GetVictim() != bot)
            return false;
        if (!IsTwinsSacrolashTank(GetTwinsTankRole(bot, botAI)))
            return false;
        if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_CONFOUNDING_BLOW)))
            return false;

        Position const& home = sacrolash->GetHomePosition();
        float const drift = sacrolash->GetExactDist2d(home.GetPositionX(), home.GetPositionY());

        // Hysteresis: walk home until comfortably inside the threshold, not merely under it, so this
        // cannot re-arm every few yards and turn into a shuffle.
        return drift > TWINS_LEASH_RANGE - TWINS_LEASH_TOLERANCE &&
               bot->GetExactDist2d(home.GetPositionX(), home.GetPositionY()) > TWINS_LEASH_TOLERANCE;
    }

    bool GetTwinsLeashSpot(Player* bot, Position& spot)
    {
        Creature* sacrolash = FindSacrolash(bot);
        if (!sacrolash)
            return false;

        Position const& home = sacrolash->GetHomePosition();

        float x = home.GetPositionX();
        float y = home.GetPositionY();
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }

    Player* FindTwinsConflagrationTarget(Player* bot)
    {
        uint32 const conflagration = static_cast<uint32>(SunwellSpells::SPELL_CONFLAGRATION);

        // Keyed on the SPELL ID across both twins, never on "the fire sister". If Alythess dies first
        // then Sacrolash inherits Conflagration (boss_sacrolash::DoAction schedules it on a 30-35s
        // repeat), so a caster-specific test would go quiet in the one case where the raid is already
        // fighting an empowered, fully-healed boss.
        for (Creature* twin : { FindSacrolash(bot), FindAlythess(bot) })
        {
            if (!twin)
                continue;

            // Both slots: the spell has a cast time, so it sits in the GENERIC slot for the ~3.5s that
            // is the entire reaction window.
            for (uint32 slot : { uint32(CURRENT_GENERIC_SPELL), uint32(CURRENT_CHANNELED_SPELL) })
            {
                Spell* spell = twin->GetCurrentSpell(slot);
                if (!spell || !spell->m_spellInfo)
                    continue;

                if (spell->m_spellInfo->Id != conflagration)
                    continue;

                if (Unit* target = spell->m_targets.GetUnitTarget())
                    if (Player* victim = target->ToPlayer())
                        if (victim->IsAlive())
                            return victim;
            }
        }

        return nullptr;
    }

    Player* FindTwinsConflagrationBomb(Player* bot)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;

        uint32 const conflagration = static_cast<uint32>(SunwellSpells::SPELL_CONFLAGRATION);
        Player* const castTarget = FindTwinsConflagrationTarget(bot);

        Player* nearest = nullptr;
        float best = 0.0f;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;
            if (member->GetMapId() != bot->GetMapId())
                continue;

            // A bomb is either about to become one (still being cast at) or already carrying the aura.
            // Both matter: the cast window is the only time a neighbour can avoid being given the aura
            // itself, and the aura's 10s is how long it keeps pulsing 1600 into everything within 8y.
            if (member != castTarget && !member->HasAura(conflagration))
                continue;

            float const dist = bot->GetExactDist2d(member->GetPositionX(), member->GetPositionY());
            if (!nearest || dist < best)
            {
                nearest = member;
                best = dist;
            }
        }

        return nearest;
    }

    bool TwinsShouldFleeConflagration(Player* bot)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        // Cast window only. The aura carries SPELL_AURA_MOD_CONFUSE, so the moment it lands the victim
        // has lost control (PlayerbotAI::CanMove reads false) and the blast is already committed to
        // wherever it is standing - there is nothing left for it to do.
        return FindTwinsConflagrationTarget(bot) == bot;
    }

    bool TwinsShouldClearConflagration(Player* bot)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        // The victim has its own outward run at a higher relevance; it must not also be trying to clear
        // itself, which would fight that run for the same tick.
        if (FindTwinsConflagrationTarget(bot) == bot)
            return false;

        Player* bomb = FindTwinsConflagrationBomb(bot);
        if (!bomb)
            return false;

        return bot->GetExactDist2d(bomb->GetPositionX(), bomb->GetPositionY()) <
               TWINS_CONFLAG_CLEAR_RANGE;
    }

    bool GetTwinsConflagrationFleeSpot(Player* bot, Position& spot, uint32 attempt)
    {
        if (attempt >= TWINS_FLEE_CANDIDATES)
            return false;

        Position centroid;
        if (!GetTwinsRaidCentroid(bot, centroid))
            return false;

        float dx = bot->GetPositionX() - centroid.GetPositionX();
        float dy = bot->GetPositionY() - centroid.GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.5f)
        {
            // Standing on the raid's centre of mass: take a deterministic bearing from the GUID rather
            // than a degenerate one, so two simultaneous victims still leave on different headings.
            float const bearing = float(bot->GetGUID().GetCounter() % 16u) * 2.0f * float(M_PI) / 16.0f;
            dx = std::cos(bearing);
            dy = std::sin(bearing);
            len = 1.0f;
        }

        // Every rung still clears the 8.4y blast, so even the shortest accepted run takes the bomb out
        // of the stack. That is the whole reason the ladder varies the DISTANCE and not the bearing.
        float const range = TWINS_CONFLAG_VICTIM_FLEE - float(attempt) * 3.0f;
        if (range < TWINS_CONFLAG_RADIUS + 2.0f)
            return false;

        float x = centroid.GetPositionX() + dx / len * range;
        float y = centroid.GetPositionY() + dy / len * range;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }

    bool GetTwinsConflagrationClearSpot(Player* bot, Player* bomb, Position& spot, uint32 attempt)
    {
        if (!bomb || attempt >= TWINS_FLEE_CANDIDATES)
            return false;

        float dx = bot->GetPositionX() - bomb->GetPositionX();
        float dy = bot->GetPositionY() - bomb->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.5f)
        {
            float const bearing = float(bot->GetGUID().GetCounter() % 16u) * 2.0f * float(M_PI) / 16.0f;
            dx = std::cos(bearing);
            dy = std::sin(bearing);
            len = 1.0f;
        }

        float const range = TWINS_CONFLAG_CLEAR_TO - float(attempt) * 1.5f;
        if (range < TWINS_CONFLAG_RADIUS + 2.0f)
            return false;

        float x = bomb->GetPositionX() + dx / len * range;
        float y = bomb->GetPositionY() + dy / len * range;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }

    uint32 GetTwinsTouchStacks(Unit* unit, uint32 spellId)
    {
        if (!unit)
            return 0;

        Aura* aura = unit->GetAura(spellId);
        return aura ? aura->GetStackAmount() : 0;
    }

    bool GetTwinsBlazeStation(Player* bot, Position& station, bool& seeking)
    {
        seeking = false;

        if (!IsTwinsEncounterActive(bot))
            return false;

        // Conflagration outranks all footwork: its pulses are 3200/s against a blaze patch's trickle,
        // and standing down here is what stops the two from fighting over the same tick.
        if (TwinsShouldFleeConflagration(bot) || TwinsShouldClearConflagration(bot))
            return false;

        uint32 const blaze = static_cast<uint32>(SunwellObjects::GO_BLAZE);

        // Dark Flame means the bot is already immune to both touches for the next few seconds, so a
        // cleanse trip would be walking for nothing.
        bool const wantsFire =
            !bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_DARK_FLAME)) &&
            GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_DARK_TOUCHED)) >=
                TWINS_TOUCH_CLEANSE_STACKS;

        if (wantsFire)
        {
            GameObject* patch = bot->FindNearestGameObject(blaze, TWINS_BLAZE_SEEK_RANGE);
            if (!patch)
                return false;  // no fire to be had - carry the stacks and let the healers cope

            // THE COMPLETION TEST IS THE STACK COUNT, NOT PROXIMITY, and that is deliberate. The patch
            // is only 2.5y across, while bots settle a couple of yards off any requested point - so a
            // proximity tolerance wide enough to be honest (>=5y, per BRUTALLUS_STATION_TOLERANCE) would
            // declare the trip finished with the bot still outside the fire. Re-issuing until the aura
            // actually converts is the only arrival test that cannot round the wrong way; it is the same
            // correction the Felmyst fog trigger needed.
            seeking = true;
            station.Relocate(patch->GetPositionX(), patch->GetPositionY(), patch->GetPositionZ());
            return true;
        }

        // Not seeking, so fire is pure damage. Only react to a patch we are actually standing in - the
        // radius plus a little for settling error - and then step clear of it.
        GameObject* patch = bot->FindNearestGameObject(blaze, TWINS_BLAZE_RADIUS + 1.5f);
        if (!patch)
            return false;

        float dx = bot->GetPositionX() - patch->GetPositionX();
        float dy = bot->GetPositionY() - patch->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.5f)
        {
            float const bearing = float(bot->GetGUID().GetCounter() % 16u) * 2.0f * float(M_PI) / 16.0f;
            dx = std::cos(bearing);
            dy = std::sin(bearing);
            len = 1.0f;
        }

        // TWINS_BLAZE_STEP_OUT is comfortably more than twice the radius, so one step always clears the
        // patch it was in. Landing in a DIFFERENT patch is possible - they cluster under Alythess' victim,
        // who gets a fresh one every ~3.8s - and the bot simply steps again. That is progress rather than
        // an oscillation, because each step is measured from whichever patch is nearest NOW.
        float x = patch->GetPositionX() + dx / len * TWINS_BLAZE_STEP_OUT;
        float y = patch->GetPositionY() + dy / len * TWINS_BLAZE_STEP_OUT;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        station.Relocate(x, y, z);
        return true;
    }

    bool TwinsShouldWorkBlaze(Player* bot)
    {
        Position station;
        bool seeking = false;
        return GetTwinsBlazeStation(bot, station, seeking);
    }

    bool TwinsShouldSeekShadow(Player* bot)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        if (TwinsShouldFleeConflagration(bot) || TwinsShouldClearConflagration(bot))
            return false;

        if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_DARK_FLAME)))
            return false;

        if (GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_FLAME_TOUCHED)) <
            TWINS_TOUCH_CLEANSE_STACKS)
        {
            return false;
        }

        // Shadow Blades is the only shadow source a bot can choose to walk into, and it is hers. Once
        // she is dead the survivor's Shadow Nova is the only shadow left, and that lands 10y around a
        // random raider - nothing to aim at, so there is no trip worth making.
        Creature* sacrolash = FindSacrolash(bot);
        if (!sacrolash || !sacrolash->IsAlive())
            return false;

        return bot->GetExactDist2d(sacrolash->GetPositionX(), sacrolash->GetPositionY()) >
               TWINS_SHADOW_SEEK_RANGE;
    }

    bool GetTwinsShadowSeekSpot(Player* bot, Position& spot)
    {
        Creature* sacrolash = FindSacrolash(bot);
        if (!sacrolash)
            return false;

        // In along the bot's OWN bearing, stopping at the edge of the 20y Shadow Blades radius. Keeping
        // its own bearing means bots converging from different sides never pile onto one point, and
        // stopping at the edge keeps them out of the melee stack where Conflagration lands.
        float dx = bot->GetPositionX() - sacrolash->GetPositionX();
        float dy = bot->GetPositionY() - sacrolash->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.5f)
            return false;

        float x = sacrolash->GetPositionX() + dx / len * TWINS_SHADOW_SEEK_RANGE;
        float y = sacrolash->GetPositionY() + dy / len * TWINS_SHADOW_SEEK_RANGE;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }

    bool TwinsIsAlythessSoloPhase(Player* bot)
    {
        // FindNearestCreature defaults to alive = true, so this goes null the moment she dies - which is
        // exactly the phase edge we want, and also the reason the phase-1 cleanse silently stopped working
        // here instead of reporting anything.
        if (FindSacrolash(bot))
            return false;

        Creature* alythess = FindAlythess(bot);
        return alythess && alythess->IsAlive() && alythess->IsInCombat();
    }

    bool TwinsShouldStackOnAlythess(Player* bot)
    {
        if (!IsTwinsEncounterActive(bot) || !TwinsIsAlythessSoloPhase(bot))
            return false;

        // Conflagration is cancelled in this phase, but check anyway: if one is somehow live, clearing it
        // beats clumping, and this keeps the two from arguing over the same tick.
        if (TwinsShouldFleeConflagration(bot) || TwinsShouldClearConflagration(bot))
            return false;

        // NEVER pull a bot back toward fire it just stepped out of. Blaze lands on her victim, i.e. in the
        // middle of the clump, so without this the stack and the footwork would trade the same bot back and
        // forth across a patch that lives 15s. Footwork owns the bot until it is clear.
        if (TwinsShouldWorkBlaze(bot))
            return false;

        Creature* alythess = FindAlythess(bot);
        if (!alythess)
            return false;

        return bot->GetExactDist2d(alythess->GetPositionX(), alythess->GetPositionY()) >
               TWINS_STACK_RANGE;
    }

    char const* TwinsOffensiveDispelSpell(Player* bot)
    {
        // Delegates: the same three spells are the whole answer to M'uru's Dark Fiends, so the table
        // lives in one place now. Kept as its own name because the Twins triggers read better for it.
        return SwpOffensiveDispelSpell(bot);
    }

    bool TwinsShouldDispelPyrogenics(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsTwinsEncounterActive(bot))
            return false;

        if (!TwinsOffensiveDispelSpell(bot))
            return false;

        // Never at the cost of a movement this fight has already decided on: playerbots cancels a
        // cast-time spell the instant the bot moves, so a dispel competing with a Conflagration flee
        // loses both. Same rule as the Felmyst Mass Dispel.
        if (TwinsShouldSuppressGenericMovement(bot, botAI))
            return false;

        Creature* alythess = FindAlythess(bot);
        if (!alythess || !alythess->IsAlive())
            return false;

        if (!alythess->HasAura(static_cast<uint32>(SunwellSpells::SPELL_PYROGENICS)))
            return false;

        return bot->GetExactDist2d(alythess->GetPositionX(), alythess->GetPositionY()) <=
               TWINS_DISPEL_RANGE;
    }

    bool GetTwinsRaidCentroid(Player* bot, Position& centroid)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        float sumX = 0.0f;
        float sumY = 0.0f;
        uint32 count = 0;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;
            if (member->GetMapId() != bot->GetMapId())
                continue;

            sumX += member->GetPositionX();
            sumY += member->GetPositionY();
            ++count;
        }

        if (!count)
            return false;

        float const x = sumX / float(count);
        float const y = sumY / float(count);
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        centroid.Relocate(x, y, z);
        return true;
    }

    bool TwinsShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI)
    {
        // NARROW BY DESIGN - only while a move is actually owed. Felmyst's veto covers a whole phase, and
        // that is what stranded its vapor carriers: with ReachTargetAction gone for the duration, a bot
        // sent away had no generic way back and needed a bespoke regroup action to rescue it. Here
        // generic movement is right for most of the fight (melee chase, ranged hold range), so it is
        // taken away only for the seconds a bot is executing one of these moves, and the way home comes
        // back on its own the moment the move is done.
        return TwinsShouldFleeConflagration(bot) || TwinsShouldClearConflagration(bot) ||
               TwinsShouldWorkBlaze(bot) || TwinsShouldSeekShadow(bot) ||
               TwinsShouldFixLeash(bot, botAI) || TwinsShouldStackOnAlythess(bot);
    }

    // ---------------------------------------------------------------------------------------------
    // M'uru / Entropius

    char const* SwpOffensiveDispelSpell(Player* bot)
    {
        switch (bot->getClass())
        {
            case CLASS_SHAMAN:
                return "purge";
            case CLASS_PRIEST:
                return "dispel magic";
            case CLASS_MAGE:
                return "spellsteal";
            default:
                return nullptr;
        }
    }

    // Retry rungs, expressed as SECTOR offsets rather than raw angles: same sector, then the neighbours.
    // The ladder itself exists for the usual reason - an authored destination can be off-mesh, and MoveTo
    // answers that by silently refusing forever rather than by failing loudly.
    constexpr int32 MURU_SECTOR_STEPS[MURU_FLEE_CANDIDATES] = { 0, 1, -1, 2, -2 };

    // Shared escape-point builder for all four M'uru hazards.
    //
    // The bearing from the hazard to the bot is SNAPPED to the centre of one of MURU_CLEAR_SECTORS fixed
    // sectors. That is the whole point: the destination then stays bit-identical across ticks while the bot
    // walks radially outward, so MoveTo's IsDuplicateMove keeps the original order alive and the spline
    // completes. Rev 2 fed the bot's live position straight in, so the destination crept every tick and
    // 130 of 268 Darkness escapes accepted no order at all.
    //
    // Keeping each bot in its OWN sector also means bots escaping a shared hazard fan out to where they
    // already were instead of funnelling onto one point, which is what made Brutallus' authored stations
    // oscillate.
    bool BuildMuruEscapeSpot(Player* bot, float centerX, float centerY, float radius, uint32 attempt,
                             Position& spot)
    {
        float const dx = bot->GetPositionX() - centerX;
        float const dy = bot->GetPositionY() - centerY;
        float const len = std::sqrt(dx * dx + dy * dy);
        float const raw = (len < 0.5f) ? bot->GetOrientation() : std::atan2(dy, dx);

        float const sectorWidth = 2.0f * float(M_PI) / float(MURU_CLEAR_SECTORS);
        int32 sector = int32(std::floor(raw / sectorWidth));
        sector += MURU_SECTOR_STEPS[attempt % MURU_FLEE_CANDIDATES];

        float const snapped = (float(sector) + 0.5f) * sectorWidth;

        float x = centerX + std::cos(snapped) * radius;
        float y = centerY + std::sin(snapped) * radius;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        spot.Relocate(x, y, z);
        return true;
    }

    Creature* FindMuru(Player* bot)
    {
        // NOT gated on alive-and-visible: he is clamped to 1 HP and hidden at handoff but stays the
        // anchor for the Darkness geometry and the "stop attacking me" flag for another 7 seconds.
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_MURU), MURU_SEARCH_RANGE);
    }

    Creature* FindEntropius(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_ENTROPIUS),
                                        MURU_SEARCH_RANGE);
    }

    bool IsMuruEncounterActive(Player* bot)
    {
        // DELIBERATELY FREE OF GRID SEARCHES. This is the first line of all four hazard predicates, each
        // of which the movement multiplier evaluates for EVERY queued action, for every bot, every tick.
        // A boss-presence version cost two FindNearestCreature calls per call and up to eight per
        // multiplier evaluation - the same per-tick world-query trap that produced 400-500ms world ticks
        // in Wintergrasp.
        if (!IsInSunwell(bot))
            return false;

        // THE ENGAGE GATE IS THE INSTANCE BOSS STATE. `bot->IsInCombat()` plus geometry was NOT good
        // enough, and the difference was a wipe-grade bug rather than a nicety.
        //
        // The old form said "in Sunwell, in combat with anything, in the z band, within 120y of his
        // spawn". Eleven Sunwell Honor Guards (37781) spawn 42.5y from M'uru at z 71.2, i.e. INSIDE that
        // volume. Pulling that trash pack therefore satisfied the gate - and because Honor Guards match no
        // lane in GetMuruKillOrder, GetMuruFocusTarget fell all the way through to its last resort and
        // handed every DPS bot M'uru himself (FindMuru reaches 200y). `muru focus target` then charged the
        // raid 42y into the room while MuruTargetHoldMultiplier held the generic choosers vetoed. The room
        // door is DOOR_TYPE_ROOM on DATA_MURU, so it shut behind them and locked the player OUT of the
        // fight the bots had just started.
        //
        // The boss state cannot be fooled that way, is authoritative for "has the player engaged", and is
        // CHEAPER than what it replaces: InstanceScript::GetBossState is a bounds-checked vector index,
        // against no grid search and not even an IsInCombat walk. Every other boss in this file gates on
        // boss->IsInCombat() - one grid search each - so this is the better pattern, not a concession.
        InstanceScript* instance = bot->GetInstanceScript();
        if (!instance || instance->GetBossState(SWP_DATA_MURU) != IN_PROGRESS)
            return false;

        // bot->IsInCombat() is deliberately NOT retained: the boss state already proves the fight is on,
        // and requiring bot combat would keep a not-yet-aggroed bot (a fresh resurrect, a ranged bot
        // nothing has swung at) from dodging Darkness. The hazard predicates each still prove their OWN
        // hazard exists - a live Darkness DynamicObject, a fiend carrying 45934, an armed singularity, a
        // Sentinel within 10y - so this gate has never been load-bearing on its own.

        // The Sunwell rooms stack VERTICALLY at nearly the same x/y - the Eredar Twins sit directly below
        // this one at z 33.4 and the approach trash above at z 85.3 - so the z band is what identifies the
        // room, not the 2D distance. M'uru's own aggro gate uses z > 69.0 for the same reason.
        float const z = bot->GetPositionZ();
        if (z < 55.0f || z > 82.0f)
            return false;

        return bot->GetExactDist2d(MURU_HOME_X, MURU_HOME_Y) <= MURU_PARTICIPANT_RANGE;
    }

    bool MuruIsHandedOff(Player* bot)
    {
        Creature* muru = FindMuru(bot);
        return muru && muru->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    }

    bool MuruIsEntropiusPhase(Player* bot)
    {
        Creature* entropius = FindEntropius(bot);
        return entropius && entropius->IsAlive();
    }

    bool GetMuruDarknessCenter(Player* bot, Position& center)
    {
        Creature* muru = FindMuru(bot);
        if (!muru)
            return false;

        // Three signals, in order of how much warning they give:
        //   * the 45999 pre-effect on M'uru - 3 seconds BEFORE the zone lands, which is the only way a
        //     bot leaves without eating a 3000 tick it cannot be healed for;
        //   * the live DynamicObject for 45996 - the zone itself, for the 20s it exists;
        //   * 45996 on the bot - last-resort proof it is standing in one.
        bool const live = muru->HasAura(static_cast<uint32>(SunwellSpells::SPELL_MURU_DARKNESS_PRE)) ||
                          muru->GetDynObject(static_cast<uint32>(SunwellSpells::SPELL_MURU_DARKNESS)) ||
                          bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_MURU_DARKNESS));
        if (!live)
            return false;

        // 45996 is cast at TARGET_DEST_CASTER and M'uru never moves, so he IS the centre.
        center.Relocate(muru->GetPositionX(), muru->GetPositionY(), muru->GetPositionZ());
        return true;
    }

    bool MuruShouldClearDarkness(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        Position center;
        if (!GetMuruDarknessCenter(bot, center))
            return false;

        float const distance = bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY());

        // Case 1: inside (or on the lip). Leave.
        if (distance < MURU_DARKNESS_RADIUS + MURU_DARKNESS_TOLERANCE)
            return true;

        // Case 2: already outside, but melee with nothing to hit except a boss who is STANDING IN THE
        // ZONE. M'uru is the centre of his own Darkness, so generic movement would walk this bot back in
        // the instant the veto lifted - out, in, out, in, for the full 20 seconds, dealing no damage and
        // taking 3000 per second on every pass. Staying true parks it outside instead.
        //
        // Ranged need no equivalent: their preferred range already sits outside 21y.
        if (!botAI->IsMelee(bot) || botAI->IsTank(bot) || botAI->IsHeal(bot))
            return false;

        Unit* victim = bot->GetVictim();
        Creature* muru = FindMuru(bot);
        return muru && victim == muru;
    }

    bool GetMuruDarknessClearSpot(Player* bot, Position& spot, uint32 attempt)
    {
        Position center;
        if (!GetMuruDarknessCenter(bot, center))
            return false;

        // Already clear: no destination. This is what makes the melee hold case a HOLD - the predicate
        // stays true so the movement veto keeps standing, but nothing issues a move, so the bot stops
        // rather than shuffling on the spot.
        if (bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY()) >= MURU_DARKNESS_CLEAR_TO)
            return false;

        return BuildMuruEscapeSpot(bot, center.GetPositionX(), center.GetPositionY(),
                                   MURU_DARKNESS_CLEAR_TO, attempt, spot);
    }

    Creature* FindMuruDarkFiend(Player* bot, PlayerbotAI* /*botAI*/)
    {
        if (!SwpOffensiveDispelSpell(bot))
            return nullptr;

        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, static_cast<uint32>(SunwellNpcs::NPC_DARK_FIEND),
                                            MURU_FIEND_SEARCH_RANGE);

        std::vector<Creature*> fiends;
        for (Creature* fiend : found)
        {
            if (!fiend->IsAlive())
                continue;

            // No 45934 means it is already dispelled and about to despawn on its own next tick. Health
            // is NOT a filter here: the 1% clamp in npc_dark_fiend::DamageTaken means a fiend the raid
            // has been beating on all fight is still at 1% and still perfectly lethal.
            if (!fiend->HasAura(static_cast<uint32>(SunwellSpells::SPELL_DARK_FIEND_APPEARANCE)))
                continue;

            if (bot->GetExactDist2d(fiend->GetPositionX(), fiend->GetPositionY()) > MURU_DISPEL_RANGE)
                continue;

            fiends.push_back(fiend);
        }

        if (fiends.empty())
            return nullptr;

        std::sort(fiends.begin(), fiends.end(),
                  [](Creature* a, Creature* b) { return a->GetGUID() < b->GetGUID(); });

        // Spread the dispels. Eight fiends spawn at once and each needs exactly one strip, so six
        // dispellers all picking "the nearest" would waste five casts on a fiend that is already gone
        // while five others walked in. Indexing by the bot's own rank among the raid's dispel-capable
        // bots is deterministic and needs no shared state - every bot derives the same assignment.
        uint32 rank = 0;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                    continue;
                if (!SwpOffensiveDispelSpell(member))
                    continue;
                if (member->GetGUID() < bot->GetGUID())
                    ++rank;
            }
        }

        return fiends[rank % fiends.size()];
    }

    bool MuruShouldDispelFiend(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        if (!SwpOffensiveDispelSpell(bot))
            return false;

        // Never against a move this fight already owes: playerbots cancels a cast-time spell the instant
        // the bot moves, so a dispel racing a Darkness clear loses both. Same rule as the Twins dispel
        // and the Felmyst Mass Dispel.
        if (MuruShouldSuppressGenericMovement(bot, botAI))
            return false;

        return FindMuruDarkFiend(bot, botAI) != nullptr;
    }

    Creature* FindMuruVoidSentinel(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_VOID_SENTINEL), 30.0f);
    }

    bool MuruShouldAvoidShadowPulse(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        // The Sentinel's holder MUST stand in the pulse - that is the job. Only melee DPS have any
        // business stepping off, and ranged are outside 10y by construction anyway.
        if (botAI->IsTank(bot) || !botAI->IsMelee(bot))
            return false;

        // Anything deadlier owns the bot. Without this the pulse step-out would walk a bot back into a
        // Darkness it had just left, which is the exact trade the Twins blaze/stack pair had to be
        // ordered around.
        if (MuruShouldClearDarkness(bot, botAI) || MuruShouldClearVoidZone(bot, botAI) ||
            MuruShouldFleeSingularity(bot, botAI))
        {
            return false;
        }

        Creature* sentinel = FindMuruVoidSentinel(bot);
        if (!sentinel || !sentinel->IsAlive())
            return false;

        return bot->GetExactDist2d(sentinel->GetPositionX(), sentinel->GetPositionY()) <
               MURU_PULSE_RADIUS;
    }

    bool GetMuruShadowPulseSpot(Player* bot, Position& spot, uint32 attempt)
    {
        Creature* sentinel = FindMuruVoidSentinel(bot);
        if (!sentinel)
            return false;

        return BuildMuruEscapeSpot(bot, sentinel->GetPositionX(), sentinel->GetPositionY(),
                                   MURU_PULSE_CLEAR_TO, attempt, spot);
    }

    Creature* FindMuruVoidZone(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_ENTROPIUS_ZONE), 20.0f);
    }

    bool MuruShouldClearVoidZone(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        if (MuruShouldClearDarkness(bot, botAI))
            return false;  // 3000/s with no healing possible beats 3000/s that can be healed

        Creature* zone = FindMuruVoidZone(bot);
        if (!zone)
            return false;

        return bot->GetExactDist2d(zone->GetPositionX(), zone->GetPositionY()) < MURU_VOID_ZONE_RADIUS;
    }

    bool GetMuruVoidZoneClearSpot(Player* bot, Position& spot, uint32 attempt)
    {
        Creature* zone = FindMuruVoidZone(bot);
        if (!zone)
            return false;

        return BuildMuruEscapeSpot(bot, zone->GetPositionX(), zone->GetPositionY(),
                                   MURU_VOID_ZONE_CLEAR_TO, attempt, spot);
    }

    Creature* FindMuruSingularity(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_SINGULARITY), 40.0f);
    }

    bool MuruShouldFleeSingularity(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        if (MuruShouldClearDarkness(bot, botAI))
            return false;

        // Already caught. There is nothing to run from mid-jump, and 46230 makes FindAndFollowTarget skip
        // this bot on its next 6s re-pick - so the bot holding the debuff is briefly the SAFEST body in
        // the room. Fighting the jump would just waste the tick.
        if (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BLACK_HOLE_EFFECT)))
            return false;

        Creature* singularity = FindMuruSingularity(bot);
        if (!singularity || !singularity->IsAlive())
            return false;

        // ARMED CHECK, and it is load-bearing. A singularity lives 18s but only gains 46228 - and with it
        // the pull - at the 8 second mark. It is following somebody the whole time, so "it exists" would
        // keep half the raid running from a harmless orb for nine seconds out of every spawn. Same shape
        // as the Lich King phase-3 fix, where holding for DORMANT Vile Spirits is what made bots leave
        // late and cross into the wave as it armed.
        if (!singularity->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BLACK_HOLE_PASSIVE)))
            return false;

        return bot->GetExactDist2d(singularity->GetPositionX(), singularity->GetPositionY()) <
               MURU_SINGULARITY_RADIUS + 3.0f;
    }

    bool GetMuruSingularityFleeSpot(Player* bot, Position& spot, uint32 attempt)
    {
        Creature* singularity = FindMuruSingularity(bot);
        if (!singularity)
            return false;

        return BuildMuruEscapeSpot(bot, singularity->GetPositionX(), singularity->GetPositionY(),
                                   MURU_SINGULARITY_FLEE, attempt, spot);
    }

    std::vector<uint32> GetMuruKillOrder(Player* bot, PlayerbotAI* botAI)
    {
        // The wiki's order, verbatim. Melee stop at the blood elves: a Void Sentinel puts 3750 into
        // everything within 10y every 3s, and a Void Spawn pack is a ranged problem the melee lane has no
        // reason to walk into while elites are up.
        if (botAI->IsMelee(bot))
        {
            return { static_cast<uint32>(SunwellNpcs::NPC_SW_FURY_MAGE),
                     static_cast<uint32>(SunwellNpcs::NPC_SW_BERSERKER) };
        }

        return { static_cast<uint32>(SunwellNpcs::NPC_VOID_SENTINEL),
                 static_cast<uint32>(SunwellNpcs::NPC_VOID_SPAWN),
                 static_cast<uint32>(SunwellNpcs::NPC_SW_FURY_MAGE),
                 static_cast<uint32>(SunwellNpcs::NPC_SW_BERSERKER) };
    }

    Unit* GetMuruFocusTarget(Player* bot, PlayerbotAI* botAI)
    {
        // Tanks and healers are not steered here. Tanks have their own pickup action and would fight it;
        // a healer with a forced attack target stops healing, which no add is worth.
        if (botAI->IsTank(bot) || botAI->IsHeal(bot))
            return nullptr;

        std::vector<uint32> const lanes = GetMuruKillOrder(bot, botAI);

        // ONE grid search for every add kind, then filter per tier - four searches would cost four times
        // as much for the same answer.
        std::list<Creature*> adds;
        bot->GetCreatureListWithEntryInGrid(adds, lanes, MURU_ADD_PICKUP_RANGE);

        for (uint32 entry : lanes)
        {
            Creature* best = nullptr;

            for (Creature* add : adds)
            {
                if (add->GetEntry() != entry || !add->IsAlive() || !add->IsInCombat())
                    continue;

                // Within a tier: BUCKETED health, GUID tie-broken. See MURU_FOCUS_HEALTH_BUCKET - a raw
                // percent comparison flips between two similarly-damaged adds every tick, and each flip
                // costs the bot its movement, so it never reaches melee range at all.
                uint32 const bucket = uint32(add->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET);

                if (!best)
                {
                    best = add;
                    continue;
                }

                uint32 const bestBucket = uint32(best->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET);
                if (bucket < bestBucket || (bucket == bestBucket && add->GetGUID() < best->GetGUID()))
                    best = add;
            }

            if (best)
                return best;
        }

        if (Creature* entropius = FindEntropius(bot))
        {
            if (entropius->IsAlive())
                return entropius;
        }

        // M'uru last, and only while he is still a legal target. He gains UNIT_FLAG_NOT_SELECTABLE the
        // instant he is clamped to 1 HP - a full 7 seconds before Entropius exists - so without this the
        // whole raid spends that window swinging at something it cannot touch.
        Creature* muru = FindMuru(bot);
        if (!muru || !muru->IsAlive() || muru->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return nullptr;

        // And never hand a melee bot the boss while his Darkness is up. He is the CENTRE of it, so melee
        // range is 15y inside a zone that deals 3000 per second and blocks all healing. Returning nothing
        // leaves MuruShouldClearDarkness holding the bot outside instead of feeding the in-out oscillator.
        if (botAI->IsMelee(bot) && MuruShouldClearDarkness(bot, botAI))
            return nullptr;

        return muru;
    }

    bool MuruShouldRetarget(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        if (MuruShouldSuppressGenericMovement(bot, botAI))
            return false;  // survival first; the switch costs nothing to defer a second

        Unit* wanted = GetMuruFocusTarget(bot, botAI);
        if (!wanted)
            return false;

        // Stand down once the ENGINE agrees, not just once the victim matches. AttackAction::Attack sets
        // the "current target" context value unconditionally but only calls bot->Attack() when
        // WaitForAttackStrategy allows it, so keying purely on GetVictim() can leave this true forever -
        // and re-firing is destructive, because Attack() clears the motion master and calls StopMoving()
        // every time. A melee bot in that state stands still and swings at nothing.
        if (botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == wanted)
            return false;

        return bot->GetVictim() != wanted;
    }

    Creature* FindMuruUntankedAdd(Player* bot, PlayerbotAI* botAI)
    {
        if (!botAI->IsTank(bot))
            return nullptr;

        Group* group = bot->GetGroup();
        ObjectGuid mainGuid, offGuid;
        if (group)
            GetBrutallusTankGuids(group, botAI, mainGuid, offGuid);

        bool const isOff = group && bot->GetGUID() == offGuid;

        // Split the two hardest-hitting things in the fight across two bodies. Void Blast is 9426 plus
        // -35% attack speed for 10s; a tank holding that AND six level-71 elites dies to the overlap.
        std::vector<uint32> wanted;
        if (isOff)
        {
            wanted = { static_cast<uint32>(SunwellNpcs::NPC_VOID_SENTINEL) };
        }
        else
        {
            // Main tank (and any third tank, which falls through here - there is always more wave than
            // one body can hold) owns the blood elves, plus Entropius once he is up.
            wanted = { static_cast<uint32>(SunwellNpcs::NPC_ENTROPIUS),
                       static_cast<uint32>(SunwellNpcs::NPC_SW_FURY_MAGE),
                       static_cast<uint32>(SunwellNpcs::NPC_SW_BERSERKER) };
        }

        for (uint32 entry : wanted)
        {
            std::list<Creature*> adds;
            bot->GetCreatureListWithEntryInGrid(adds, entry, MURU_ADD_PICKUP_RANGE);
            for (Creature* add : adds)
            {
                if (!add->IsAlive() || !add->IsInCombat())
                    continue;

                Unit* victim = add->GetVictim();
                if (!victim)
                    return add;  // freshly spawned, nobody has it yet
                if (victim == bot)
                    continue;    // already mine

                // Loose only if it is chewing on somebody who cannot take it. Another tank holding it is
                // a correct state, not a problem to fix - re-taunting a co-tank's add is how two tanks
                // spend a fight trading one mob.
                Player* holder = victim->ToPlayer();
                if (!holder || !PlayerbotAI::IsTank(holder))
                    return add;
            }
        }

        return nullptr;
    }

    bool MuruShouldTankAdds(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsMuruEncounterActive(bot))
            return false;

        if (MuruShouldSuppressGenericMovement(bot, botAI))
            return false;

        return FindMuruUntankedAdd(bot, botAI) != nullptr;
    }

    bool MuruShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI)
    {
        // NARROW, for the reason spelled out on TwinsShouldSuppressGenericMovement: this fight needs
        // generic movement for almost all of its duration (melee chasing six elites, ranged holding
        // range on a boss that never moves), so it is only taken away for the seconds a bot is actually
        // executing one of these four moves.
        return MuruShouldClearDarkness(bot, botAI) || MuruShouldClearVoidZone(bot, botAI) ||
               MuruShouldFleeSingularity(bot, botAI) || MuruShouldAvoidShadowPulse(bot, botAI);
    }

    // ---------------------------------------------------------------------------------------------
    // Kil'jaeden
    //
    // BuildMuruEscapeSpot is reused verbatim below rather than reimplemented. Its sector snapping is the
    // load-bearing part - see the comment on MURU_CLEAR_SECTORS - and it is boss-agnostic: "snap the
    // bearing from this hazard to a fixed sector so the destination is bit-identical across ticks".

    Creature* FindKiljaeden(Player* bot)
    {
        return bot->FindNearestCreature(static_cast<uint32>(SunwellNpcs::NPC_KILJAEDEN),
                                        KJ_PARTICIPANT_RANGE);
    }

    bool IsKjEncounterActive(Player* bot)
    {
        // No grid search, for the same reason as IsMuruEncounterActive: the movement multiplier runs this
        // for every queued action of every bot on every tick, and a boss-presence version would cost a
        // FindNearestCreature each time. It is not load-bearing on its own - every predicate that reads
        // it then proves its own hazard exists.
        if (!IsInSunwell(bot) || !bot->IsInCombat())
            return false;

        float const z = bot->GetPositionZ();
        if (z < KJ_Z_MIN || z > KJ_Z_MAX)
            return false;

        return bot->GetExactDist2d(KJ_ANCHOR_X, KJ_ANCHOR_Y) <= KJ_PARTICIPANT_RANGE;
    }

    bool KjIsDeceiverPhase(Player* bot)
    {
        std::list<Creature*> hands;
        bot->GetCreatureListWithEntryInGrid(hands, static_cast<uint32>(SunwellNpcs::NPC_KJ_HAND),
                                           KJ_PARTICIPANT_RANGE);
        for (Creature* hand : hands)
            if (hand->IsAlive())
                return true;

        return false;
    }

    bool KjDarknessWindow(Player* bot, uint32& remainingMs)
    {
        remainingMs = 0;

        Creature* kj = FindKiljaeden(bot);
        if (!kj)
            return false;

        // The 8s DUMMY aura on KJ himself IS the mechanic - spell_kiljaeden_darkness_aura only fires
        // 45657 when this aura is removed by natural expiry. So its remaining duration is exactly how
        // long the raid has, and there is no other signal: the cast itself is 750ms and over before any
        // bot could react to it.
        Aura* aura = kj->GetAura(static_cast<uint32>(SunwellSpells::SPELL_KJ_DARKNESS));
        if (!aura)
            return false;

        int32 const left = aura->GetDuration();
        remainingMs = left > 0 ? uint32(left) : 0u;
        return true;
    }

    void GetKjAnchor(Player* bot, Position& anchor)
    {
        float x = KJ_ANCHOR_X;
        float y = KJ_ANCHOR_Y;
        float z = KJ_ANCHOR_Z;
        bot->UpdateAllowedPositionZ(x, y, z);
        anchor.Relocate(x, y, z);
    }

    bool KjShouldHoldAnchor(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        // Only the raid's own Main Tank flag holder, resolved by the same helper the Brutallus formation
        // uses so the player decides who anchors. A second or later tank fights as normal melee.
        if (GetBrutallusTankRole(bot, botAI) != BrutallusTankRole::Main)
            return false;

        // Nothing to anchor during phase 1: KJ does not exist until the last Hand dies, and the three
        // Hands are spread across the platform on their own spawn points.
        if (KjIsDeceiverPhase(bot))
            return false;

        Position anchor;
        GetKjAnchor(bot, anchor);
        return bot->GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY()) > KJ_ANCHOR_TOLERANCE;
    }

    namespace
    {
        // Rank among the alive raid bots matching `wanted`, ordered by GUID. The seating-chart idiom from
        // Brutallus: deterministic, needs no shared state, and every bot derives the same chart.
        //
        // A death shifts the ranks after it, which moves those bots by exactly one slot. That is
        // deliberate and it is why the slot divisors below are FIXED constants rather than the live
        // count: dividing the circle by the live count would re-seat the ENTIRE ring every time anybody
        // died, which is the "never derive a destination from live state" rule.
        uint32 SwpRankAmong(Player* bot, PlayerbotAI* botAI, bool (*wanted)(Player*, PlayerbotAI*))
        {
            Group* group = bot->GetGroup();
            if (!group)
                return 0;

            uint32 rank = 0;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                    continue;
                if (!wanted(member, botAI))
                    continue;
                if (member->GetGUID() < bot->GetGUID())
                    ++rank;
            }

            return rank;
        }

        bool SwpAnyRaider(Player* /*member*/, PlayerbotAI* /*botAI*/) { return true; }

        bool SwpRangedOrHealer(Player* member, PlayerbotAI* botAI)
        {
            return !botAI->IsTank(member) && !botAI->IsMelee(member);
        }
    }  // namespace

    bool GetKjDarknessStackSpot(Player* bot, Position& spot)
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        Position anchor;
        GetKjAnchor(bot, anchor);

        // THE STACK BEARING IS THE STATION BEARING, and that pairing is the whole point of this split.
        //
        // Rev 1 derived the two independently, so a ranged bot parked on the 26y ring could be handed a
        // stack slot on the OPPOSITE side of the anchor - a 26 + 6 = 32y walk instead of 26 - 6 = 20y.
        // Measured in the first kill: at the moment the shield went up, bots were still 20-28y from their
        // slots and coverage read 13/22, 18/22, 18/21. Nobody died, because 45848's effect 0 is a
        // persistent area aura that re-runs its target search every update and caught the late arrivals -
        // but that was slack, not design.
        //
        // Reusing the station bearing makes every ranged walk purely RADIAL and therefore the shortest one
        // available, always 20y regardless of which slot a bot holds. Under the -51% slow that Flame Dart
        // and Shadow Spike both apply, that is ~5.9s instead of ~9.4s against an 8s deadline.
        float radius;
        float bearing;
        if (!botAI->IsTank(bot) && !botAI->IsMelee(bot))
        {
            radius = KJ_STACK_RING_OUTER;
            uint32 const rank = SwpRankAmong(bot, botAI, &SwpRangedOrHealer);
            bearing = float(rank % KJ_RANGED_SLOTS) * 2.0f * float(M_PI) / float(KJ_RANGED_SLOTS);
        }
        else
        {
            // Melee are already standing on the anchor, so their walk is a couple of yards either way and
            // the only job here is to keep them from all requesting the identical point.
            radius = KJ_STACK_RING_INNER;
            uint32 const rank = SwpRankAmong(bot, botAI, &SwpAnyRaider);
            bearing = float(rank % KJ_STACK_SLOTS_PER_RING) * 2.0f * float(M_PI) /
                      float(KJ_STACK_SLOTS_PER_RING);
        }

        float x = anchor.GetPositionX() + std::cos(bearing) * radius;
        float y = anchor.GetPositionY() + std::sin(bearing) * radius;

        // Already seated: no destination. Same idiom as GetMuruDarknessClearSpot - the predicate stays
        // true so the movement veto keeps standing, but nothing issues a move, so the bot parks instead
        // of shuffling on the spot for the whole eight seconds.
        if (bot->GetExactDist2d(x, y) <= KJ_STACK_TOLERANCE)
            return false;

        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);
        spot.Relocate(x, y, z);
        return true;
    }

    bool KjShouldStackForDarkness(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        uint32 remaining = 0;
        if (!KjDarknessWindow(bot, remaining))
            return false;

        // The pilot is exempt on both counts: it is immune to all schools for the duration of the
        // possession (45838) and its body carries UNIT_FLAG_DISABLE_MOVE, so it could not walk here if it
        // wanted to.
        if (GetKjDrake(bot))
            return false;

        // Tanks are exempt because they are ALREADY inside the shield: the main tank holds the anchor the
        // drake parks on. Walking a tank 8y to a stack slot would drag the boss with him for no gain.
        if (botAI->IsTank(bot))
            return false;

        return true;
    }

    Creature* FindKjArmageddonMarker(Player* bot)
    {
        std::list<Creature*> markers;
        bot->GetCreatureListWithEntryInGrid(markers, static_cast<uint32>(SunwellNpcs::NPC_KJ_ARMAGEDDON),
                                           40.0f);

        Creature* nearest = nullptr;
        float best = 0.0f;
        for (Creature* marker : markers)
        {
            if (!marker->IsAlive())
                continue;

            // 2D, ALWAYS. boss_kiljaeden::JustSummoned pushes the marker 20y into the air and
            // spell_kiljaeden_armageddon_missile offsets the impact destination 20y back down, so the
            // damage lands on the floor directly beneath it. A 3D distance would read a bot standing in
            // the impact zone as 20y clear of it.
            float const dist = bot->GetExactDist2d(marker->GetPositionX(), marker->GetPositionY());
            if (!nearest || dist < best)
            {
                nearest = marker;
                best = dist;
            }
        }

        return nearest;
    }

    bool KjShouldClearArmageddon(Player* bot)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        // A pilot cannot move and does not need to - 45915 is fire, and 45838 makes it immune to every
        // school. Asking it to run would only burn the tick its shield cast needs.
        if (GetKjDrake(bot))
            return false;

        Creature* marker = FindKjArmageddonMarker(bot);
        if (!marker)
            return false;

        return bot->GetExactDist2d(marker->GetPositionX(), marker->GetPositionY()) <
               KJ_ARMAGEDDON_CLEAR_FROM;
    }

    bool GetKjArmageddonClearSpot(Player* bot, Position& spot, uint32 attempt)
    {
        Creature* marker = FindKjArmageddonMarker(bot);
        if (!marker)
            return false;

        if (bot->GetExactDist2d(marker->GetPositionX(), marker->GetPositionY()) >=
            KJ_ARMAGEDDON_CLEAR_TO)
        {
            return false;
        }

        return BuildMuruEscapeSpot(bot, marker->GetPositionX(), marker->GetPositionY(),
                                   KJ_ARMAGEDDON_CLEAR_TO, attempt, spot);
    }

    bool KjShouldQuarantineFireBloom(Player* bot, PlayerbotAI* /*botAI*/)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        if (!bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_KJ_FIRE_BLOOM)))
            return false;

        // Never during a Darkness window, and this is not a priority judgement - it is the opposite
        // instruction. spell_kiljaeden_shield_of_the_blue removes 45641 outright on apply, so a carrier
        // that runs to 38y keeps the DoT for its full 20s while a carrier that stacks loses it entirely
        // AND survives the 47,499. Running out is the wrong move, not merely the lower-priority one.
        uint32 remaining = 0;
        if (KjDarknessWindow(bot, remaining))
            return false;

        // A pilot's body is frozen and immune; a quarantine walk is impossible and pointless.
        if (GetKjDrake(bot))
            return false;

        return bot->GetExactDist2d(KJ_ANCHOR_X, KJ_ANCHOR_Y) < KJ_FIRE_BLOOM_RANGE - 2.0f;
    }

    bool GetKjFireBloomSpot(Player* bot, Position& spot, uint32 attempt)
    {
        // Outward from the ANCHOR, not from the raid's centre of mass. The anchor is a fixed world point,
        // so the destination is stable across ticks; a centroid moves as the raid moves and would creep
        // the destination every tick, which is what made 130 of 268 M'uru escapes accept no move at all.
        //
        // Outward also separates two simultaneous carriers for free, because each keeps its own sector.
        if (bot->GetExactDist2d(KJ_ANCHOR_X, KJ_ANCHOR_Y) >= KJ_FIRE_BLOOM_RANGE - 2.0f)
            return false;

        return BuildMuruEscapeSpot(bot, KJ_ANCHOR_X, KJ_ANCHOR_Y, KJ_FIRE_BLOOM_RANGE, attempt, spot);
    }

    bool GetKjStation(Player* bot, PlayerbotAI* botAI, Position& station)
    {
        if (botAI->IsTank(bot) || botAI->IsMelee(bot))
            return false;

        Position anchor;
        GetKjAnchor(bot, anchor);

        uint32 const rank = SwpRankAmong(bot, botAI, &SwpRangedOrHealer);
        float const bearing = float(rank % KJ_RANGED_SLOTS) * 2.0f * float(M_PI) / float(KJ_RANGED_SLOTS);

        float x = anchor.GetPositionX() + std::cos(bearing) * KJ_RANGED_RING;
        float y = anchor.GetPositionY() + std::sin(bearing) * KJ_RANGED_RING;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        station.Relocate(x, y, z);
        return true;
    }

    bool KjHasStation(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        if (GetKjDrake(bot))
            return false;

        Position station;
        return GetKjStation(bot, botAI, station);
    }

    bool KjShouldHoldStation(Player* bot, PlayerbotAI* botAI)
    {
        if (!KjHasStation(bot, botAI))
            return false;

        // Every hazard outranks the formation. Without this the station would walk a bot back into a
        // meteor it had just left, or out of the Darkness stack mid-window - the same ordering the Twins
        // blaze/stack pair needed.
        uint32 remaining = 0;
        if (KjDarknessWindow(bot, remaining))
            return false;
        if (KjShouldClearArmageddon(bot) || KjShouldQuarantineFireBloom(bot, botAI))
            return false;

        // NO PHASE-1 GATE, deliberately. Rev 1 called KjIsDeceiverPhase here, which is a GRID SEARCH - and
        // this predicate is reached from the movement multiplier for every queued action of every bot on
        // every tick, which is the Wintergrasp per-react-tick world-query trap. It did not bite (three
        // tick-diff warnings all fight, 108-128ms, none during the pull) but it was luck.
        //
        // It is also unnecessary: the three Hands spawn within 19y of the anchor, so a 26y ring is a
        // perfectly good ranged position during phase 1 too. Deleting the gate removes the grid search
        // from the hot path and loses nothing.

        Position station;
        if (!GetKjStation(bot, botAI, station))
            return false;

        return bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()) >
               KJ_STATION_TOLERANCE;
    }

    Creature* GetKjDrake(Player* bot)
    {
        // Possession makes the PLAYER the charmer, so the drake is the charm - not the charmer. The entry
        // filter matters: a warlock pet or a mind-controlled anything would also answer GetCharm().
        Unit* charm = bot->GetCharm();
        if (!charm)
            return nullptr;

        Creature* creature = charm->ToCreature();
        if (!creature || creature->GetEntry() != static_cast<uint32>(SunwellNpcs::NPC_KJ_BLUE_DRAKE))
            return nullptr;

        return creature->IsAlive() ? creature : nullptr;
    }

    bool KjRaidHasDrake(Player* bot)
    {
        if (GetKjDrake(bot))
            return true;

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;
            if (member->GetMapId() != bot->GetMapId())
                continue;
            if (GetKjDrake(member))
                return true;
        }

        return false;
    }

    bool KjIsDesignatedPilot(Player* bot, PlayerbotAI* botAI)
    {
        // Already holding one: it stays the pilot no matter how the ranking would come out now. Otherwise
        // a lower-GUID DPS reviving mid-fight would "become" the pilot while the drake is charmed by
        // somebody else, and the raid would end up with a drake nobody is driving.
        if (GetKjDrake(bot))
            return true;

        if (botAI->IsTank(bot) || botAI->IsHeal(bot))
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return true;  // solo: whoever is here does it

        // Ranged preferred - a melee pilot costs the raid melee uptime on a boss that is already tank-and-
        // spank between mechanics - but never REQUIRED, because a raid with no surviving ranged DPS still
        // needs the shield or it wipes on the next Darkness. The bot itself is included in the sweep, so
        // the winner is always a real group member and there is no empty-set case to handle.
        ObjectGuid best;
        bool bestRanged = false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
                continue;
            if (member->GetMapId() != bot->GetMapId())
                continue;
            if (botAI->IsTank(member) || botAI->IsHeal(member))
                continue;

            bool const ranged = !botAI->IsMelee(member);
            if (!best || (ranged && !bestRanged) ||
                (ranged == bestRanged && member->GetGUID() < best))
            {
                best = member->GetGUID();
                bestRanged = ranged;
            }
        }

        return best == bot->GetGUID();
    }

    GameObject* FindKjEmpoweredOrb(Player* bot)
    {
        static uint32 const orbs[4] = {
            static_cast<uint32>(SunwellObjects::GO_KJ_ORB_1),
            static_cast<uint32>(SunwellObjects::GO_KJ_ORB_2),
            static_cast<uint32>(SunwellObjects::GO_KJ_ORB_3),
            static_cast<uint32>(SunwellObjects::GO_KJ_ORB_4),
        };

        GameObject* nearest = nullptr;
        float best = 0.0f;
        for (uint32 entry : orbs)
        {
            GameObject* orb = bot->FindNearestGameObject(entry, KJ_ORB_SEARCH_RANGE);
            if (!orb)
                continue;

            // npc_kiljaeden_controller::ResetOrbs flags all four NOT_SELECTABLE, and
            // boss_kiljaeden::EmpowerOrb removes the flag one orb at a time (all remaining at 25%). So
            // the flag is a direct, free read of "is this orb live" - no need to hunt for the Ring of Blue
            // Flames trigger creature it also spawns.
            if (orb->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
                continue;

            float const dist = bot->GetExactDist2d(orb->GetPositionX(), orb->GetPositionY());
            if (!nearest || dist < best)
            {
                nearest = orb;
                best = dist;
            }
        }

        return nearest;
    }

    bool KjShouldClaimOrb(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        if (GetKjDrake(bot))
            return false;  // already piloting; the drive action owns this bot

        if (KjRaidHasDrake(bot))
            return false;  // somebody else has it - one drake is enough, and its shield is off cooldown
                           // every 20s against a Darkness every 45s

        if (!KjIsDesignatedPilot(bot, botAI))
            return false;

        return FindKjEmpoweredOrb(bot) != nullptr;
    }

    bool KjShouldRefreshPossession(Player* bot)
    {
        if (!GetKjDrake(bot))
            return false;

        Aura* possess = bot->GetAura(static_cast<uint32>(SunwellSpells::SPELL_KJ_VENGEANCE_BLUE));
        if (!possess)
            return false;

        if (possess->GetDuration() > int32(KJ_POSSESS_REFRESH_MS))
            return false;

        // NEVER inside a Darkness window. Dropping possession there throws away the only thing standing
        // between the raid and 47,499 unmitigated - and the refresh can always wait, because this fires
        // with 30s of possession left against an 8s window.
        uint32 remaining = 0;
        if (KjDarknessWindow(bot, remaining))
            return false;

        return true;
    }

    uint32 CountKjShieldCoverage(Player* bot, Creature* drake, uint32& total)
    {
        total = 0;
        if (!drake)
            return 0;

        uint32 covered = 0;

        Group* group = bot->GetGroup();
        if (!group)
            return 0;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
                continue;

            // The pilot is excluded from BOTH counts: 45838 already makes it immune to every school, so it
            // needs no shield and counting it would flatter the coverage figure.
            if (GetKjDrake(member))
                continue;

            ++total;
            if (member->GetExactDist2d(drake->GetPositionX(), drake->GetPositionY()) <= KJ_SHIELD_RADIUS)
                ++covered;
        }

        return covered;
    }

    bool GetKjBreathBearing(Player* bot, Creature* drake, bool healing, float& bearing)
    {
        if (!drake)
            return false;

        // Haste goes where the melee are, and the melee are on the boss - who is held on the anchor the
        // drake is parked on, so this is a short bearing but a real one.
        if (!healing)
        {
            if (Creature* kj = FindKiljaeden(bot))
            {
                float const d = drake->GetExactDist2d(kj->GetPositionX(), kj->GetPositionY());
                if (d >= KJ_BREATH_AIM_MIN && d <= KJ_BREATH_AIM_MAX)
                {
                    bearing = drake->GetAngle(kj);
                    return true;
                }
            }
        }

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        Player* best = nullptr;
        float bestPct = 0.0f;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
                continue;
            if (GetKjDrake(member))
                continue;  // spell_kiljaeden_dragon_breath filters the pilot out anyway

            float const d = drake->GetExactDist2d(member->GetPositionX(), member->GetPositionY());
            if (d < KJ_BREATH_AIM_MIN || d > KJ_BREATH_AIM_MAX)
                continue;

            float const pct = member->GetHealthPct();
            if (!best || pct < bestPct)
            {
                best = member;
                bestPct = pct;
            }
        }

        if (!best)
            return false;

        bearing = drake->GetAngle(best);
        return true;
    }

    std::vector<uint32> GetKjKillOrder(Player* bot, PlayerbotAI* botAI)
    {
        // Phase 1 is its own fight: three Hands that pull together (CALL_FOR_HELP 50y on aggro) and have
        // to die one at a time, because each one alive is another Felfire Portal every 10-25s.
        if (KjIsDeceiverPhase(bot))
            return { static_cast<uint32>(SunwellNpcs::NPC_KJ_HAND) };

        // Melee stop at the reflections. A Shield Orb orbits at z=40 on an 18y circle, which is ~12y
        // above the floor and ~21y away in 3D from a melee standing on the boss - outside any melee
        // reach. Handing one to a melee bot would only freeze it walking at something it can never hit.
        if (botAI->IsMelee(bot))
            return { static_cast<uint32>(SunwellNpcs::NPC_KJ_REFLECTION) };

        // Ranged take the orbs FIRST, ahead of the reflections, and the asymmetry is deliberate: an orb
        // is 999 shadow every half-second to the whole raid inside 45y, uncapped, and only ranged can
        // reach one - whereas a reflection is a single-target melee add that the melee lane is already
        // covering. Orb HP is trivial (HealthModifier 2.002 at level 70), so this costs seconds.
        return { static_cast<uint32>(SunwellNpcs::NPC_KJ_SHIELD_ORB),
                 static_cast<uint32>(SunwellNpcs::NPC_KJ_REFLECTION) };
    }

    Unit* GetKjFocusTarget(Player* bot, PlayerbotAI* botAI)
    {
        // A pilot has no targeting at all: its body is pacified and silenced by 45839, so every attack
        // action it could take is a no-op that costs it the tick its drake needs.
        if (GetKjDrake(bot))
            return nullptr;

        // Tanks and healers are not steered, same as M'uru: a tank has its own job and a healer handed an
        // attack target stops healing.
        if (botAI->IsTank(bot) || botAI->IsHeal(bot))
            return nullptr;

        std::vector<uint32> const lanes = GetKjKillOrder(bot, botAI);

        std::list<Creature*> adds;
        bot->GetCreatureListWithEntryInGrid(adds, lanes, KJ_PARTICIPANT_RANGE);

        for (uint32 entry : lanes)
        {
            Creature* best = nullptr;

            for (Creature* add : adds)
            {
                if (add->GetEntry() != entry || !add->IsAlive())
                    continue;

                // Shield Orbs are the one lane that must NOT require IsInCombat(): they are summoned with
                // TEMPSUMMON_CORPSE_DESPAWN and orbit on a cyclic spline, damaging the raid through an
                // aura rather than through a threat list, so an orb can be shooting the whole raid while
                // never having a victim of its own.
                bool const needsCombat = entry != static_cast<uint32>(SunwellNpcs::NPC_KJ_SHIELD_ORB);
                if (needsCombat && !add->IsInCombat())
                    continue;

                uint32 const bucket = uint32(add->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET);

                if (!best)
                {
                    best = add;
                    continue;
                }

                uint32 const bestBucket = uint32(best->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET);
                if (bucket < bestBucket || (bucket == bestBucket && add->GetGUID() < best->GetGUID()))
                    best = add;
            }

            if (best)
                return best;
        }

        Creature* kj = FindKiljaeden(bot);
        if (!kj || !kj->IsAlive())
            return nullptr;

        // boss_kiljaeden::InitializeAI holds UNIT_FLAG_NOT_SELECTABLE for the first 11 seconds (the
        // rebirth animation) and DamageTaken sets UNIT_FLAG_NON_ATTACKABLE on the killing blow, so both
        // ends of the fight have a window where he exists and cannot be touched.
        if (kj->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || kj->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            return nullptr;

        return kj;
    }

    namespace
    {
        // The five hazard moves, shared by the movement veto and the targeting gate below so the two can
        // never drift apart on what counts as a hazard. They differ ONLY in how they treat the station.
        bool KjHazardMoveOwed(Player* bot, PlayerbotAI* botAI)
        {
            return KjShouldClearArmageddon(bot) || KjShouldStackForDarkness(bot, botAI) ||
                   KjShouldQuarantineFireBloom(bot, botAI) ||
                   KjShouldHoldAnchor(bot, botAI) || KjShouldClaimOrb(bot, botAI);
        }

        // DELIBERATELY NOT KjShouldSuppressGenericMovement, and that difference is the whole of this fix.
        //
        // Rev 2 widened the MOVEMENT veto to KjHasStation - which reads "this bot is ranged and the fight
        // is on", i.e. it is true for every ranged bot and healer for the ENTIRE encounter. That widening
        // is correct for movement (see KjShouldSuppressGenericMovement). The defect was that
        // KjShouldRetarget read the same predicate, so `kj focus target` could never fire for a ranged
        // bot - while KjTargetHoldMultiplier went on vetoing all seven generic target choosers, because it
        // keys purely on GetKjFocusTarget being non-null. Between them a ranged bot was left with NO
        // target chooser at all: it walked to station and stood there for the whole fight without ever
        // acquiring a victim. Melee were untouched, because GetKjStation returns false for tanks and
        // melee, so KjHasStation is never true for them - exactly the "melee DPS was fine" half of the
        // report.
        //
        // The lesson, and it generalises past this fight: a veto that is permanently true is safe for
        // MOVEMENT (the bot is already where it should be) and lethal for TARGETING (a bot that never
        // acquires a first victim never gets one). Vetoing target choosers does not drop an existing
        // victim, so deferring for the seconds a hazard move is in flight costs nothing - deferring
        // forever costs the bot the entire fight.
        //
        // Targeting is therefore deferred only while a move is genuinely IN FLIGHT. The reason to defer at
        // all is that AttackAction::Attack calls MotionMaster::Clear(false) + StopMoving() whenever
        // lastMovement.priority < MOVEMENT_COMBAT, so retargeting mid-escape cancels the escape.
        bool KjShouldDeferTargeting(Player* bot, PlayerbotAI* botAI)
        {
            if (GetKjDrake(bot))
                return true;  // body is pacified and silenced; the pilot has exactly one job

            // KjShouldHoldStation - "still walking to it" - and this must stay rather than being dropped
            // with the rest of the station handling. Without it Attack() would clear the motion master
            // mid-walk, and the re-issued MoveTo would then hit the ~5s IsDuplicateMove dedup on an
            // unchanged destination, parking the bot short of station instead of seating it.
            if (KjShouldHoldStation(bot, botAI))
                return true;

            return KjHazardMoveOwed(bot, botAI);
        }
    }  // namespace

    bool KjShouldRetarget(Player* bot, PlayerbotAI* botAI)
    {
        if (!IsKjEncounterActive(bot))
            return false;

        if (KjShouldDeferTargeting(bot, botAI))
            return false;  // survival first; the switch costs nothing to defer a tick

        Unit* wanted = GetKjFocusTarget(bot, botAI);
        if (!wanted)
            return false;

        // Stand down once the ENGINE agrees, not just once the victim matches - see MuruShouldRetarget.
        // Re-firing is destructive: AttackAction::Attack clears the motion master and calls StopMoving()
        // every time, so a bot that re-selects the same target every tick never closes to range.
        if (botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == wanted)
            return false;

        return bot->GetVictim() != wanted;
    }

    bool KjShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI)
    {
        // The one BROAD case in this repo, and it is broad because the bot genuinely has nothing else it
        // can do: a pilot's body carries UNIT_FLAG_DISABLE_MOVE (set on the charmer by
        // Unit::SetCharmedBy) and is pacified and silenced by 45839. Generic movement cannot move it and
        // generic attacks cannot fire, so leaving them in the queue only lets them win ticks the drake
        // needs.
        if (GetKjDrake(bot))
            return true;

        // KjHasStation, NOT KjShouldHoldStation, and that widening is a measured fix.
        //
        // Rev 1 used the narrow "is a move owed" form everywhere, and for the station that produced the
        // Brutallus oscillator: the moment a ranged bot arrived, KjShouldHoldStation went false, the veto
        // lifted, generic ranged movement dragged it off, and the station re-triggered. Measured over the
        // first kill, one bot logged 463 station lines with min 6.0y and max 36.4y - it reached station and
        // was pulled away again, repeatedly, for the whole fight.
        //
        // Brutallus already documents the resolution: a bot given a deliberate station can only hold it if
        // the veto covers the WHOLE time the station exists, not just the walk. Widening cannot strand
        // anybody here, because `kj hold station` is always there to walk them back - which is the exact
        // hazard that made the Felmyst and Twins vetoes narrow in the first place.
        // MOVEMENT ONLY. Targeting must NOT read this predicate - see KjShouldDeferTargeting above for the
        // ranged-DPS stand-still this exact reuse caused.
        if (KjHasStation(bot, botAI))
            return true;

        return KjHazardMoveOwed(bot, botAI);
    }

}  // namespace SunwellPlateauHelpers
