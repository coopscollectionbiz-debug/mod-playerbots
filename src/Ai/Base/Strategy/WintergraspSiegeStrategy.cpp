#include "WintergraspSiegeStrategy.h"

#include "Playerbots.h"
#include "AiObjectContext.h"
#include "AreaDefines.h"
#include "PositionValue.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "BattlefieldWG.h"
#include "Config.h"
#include "Creature.h"
#include "GameObject.h"
#include "Map.h"
#include "ModelIgnoreFlags.h"
#include "MotionMaster.h"
#include "PathGenerator.h"
#include "Player.h"
#include "Timer.h"

#include <list>
#include <unordered_map>

namespace
{
    constexpr uint32 WG_ZONE_ID          = 4197;
    constexpr uint32 WG_SIEGE_MOVE_ID    = 987301;

    // SPELL_LIEUTENANT (55629) comes from BattlefieldWG.h (the WG rank aura). Reuse it.
    constexpr uint32 SPELL_CREATE_DEMOLISHER = 56575; // "Build" spell: summons 28094 + auto-seats
    constexpr uint32 SPELL_HURL_BOULDER      = 50896; // demolisher wall weapon (dest-location projectile)

    constexpr uint32 GO_TITAN_RELIC   = 192829;

    // Fix 4a: max water depth a siege vehicle can cross. Endpoints in deeper water are rejected
    // like a standing wall, keeping engines out of the lakes on the drive up from the south
    // workshops. 3.0 (was 1.5): the west river's ford crest (the SW road crossing, ground
    // z351-353.7 across y~3430-3460) sits under ~2y of water and is the ONLY vehicle crossing
    // on the west side (`.wgbots pathprobe`-mapped; no bridge deck exists in the navmesh).
    // The east lake channel is 16y+ deep, so it stays blocked at any sane wade depth.
    constexpr float WG_MAX_WADE_DEPTH = 3.0f;

    // The 4 capturable outer workshops (banner position + workshop area id). Vehicles are built
    // where real players build them — at a factory the attacker CONTROLS — then driven to the
    // keep (the old spike built at the gate, making engines "appear" in front of the door).
    struct FactoryDef { float x, y; uint32 area; };
    constexpr FactoryDef WG_FACTORIES[4] =
    {
        { 4949.34f, 2432.59f, AREA_THE_SUNKEN_RING     },
        { 4948.52f, 3342.34f, AREA_THE_BROKEN_TEMPLE   },
        { 4398.08f, 2356.50f, AREA_EASTSPARK_WORKSHOP  },
        { 4390.78f, 3304.09f, AREA_WESTSPARK_WORKSHOP  },
    };

    // Keep-approach waypoint for the drive up from a factory (west of the fortress door).
    constexpr float WG_APPROACH_X = 5100.0f, WG_APPROACH_Y = 2841.0f, WG_APPROACH_Z = 405.0f;

    // Fix 4b (reworked): authored road routes from the far south (spark) workshops to where the
    // generic approach logic proveably takes over. EVERY consecutive hop below is `.wgbots
    // pathprobe`-verified DRIVABLE (NORMAL path, no >2.5/4y climb step, no deep water) — the
    // original estimate coords were unverified and pinned every south vehicle at its first
    // bridge: the X-only "waypoint is behind" test dropped the bridge waypoint ~75y early and
    // retargeted across the gorge (SHORTCUT/NOPATH legs), oscillating vehicles in place
    // (runtime-observed, 2026-07-18). Deep-water guard (Fix 4a) still backstops every leg.
    struct RoadWp { float x, y, z; };
    constexpr float WG_ROAD_ARRIVE = 20.0f;   // 2D arrive radius: legs END on the waypoint, so
                                              // this only needs to absorb spline-end jitter
    // Both chains funnel through the SAME west-side crossing: the west river ford crest is the
    // only vehicle-passable water crossing on the whole map (east lake channel = 16y+ deep swim
    // water; central basin's east lip = >2.5/step cliff; every apparent "bridge" is either a
    // swim-surface poly or absent from the navmesh). Eastspark vehicles therefore take the
    // south road west past the south tower (east side — the tower's WMO wall-blocks the west
    // line) to Westspark and share the SW road from there.
    // SW chain (Westspark, y >= 2841): NE to the ford, over the shallow shelf at y~3455-3462
    // (needs WG_MAX_WADE_DEPTH 3.0), then up the east bank to the Broken Temple road and the
    // proven BT->approach descent corridor. Two spots are EXPECTED road-unstick hops (the map
    // has no gentler line — probe-exhausted): the braided river's deep channel at x~4725-4750
    // (4y+, over any sane wade) and the stepped east-bank lip at (4807-4830, y~3350-3457)
    // (3.3-5.0 z/step everywhere it was sampled). Everything between is verified DRIVABLE.
    constexpr RoadWp WG_ROAD_SW[] =
    {
        { 4560.0f, 3455.0f, 360.0f },
        { 4680.0f, 3462.0f, 352.0f },   // ford shelf — deep channel just east (unstick hop)
        { 4800.0f, 3430.0f, 355.0f },   // dry shelf east of the channel
        { 4880.0f, 3350.0f, 372.0f },   // bank top (lip below is an unstick hop)
        { 4950.0f, 3300.0f, 378.0f },
        { 5010.0f, 3155.0f, 356.0f },   // handoff to the generic approach
    };
    // SE chain (Eastspark, y < 2841): south road north (battle-proven legs), west past the
    // south tower's EAST flank at x~4440-4445 (25y off the tower model), down the strip to
    // Westspark, then the SW chain's line verbatim.
    constexpr RoadWp WG_ROAD_SE[] =
    {
        { 4453.0f, 2483.0f, 358.0f },
        { 4475.0f, 2722.0f, 384.0f },
        { 4400.0f, 2770.0f, 403.0f },
        { 4445.0f, 2830.0f, 403.0f },   // east-of-tower bypass — the west line WALL-blocks
        { 4440.0f, 2890.0f, 390.0f },
        { 4400.0f, 3000.0f, 363.0f },
        { 4390.0f, 3150.0f, 359.0f },
        { 4390.0f, 3304.0f, 372.0f },   // Westspark — join the SW road
        { 4560.0f, 3455.0f, 360.0f },
        { 4680.0f, 3462.0f, 352.0f },
        { 4800.0f, 3430.0f, 355.0f },
        { 4880.0f, 3350.0f, 372.0f },
        { 4950.0f, 3300.0f, 378.0f },
        { 5010.0f, 3155.0f, 356.0f },
    };

    // Per-vehicle route latch. The chain is picked ONCE from the build position (a live-Y test
    // flip-flops chains near the 2841 axis — runtime-observed) and the index only advances on a
    // real 2D arrival, so a waypoint can never be dropped early by X-projection. Process-lifetime
    // map keyed by vehicle GUID — bounded by vehicle churn, stale entries harmless (same
    // lifecycle as s_dryFire / s_lastLeg).
    struct RoadProgress
    {
        int8  chain = -1;   // -1 none (not a far-south build), 0 = SE, 1 = SW
        uint8 idx   = 0;
    };
    std::unordered_map<ObjectGuid, RoadProgress> s_road;

    RoadWp const* NextRoadWaypoint(Unit* veh)
    {
        auto it = s_road.find(veh->GetGUID());
        if (it == s_road.end())
        {
            RoadProgress rp;
            // Only far-south (spark) builds get a chain; BT/Sunken Ring builds (x ~4950) use
            // the generic channel/approach, which already works from there.
            if (veh->GetPositionX() < 4550.0f)
                rp.chain = veh->GetPositionY() >= 2841.0f ? 1 : 0;
            it = s_road.emplace(veh->GetGUID(), rp).first;
        }
        RoadProgress& rp = it->second;
        if (rp.chain < 0)
            return nullptr;
        RoadWp const* chain = rp.chain ? WG_ROAD_SW : WG_ROAD_SE;
        size_t n = rp.chain ? std::size(WG_ROAD_SW) : std::size(WG_ROAD_SE);
        while (rp.idx < n && veh->GetExactDist2d(chain[rp.idx].x, chain[rp.idx].y) <= WG_ROAD_ARRIVE)
            ++rp.idx;
        return rp.idx < n ? &chain[rp.idx] : nullptr;
    }

    // The structures ON the breach path to the relic (central axis Y~2841), in outer->inner order:
    // fortress door (X~5163) -> the mid wall-with-passage 191805 (X~5279) -> last inner door
    // (X~5397) -> relic (X~5440). Targeting the NEAREST intact of THESE (not all ~24 perimeter walls,
    // which are off-axis at Y 2630-3047) makes demolishers breach the path in sequence and reach the
    // relic, instead of getting stuck trying to level the whole keep.
    constexpr uint32 WG_KEEP_STRUCTURES[] = {
        190375,   // fortress door (outer entrance)
        191805,   // mid wall on the central axis (the "second wall", no door)
        191810,   // last inner door -> opens the relic
    };

    // Damage feedback: per-bot dry-fire tracking. CastVehicleSpell's return value does not
    // reflect the real cast outcome (upstream ignores spell->prepare's result), so target
    // health is the only ground truth — a vehicle whose shots change nothing (sunk in the
    // terrain, LOS-blocked launch, any dest quirk) must reposition instead of shooting
    // forever (runtime-observed: a demolisher at the fortress door, boulders dying a few
    // feet out, door at zero damage). World-thread only. Process-lifetime cache keyed by
    // permanent character GUID — bounded by the bot roster size (~2000 entries), never
    // cleared; stale entries self-heal on the next fire tick and are harmless.
    struct DryFire
    {
        ObjectGuid wall;         // target GO at the last fire tick
        uint32 health = 0;       // its Building.Health at the last fire tick
        uint32 lastChangeMs = 0; // last time the target's health moved (or target switched)
        bool   side   = false;   // alternate reposition side per retry
    };
    std::unordered_map<ObjectGuid, DryFire> s_dryFire;

    // GO lookup cache: this strategy re-runs at the combat react cadence (~250-500ms) per
    // driving vehicle, and FindNearestGameObject is a 600y GRID SCAN — FindTargetWall alone
    // was 3 scans per call, 100+ scans/second across a 10-vehicle siege, all on the map
    // update thread (production-observed 400-500ms world-tick diffs during battles). The
    // objects never move: 190375/191805/191810 each exist exactly ONCE in the zone
    // (BattlefieldWG.h WGGameObjectBuilding, rebuilt in place between battles) and the
    // relic 192829 is respawned per battle with a new GUID. Cache entry -> GUID and
    // resolve via Map::GetGameObject (hash lookup); a stale GUID (relic respawn) falls
    // back to one grid scan and re-caches. World-thread only, bounded at 4 entries.
    std::unordered_map<uint32, ObjectGuid> s_wgGoCache;

    GameObject* FindWgGameObject(Player* bot, uint32 entry, float range)
    {
        auto it = s_wgGoCache.find(entry);
        if (it != s_wgGoCache.end())
            if (GameObject* go = bot->GetMap()->GetGameObject(it->second))
            {
                // Preserve each caller's radius semantics — but a live single-instance
                // object out of range means "not found", never "rescan" (a grid scan,
                // which is also distance-clamped, could only find the same object).
                return bot->IsWithinDist(go, range) ? go : nullptr;
            }
        GameObject* go = bot->FindNearestGameObject(entry, range);
        if (go)
            s_wgGoCache[entry] = go->GetGUID();
        return go;
    }

    bool SiegeEnabled() { return sConfigMgr->GetOption<bool>("WintergraspBots.SiegeEnable", true); }

    Battlefield* WgBattlefield() { return sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID); }

    bool RelicReady(Battlefield* bf)
    {
        if (!bf || bf->GetTypeId() != BATTLEFIELD_WG)
            return false;
        return static_cast<BattlefieldWG*>(bf)->CanInteractWithRelic();
    }

    // True when a GO model roofs the position (mover standing inside/under a structure, e.g. the
    // workshop bay a fresh demolisher spawns in).
    bool UnderStructure(Unit* u)
    {
        return !u->GetMap()->isInLineOfSight(
            u->GetPositionX(), u->GetPositionY(), u->GetPositionZ() + 0.5f,
            u->GetPositionX(), u->GetPositionY(), u->GetPositionZ() + 40.0f,
            u->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO, VMAP::ModelIgnoreFlags::Nothing);
    }

    // Vehicle path-segment test: wall ray + climb rejection. The climb guard is what stops the
    // "tower clipping" — rampart stairs/roofs ARE walkable navmesh (players use them) and paths
    // routed demolishers up onto the gatehouse roof (runtime-proven via Z~438 leg targets).
    // NOTE: no lateral side-rays — they over-blocked the tower-flanked gate approach and
    // stalled the entire siege at the door (two battles, door at 100% HP, runtime-verified).
    bool SegmentClearWide(Unit* mover, G3D::Vector3 const& a, G3D::Vector3 const& b)
    {
        if (b.z - a.z > 2.5f)   // >~32 degrees over a 4y path step = stairs/rampart ramp
            return false;

        // Fix 4a: a siege vehicle can't swim. Block a segment ending in deep water; the navmesh
        // z at b is the bridge DECK where a bridge crosses (passes) and the lakebed in open water
        // (blocked).
        // IsUnderWater is true when (waterLevel - z) > collisionHeight, so passing the wade depth
        // as collisionHeight rejects only points whose surface is > WG_MAX_WADE_DEPTH above them.
        if (mover->GetMap()->IsUnderWater(mover->GetPhaseMask(), b.x, b.y, b.z, WG_MAX_WADE_DEPTH))
            return false;

        return mover->GetMap()->isInLineOfSight(a.x, a.y, a.z + 2.0f, b.x, b.y, b.z + 2.0f,
                                                mover->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO,
                                                VMAP::ModelIgnoreFlags::Nothing);
    }

    // The core cannot build a walkable path longer than ~296yd (74 smooth-path points x 4yd);
    // past that PathGenerator degrades to SHORTCUT|NOPATH and MovePoint launches a straight 3D
    // spline (the vehicle glides through walls/terrain). Issue movement as short walkable legs:
    // lerp <=maxLeg toward the target, ground-snap, and move to the endpoint of a REAL path.
    // Mirrors ComputeLegWaypoint in mod-wintergrasp-bots (module and fork can't share headers).
    // Re-path throttle state: mover GUID -> last leg-issue time. Bounded by the vehicle/bot
    // population; stale entries are harmless (same lifecycle as s_dryFire).
    std::unordered_map<ObjectGuid, uint32> s_lastLeg;
    // Road-unstick state: mover GUID -> time the mover FIRST failed to find any leg (0 = fine).
    std::unordered_map<ObjectGuid, uint32> s_legStuck;

    // roadUnstick: after 20s of continuous no-leg HOLDs, ground-snap-teleport the mover 30y
    // toward the target. ONLY road-route callers pass true — roads are far from every keep wall
    // line, so the hop can never cheat through siege geometry (where HOLD is intentional).
    // Recovers a vehicle that strayed off the navmesh onto a lake shelf / terrain pothole
    // (runtime-observed at 4701,2303 during road verification: FARFROMPOLY on every attempt,
    // crewed vehicles are never reaped, and it would otherwise statue for the whole battle).
    void MoveByLeg(Unit* mover, float tx, float ty, float tz, float maxLeg = 220.0f,
                   bool roadUnstick = false)
    {
        // Re-path throttle: every call below is up to 3 PathGenerator builds plus
        // per-segment WMO raycasts on the map-update thread, re-invoked at the combat
        // react cadence (~250-500ms) per driving vehicle. While the previous point order
        // is still executing there is nothing to correct (targets are static walls and
        // slow-moving aim points) — re-path at most once a second. A finished spline
        // re-legs immediately (isMoving fails), preserving the anti-stutter behavior.
        uint32 now = getMSTime();
        uint32& lastLeg = s_lastLeg[mover->GetGUID()];
        if (mover->isMoving() &&
            mover->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE &&
            getMSTimeDiff(lastLeg, now) < 1000)
            return;
        lastLeg = now;

        bool dbg = sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false);
        float dist = mover->GetExactDist2d(tx, ty);
        float frac = dist > maxLeg ? maxLeg / dist : 1.0f;
        for (int attempt = 0; attempt < 3; ++attempt, frac *= 0.5f)
        {
            float x = mover->GetPositionX() + (tx - mover->GetPositionX()) * frac;
            float y = mover->GetPositionY() + (ty - mover->GetPositionY()) * frac;
            float zSeed = mover->GetPositionZ() + (tz - mover->GetPositionZ()) * frac;
            float z = mover->GetMap()->GetHeight(mover->GetPhaseMask(), x, y, zSeed + 30.0f, true, 200.0f);
            if (z <= INVALID_HEIGHT + 1.0f)
                z = zSeed;

            PathGenerator pg(mover);
            pg.CalculatePath(x, y, z, false);
            if (dbg)
                LOG_INFO("playerbots",
                    "[WGSiege] leg {} ({:.0f},{:.0f},{:.0f})->({:.0f},{:.0f},{:.0f}) try={} type=0x{:02x} pts={}",
                    mover->GetEntry(), mover->GetPositionX(), mover->GetPositionY(), mover->GetPositionZ(),
                    x, y, z, attempt, uint32(pg.GetPathType()), pg.GetPath().size());
            if ((pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH)) ||
                pg.GetPath().size() < 2)
                continue;

            // Intact walls/doors are dynamic GO WMO models absent from the baked navmesh —
            // truncate the leg at the first path segment that crosses one, so the vehicle
            // rolls up to the obstacle (usually its own target wall) instead of through it.
            // The FIRST segment is exempt ONLY while the mover is roofed by a structure
            // (escaping the workshop bay it was built in): a blanket exemption rolls forward
            // with per-tick re-pathing and drove demolishers straight into towers.
            auto const& pts = pg.GetPath();
            size_t lastClear = pts.size() - 1;
            bool escapeOk = UnderStructure(mover);
            for (size_t i = 1; i < pts.size(); ++i)
            {
                bool clear = SegmentClearWide(mover, pts[i - 1], pts[i]);
                if (!clear && i == 1 && escapeOk)
                    continue;
                if (!clear)
                {
                    lastClear = i - 1;
                    break;
                }
            }
            if (lastClear == 0)
                continue;

            G3D::Vector3 const& last = pts[lastClear];
            s_legStuck[mover->GetGUID()] = 0;   // found a leg: not stuck
            mover->GetMotionMaster()->MovePoint(WG_SIEGE_MOVE_ID, last.x, last.y, last.z);
            return;
        }
        // No walkable, wall-clear leg: HOLD. A raw move here is exactly the straight-line spline
        // that drives vehicles through standing walls (runtime-observed) — never fall back to it.
        if (roadUnstick)
        {
            uint32& t0 = s_legStuck[mover->GetGUID()];
            if (!t0)
                t0 = now;
            else if (getMSTimeDiff(t0, now) > 20000)
            {
                t0 = 0;
                float d = std::max(1.0f, dist);
                float nx = mover->GetPositionX() + (tx - mover->GetPositionX()) / d * std::min(30.0f, d);
                float ny = mover->GetPositionY() + (ty - mover->GetPositionY()) / d * std::min(30.0f, d);
                float nz = mover->GetMap()->GetHeight(mover->GetPhaseMask(), nx, ny,
                                                      mover->GetPositionZ() + 40.0f, true, 120.0f);
                if (nz > INVALID_HEIGHT + 1.0f)
                {
                    if (dbg)
                        LOG_INFO("playerbots", "[WGSiege] road unstick {} ({:.0f},{:.0f}) -> ({:.0f},{:.0f},{:.1f})",
                                 mover->GetEntry(), mover->GetPositionX(), mover->GetPositionY(), nx, ny, nz);
                    mover->NearTeleportTo(nx, ny, nz + 0.5f, mover->GetOrientation());
                }
            }
        }
    }

    // Nearest still-standing keep wall/door within range — the demolisher breaks whatever is closest
    // (so it holds outside each wall and destroys it before advancing, rather than clipping through).
    GameObject* FindTargetWall(Player* bot)
    {
        GameObject* best = nullptr;
        // 600y: must see the LAST door (X~5397) from the keep approach (X~5100) once the nearer
        // structures are down, or arrived vehicles stall there (runtime-verified at 250y).
        float bestDist = 600.0f;
        for (uint32 entry : WG_KEEP_STRUCTURES)
        {
            GameObject* g = FindWgGameObject(bot, entry, 600.0f);
            if (!g || g->GetGOValue()->Building.Health == 0)
                continue;
            float d = bot->GetDistance(g);
            if (d < bestDist)
            {
                bestDist = d;
                best = g;
            }
        }
        return best;
    }
}

bool WgSiegeActiveTrigger::IsActive()
{
    if (!SiegeEnabled())
        return false;
    // Already crewing a controllable vehicle -> keep operating it regardless of other state.
    if (botAI->IsInVehicle(true))
        return true;
    Battlefield* bf = WgBattlefield();
    if (!bf || !bf->IsWarTime())
        return false;
    Player* bot = botAI->GetBot();
    // Attacker who can build a demolisher (Lieutenant). Non-Lieutenants fall through to ground AI.
    return bot->GetTeamId() == bf->GetAttackerTeam() && bot->HasAura(SPELL_LIEUTENANT);
}

bool WintergraspSiegeAction::Execute(Event /*event*/)
{
    if (!SiegeEnabled())
        return false;
    Battlefield* bf = WgBattlefield();
    if (!bf || !bf->IsWarTime())
        return false;
    if (botAI->GetBot()->GetTeamId() != bf->GetAttackerTeam())
        return false;

    // Relic handling takes priority whether on foot or in a vehicle: DoRelic dismounts first, so it
    // must be reachable after the bot has left the demolisher (else it would loop back to building).
    if (RelicReady(bf))
        return DoRelic();

    if (botAI->IsInVehicle(true))
        return DoSiegeCombat();
    return TryBuild();
}

// Not in a vehicle: build a demolisher, but only once the bot has pushed up to the keep gate (so the
// vehicle spawns beside the wall it will attack — no long, unreliable vehicle drive).
bool WintergraspSiegeAction::TryBuild()
{
    Player* bot = botAI->GetBot();
    Battlefield* bf = WgBattlefield();
    if (!bf)
        return false;
    if (!bot->HasAura(SPELL_LIEUTENANT))
        return false;

    TeamId t = bot->GetTeamId();

    // Build at a workshop the attacker CONTROLS (like real players at the goblin engineer) —
    // DoSiegeCombat then drives the fresh demolisher up to the keep.
    FactoryDef const* factory = nullptr;
    for (FactoryDef const& w : WG_FACTORIES)
        if (TeamId(bf->GetData(w.area)) == t && bot->GetExactDist2d(w.x, w.y) < 60.0f)
        {
            factory = &w;
            break;
        }
    if (!factory)
        return false;

    // Exit spacing: never summon while a live friendly siege vehicle is still within 18y of
    // the FACTORY BAY (not the builder — he can stand anywhere in the 60y work ring, and a
    // nearest-only lookup can miss a second vehicle still in the bay). Overlapping spawns
    // wedge in the bay door permanently (runtime-observed: a battle's whole vehicle cap
    // stuck in one bay). The previous vehicle clears the bay first.
    for (uint32 entry : { uint32(NPC_WINTERGRASP_CATAPULT), uint32(NPC_WINTERGRASP_DEMOLISHER),
                          uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE) })
    {
        std::list<Creature*> vehicles;
        bot->GetCreatureListWithEntryInGrid(vehicles, entry, 80.0f);
        for (Creature* c : vehicles)
            if (c->IsAlive() && c->IsFriendlyTo(bot) && c->GetExactDist2d(factory->x, factory->y) < 18.0f)
                return false;   // IsAlive matters here: the grid list, unlike FindNearestCreature, includes corpses
    }

    // Respect the vehicle cap minus a human reserve, per faction.
    uint32 cur = bf->GetData(t == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_VEHICLE_A : BATTLEFIELD_WG_DATA_VEHICLE_H);
    uint32 mx  = bf->GetData(t == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H);
    uint32 reserve = sConfigMgr->GetOption<uint32>("WintergraspBots.HumanReserve", 2);
    if (mx <= reserve || cur >= mx - reserve)
        return false;

    bot->CastSpell(bot, SPELL_CREATE_DEMOLISHER, true); // summons a demolisher and auto-seats the bot
    return true;
}

// In a demolisher: if the relic is open, go win; otherwise close on the nearest wall and fire.
bool WintergraspSiegeAction::DoSiegeCombat()
{
    Player* bot = botAI->GetBot();
    Unit* veh = bot->GetVehicleBase();
    if (!veh)
        return false;

    Battlefield* bf = WgBattlefield();
    if (RelicReady(bf))
        return DoRelic();

    GameObject* wall = FindTargetWall(bot);

    // Fix 4b: a fresh vehicle still far west follows the authored road to avoid the water. Once
    // it's within striking distance of a keep structure, the approach/channel logic below takes
    // over. The deep-water guard (Fix 4a) protects each of these legs.
    if (veh->GetEntry() != 28366 /*tower cannon: never drive it*/)
        if (RoadWp const* wp = NextRoadWaypoint(veh))
            if (!wall || veh->GetExactDist2d(wall->GetPositionX(), wall->GetPositionY()) > 200.0f)
            {
                if (sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false))
                    LOG_INFO("playerbots", "[WGSiege] road-route {} ({:.0f},{:.0f}) -> wp ({:.0f},{:.0f})",
                             bot->GetName(), veh->GetPositionX(), veh->GetPositionY(), wp->x, wp->y);
                MoveByLeg(veh, wp->x, wp->y, wp->z, 220.0f, /*roadUnstick*/ true);
                return true;
            }

    // West-field channeling: a vehicle approaching diagonally from the north/south field pins
    // at the keep-plateau bank — the bank's per-segment grade trips the climb guard, which can
    // NOT be relaxed (bank and rampart grades overlap; runtime-observed vehicles frozen at the
    // slope base issuing near-zero truncated legs). Merge onto the west ROAD axis first, like a
    // human driver: the road grade passes the guard (proven by every battle that breached).
    if (veh->GetEntry() != 28366 /*tower cannon: never drive it*/ &&
        veh->GetPositionX() < 5145.0f && std::fabs(veh->GetPositionY() - 2841.0f) > 70.0f &&
        (!wall || veh->GetExactDist2d(wall->GetPositionX(), wall->GetPositionY()) > 120.0f))
    {
        MoveByLeg(veh, 5085.0f, 2841.0f, 395.0f);
        return true;
    }

    if (!wall)
    {
        // No keep structure within range — fresh from a factory: drive the approach toward the
        // fortress door; FindTargetWall picks up structures as we close in.
        if (sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false))
            LOG_INFO("playerbots", "[WGSiege] drive {} at ({:.0f},{:.0f}) -> keep approach",
                     bot->GetName(), veh->GetPositionX(), veh->GetPositionY());
        MoveByLeg(veh, WG_APPROACH_X, WG_APPROACH_Y, WG_APPROACH_Z);
        return true;
    }

    // Axis channeling through fallen barriers: FindTargetWall aims at the next STANDING
    // structure, but a straight approach from an off-axis vehicle crosses the intact
    // outer wall / mid wall beside the opening — the leg truncates against it and the
    // vehicle parks on a wall it can never pass (production-observed after the gate
    // fell). Real drivers thread the openings: stage onto the keep's y~2841 central
    // axis on the near side of the fallen barrier, cross to the far side, then resume
    // the normal approach. Ladder composes per tick: outside -> gate opening ->
    // courtyard -> arch opening -> last door.
    if (veh->GetEntry() != 28366) // tower cannon: never drive it
    {
        struct BarrierDef { uint32 entry; float lineX; };
        // entries/lines mirror WG_KEEP_STRUCTURES[0..1] — keep in sync.
        constexpr BarrierDef WG_BARRIERS[] = { { 190375, 5163.0f },     // fortress gate
                                               { 191805, 5279.0f } };   // mid wall
        constexpr float WG_AXIS_Y = 2841.0f;
        for (BarrierDef const& b : WG_BARRIERS)
        {
            if (veh->GetPositionX() > b.lineX)
                continue;                                   // already past this line
            if (wall->GetPositionX() < b.lineX + 5.0f)
                break;                                      // target is on our side of it
            // A barrier between us and the target: it must be FALLEN (FindTargetWall
            // picks the NEAREST standing structure, so a standing barrier would be the
            // target itself) — but verify, and fall through to the normal truncating
            // approach if it somehow stands.
            GameObject* bgo = FindWgGameObject(bot, b.entry, 600.0f);
            if (bgo && bgo->GetGOValue()->Building.Health != 0)
                break;
            float sx, sy;
            if (std::fabs(veh->GetPositionY() - WG_AXIS_Y) > 25.0f)
            {
                sx = b.lineX - 40.0f;   // stage onto the axis on the NEAR side first — the
                sy = WG_AXIS_Y;         // staging point is WEST of the wall line, so this
                                        // leg never crosses a standing wall
            }
            else
            {
                sx = b.lineX + 30.0f;   // on axis: thread the opening
                sy = WG_AXIS_Y;
            }
            if (sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false))
                LOG_INFO("playerbots", "[WGSiege] channel {} at ({:.0f},{:.0f}) -> ({:.0f},{:.0f}) through line {:.0f}",
                         bot->GetName(), veh->GetPositionX(), veh->GetPositionY(), sx, sy, b.lineX);
            MoveByLeg(veh, sx, sy, veh->GetPositionZ());
            return true;
        }
    }

    // CastVehicleSpell allows up to 120y, but casts at the wall origin's ARCH height fail
    // LOS from mid-range ground (runtime-observed: 10 min of dry fire from 77-89y) — every
    // verified breach fired from 25-40y. Close to 45y, then fire; the >gate branch already
    // drives to a 25y-front point.
    float dist = veh->GetExactDist2d(wall->GetPositionX(), wall->GetPositionY());
    if (dist > 45.0f)
    {
        // Aim at a GROUND point in front of the wall on the vehicle's side, NOT the GO origin:
        // door/wall origins sit at arch height ON the wall line, so height-snapping them lands
        // on the battlement roof and the mesh routes the vehicle up the rampart (runtime-
        // observed "tower clipping").
        float dirX = veh->GetPositionX() - wall->GetPositionX();
        float dirY = veh->GetPositionY() - wall->GetPositionY();
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 1.0f)
            len = 1.0f;
        float ax = wall->GetPositionX() + dirX / len * 25.0f;
        float ay = wall->GetPositionY() + dirY / len * 25.0f;
        MoveByLeg(veh, ax, ay, veh->GetPositionZ());
        return true;
    }

    // Dry-fire feedback: 10s parked in fire range with ZERO damage on the target => this
    // spot is bad (sunk in terrain, blocked launch, any dest quirk — cast results are
    // unreliable upstream; target health is the only ground truth). Damage from ANY vehicle
    // refreshes the window: the spot is only declared bad while the whole target stalls.
    DryFire& df = s_dryFire[bot->GetGUID()];
    uint32 hp = wall->GetGOValue()->Building.Health;
    uint32 now = getMSTime();
    if (df.wall != wall->GetGUID() || hp != df.health || !df.lastChangeMs)
    {
        df.wall = wall->GetGUID();
        df.health = hp;
        df.lastChangeMs = now;   // new target or damage landed — the spot is productive
    }
    else if (now - df.lastChangeMs > 10000)
    {
        df.lastChangeMs = now;
        df.side = !df.side;

        // Ground snap: a vehicle sunk below the mesh launches dead shots from inside the
        // terrain. Lift it back to ground height in place.
        float gz = veh->GetMap()->GetHeight(veh->GetPhaseMask(), veh->GetPositionX(),
                                            veh->GetPositionY(), veh->GetPositionZ() + 5.0f);
        if (gz > INVALID_HEIGHT && veh->GetPositionZ() < gz - 0.5f)
            veh->NearTeleportTo(veh->GetPositionX(), veh->GetPositionY(), gz + 0.5f,
                                veh->GetOrientation());

        // Fresh firing point: the 25y-front ground point pushed ~20y sideways
        // (perpendicular to the approach axis), alternating side per retry.
        float dirX = veh->GetPositionX() - wall->GetPositionX();
        float dirY = veh->GetPositionY() - wall->GetPositionY();
        float len = std::max(1.0f, std::sqrt(dirX * dirX + dirY * dirY));
        float s = df.side ? 1.0f : -1.0f;
        float ax = wall->GetPositionX() + dirX / len * 25.0f - dirY / len * 20.0f * s;
        float ay = wall->GetPositionY() + dirY / len * 25.0f + dirX / len * 20.0f * s;
        if (sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false))
            LOG_INFO("playerbots", "[WGSiege] dry fire: {} at ({:.0f},{:.0f}) wall {} -> reposition ({:.0f},{:.0f})",
                     bot->GetName(), veh->GetPositionX(), veh->GetPositionY(), wall->GetEntry(), ax, ay);
        MoveByLeg(veh, ax, ay, veh->GetPositionZ());
        return true;
    }

    // Never re-cast over an in-flight throw: this action re-runs at the combat react cadence
    // (~250-500ms) and an unconditional CastVehicleSpell interrupts CURRENT_GENERIC_SPELL —
    // self-cancelling the boulder mid-windup (a "fires but nothing ever lands" loop that no
    // reposition can fix). Wait out the current cast and the throw cooldown instead.
    if (veh->GetCurrentSpell(CURRENT_GENERIC_SPELL) || veh->HasSpellCooldown(SPELL_HURL_BOULDER))
        return true;

    // Aim: set "bg siege" to the wall, then fire with the vehicle base as target so the projectile
    // destination resolves to the bg-siege position (the wall). Proven in the Phase 3 spike.
    PositionMap& pm = botAI->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
    PositionInfo sp = pm["bg siege"];
    sp.Set(wall->GetPositionX(), wall->GetPositionY(), wall->GetPositionZ(), bot->GetMapId());
    pm["bg siege"] = sp;
    return botAI->CastVehicleSpell(SPELL_HURL_BOULDER, veh);
}

// Last door is down: dismount, walk to the Titan relic, and use it -> attacker victory.
bool WintergraspSiegeAction::DoRelic()
{
    Player* bot = botAI->GetBot();
    if (bot->GetVehicleBase())
    {
        bot->ExitVehicle();
        return true;
    }
    // Cached lookup — the relic is respawned each battle with a NEW guid; the cache's
    // stale-GUID fallback rescans once per battle and re-caches.
    GameObject* relic = FindWgGameObject(bot, GO_TITAN_RELIC, 200.0f);
    if (!relic)
        return false; // not close enough yet; ground AI advances the bot into the keep
    // Use from a generous range (interaction range is a few yards, but bot movement can hover short
    // of the exact point — 5y was too tight and the bot never clicked). Close in, then use.
    if (bot->GetDistance(relic) > 12.0f)
    {
        MoveByLeg(bot, relic->GetPositionX(), relic->GetPositionY(), relic->GetPositionZ());
        return true;
    }
    relic->Use(bot); // ProcessEvent -> EndBattle(false) = attacker win
    return true;
}

void WintergraspSiegeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("wg siege active", { NextAction("wg siege", ACTION_MOVE + 9.0f) }));

    // Defenders manning keep tower cannons (the WG director seats them and curates their
    // "current target"): fire at hostile siege vehicles in range. No-op for vehicles that don't
    // have the spell (e.g. attacker demolishers, which fire imperatively via DoSiegeCombat).
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("fire cannon", ACTION_MOVE + 9.0f) }));
}
