/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40HELPERS_H
#define PLAYERBOTS_AQ40HELPERS_H


#include <cmath>

#include "Aq40HuhuranRules.h"
#include "Aq40TwinsRules.h"
#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

namespace TempleOfAhnQirajHelpers
{

constexpr uint32 AQ40_MAP_ID = 531;

// InstanceScript data ids from src/server/scripts/Kalimdor/TempleOfAhnQiraj/temple_of_ahnqiraj.h.
// Hardcoded because a module cannot include a script-local header; the startup log line prints
// them so a drift shows up in game rather than as a silently dead encounter gate.
constexpr uint32 AQT_DATA_TWIN_EMPERORS = 7;
constexpr uint32 AQT_DATA_VEKLOR = 15;
constexpr uint32 AQT_DATA_VEKNILASH = 16;

constexpr uint32 NPC_VEKNILASH = 15275;
constexpr uint32 NPC_VEKLOR = 15276;
constexpr uint32 NPC_QIRAJI_SCARAB = 15316;
constexpr uint32 NPC_QIRAJI_SCORPION = 15317;

constexpr uint32 SPELL_MUTATE_BUG = 802;   // +300% health, +1800% physical damage, 240s
constexpr uint32 SPELL_EXPLODE_BUG = 804;  // 3s fuse, then ~3000 fire in 15y
constexpr uint32 SPELL_BLIZZARD = 26607;   // 10y patch, ~1500 per 2s, -30% speed

// ---------------------------------------------------------------------------------------------
// POSITIONS (v2, spec 2026-10-22 §5.1/§5.3, Revision 2 §11). Two FIXED stations, A and B, 105-120 y apart
// (authored at 120 y): past Heal Brother's 69 y gate (the script's IsWithinDist(twin, 60) plus both combat
// reaches) with >= 20 y of margin. Tank waiting spots are on each station's outer side; healers on the inner side,
// between the bosses. The ranged arcs (magic DPS around Vek'lor's station, hunters around Vek'nilash's) are also
// inner, but since Revision 9 §18.1 only form-up uses them (hunters stage on B's arc); in the fight ranged Dps own no
// spot.
// Each side keeps a melee tank and a warlock tank, one active and one waiting for the twin that lands there at the
// next Twin Teleport. Healers never leave their side. A is Vek'lor's spawn, because he barely moves at the pull.
// The pure geometry rules are in Aq40TwinsRules.h.
// ---------------------------------------------------------------------------------------------

// The owned spots of one station. Field order is the order tools/aq40-twins-stations.py prints.
// healerCount / rangedCount say how many of healers[] / ranged[] the tool placed (Revision 2: B seats 7 healers);
// index them with `% healerCount` / `% rangedCount`, never `% 8` / `% 12`.
struct TwinsStationSpots
{
    Position station, warriorWait, warlockWait, warlockCast;
    Position healers[8];
    uint8 healerCount;
    Position ranged[12];
    uint8 rangedCount;
};

// Authored by tools/aq40-twins-stations.py (floor-probed on map 531; data in tools/aq40-twins-stations.json).
// Never hand-edit one coordinate: re-run the tool and replace this whole block with its printed output.
// While TWINS_STATIONS_PROVISIONAL is true GetTwinsView returns false (the whole v2 Twins strategy is inert)
// and the strategy logs a LOG_WARN at load; it is false now.
constexpr bool TWINS_STATIONS_PROVISIONAL = false;
inline TwinsStationSpots const TWINS_STATIONS[2] = {
    { // A
        Position(-8868.67f, 1206.24f, -104.30f),
        Position(-8857.20f, 1214.27f, -104.30f),
        Position(-8858.67f, 1206.24f, -104.30f),
        Position(-8894.67f, 1206.24f, -109.80f),
        {
            Position(-8868.67f, 1184.24f, -104.30f),
            Position(-8868.67f, 1228.24f, -104.30f),
            Position(-8875.40f, 1231.35f, -105.40f),
            Position(-8876.19f, 1185.57f, -104.30f),
            Position(-8877.12f, 1224.37f, -106.70f),
            Position(-8883.58f, 1227.54f, -108.60f),
            Position(-8882.81f, 1189.39f, -104.30f),
            Position(-8884.15f, 1220.43f, -107.90f)
        },
        8,
        {
            Position(-8890.67f, 1206.24f, -107.80f),
            Position(-8892.32f, 1202.15f, -109.10f),
            Position(-8892.32f, 1210.33f, -110.40f),
            Position(-8891.28f, 1198.19f, -108.40f),
            Position(-8891.28f, 1214.29f, -111.10f),
            Position(-8889.58f, 1194.46f, -106.50f),
            Position(-8889.58f, 1218.02f, -109.20f),
            Position(-8887.27f, 1191.07f, -105.80f),
            Position(-8888.04f, 1222.04f, -109.90f),
            Position(-8885.73f, 1186.62f, -104.30f),
            Position(-8881.91f, 1223.81f, -108.60f),
            Position(-8881.10f, 1185.71f, -104.30f)
        },
        12,
    },
    { // B
        Position(-8988.67f, 1206.24f, -112.30f),
        Position(-9000.14f, 1198.21f, -112.30f),
        Position(-8998.67f, 1206.24f, -112.30f),
        Position(-9008.59f, 1204.50f, -112.30f),
        {
            Position(-8988.67f, 1232.24f, -112.30f),
            Position(-8988.67f, 1186.24f, -112.30f),
            Position(-8985.20f, 1225.94f, -112.30f),
            Position(-8984.16f, 1180.63f, -112.30f),
            Position(-8980.63f, 1186.84f, -112.30f),
            Position(-8977.92f, 1223.11f, -112.30f),
            Position(-8975.16f, 1191.49f, -112.30f)
        },
        7,
        {
            Position(-8964.67f, 1206.24f, -112.30f),
            Position(-8965.02f, 1210.33f, -112.30f),
            Position(-8965.02f, 1202.15f, -112.30f),
            Position(-8966.06f, 1214.29f, -112.30f),
            Position(-8966.06f, 1198.19f, -112.30f),
            Position(-8967.76f, 1218.02f, -112.30f),
            Position(-8967.76f, 1194.46f, -112.30f),
            Position(-8970.07f, 1221.41f, -112.30f),
            Position(-8970.07f, 1191.07f, -112.30f),
            Position(-8972.92f, 1224.35f, -112.30f),
            Position(-8972.92f, 1188.13f, -112.30f),
            Position(-8976.24f, 1226.77f, -112.30f)
        },
        12,
    }
};
// warrior B's form-up staging spot: 30 y from Vek'nilash's spawn toward B (R16)
inline Position const TWINS_STAGE_B = Position(-9000.89f, 1195.76f, -112.30f);

constexpr float TWINS_EXPLODE_RADIUS = 15.0f;
constexpr float TWINS_BLIZZARD_RADIUS = 10.0f;
// Revision 5 §14.3: a Blizzard centre within TWINS_BLIZZARD_RADIUS + this of a bot's spot moves the spot to its escape.
constexpr float TWINS_BLIZZARD_SPOT_MARGIN = 3.0f;
// Fix round 1: escape attempts (each 4 sectors on) tried for one outside Vek'lor's berth; none = the spot stays put.
constexpr uint32 TWINS_BLIZZARD_SPOT_ATTEMPTS = 4;

// "Close enough, stop shuffling."
constexpr float TWINS_STATION_TOLERANCE = 4.0f;
// Grid searches for bugs and hazards, always issued FROM THE BOT - Cell::Visit clamps every radius
// to ~533y, so a search from far away silently returns null.
constexpr float TWINS_SEARCH_RANGE = 60.0f;
// Escape bearings snap to this many fixed sectors, so a flee destination is stable across ticks. A
// spot derived from the bot's live position creeps, and MoveTo's duplicate guard then refuses every
// order.
constexpr uint32 TWINS_ESCAPE_SECTORS = 16;

// The per-instance state refreshes at most this often; between refreshes GetTwinsView copies it.
constexpr uint32 TWINS_SNAPSHOT_TTL_MS = 500;

// The active melee tank walks Vek'nilash back once he is more than this from his side's station (2-D).
constexpr float TWINS_BOSS_ON_STATION = 5.0f;
// Hold-station route step (copied from BWL's GetNefMtPathStep): at least MIN along the route, clipped to MAX, so
// aggro and facing are re-checked often. Within MIN of the station the step is the station itself.
constexpr float TWINS_MT_STEP_MIN = 2.0f;
constexpr float TWINS_MT_STEP_MAX = 3.5f;
// Ruling R15: the active warlock uses its fixed casting spot only while Vek'lor is within this of his station (2-D).
constexpr float TWINS_VEKLOR_ON_STATION = 5.0f;
// Ruling R15: an off-station active warlock inside WRONG_VICTIM_MIN of Vek'lor escapes to this far from him.
constexpr float TWINS_WARLOCK_ESCAPE_DIST = 14.0f;
// "aq40 twins off spot" fires, and "hold spot" walks, only beyond this from the owned spot (2-D).
constexpr float TWINS_SPOT_ARRIVE = 1.0f;
// "aq40 twins victim hold" walks only beyond this from its spot (2-D): the spot is the bot's own position while
// it already stands 12-40 y from Vek'lor, or Vek'nilash's active melee tank, which it must not stand inside.
constexpr float TWINS_VICTIM_ARRIVE = 2.0f;
// Revision 2 §11.2: a wrong victim of Vek'lor beyond WRONG_VICTIM_MAX (40 y) of him walks to this far from him.
constexpr float TWINS_VICTIM_FAR_DIST = 38.0f;
// Fix round 1 (R22): an anchor never sits farther than this from its AUTHORED station (2-D). Without the clamp the
// anchors ratcheted outward by a waiting-spot offset per swap, and separation only grew, so the guard never fired.
constexpr float TWINS_ANCHOR_MAX_OFFSET = 12.0f;
// Revision 3 §12.2, unreachable spots: "hold spot" / "stage" count a failure when MoveTo returns false while the bot
// stands (not moving) more than TWINS_UNREACHABLE_DIST from its spot, at most one per TWINS_UNREACHABLE_DEBOUNCE_MS
// (MoveTo also returns false mid-walk and for a few ticks after another mover's walk, through its duplicate and
// wait-for-last-move guards). A successful MoveTo resets the count; TWINS_UNREACHABLE_FAILS in a row park the bot
// where it stands until the anchor epoch changes (fight) or the form-up episode ends (staging).
constexpr float TWINS_UNREACHABLE_DIST = 2.0f;
constexpr uint8 TWINS_UNREACHABLE_FAILS = 2;
constexpr uint32 TWINS_UNREACHABLE_DEBOUNCE_MS = 1000;

// ---------------------------------------------------------------------------------------------

// Revision 2 §11.4: which inner ranged arc a Dps bot belongs to. Magic = not IsPhysical and not a healer (spare
// warlocks and fill-ins included): the arc around Vek'lor's station. Hunter: the arc around Vek'nilash's station.
// None: everyone else (melee Dps and spare melee tanks keep native positioning). Revision 9 §18.1: form-up only (a
// hunter's staging spot is its B arc spot); in the fight no Dps owns an arc spot.
enum class TwinsRangedKind : uint8 { None, Magic, Hunter };

// The bot's latched assignment (spec §5.2). {None, Side::None, false} before the latch and for any guid the
// latch did not see (a late joiner). A spare melee tank or warlock is Role::Dps with spare = true.
struct TwinsMe
{
    Aq40TwinsRules::Role role = Aq40TwinsRules::Role::None;
    Aq40TwinsRules::Side side = Aq40TwinsRules::Side::None;
    bool spare = false;
    // Healers only: the bot's guid rank among its side's latched healers (0 = lowest guid). Healers are never
    // replaced, so it holds for the attempt. GetTwinsSpot uses healers[rankOnSide % healerCount].
    uint8 rankOnSide = 0;
    // Revision 2 §11.4, computed once at the latch (TwinsRangedRanks). Magic: guid rank among the latched magic Dps,
    // then the latched magic warlock tanks after them (so a fill-in or warlock tank demoted to Dps later gets a
    // distinct spot). Hunter: guid rank among the latched hunters. Form-up staging uses ranged[rangedRank %
    // rangedCount] (Revision 9 §18.1: GetTwinsSpot no longer does).
    TwinsRangedKind rangedKind = TwinsRangedKind::None;
    uint8 rangedRank = 0;
    // Revision 3 §12.2: the bot failed to reach its owned spot TWINS_UNREACHABLE_FAILS times in a row this anchor
    // epoch, so GetTwinsSpot returns its own position (it holds where it stands). Never set for staging.
    bool parked = false;
    // Revision 4 §13.2, healers only: the step-in is due (Aq40TwinsRules::HealerStepInDue, tracked per healer by the
    // refresh), and stepInTank is that tank's position at the last refresh. Never set for staging.
    bool stepIn = false;
    Position stepInTank;
    // Revision 11 §20.2: the step-in is (also) the LOS one (Aq40TwinsRules::HealerLosStepInDue), so its point is
    // STEP_IN_LOS_DIST from the tank instead of STEP_IN_DIST. Implies stepIn.
    bool stepInLos = false;
};

// Per-call view of the per-instance state (refreshed at most every TWINS_SNAPSHOT_TTL_MS). Pointers are valid
// for the calling tick only.
struct TwinsView
{
    bool valid = false;
    Creature* veklor = nullptr;
    Creature* veknilash = nullptr;
    // Vek'lor's side from geometry with hysteresis (Aq40TwinsRules::UpdateVeklorSide); A at the latch, because
    // he spawns there. Vek'nilash is always on Other(veklorSide).
    Aq40TwinsRules::Side veklorSide = Aq40TwinsRules::Side::A;
    // Ruling R1: Vek'lor's side captured at the landing window's rising edge, i.e. the side he teleported FROM
    // (= the side Vek'nilash is landing on). Read it only while landingWindow; it keeps its last value after.
    Aq40TwinsRules::Side windowVeklorFrom = Aq40TwinsRules::Side::None;
    // Ruling R13 F4: each twin's position at the window's rising edge. Twin Teleport has ALREADY moved both when the
    // window opens (boss_twinemperors SpellHit: both NearTeleportTo, then ACTION_AFTER_TELEPORT sets REACT_PASSIVE),
    // so these are where each twin LANDED; both stay rooted for the window, and the swap is exact (Vek'lor's is
    // Vek'nilash's pre-teleport position and vice versa). The landing zones are centred on them, not on the
    // stations. Read them only while landingWindow; they keep their last values after.
    Position windowVeklorPos;
    Position windowVeknilashPos;
    // Either twin IsInCombat() && REACT_PASSIVE: the 2 s after Twin Teleport (spec §4).
    bool landingWindow = false;
    // 0 inside the window; else ms since the last window ended, or since the latch (engage) if none yet.
    uint32 msSinceWindowOrEngage = 0;
    // Revision 3 §12.1: 0 inside the window; else ms since the last window STARTED, or since the latch (engage) if
    // none yet. The melee pre-swap move keys on it (Aq40TwinsRules::PreSwapDue).
    uint32 msSinceWindowStart = 0;
    // Revision 3 §12.2: each side's anchor (index = Side), the authored station at the latch, then wherever that
    // side's twin last settled (Aq40TwinsRules::AnchorShouldRelatch; never inside the landing window). Every per-side
    // spot is TwinsAnchored(...) of its authored spot.
    Position anchor[2];
    // Revision 8 §17.1: each side's pickup basis (index = Side), the twin on that side's position, re-latched only
    // past Aq40TwinsRules::PICKUP_BASIS_RELATCH (never inside the landing window). The two waiting pickup spots are
    // measured from it. pickupBasisSet false = not latched yet this attempt (the waiting spots fall back to anchored).
    Position pickupBasis[2];
    bool pickupBasisSet[2] = {false, false};
    // Fix round 1 (R22): the separation guard's drag station (the authored station farther from Vek'lor when the
    // guard came up), held until separation is back at SEPARATION_GUARD; None while the guard is down.
    Aq40TwinsRules::Side dragSide = Aq40TwinsRules::Side::None;
    // Designated tanks per side (index = Side), after death replacement. Whether each is active or waiting
    // depends on veklorSide: the active warlock is activeWarlock[veklorSide], the active melee tank is
    // activeMelee[Other(veklorSide)]. Empty = the side has none (degraded).
    ObjectGuid activeMelee[2];
    ObjectGuid activeWarlock[2];
};

// ---- encounter state ----
bool IsInAq40(Player* bot);
// GetBossState(AQT_DATA_TWIN_EMPERORS) == IN_PROGRESS. Free, exact, spans the whole encounter.
bool IsTwinsEncounterActive(Player* bot);
// InstanceScript::GetCreature - no grid search, which matters at a 95y spread.
Creature* GetTwin(Player* bot, uint32 dataId);
// False unless the encounter is IN_PROGRESS and both twins are alive, and always while
// TWINS_STATIONS_PROVISIONAL. Refreshes the per-instance state at most every TWINS_SNAPSHOT_TTL_MS: latch on the
// first refresh (one O(n) group scan -> Aq40TwinsRules::AssignTwins), replace dead designated tanks
// (PickReplacement), update Vek'lor's side, track the landing window (falling edge -> one "[Aq40Twins] landing"
// line) and emit the "[Aq40Twins] diag" line every Aq40TwinsRules::DIAG_INTERVAL_MS. Revision 4: the refresh also
// runs the healer step-in clocks (§13.2) and the one-time healer mana refill under the `raid` cheat (§13.4, logged
// "[Aq40Twins] mana refill <name>"), and clears the drag target at each window's rising edge (§13.5). A call while the
// encounter is not IN_PROGRESS erases the instance's state, so a re-pull re-latches.
bool GetTwinsView(Player* bot, TwinsView& view);
// The bot's latched assignment; a lookup only (never latches).
TwinsMe GetTwinsMe(Player* bot);
// veklorSide for AQT_DATA_VEKLOR, Other(veklorSide) for AQT_DATA_VEKNILASH, None otherwise.
Aq40TwinsRules::Side TwinsSideOf(TwinsView const& view, uint32 dataId);
// AQT_DATA_VEKNILASH: the designated melee tank of Vek'nilash's side; AQT_DATA_VEKLOR: the designated warlock of
// Vek'lor's side. Null if that slot is empty, or its holder is dead or not on the bot's map.
Player* TwinsActiveTankFor(TwinsView const& view, Player* bot, uint32 dataId);
// Revision 3 §12.2: `authoredSpot` (one of side `s`'s TWINS_STATIONS spots, or a point derived from them) moved by
// that side's anchor offset: anchor[s] + (authoredSpot - TWINS_STATIONS[s].station). z comes from the calling bot's
// UpdateAllowedPositionZ. Returns authoredSpot unchanged while the anchor sits on the station.
Position TwinsAnchored(Player* bot, TwinsView const& view, Aq40TwinsRules::Side s, Position const& authoredSpot);
// Revision 3 §12.2: "aq40 twins hold spot" reports each MoveTo toward its owned spot (`moved` = MoveTo's result,
// `dist` = the bot's 2-D distance from the spot). See TWINS_UNREACHABLE_*. Per-instance, per-bot, under twinsMutex.
void TwinsReportSpotMove(Player* bot, bool moved, float dist);
// Erases the instance's whole Twins state: latch, side, window, diag throttle. Never the form-up staging cache.
void TwinsEraseTrackers(Player* bot);
bool IsTwinsPhysicalGroup(Player* member, PlayerbotAI* botAI);

// ---- form-up and the pull (spec §5.5, rewritten 2026-10-22; rulings R11, R12) ----
// Out of combat, before the pull, every bot walks to a staging spot and holds it (follow vetoed); the owner then
// says "twins pull" and only warlock A opens on Vek'lor, from A's casting spot (ruling R16).
inline Position const TWINS_VEKLOR_SPAWN(-8868.31f, 1205.97f, -104.231f);
inline Position const TWINS_VEKNILASH_SPAWN(-9023.67f, 1176.24f, -104.226f);
// The staging assignment cache refreshes at most this often per instance (one group scan per refresh).
constexpr uint32 TWINS_STAGING_TTL_MS = 1000;
// No form-up call for this long ends the episode: the next one may announce again.
constexpr uint32 TWINS_STAGING_EPISODE_GAP_MS = 5000;

// True while: the bot is in AQ40 (in world, not in combat) within Aq40TwinsRules::ROOM_RADIUS of the spawns'
// midpoint, the Twins boss state is NOT_STARTED, both twins are alive and out of combat, and the bot's master is a
// real player (no bot AI, or a selfbot), not the bot itself, on the bot's map within ROOM_RADIUS of that midpoint.
// When the instance-wide part fails it drops the instance's staging cache (lock-free while no instance holds one),
// so the next form-up is a new episode.
bool IsTwinsFormUp(Player* bot);
// The bot's form-up assignment: Aq40TwinsRules::AssignTwins over its group (the same inputs the engage latch uses,
// so the same deterministic result), cached per instance for TWINS_STAGING_TTL_MS under its own mutex, apart from
// the engage latch. {None, None, false} outside form-up, ungrouped, or for a guid the scan did not see.
TwinsMe GetTwinsStagingMe(Player* bot);
// False outside form-up or without a spot (humans). At the pull Vek'lor is on A, so:
//   side-B melee tank -> TWINS_STAGE_B (ruling R16 H3: on Vek'nilash's path to A); side-B warlock -> B warlockWait;
//   side-B healers -> their B healer spots; hunters -> B's ranged arc (Revision 2 §11.4: they fight Vek'nilash at B
//   first);
//   side-A warlock -> A warlockCast (ruling R16 H2: 26 y down the crossing lane, outside the ~23 y aggro);
//   side-A melee tank, side-A healers and every magic Dps -> S_A (ruling R11: STAGE_A_DIST along A -> B, z from
//   UpdateAllowedPositionZ);
//   other physical Dps and spare melee tanks -> B warriorWait.
// Revision 3 §12.2: a bot parked by TwinsReportStageMove gets its own position (and counts as formed up).
bool GetTwinsStagingSpot(Player* bot, Position& spot);
// "aq40 twins stage" reports each MoveTo toward its staging spot (TwinsReportSpotMove's rule; parked for the rest of
// the form-up episode).
void TwinsReportStageMove(Player* bot, bool moved, float dist);
// Ruling R12: once per form-up episode per instance, when every assigned bot is within
// Aq40TwinsRules::STAGE_ARRIVE of its staging spot, the lowest-guid designated melee tank (else the lowest-guid
// assigned bot) whispers the master
// `Twins: formed up - say "twins pull" when ready.` Call it from the bot's own staging action.
void TwinsMaybeAnnounceFormedUp(Player* bot);
// All-bot raids (AiPlayerbot.Aq40.TwinEmperors.AutoPull): warlock A, everyone formed up, a bot master.
bool TwinsAutoPullDue(Player* bot);

// ---- positions (spec §5.3/§5.4) ----
// Every predicate below fetches the view once (then the bot's own assignment once), returns false / null when
// there is no view, and returns false / null for an unlatched bot (TwinsMe role None = "no opinion").
//
// Labels inside the landing window: GetTwinsSpot names "active" and "waiting" from windowVeklorFrom (the pre-
// teleport side), so the Vek'lor pickup warlock holds its waiting spot (and casts from it, Revision 10 §19.2) and the
// old active warlock holds where it stands until the roll at T+2 s. Revision 10 §19.1: the Vek'nilash pickup (the
// melee tank of windowVeklorFrom) owns no spot in the window; it engages him with native reach. Every other predicate
// uses the live veklorSide, the same as TwinsActiveTankFor.

// The bot's owned fixed spot; true whether it is on the spot or walking to it (a station veto never fails open while
// travelling). Revision 3 §12.2: every per-side spot below is anchored (TwinsAnchored) unless noted, and a parked bot
// (TwinsMe::parked) gets its own position instead of any spot it would own, except the escape and pre-swap points.
// Revision 3 §12.1: a melee Dps or spare melee tank owns the pre-swap point while the pre-swap move is due (outside the
// window, PreSwapDue on msSinceWindowStart, not a wrong victim, and (Revision 4 §13.1) no mutated bug within
// BUG_SWITCH_RADIUS of it): TwinsAnchored(Vek'lor's side, PointAlong(Vek'lor's station, Vek'nilash's station,
// PRE_SWAP_DIST)). Healer -> its side's healers[rankOnSide % healerCount], or (Revision 4 §13.2) while TwinsMe::stepIn
// the step-in point PointAlong(stepInTank, that anchored spot, STEP_IN_DIST); Revision 8 §17.1, NOT anchored: waiting
// warlock -> pickupBasis[its side] (Vek'nilash) + WARLOCK_WAIT_DIST along the authored warlockWait bearing; waiting
// melee tank -> pickupBasis[its side] (Vek'lor) + WARRIOR_WAIT_DIST along the authored warriorWait bearing (bearing =
// authored spot - authored station; inside the window the basis from before it; anchored until the first basis
// latch; Revision 10 §19.1: none for that melee tank inside the landing window, where it engages Vek'nilash);
// Revision 9 §18.1: magic Dps and hunters -> none (native reach to range; the §11.4 arcs are form-up only);
// other melee Dps and spare melee tanks -> none (native). The active melee tank owns no spot
// (TwinsShouldHoldStation). The active warlock (rulings R13 F1, R15) owns no spot inside the landing window or while it
// can cast from where it stands (it holds there: TwinsShouldSuppressGenericMovement still holds it). When it cannot:
// warlockCast while Vek'lor is within TWINS_VEKLOR_ON_STATION of his side's ANCHOR (Revision 3); else, inside
// WRONG_VICTIM_MIN of him, a stable-sector escape point TWINS_WARLOCK_ESCAPE_DIST from him; else, in spell range but
// out of LOS (final-review I1), the first point with LOS on the straight line to him (TwinsWarlockLosSpot, never
// closer than Aq40TwinsRules::WARLOCK_LOS_MIN); else (out of range of an off-station Vek'lor) no spot, and
// TwinsWarlockMayReach lets native caster reach bring it to spell distance.
// Revision 5 §14.3: whichever spot that is, while an active Blizzard's centre is within TWINS_BLIZZARD_RADIUS +
// TWINS_BLIZZARD_SPOT_MARGIN of it, the spot is BuildTwinsEscapeSpot(centre, that clearance, no park) at the first
// attempt 0..3 whose point is at least BERTH_RADIUS from a living Vek'lor (none: the spot is left unmoved).
// Revision 9 §18.2: last, any spot inside Aq40TwinsRules::BURST_ESCAPE_TO of a living, non-passive Vek'lor is pushed
// radially out to it (PushOutFrom); a parked bot standing inside that radius is unparked for the evaluation instead.
// `parkable` (optional) is false for every active-warlock spot (casting spot, escape point, LOS point: a parked warlock
// that cannot cast where it stands would never cast), the melee pre-swap point, the healer step-in point and a
// Blizzard escape: never parked, never reported to TwinsReportSpotMove (fix round 1, R22; final-review M2).
bool GetTwinsSpot(Player* bot, PlayerbotAI* botAI, Position& spot, bool* parkable = nullptr);
// The active melee tank, Vek'nilash's victim, outside the landing window, while the twins are closer than
// Aq40TwinsRules::SEPARATION_GUARD (Revision 3 §12.2), with him more than TWINS_BOSS_ON_STATION from the drag station:
// the AUTHORED station (A or B) farther from Vek'lor when the guard came up (TwinsView::dragSide, latched until it goes
// down), so he is kited away from the other boss along a probe-verified crossing. At or above the guard nobody drags
// him and his anchor follows him. `step` is the next point along a PathGenerator route to that station (the station
// itself within TWINS_MT_STEP_MIN of it; a straight-line step when there is no route).
bool TwinsShouldHoldStation(Player* bot, PlayerbotAI* botAI, Position& step);
// In the landing window: the bot (unless it is that zone's pickup tank) or its pet (bot->GetPet(), the pickup's
// pet included) is within a landing zone. Ruling R13 F4: the zones are centred where the twins landed (windowVeklorPos
// radius 13, pickup = Other(windowVeklorFrom)'s waiting warlock; windowVeknilashPos radius 17, pickup =
// windowVeklorFrom's waiting melee tank), not on the stations.
// `escape` = BuildTwinsEscapeSpot with that clearance, or the bot's own position when only its pet is inside (the
// action recalls the pet; a pickup tank holds still).
bool TwinsInLandingZone(Player* bot, PlayerbotAI* botAI, Position& escape);
// Revision 2 §11.2, the no-drag rule: the twin this latched bot is a WRONG VICTIM of, or null
// (Aq40TwinsRules::IsWrongVictim). The bot is that twin's victim, the twin is not REACT_PASSIVE (a teleported twin
// keeps its stale pre-teleport victim until its next UpdateVictim AttackStop()s it, so a passive twin attacks
// nobody), the bot is not that twin's active tank (live veklorSide labels, as TwinsActiveTankFor), and it is not a
// designated tank of the other kind (any warlock slot for Vek'nilash, any melee slot for Vek'lor). Not gated on the
// landing window: the rule is about who the twin is attacking. One view fetch, one assignment fetch.
Creature* TwinsWrongVictimOf(Player* bot, PlayerbotAI* botAI);
// The victim hold's destination for a wrong victim (TwinsWrongVictimOf). Vek'lor: its own position while 12-40 y from
// him; inside 12 y a stable-sector escape point TWINS_WARLOCK_ESCAPE_DIST (14 y) from him; beyond 40 y the point
// TWINS_VICTIM_FAR_DIST (38 y) from him on the bot's bearing. Vek'nilash (Revision 11 §20.1): while the twins are
// closer than SEPARATION_GUARD, or with no living active melee tank (Aq40TwinsRules::VictimKites), the drag station
// (the authored station farther from Vek'lor, latched as TwinsView::dragSide; `kite` set), else his active melee
// tank's position. `kite` (optional) is false for every other destination.
bool TwinsWrongVictim(Player* bot, PlayerbotAI* botAI, Position& spot, bool* kite = nullptr);
// The next hold-station route step from the bot toward `station`: the station itself within TWINS_MT_STEP_MIN of it,
// else a PathGenerator route step (TWINS_MT_STEP_MIN..TWINS_MT_STEP_MAX), else a straight-line step clipped to
// TWINS_MT_STEP_MAX. Shared by the hold station and (Revision 11 §20.1) the victim hold's kite.
void TwinsStationStep(Player* bot, Position const& station, Position& step);
// Revision 4 §13.3, the Vek'lor berth: the MoveTo destination for a strategy move toward `dest`, i.e.
// Aq40TwinsRules::BerthWaypoint from the bot round Vek'lor's live x/y (z from the bot's UpdateAllowedPositionZ), or
// `dest` itself. Inactive (returns `dest`) inside the landing window, with Vek'lor dead or missing, mid-encounter
// without a view, and for Vek'lor's active warlock (at form-up: warlock A, his active warlock at the pull), whose own
// range logic keeps it 12-28 y from him. Called by every Twins mover at MoveTo time: hold spot (pre-swap and step-in
// included), stage, hold station, landing clear and victim hold.
Position TwinsBerthed(Player* bot, Position const& dest);

// ---- targeting ----
// The nearest living mutated bug within TWINS_SEARCH_RANGE of the bot. Revision 7 §16.2: with `meleeSkipNearVeklor`, a
// physical melee non-hunter bot skips every bug within Aq40TwinsRules::MELEE_BUG_VEKLOR_SKIP (14 y, 2-D) of a living
// Vek'lor (Aq40TwinsRules::MeleeSkipsBug) and takes the nearest other one; ranged bots are unaffected. The Dps target
// pick and the pre-swap bug override pass true; the diag's `mutated=` does not.
Creature* FindTwinsMutatedBug(Player* bot, bool meleeSkipNearVeklor = false);
// Healers and waiting tanks -> null; active warlock -> Vek'lor; active melee tank and (Revision 10 §19.1) the
// Vek'nilash pickup inside the landing window -> Vek'nilash; Dps -> a mutated bug within BUG_SWITCH_RADIUS, else the
// twin its damage school can hurt (Aq40TwinsRules::IsPhysical).
Unit* GetTwinsAttackTarget(Player* bot, PlayerbotAI* botAI);
// A WITHHOLDING decision needs its own predicate. A nullptr from GetTwinsAttackTarget reads as "no
// opinion" to the target-hold multiplier, which then hands the immune twin straight back via
// dps assist. True for waiting tanks (never the Vek'nilash pickup inside the landing window, Revision 10 §19.1),
// for Dps targeting Vek'nilash whose victim is not his active tank while Aq40TwinsRules::HoldFireOnTwin holds (8 s
// fail-open; Revision 5 §14.1: never for Vek'lor), and (Revision 3 §12.1) for a bot whose melee pre-swap move is
// due. A mutated bug never holds fire (Revision 4 §13.1: one within BUG_SWITCH_RADIUS also ends the pre-swap
// move while it lives).
bool IsTwinsHoldingFire(Player* bot, PlayerbotAI* botAI);
bool TwinsShouldRetarget(Player* bot, PlayerbotAI* botAI);

// ---- threat (spec §5.6) ----
// Aq40TwinsRules::ThreatVetoed / ThreatHardStop with the bot's threat on `twin` against that twin's active tank's
// (TwinsActiveTankFor; no tank = no cap). Revision 5 §14.1: for Vek'lor, against his current victim's when that is
// a player or pet other than the bot (else his active warlock's). False when `twin` is not a twin, and for healers,
// active tanks (of either twin) and unlatched bots.
bool TwinsThreatVetoed(Player* bot, Unit* twin);
bool TwinsThreatHardStop(Player* bot, Unit* twin);

// ---- warlock tank ----
// Ruling R13 F1: outside the landing window, the bot is Vek'lor's active warlock (Revision 10 §19.2: inside it, the
// Vek'lor pickup, the warlock of Other(windowVeklorFrom)), not moving, and can cast from where it stands: Vek'lor
// within Searing Pain range (IsWithinCombatRange at Playerbots.SpellDistance, 28.5 y plus both combat reaches, the
// core's own Spell::CheckRange test at a 1.5 y tighter range) and in LOS, and the bot at least
// Aq40TwinsRules::WRONG_VICTIM_MIN (12 y) from him, outside Arcane Burst (not tested while he is REACT_PASSIVE).
// Wherever it stands, not only on its casting spot (dev pull 2: it cast once from S_A, then walked 51 y to the spot
// and never cast again).
bool TwinsShouldWarlockTank(Player* bot, PlayerbotAI* botAI);
// Ruling R15: the active warlock cannot cast from where it stands, Vek'lor is more than TWINS_VEKLOR_ON_STATION off
// his station, the bot is at least WRONG_VICTIM_MIN from him and out of spell range (in range, the blocker is LOS and
// it walks to its LOS point instead: final-review I1), outside the landing window, and no Explode / Blizzard
// dodge or (Revision 7 §16.1) burst escape is due. The movement multiplier then lets ReachTargetAction (native
// caster reach) through for this bot; follow and formation movers stay vetoed.
bool TwinsWarlockMayReach(Player* bot, PlayerbotAI* botAI);

// ---- hazards ----
Creature* FindTwinsExplodingBug(Player* bot);
bool TwinsShouldClearExplode(Player* bot, PlayerbotAI* botAI);
bool GetTwinsExplodeClearSpot(Player* bot, Position& spot, uint32 attempt = 0);
bool GetTwinsBlizzardCenter(Player* bot, Position& center);
bool TwinsShouldClearBlizzard(Player* bot, PlayerbotAI* botAI);
bool GetTwinsBlizzardClearSpot(Player* bot, Position& spot, uint32 attempt = 0);
// Revision 7 §16.1, the universal burst escape: the bot is alive, the encounter is IN_PROGRESS and the bot is within
// Aq40TwinsRules::BURST_ESCAPE_TRIGGER (12.5 y, 2-D) of a living Vek'lor that is not REACT_PASSIVE
// (Aq40TwinsRules::InBurstRange). Every bot, latched or not, every role (tanks and the active warlock included).
// Checked without the view, like the Explode / Blizzard dodges.
bool TwinsInBurstRange(Player* bot);
// The burst escape point: BuildTwinsEscapeSpot round Vek'lor at Aq40TwinsRules::BURST_ESCAPE_TO (16 y), on the stable
// sector bearing `attempt` (never "parked" short of it). False when there is no Vek'lor.
bool GetTwinsBurstClearSpot(Player* bot, Position& spot, uint32 attempt = 0);
// parkWhenClear = false always builds the point (never "already clear: park"): the off-station warlock's 14 y
// escape (ruling R15) must reach 14 y even when it already stands 10-12 y out, where it still cannot cast.
bool BuildTwinsEscapeSpot(Player* bot, float cx, float cy, float clearance, uint32 attempt,
                          Position& spot, bool parkWhenClear = true);

// ---- pets ----
// The encounter is IN_PROGRESS or form-up holds, and the bot has a pet or a controlled creature.
bool TwinsShouldDirectPet(Player* bot, PlayerbotAI* botAI);
// Final-review I2: the Twins pet actions record every owner whose pets they changed (passive, park, follow, attack
// order), per instance. Once the encounter is not IN_PROGRESS (kill, wipe, or no pull yet) and that owner is out of
// form-up and out of combat, GetTwinsView's not-IN_PROGRESS path (throttled to TWINS_PET_RESTORE_CHECK_MS per
// instance, on the calling bot's map thread, so every owner it finds is on that thread too) restores its pets with
// TwinsRestorePets and forgets it. Owners never recorded (a human's pet, a bot the strategy never directed) are never
// touched.
constexpr uint32 TWINS_PET_RESTORE_CHECK_MS = 1000;
void TwinsMarkPetsTouched(Player* owner);
// Restores `owner`'s live pets and guardians: react state from Playerbots.DefaultPetStance (the playerbots "pet
// stance" mapping: 0 passive, 1 defensive, 2 aggressive, else defensive) and following the owner (clears a
// COMMAND_STAY park). True when it had at least one live pet and every one now has COMMAND_FOLLOW; false keeps the
// owner recorded for a later try (no pet out yet, or one under a controlled motion such as a fear). Defined in
// Aq40Actions_Twins.cpp beside the other pet helpers.
bool TwinsRestorePets(Player* owner);

// Shared by the triggers and the movement multiplier so the veto and the moves cannot disagree. True while an
// Explode / Blizzard dodge or (Revision 7 §16.1) the burst escape is due (checked without the view), or while any
// of GetTwinsSpot, TwinsShouldHoldStation, TwinsInLandingZone or TwinsWrongVictim holds (one view fetch). The no-drag
// veto (TwinsWrongVictimOf) is separate and stronger: TwinEmperorsMovementMultiplier applies it first.
// Revision 10 §19.1: false (past the hazard dodges) for the Vek'nilash pickup inside the landing window, so native
// reach and melee close in on him.
bool TwinsShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI);
// Revision 10 §19.3: inside the landing window a latched healer makes no move unless `isExemptDodge` (the caller's
// name test: "aq40 twins clear explode", "aq40 twins clear blizzard", "aq40 twins landing clear"). Hold spot, step-in
// and heal reach are vetoed by TwinEmperorsMovementMultiplier; casting is unaffected. A PlayerbotAI::IsHeal pre-test
// (the latch's own healer test), then one view fetch, then one assignment fetch only inside the window.
bool TwinsHealerFrozen(Player* bot, bool isExemptDodge);

// Revision 6 §15.1, side tank heals. Bot healers only heal their own raid subgroup plus the raid's main tank, and the
// warlock tanks sit in healerless subgroups. Non-null only for a latched healer with a side, a valid view, not moving
// and not a wrong victim (TwinsWrongVictimOf's test), with a class heal (Aq40TankHealSpellName): among its side's
// designated tanks (activeMelee[side] and activeWarlock[side], active or waiting), the alive one below
// Aq40TwinsRules::TANK_HEAL_PCT, within HEAL_REACH (36 y, 2-D) and in LOS with the lowest health %. Like the Huhuran
// tank heal, only when that cast can go off now (CanCastSpell plus the real mana cost, Aq40SpellManaCost); no fallback
// to another tank. Null = stock healing runs.
Player* TwinsTankHealTarget(Player* bot, PlayerbotAI* botAI);
// One successful side tank heal in the bot's instance (the diag's tankHeals=N, reset at each diag line). Under
// twinsMutex; a no-op while the instance holds no latched Twins state.
void TwinsCountTankHeal(Player* bot);

// =============================================================================================
// PRINCESS HUHURAN (spec docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md in the
// overlay repo). The main tank drags her onto a fixed park spot P; ranged DPS bots and healer bots
// then hold fixed, evenly spaced spots on two rings centred on P (radii and timings in
// Aq40HuhuranRules.h). Melee and tanks are not slotted.
// =============================================================================================

// temple_of_ahnqiraj.h DATA_HUHURAN / NPC_HUHURAN (mirrored: script headers are not includable).
constexpr uint32 AQT_DATA_HUHURAN = 6;
constexpr uint32 NPC_HUHURAN = 15509;

constexpr uint32 SPELL_HUHURAN_WYVERN_STING = 26180;    // 12s stun; 26233 damage when it is removed
constexpr uint32 SPELL_HUHURAN_NOXIOUS_POISON = 26053;  // silence + splash, leaves an aura
constexpr uint32 SPELL_HUHURAN_BERSERK = 26068;         // 30%: Poison Bolt on the 15 nearest
constexpr uint32 SPELL_HUHURAN_ACID_SPIT = 26050;       // stacking nature DoT on her victim (diag acid=)

// Park spot P, authored 2026-10-20 by tools/aq40-huhuran-stations.py (tools/aq40-huhuran-stations.json):
// 158 open-floor grid points within 36 y; in-game verify ranged 72/72, tank 12/12, healer 70/72 (160/165 deg
// reachable but one smoothing point over the straightness limit). While HUHURAN_STATIONS_PROVISIONAL is true
// GetHuhuranView returns false, so the whole Huhuran strategy is inert except the Wyvern Sting dispel rule,
// and the strategy logs a LOG_WARN at load. Re-author with the tool; never hand-edit one coordinate.
//
// The MT's tank station T is NOT fixed: GetHuhuranTankStation latches it per attempt at
// P + HUHURAN_MT_STATION_OFFSET toward the far side of P from her approach, so she trails the backing MT
// onto P from either side of her patrol. HUHURAN_MT_STATION is only the degenerate fallback, used when she
// is within HUHURAN_MT_STATION_DEGENERATE of P at latch time (no approach direction).
constexpr bool HUHURAN_STATIONS_PROVISIONAL = false;
inline Position const HUHURAN_PARK(-8576.30f, 1683.65f, -90.70f);
inline Position const HUHURAN_MT_STATION(-8576.30f, 1688.65f, -91.10f, 4.7124f);
constexpr float HUHURAN_MT_STATION_OFFSET = 5.0f;
constexpr float HUHURAN_MT_STATION_DEGENERATE = 0.5f;

// Grid-scan radius for resolving her by entry, from a bot near P (under the ~533y Cell::Visit clamp).
constexpr float HUHURAN_SEARCH_RANGE = 250.0f;
// Only a bot this close to P may cache a "not found" scan: a far bot's empty scan says nothing.
constexpr float HUHURAN_SCAN_ORIGIN_RANGE = 150.0f;
constexpr uint32 HUHURAN_SCAN_INTERVAL_MS = 500;
// Unseen this long while IN_PROGRESS = a new attempt (backstop for the erase-trackers trigger).
constexpr uint32 HUHURAN_ATTEMPT_GAP_MS = 10000;

struct HuhuranView
{
    Creature* huhuran = nullptr;
    Player* mainTank = nullptr;
    bool parked = false;
    // She has been parked at least once this attempt (rev 2, spec §7.2 #1): latched per instance under
    // huhuranMutex, cleared with the attempt reset / erase trackers. The opener hold lasts until this.
    bool everParked = false;
    bool spotsLatched = false;
    bool berserk = false;
    uint32 msSinceEngage = 0;
};

enum class HuhuranSlotRole : uint8
{
    None = 0,
    Ranged,  // inner ring (RANGED_RADIUS)
    Healer   // outer ring (HEALER_RADIUS)
};

// IsInAq40, GetBossState(AQT_DATA_HUHURAN) == IN_PROGRESS, and a living Huhuran.
bool IsHuhuranEncounterActive(Player* bot);
// False when the gate fails, and always while HUHURAN_STATIONS_PROVISIONAL. Latches engage time, parked and
// spotsLatched per instance; emits the throttled [Aq40Huhuran] diag line.
bool GetHuhuranView(Player* bot, HuhuranView& view);
// Her victim if it is a living group role tank, else the lowest-GUID living group bot role tank.
Player* GetHuhuranMainTank(Player* bot, Creature* huhuran);
// Healer = IsHeal; Ranged = IsRanged and neither tank nor healer; None otherwise and for non-bots.
HuhuranSlotRole GetHuhuranSlotRole(Player* bot, PlayerbotAI* botAI);
// The bot's fixed ring spot. False unless spots are latched and the bot holds a latched rank.
bool GetHuhuranSpot(Player* bot, PlayerbotAI* botAI, HuhuranView const& view, Position& spot);
// Dispel rule (spec §4.4): true = do NOT remove Wyvern Sting from `target` (a non-tank player during
// the encounter). Bails first when the bot is not on map 531.
bool HuhuranKeepWyvernSting(Player* bot, Unit* target);
void HuhuranEraseTrackers(Player* bot);

// Opener hold (rev 2, spec §7.2 #1): Aq40HuhuranRules::HoldFireOnHuhuran(view.everParked, msSinceEngage).
// Time-bounded by its 45 s fail-open, so the target veto built on it is never permanent.
bool HuhuranOpenerHold(HuhuranView const& view);

// ---- rev 2 (spec §7.2) ----
// Threat discipline (§7.2 #2, mirrors BWL's BwlThreatBlocked; copied, not called). Disciplined = a
// non-healer bot that is not the MT (DPS and off-tanks). Threat is read through her ThreatMgr.
bool HuhuranThreatDisciplined(Player* bot, HuhuranView const& view);
// The MT's threat on her; 0 when there is no MT or the MT is not on her threat list.
float HuhuranMtThreat(HuhuranView const& view);
float HuhuranOwnThreat(Player* bot, HuhuranView const& view);
// The bot's cap denominator, order MT > backup > DPS (fix round 1, R11; Aq40HuhuranRules::ThreatDenominator):
// an off-tank against the MT, a DPS against the top living off-tank on her (fallback the MT). 0 = no cap.
float HuhuranThreatDenominator(Player* bot, HuhuranView const& view);
// MainTank / OffTank (any other role tank) / Other.
Aq40HuhuranRules::ThreatRole HuhuranThreatRole(Player* bot, HuhuranView const& view);
// Acid Spit stacks on `unit` (0 = none): the diag's acid= watch value.
uint32 HuhuranAcidStacks(Unit* unit);
// The per-role caps on the live view (rev 3, §8.2 #3): off-tanks 1.00 / 1.05 of the MT, DPS 0.80 / 0.90; in
// Berserk off-tanks 1.05 / 1.08 and DPS 1.00 / 1.03 of the MT with no headroom (rev 5 §10.3, R14).
bool HuhuranThreatVetoed(Player* bot, HuhuranView const& view);
bool HuhuranThreatHardStop(Player* bot, HuhuranView const& view);
// The off-tank's melee spot beside the MT (§7.2 #3): Aq40HuhuranRules::OffTankSpot with the bot's rank in
// GUID order among the living group bot role tanks other than the MT. False for the MT, non-tanks, humans,
// and when there is no MT.
bool GetHuhuranOffTankSpot(Player* bot, HuhuranView const& view, Position& spot);
// Healer tank priority in Berserk (§7.2 #4). The class fast heal, by name, or nullptr for a class without one.
// Shared with the Twins side tank heal (Twins v2 spec §15.1; was HuhuranTankHealSpellName).
char const* Aq40TankHealSpellName(Player* bot);
// The bot's mana cost for `spellName` (its highest known rank), via SpellInfo::CalcPowerCost. False when the
// spell is unknown; cost 0 for a non-mana spell. CanCastSpell never sees mana (it checks with
// TRIGGERED_IGNORE_POWER_AND_REAGENT_COST), so the tank heals check the real cost with this. Shared (Huhuran, Twins).
bool Aq40SpellManaCost(Player* bot, PlayerbotAI* botAI, char const* spellName, int32& cost);
// The bot is one of the ceil(n/2) living group healer bots nearest the MT (2-D, ties by GUID), recomputed
// from live positions (fix round 1, R12). False when there is no MT.
bool IsHuhuranTankHealer(Player* bot, PlayerbotAI* botAI, HuhuranView const& view);
// The full pick: Berserk, an MT, a tank healer, a class fast heal, the MT under 90 % else the lowest-HP% role
// tank under 90 %, and only if that cast can go off now (known, in LOS, in range, CanCastSpell). False = stock
// healing runs; there is no fallback to another tank.
bool GetHuhuranTankHeal(Player* bot, PlayerbotAI* botAI, HuhuranView const& view, Player*& tank,
                        char const*& spell);

// ---- rev 3 (spec §8.2) ----
// The MT's Berserk defensive (§8.2 #2). Big = warrior "shield wall" / paladin "divine protection" / druid
// "barkskin"; LastStand = a warrior MT's "last stand" below LAST_STAND_HP_PCT. Each fires once per attempt per
// (MT guid, spell) - a new MT gets its own - and only while known and ready (CanCastSpell on self).
enum class HuhuranMtDefensive : uint8
{
    None,
    Big,
    LastStand
};
HuhuranMtDefensive GetHuhuranMtDefensive(Player* bot, PlayerbotAI* botAI, HuhuranView const& view,
                                         char const*& spell);
void MarkHuhuranMtDefensiveUsed(Player* bot, HuhuranMtDefensive which);

// The MT's order of business each tick (spec §4.2, mirrors BWL GetNefMtStep): close to melee/LOS (Close),
// take her as the target (Attack), back her toward T while she is on the MT, in melee and not parked
// (Back). None = nothing to do; the normal tank rotation runs.
enum class HuhuranMtStep : uint8
{
    None,
    Close,
    Attack,
    Back
};
// Rev 6 (spec §11.2): an off-tank at its stacked spot - and, rev 7 (§12.2 #2), the parked MT - is turned to face
// her in place when she is outside this front arc. The movement multiplier vetoes stock "tank face" for them,
// which was the only stock action that turned a tank: pull 5's off-tanks never swung and built no rage.
constexpr float HUHURAN_FACE_ARC = static_cast<float>(M_PI) / 2.0f;
constexpr float HUHURAN_MT_CHASE_SLACK = 5.0f;    // she is on the MT and this close: she walks to it
constexpr float HUHURAN_MT_APPROACH_DIST = 1.0f;  // MoveTo(WorldObject*) edge distance
constexpr float HUHURAN_MT_ARRIVE = 2.0f;         // the MT is at T: no further Back step
HuhuranMtStep GetHuhuranMtStep(Player* bot, HuhuranView const& view, Unit* currentTarget);
// Rev 7 (spec §12.2 #2): the parked MT, standing still (IsStopped), with her outside HUHURAN_FACE_ARC - turn it in
// place. False while it moves, so the turn never claims every tick when it does not take.
bool HuhuranMtShouldFace(Player* bot, HuhuranView const& view);
// The per-attempt tank station T, latched on the first call: T = P + 5 y * unit(P - her 2-D position), z = P's
// z, facing P; HUHURAN_MT_STATION when she is within 0.5 y of P. First called when the MT's step reaches
// Back, or by the ring code if the spots latch first (rev 4: they latch at engage, so normally them). The ring refBearing
// is P -> this T. Cleared with the rest of the instance state (attempt reset, erase trackers).
Position GetHuhuranTankStation(Player* bot, HuhuranView const& view);

}  // namespace TempleOfAhnQirajHelpers

#endif
