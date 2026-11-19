/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPHELPERS_H
#define PLAYERBOTS_SWPHELPERS_H

#include <ctime>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"

class Creature;
class GameObject;
class Player;
class PlayerbotAI;
class Unit;

namespace SunwellPlateauHelpers
{

constexpr uint32 SUNWELL_MAP_ID = 580;

// The boss platform floor is z~53; a lower trash level sits at z~15 directly under it,
// and the spectral realm is at z~-74. Realm membership is decided by aura 46021 (see
// IsInSpectralRealm) - this Z is only used to detect topside bots stranded BELOW the
// platform (fell or pathed off it) so they can be walked back up.
constexpr float KALECGOS_PLATFORM_Z = 51.0f;

// Where stranded topside bots return to: the platform center (Kalecgos' spawn point).
extern const Position KALECGOS_PLATFORM_CENTER;

// Main-tank hold point: ~9y north of center, so the dragon (facing his victim)
// faces away from the southern entrance where ranged stack.
extern const Position KALECGOS_TANK_POSITION;

// Ranged/healer spacing: hold range from the boss (out of Tail Lash) and light spread.
constexpr float KALECGOS_RANGED_MIN_RANGE = 18.0f;
constexpr float KALECGOS_RANGED_SPACING = 4.0f;

// The spectral realm needs sustained DPS or the boss-half HP gap can never close and
// the balance multiplier dams the topside forever. Keep this many dps bots below.
constexpr uint32 KALECGOS_SPECTRAL_DPS_TARGET = 4;

// Brutallus - Meteor Slash (45150), the whole fight. ~20k fire DIVIDED among everyone the
// cast hits (SPELL_ATTR0_CU_SHARE_DAMAGE -> `damage /= count` in Spell::EffectSchoolDMG),
// and each of them gains a +75%-fire-taken stack for 40s (refreshed per application).
// Targeting is TARGET_UNIT_CONE_ENEMY_24 => HasInArc(radians(24)) = +-12 degrees off his
// facing, out to 65y. The only angle-free inclusion is IsWithinBoundaryRadius, which is
// max(TARGET bounding radius, MIN_MELEE_REACH) = 2y CENTER-TO-CENTER - unreachable, since
// his own combat reach (18y) keeps melee ~19y out. So off-axis attackers are never hit,
// and a lone tank in the cone eats the full ~20k (verified in-game: instant death).
//
// So the raid deliberately STACKS INTO THE CONE to share the hit, in two lanes. He faces
// his victim, so the cone covers whichever lane's tank holds aggro and a taunt swings it
// to the other lane while the first sheds.
//
// THE WHOLE FORMATION HANGS OFF ONE LATCHED ANCHOR: the spot where the main tank was standing
// the first moment he held the boss in melee range, plus the boss's position at that same
// moment (BrutallusSnapshot::anchor*). Every station is a polar offset from that pair, so every
// station is a FIXED world point for the entire pull.
//
// Three revisions were needed to get here, and each failure mode is worth stating:
//  - offsets from his SPAWN point are wrong, because the pull drags him off it (he runs to
//    whoever aggroes him), so the whole formation lands beside the real fight;
//  - offsets from his LIVE position are catastrophic: 25 bots chasing a live-anchored station
//    move him, which moves all 25 stations, and the raid runs forever;
//  - offsets from his live VICTIM's direction re-shuffle the entire raid on every tank swap,
//    which is the one moment nobody can afford to be walking.
// Latching instead makes the stations static and lets the CONE move over them - which is how
// the encounter is actually run by people, and it is only possible because the main tank is
// frozen (see BRUTALLUS_ANCHOR_FREE_RANGE), so the boss he is holding is frozen too.
constexpr float BRUTALLUS_LANE_SEPARATION = 1.5707963f;  // 90 degrees off the anchor axis
// 90, not 180: at 180 the resting lane's healers were beyond HealDistance (38.5) from the lane
// eating slashes and half the raid's healing was stranded. At 90 the worst cross-lane distance is
// 30.5y, so healing still crosses.
//
// And 90, not the 64 that the naive arithmetic gives ("two +-12 degree cones only need 24 degrees
// of gap"). At 64 the INNERMOST bots took BOTH tanks' slashes, so their stacks never decayed. The
// cone is cast from where the boss ACTUALLY stands onto wherever a bot ACTUALLY stands, and both
// error terms scale as 1/radius:
//   * the boss drifts off the latched origin the stations were measured from;
//   * a bot sits up to BRUTALLUS_STATION_TOLERANCE (5y) off its station - at r=12 that is 24
//     degrees of bearing error, against 12 degrees at r=24.
// Measured worst case over +-4y of boss drift and 5y of bot error, against the OTHER lane's cone
// edge plus 3 degrees of facing slop: at 64 the 12y ring is INSIDE that cone by 9.6 degrees; at 80
// it clears by 6.4; at 90 it clears by 16.4 and stays clear out to ~7y of boss drift.
// Do not shrink this to reclaim room - the alternative is dropping the inner ring, which costs two
// bodies per lane and so raises everyone's share of the slash.
// Which way lane 1 rotates is decided ONCE per pull by an LOS probe (snapshot laneSign) - the
// corridor is narrow and the anchor can be anywhere in it, so a hardcoded compass bearing (an
// earlier revision leaned everything southeast) only works for one pull position.
//
// Lateral spread inside a cone is an ANGLE, never a yard offset: the cone is a wedge, so a fixed
// 3.5y sidestep sits inside it at 28y and outside it at 12y.
//
// EXACTLY TWO COLUMNS, never three. A centre-column bot walking out to its Burn quarantine passes
// the side-column bot on its own ring at a measured 0.01y - it hands over the DoT on the way past.
// Two columns leave the centreline free, which is also where the frozen tank stands.
constexpr float BRUTALLUS_CONE_COLUMN_ANGLE = 0.1745329f;  // +-10 degrees (edge is 12)
// Ring radii, inside out. Fixed distances from the boss, NOT offsets from the tank's radius, so
// they do not shift with wherever the pull happened to anchor.
//   * rings <= ~20.8y are inside GetMeleeRange (18 + 1.5 + 4/3), so melee seated there can swing;
//   * every ring must stay inside SpellDistance (28.5) or "enemy out of spell" fires "reach spell"
//     at ACTION_HIGH every tick and drags ranged off station;
//   * 4y apart, because Burn jumps at 2y and bots settle a couple of yards off a requested point.
// Measured with these radii and +-10 degree columns: closest pair of stations 3.54y, closest
// approach of any quarantine WALK to any other station 3.02y.
constexpr float BRUTALLUS_RING_RADII[] = { 12.0f, 16.0f, 20.0f, 24.0f, 28.0f };
constexpr uint32 BRUTALLUS_RINGS = 5;
constexpr uint32 BRUTALLUS_SEATS_PER_LANE = BRUTALLUS_RINGS * 2;
// Burn (46394) quarantine. Burn only jumps to players who do NOT already have it, so every
// burned bot can share one zone - what matters is the WALK there. This is a STATION, not a
// flee action: an earlier revision used a move-away action that fought the station action, so
// a burned bot oscillated in and out of the stack and toured Burn through the whole raid.
//
// THE WALK MATTERS MORE THAN THE DESTINATION, and three separate revisions got it wrong. The rule
// that survives measurement: a burned bot rotates 44 degrees off its lane ON THE SIDE ITS OWN
// COLUMN ALREADY STANDS ON, and steps out to 30y+. Everything else was measured to run over
// somebody:
//   * one shared zone for both lanes  -> a lane-0 bot sweeps 104 degrees and crosses lane 1's
//     entire stack (0.07y clearance). This is what toured Burn through the raid.
//   * crossing its own lane centreline -> it passes its same-ring twin on the far side at 1.3y,
//     and a melee doing it passes the frozen tank.
//   * a seat anywhere in the 10-44 degree band -> every flee path sweeps through it (0.29y).
// 44 degrees is also the ceiling: the lanes are 64 degrees apart, so a bot on the INWARD column
// rotating further would arrive inside the other lane's cone.
constexpr float BRUTALLUS_QUARANTINE_ROTATION = 0.7679449f;    // 44 degrees off its own lane
constexpr float BRUTALLUS_QUARANTINE_RANGE = 30.0f;
constexpr float BRUTALLUS_QUARANTINE_SPREAD = 2.5f;

// A BURNED BOT MUST NEVER BE IN EITHER CONE. Meteor Slash applies the +75%-fire-taken stack, so a
// slash landing on a burning bot amplifies the DoT that is already the thing killing it - and with
// the stack refreshing on every application it never sheds either. One 44-degree rotation for
// everyone put half the raid in the GAP between the lanes, which is both adjacent to two cone edges
// and the wedge the boss travels through, so those bots kept getting clipped.
//
// Where a bot can go is constrained by what its walk would cross: a bot on its lane's OUTWARD column
// can swing 90 degrees out into open ground, but a bot on the INWARD column cannot - going further
// inward runs into the other lane's stack, and going outward crosses its own lane centreline where
// the frozen tank stands (measured: the ring nearest the tank passes him at 0.1-0.6y). So the two
// columns get two different zones:
constexpr float BRUTALLUS_BURN_OUTWARD_ROTATION = 1.5707963f;  // 90 deg: 78+ deg clear of both cones
constexpr float BRUTALLUS_BURN_OUTWARD_RANGE = 30.0f;
// The inward column's only reachable zone is the gap, so it is pushed FAR out instead. Angular error
// from boss drift scales as 1/radius, which is why the old 30y gap spot kept being clipped: at 43y,
// 8y of boss drift is 10.7 degrees against 21 degrees of clearance, versus 15.3 against 21 at 30y.
constexpr float BRUTALLUS_BURN_GAP_ROTATION = 0.7853982f;      // 45 deg: the gap bisector
constexpr float BRUTALLUS_BURN_GAP_RANGE = 43.0f;
// Slots, so burned bots do NOT pile onto one point. Burn only jumps to players who do not already
// have it - so a pile is stable while everyone in it is burning, but the moment one bot's 60s expires
// it is standing on top of a still-burning neighbour and is re-infected immediately. Observed as a
// burn spot that only ever grew. 6 degrees is 4.5y at 43y and 3.1y at 30y, and the slot order fills
// from the CENTRE outward so the safest bearings are used first.
constexpr float BRUTALLUS_BURN_SLOT_ANGLE = 0.1047198f;        // 6 degrees per slot
constexpr uint32 BRUTALLUS_BURN_SLOT_COLUMNS = 5;              // 0, +6, -6, +12, -12
constexpr float BRUTALLUS_BURN_ROW_STEP = 6.0f;
// Hand off once the holding tank carries this many stacks, provided a fresher tank exists.
// Each stack is +75% fire taken, so the slash that lands ON n stacks is (1 + 0.75n)x base, and with
// ~11 bodies in the cone base is ~1.8k:
//   3 stacks -> 3.25x, ~5.9k     4 stacks -> 4x,    ~7.3k
//   5 stacks -> 4.75x, ~8.6k     6 stacks -> 5.5x, ~10.0k
// A live test that drifted to 6 killed several level-70 clothies at once. 3 leaves real headroom
// under a ~9-10k clothie pool instead of the thin margin 4 gave.
constexpr uint32 BRUTALLUS_SWAP_STACKS = 3;
// After the main tank reaches melee range, wait for him to hold still this long before latching
// the anchor. He arrives moving - a warrior Charges, and "tank face" then shuffles him around the
// boss - so latching on arrival records a bearing he is about to leave, and because the boss turns
// to follow him the whole raid ends up lined up on a stale axis (observed twice).
constexpr uint32 BRUTALLUS_ANCHOR_SETTLE_MS = 2500;
// The off tank is pulled comfortably inside melee range instead of mirroring the main tank's radius.
// If both tanks sit at the very edge (GetMeleeRange ~20.8y) the boss STEPS toward whichever one he
// is attacking, and every step invalidates the latched geometry a little further. The gap between
// the lanes feels that first, because it is the wedge he travels through when the tanks swap - which
// is how bots parked there ended up inside a cone despite having ~30 degrees of clearance at rest.
constexpr float BRUTALLUS_OFFTANK_MAX_RADIUS = 17.0f;
// Failsafe if a swap stalls: a bot this deep steps off its lane until the stacks decay. It MUST
// stay above BRUTALLUS_SWAP_STACKS - if the two are equal, half the raid leaves the cone at the
// same moment the swap fires, shrinking the denominator and making the very hit they were fleeing
// hurt more.
constexpr uint32 BRUTALLUS_BAIL_STACKS = 6;
// The off tank builds NO threat at all for this long after the pull. Without it both tanks start
// from zero threat and trade the boss back and forth for the opening seconds (observed), which
// swings the cone wildly across a raid that is still forming up.
constexpr uint32 BRUTALLUS_OFFTANK_OPENING_MS = 5000;
// Every tank movement freeze fails open past this range: a tank who genuinely cannot reach the
// boss any more must be allowed to walk, or an encounter that slides strands him forever. It sits
// just outside melee range (~20.8y), so a tank standing on the boss is always frozen and a tank
// left behind is always free.
constexpr float BRUTALLUS_ANCHOR_FREE_RANGE = 26.0f;
// Arrival tolerance. 2.5y was far too tight and cost a whole raid: bots settle a couple of
// yards off a requested point (path endings, and 24 bodies at 3.5-6y spacing shoving each
// other), so the trigger never went quiet, the action re-issued a short move every tick, and
// the bots jittered in place - which ALSO starved every heal and attack out of the queue,
// because a positioning action that returns true stops the engine for that tick. Keep this at
// or above the ICC precedent (5y).
constexpr float BRUTALLUS_STATION_TOLERANCE = 5.0f;
constexpr float BRUTALLUS_TAUNT_RANGE = 30.0f;

// ---------------------------------------------------------------------------------------------
// Felmyst
//
// THE BREATH IS NOT AIMED AT PLAYERS. Each of the three Strafe spells (45585 top / 45633 middle /
// 45635 bottom) targets a fixed list of WORLD-TRIGGER SPAWNS (creature 23472), named by `guid` in
// the world DB's `conditions` table - 25 rows for top, 8 each for middle and bottom. A trigger that
// gets hit then carries 45582 for 10s, ticking 45782 once a second, and THAT force-casts the charm
// on every enemy within 20y OF THE TRIGGER. So:
//
//   * the lethal region is the union of 20y disks around the strafed lane's triggers, and it
//     LINGERS ~10s after she has flown past - it is not a moving beam;
//   * distance to Felmyst herself is irrelevant, and so is her 70y radius correction and the
//     MaxAffectedTargets=3 on the top lane: those only pick WHICH TRIGGERS light up;
//   * being charmed is terminal AND hostile - the charm's duration is -1 (infinite), so the victim
//     is mind-controlled with a damage buff for the rest of the fight and `Unit::Kill` fires when
//     the aura is finally removed. There is no healing through it and nothing to dispel.
//
// The fog source is the trigger LINES, at x = 1451.98 (bottom) / 1472.56 (middle) / 1494.94 (top) -
// 8 triggers each at y = 540..680, and the top lane scatters a further 17 east to x 1551. Only ONE
// lane is ever fogged at a time and it disperses before the next strafe, so the raid only needs a
// short sideways hop out of the live lane, exactly as a human raid plays it.
//
// A TRAP WORTH NOT REPEATING: comparing lane CENTRE to lane CENTRE says this is impossible. The
// lines are 20.6y and 22.4y apart against a 20.4y kill radius, so an adjacent lane's centre clears
// the live lane by only +0.2y / +2.0y - and an early revision concluded from exactly that arithmetic
// that no mid-room spot could work and marched the raid 130y to the end of the room instead. The
// error is that a player told to "stand in the bottom lane" stands anywhere in that REGION, and its
// far side clears comfortably. The room is 74y wide (x 1437..1511, from her idle flight loop,
// waypoint_data 250380), which is enough for an 8y margin against every lane.
//
// So: per-lane target x, each clearing its own firing lane by 8y, chosen to minimise the worst hop
// between any two of them (scratchpad/felmyst_lanes.py). Worst hop is 34.5y = ~4.9s at run speed,
// inside the ~6.9s of warning (the lane is picked at POINT_AIR_UP+5s, then ~1.9s of flight to the
// lane point, then 5s parked before the first trigger fires). Each target deliberately sits inside
// one of the OTHER lanes' bands - harmless, because only the live one is fogged.
//
// Indices are 0=top, 1=middle, 2=bottom, matching boss_felmyst's own `_currentLane` and its
// LeftSideLanes/RightSideLanes arrays.
constexpr float FELMYST_LANE_TRIGGER_X[3] = { 1494.94f, 1472.56f, 1451.98f };
constexpr float FELMYST_LANE_TRIGGER_Y_MIN = 540.0f;
constexpr float FELMYST_LANE_TRIGGER_Y_MAX = 680.0f;
constexpr float FELMYST_TOP_LANE_EAST_MAX = 1551.0f;  // top lane's eastern scatter
// Lethal x band per firing lane (line +- the fog radius; the top lane's band runs east to its
// scattered triggers). A bot flees to the nearer EDGE of this band plus FELMYST_FOG_MARGIN.
constexpr float FELMYST_LANE_LETHAL_WEST[3] = { 1474.54f, 1452.16f, 1431.58f };
constexpr float FELMYST_LANE_LETHAL_EAST[3] = { 1571.40f, 1492.96f, 1472.38f };

// Walkable x range. NOT the flight loop's 1437..1511 - that loop is her AIR patrol and is no
// evidence of floor. Trusting it cost a wipe: x=1501 was authored as the middle lane's east refuge
// and MoveTo refused every order there (SearchForBestPath returned INVALID_HEIGHT), so the raid
// froze mid-hop standing on the live fog line. Kept deliberately tight around the ground actually
// proven walkable - the trigger spawns run x 1451.98..1517 at ground z - and the candidate ladder
// below is what covers the rest.
constexpr float FELMYST_ROOM_X_MIN = 1440.0f;
constexpr float FELMYST_ROOM_X_MAX = 1508.0f;

// Refuge candidates, tried in order until MoveTo accepts one. Candidate 0 is the refuge itself; the
// rest are progressively SHORTER STEPS along the same axis, ending at a step so small it is almost
// certainly walkable. That shape matters:
//
// An earlier ladder only varied the END POINT, which fixed "the refuge is off-mesh" but not "this
// particular 30y hop cannot be pathed". When every candidate was refused the bot did nothing at all and
// was charmed standing still - measured 299 samples of try=5 with moving=0 across three bots. Because
// every candidate here is a step in the SAFE direction, any accepted one is strictly progress, and
// re-issuing each tick walks the bot out of the band even if it never gets a single long order.
constexpr uint32 FELMYST_REFUGE_CANDIDATES = 6;
constexpr float FELMYST_REFUGE_STEPS[FELMYST_REFUGE_CANDIDATES] =
    { 0.0f /*= the refuge itself*/, 24.0f, 18.0f, 12.0f, 8.0f, 4.0f };
constexpr float FELMYST_FOG_KEEPOUT = 2.0f;

// 45782's effect radius is index 9 = EFFECT_RADIUS_20_YARDS (confirmed against the core's own
// SpellMgr.h enum), and the area check adds the target's object size (~0.4 for a player).
constexpr float FELMYST_FOG_RADIUS = 20.4f;
// Bots keep vacating until they are this far BEYOND the kill radius. This doubles as the arrival test:
// the fog trigger deliberately does NOT use a proximity-to-refuge tolerance any more. It used to, and
// with the tolerance (6y) equal to this margin a bot could "arrive" 6y short of its refuge and be
// sitting at exactly 20.4y from the line - the kill radius. Measured: a bot charmed while logging
// dist 6.6, i.e. 19.8y from the line. Arrival is now "am I clear of the band", which cannot round the
// wrong way.
constexpr float FELMYST_FOG_MARGIN = 6.0f;

// Detection: the six lane points she parks at (3 right at low y, 3 left at high y), lane = i % 3.
constexpr float FELMYST_LANE_POINT_X[6] =
    { 1492.82f, 1466.73f, 1441.64f, 1494.75f, 1469.92f, 1446.52f };
constexpr float FELMYST_LANE_POINT_Y[6] =
    {  515.67f,  515.60f,  520.52f,  704.00f,  703.24f,  701.52f };
// She must be committed to a lane point, and clearly nearer it than to any point of a DIFFERENT
// lane, before we believe it. The hysteresis matters: she stages ~28y from the middle lane point, so
// a bare "nearest point" test would name the middle lane every time she is parked at a side, and the
// raid would hop to the middle target and back on every strafe - the oscillator this repo keeps
// re-inventing. When the read is ambiguous we return no lane and bots simply hold position.
constexpr float FELMYST_LANE_DETECT_RANGE = 40.0f;
constexpr float FELMYST_LANE_DETECT_MARGIN = 8.0f;

// The Demonic Vapor carrier runs RADIALLY OUTWARD from the raid's centre of mass to this range - not
// to a fixed far corner. A fixed corner was the first attempt and it stranded them: carriers ended up
// ~145y from the fight, and because the movement veto below takes ReachTargetAction away for the whole
// flight phase, nothing could walk them back once the vapor despawned. They sat at the wall and died.
// Radial also separates the two carriers automatically, since they start on different bearings.
//
// 60y is ample: the vapor moves at 5.6 yd/s against a bot's 7.0, so the bot outpaces it and the trail
// it leaves is well clear of the raid.
constexpr float FELMYST_VAPOR_KITE_RANGE = 60.0f;

// A bot this far from the raid's centre of mass walks back, provided no fog lane is live. This is what
// actually recovers a kited carrier - without it nothing in the strategy ever returns a bot that was
// deliberately sent away.
constexpr float FELMYST_REGROUP_RANGE = 45.0f;

constexpr float FELMYST_STATION_TOLERANCE = 6.0f;

// Encapsulate. The chain is worth writing down because none of it is in the boss script: 45661 is a
// 7s channel on a random raider (KNOCK_BACK + aura 201, so they are lifted and pacified), and the
// world DB's `spell_linked_spell` links it (SPELL_LINK_HIT) to 45665 - a 6s aura whose 1s periodic
// casts 45662. THAT is the damage: 3500 to TARGET_UNIT_SRC_AREA_ALLY within radius index 9 = 20y of
// the victim, i.e. to the raid, every second for six seconds. ~21k on anyone who stays stacked.
//
// The victim cannot help itself (pacified + rooted), so this is entirely a job for the bots AROUND it.
constexpr float FELMYST_ENCAPSULATE_RADIUS = 20.4f;  // 20y + the target's own object size
// Flee target distance. Margin over the radius has to cover the few yards bots settle off a point,
// but every extra yard is another second in the blast, so this is deliberately not generous.
constexpr float FELMYST_ENCAPSULATE_FLEE = 27.0f;

// Gas Nova (45855) is the one piece of this fight nobody can dodge: radius index 12 = 100y, i.e. the
// whole room, for ~1900 up front plus a 30s DoT and a mana drain. But DispelType is 1 = MAGIC, so it
// is removable - and Mass Dispel (32375) strips it from every ally within radius index 18 = 15y of a
// ground target, which is worth far more than the one heal the cast displaces.
// How far away a charmed raider is still worth chasing down.
constexpr float FELMYST_CHARMED_HUNT_RANGE = 60.0f;

constexpr float FELMYST_MASS_DISPEL_RADIUS = 15.0f;
// Don't burn a 15s cooldown to clean one bot; wait until the cast is actually worth its GCD.
constexpr uint32 FELMYST_GAS_NOVA_MIN_TARGETS = 3;

// The hop happens in legs of this length, never as one long order. MoveTo refuses a repeat of the
// SAME destination for ~5s (IsDuplicateMove), so an action that re-requests one unchanging point every
// tick gets at most one accepted order per 5s: measured 1-4y of progress per 5s on the first build.
// See FelmystLeaveFogLaneAction, and [[pathgenerator-296yd-ceiling]] for the general rule.
constexpr float FELMYST_TRAVEL_LEG = 30.0f;

// The room is ~230y end to end and she flies its full length ~28y up, so "am I in this fight" has to
// be generous - a tighter range would switch the fog hop and the movement veto off for whichever bots
// happen to be at the far end from her.
constexpr float FELMYST_PARTICIPANT_RANGE = 250.0f;

// ---------------------------------------------------------------------------------------------
// Eredar Twins - Lady Sacrolash (shadow) + Grand Warlock Alythess (fire)
//
// FOUR THINGS ABOUT THIS FIGHT ARE NOT WHAT THE PUBLISHED STRATEGIES SAY, and each one was read out
// of the runtime rather than the guides:
//
// 1. CONFLAGRATION IS A CHAIN BOMB, not a single-target hit. 45342 resolves ImplicitTargetB = 16
//    (TARGET_UNIT_DEST_AREA_ENEMY) at radius index 14 = 8y, so the 10s aura lands on EVERYONE within
//    8y of the victim - and each afflicted player then carries a 1s periodic that triggers 46768,
//    another 8y area hit for 1600, on top of its own 1600/s direct tick. That is the guides' "3200
//    per second", and it means one bad landing in a melee stack creates a dozen more bombs.
//    The destination is resolved at cast COMPLETION (Spell::prepare only pre-selects targets when
//    m_CastItem is set, which a boss cast never has, so SelectSpellTargets runs from Spell::cast) -
//    which is exactly why the victim running during the ~3.5s cast genuinely moves the blast.
//
// 2. DARK FLAME IS HARMLESS ON THIS CORE, so mixing schools is the CURE and not the punishment.
//    45345 is a single APPLY_AURA of SPELL_AURA_DUMMY with base points 0 and a 3s duration - no
//    damage at all. Its only job is spell_eredar_twins_handle_touch's first branch: while you carry
//    it you cannot gain either touch. And the conversion branch REMOVES the opposite touch outright:
//    take one hit of the other school and ALL of your stacks are gone, plus 3s of immunity.
//
// 3. THE TOUCH STACKS ARE THE SLOW WIPE, and they are the reason to want that cure. Both are 20-stack
//    180s auras: Dark Touched 45347 is SPELL_AURA_MOD_HEALING_PCT at -5% per stack (20 stacks = a bot
//    that cannot be healed at all), Flame Touched 45348 is 300 fire per 2s PER STACK (20 stacks =
//    1500 dps). Melee gain a Dark Touched stack every ~10s from Shadow Blades; Flame Sear sprays fire
//    on 5 random raiders every ~8-15s, which resets a fifth of the raid each cast. So in a 25-man the
//    two schools largely cancel by accident and most bots hover low - but an unlucky melee that Flame
//    Sear skips for a couple of minutes climbs to the cap and dies. Hence a cleanse trip, ranked LOW:
//    it is a tail risk, not the thing that wipes the raid.
//
// 4. KILLING ALYTHESS FIRST IS STRICTLY WORSE, and Empower makes any split DPS pure waste. 45366 is
//    Effect 10 = SPELL_EFFECT_HEAL with 5999999 base points: the survivor is FULLY HEALED. So every
//    point of damage put into the second twin before the first dies is thrown away, against a 6-minute
//    enrage on 3.5M + 3.5M HP. And the two DoAction branches are not symmetric - Sacrolash surviving
//    INHERITS Conflagration (boss_sacrolash::DoAction schedules it), while Alythess surviving loses it
//    and gains only Shadow Nova. Sacrolash must die first, so the raid focuses her.
constexpr float TWINS_CONFLAG_RADIUS = 8.4f;  // 8y + a player's object size
// Start clearing at this distance from a bomb, and clear out to here. The gap between them is what
// stops the pair behaving like an oscillator around the boundary.
constexpr float TWINS_CONFLAG_CLEAR_RANGE = 15.0f;
constexpr float TWINS_CONFLAG_CLEAR_TO = 18.0f;
// The victim's own outward run from the raid's centre of mass. ~3.5s of cast at ~7 yd/s is ~24y, so
// this is about as far as is actually reachable - and it does not need to be reached, because only the
// last position matters and every yard of it takes the blast further from the stack.
constexpr float TWINS_CONFLAG_VICTIM_FLEE = 24.0f;

// Blaze: GO 187366, a TRAP whose radius is trap.diameter * 0.5 = 5 * 0.5, firing 45246 (a Flame
// Touched applier) on a 1s cooldown for its 15s life. It is the only fire source the raid can choose
// to stand in, which makes it both the hazard Alythess's tank steps out of AND the cure for a bot
// buried in Dark Touched. Those two wants are served by ONE function (GetTwinsBlazeStation) precisely
// so they can never become the in-and-out oscillator this repo keeps re-inventing.
constexpr float TWINS_BLAZE_RADIUS = 2.5f;
constexpr float TWINS_BLAZE_SEEK_RANGE = 80.0f;   // no patch this far out => no cleanse trip at all
// 5y, not the 7y this originally used. It only has to clear a 2.5y patch, and every extra yard pushes the
// bot further out of the phase-2 stack it is supposed to be standing in - which is what turns "step out of
// the fire" and "get back in the clump" into an oscillator.
constexpr float TWINS_BLAZE_STEP_OUT = 5.0f;

// Shadow Blades (45248) is a 20y area hit around Sacrolash every ~10s - the reliable shadow source, and
// so the cure for Flame Touched. Bots stop at the EDGE of it rather than running to her: 18y still
// catches the hit, but keeps them out of the melee stack where Conflagration lands.
constexpr float TWINS_SHADOW_SEEK_RANGE = 18.0f;

// 6, lowered from 10 after the first kill. 10 stacks is already -50% healing or 900 dps of DoT, i.e. the
// bot is in trouble before anything reacts, and the measured cross-spray was nowhere near as generous as
// the estimate below it assumed - the cleanse trigger fired only 12 times in a whole fight.
constexpr uint32 TWINS_TOUCH_CLEANSE_STACKS = 6;

// PHASE 2 - Sacrolash dead, Alythess alive - IS A DIFFERENT FIGHT, and missing that killed her tank on
// the first kill. Her DoAction calls scheduler.CancelAll() and re-schedules Blaze / Pyrogenics /
// Flame Sear / Shadow Nova, so:
//   * Conflagration is GONE, which is what makes stacking safe (and it is only safe in this direction -
//     if ALYTHESS died first it is Sacrolash who inherits Conflagration, so never stack for that case);
//   * Shadow Blades died with Sacrolash, so SHADOW NOVA IS THE ONLY SHADOW LEFT IN THE WORLD - a 10y
//     hit on one random raider every 20-26s;
//   * fire keeps arriving from Blaze (her victim, every ~3.8s) and Flame Sear (5 random raiders, ~9s).
// So Flame Touched climbs on everybody with exactly one thing able to strip it, and the only way that one
// thing reaches the raid is if the raid is standing close enough together to share it. That is precisely
// what the published strategies mean by "stack up on Alythess to finish her" - it is a cleanse mechanic,
// not a convenience. The bot that suffers most without it is her TANK, which stands in Blaze
// continuously and has no shadow source at all.
//
// Note this makes the phase-1 Flame Touched cleanse (seek Shadow Blades) inert here BY DESIGN - there is
// nothing to seek. The stack replaces it.
constexpr float TWINS_STACK_RANGE = 8.0f;

// Both twins stand in one room and neither roams (Alythess never even melees - her AttackStart passes
// meleeAttack = false), so this can be far tighter than Felmyst's 250y.
constexpr float TWINS_PARTICIPANT_RANGE = 120.0f;
// Same ladder shape as Felmyst's refuge: shorten the move rather than freeze when the ground behind the
// bot is off-mesh. See FELMYST_REFUGE_STEPS for why a ladder must vary the STEP and not just the end
// point - an unreachable end point otherwise produces no movement at all.
constexpr uint32 TWINS_FLEE_CANDIDATES = 5;
constexpr float TWINS_TRAVEL_LEG = 30.0f;
// Purge / Dispel Magic / Spellsteal are all 30y.
constexpr float TWINS_DISPEL_RANGE = 30.0f;

// THIS FIGHT NEEDS THREE TANKS, and Confounding Blow is why - though NOT for the reason the published
// guides give. They say it drops threat on the primary target; on this core it does no such thing.
// 45256 is 7353 damage plus exactly two auras - SPELL_AURA_MOD_DECREASE_SPEED at -60% and
// SPELL_AURA_MOD_CONFUSE - for 6s, with no threat effect, no spell_linked_spell row and no script
// beyond the Dark Touched applier. Threat is fully retained.
//
// What it actually does is take the tank's CONTROL away, which is worse. For 6s every ~20-25s the
// holder random-walks at 2.8 yd/s (still 40% of run speed) - up to ~17y of path - and Sacrolash chases
// her victim the whole way. `CheckInRoom` then fires Fireblast 45232 on the WHOLE RAID for 13,125 with
// SPELL_ATTR0_NO_IMMUNITIES the moment she is 50y from home. So the second Sacrolash tank exists to
// take her off a tank who cannot steer, and the leash guard exists to undo drift that already happened.
constexpr float TWINS_LEASH_RANGE = 38.0f;  // 12y of margin under the core's 50y Fireblast trip
constexpr float TWINS_LEASH_TOLERANCE = 6.0f;

// -------------------------------------------------------------------------------------------------
// M'uru / Entropius

// M'uru's spawn point, from `creature` (map 580). He never moves - his AI has no movement at all - so
// this doubles as the Darkness centre and as a free room test that costs no grid search.
constexpr float MURU_HOME_X = 1816.25f;
constexpr float MURU_HOME_Y = 625.484f;
constexpr float MURU_HOME_Z = 69.6036f;

// InstanceScript DATA_* id from src/server/scripts/EasternKingdoms/SunwellPlateau/sunwell_plateau.h.
// Hardcoded with the header named here because a module cannot include a script-side header. If the
// upstream enum is ever reordered this constant goes stale silently, which is the cost of the pattern.
//
// This is the ENGAGE GATE for the whole fight, and it exists because position-plus-combat was not one.
// boss_muru is a BossAI(creature, DATA_MURU), so the state goes IN_PROGRESS from Player-side aggro (via
// BossAI::_JustEngagedWith) and only leaves it when Entropius dies - boss_entropius::JustDied calls
// muru->KillSelf(), which runs M'uru's own _JustDied and sets DONE. So IN_PROGRESS spans BOTH phases
// exactly, which no geometric test can do.
constexpr uint32 SWP_DATA_MURU = 5;

constexpr float MURU_PARTICIPANT_RANGE = 120.0f;
constexpr float MURU_SEARCH_RANGE = 200.0f;
constexpr float MURU_TRAVEL_LEG = 30.0f;
constexpr float MURU_DISPEL_RANGE = 30.0f;  // Purge / Dispel Magic / Spellsteal all reach 30y

// Darkness. 45998 ticks every 45s and casts 45999, a 3s PRE-EFFECT aura on M'uru himself, which then
// casts 45996: a PERSISTENT_AREA_AURA at M'uru for 20s, radius index 18 = 15y, dealing 3000 shadow per
// second AND applying aura 118 at -100%, so nobody inside can be healed at all. Four ticks kills a
// level-70 bot with no way to save it, which is why this outranks even the fiend dispel.
//
// The 3s pre-effect is the whole reason this can be handled proactively rather than reactively: a bot
// waiting to feel 45996 has already eaten a 3000 tick it can never have healed back.
constexpr float MURU_DARKNESS_RADIUS = 15.0f;
constexpr float MURU_DARKNESS_TOLERANCE = 2.0f;   // don't re-run a bot that is already clear
constexpr float MURU_DARKNESS_CLEAR_TO = 21.0f;   // 6y of margin outside the edge

// Dark Fiend detonation, 45944. VERIFIED LITERAL: Effect[0] = 2000 shadow per second for 10s plus
// Effect[1] = 5000 direct, both at TARGET_SRC_CASTER -> TARGET_UNIT_SRC_AREA_ENEMY with radius index
// 27 = FIFTY YARDS and MaxAffectedTargets 0. No spell script, no linked spell, no condition, no
// SpellInfoCorrections entry, and no reference to 45944 anywhere in src/server - all four checked.
//
// The proof it is intentional: M'uru's other unbounded AoE, Negative Energy 46008, shares the same
// "whole room" radius index but DOES carry spell_gen_select_target_count_15_5, which resizes its
// target list to 5. One of these two spells is explicitly capped by the core and the other is not.
//
// So a single fiend that reaches its victim deals 25,000 shadow to the entire raid. That is not a
// damage spike to heal through - it is a wipe, and no positioning helps, because the radius covers the
// room. The only answer is that no fiend may ever arrive: strip 45934 and it despawns on the spot.
constexpr float MURU_FIEND_SEARCH_RANGE = 60.0f;

// Entropius' void zone: creature 25879 carrying 46262, which triggers 46264 once a second for 3000
// shadow within radius index 15 = 3y. Small, so a couple of steps is enough - but it lands UNDER a
// random player, so the clear has to be relative to the zone rather than to any authored point.
constexpr float MURU_VOID_ZONE_RADIUS = 3.5f;  // 3y + a player's object size
constexpr float MURU_VOID_ZONE_CLEAR_TO = 9.0f;

// Singularity 25855. Its pull, 46230, has radius index 37 = 7y and fires every 0.3s off 46228, which
// the singularity only gains at the 8-second mark of its 18-second life. It is NOT_SELECTABLE
// (unit_flags 0x2000000), so there is nothing to kill and nothing to tank: pure footwork.
constexpr float MURU_SINGULARITY_RADIUS = 7.0f;
constexpr float MURU_SINGULARITY_FLEE = 18.0f;

// Shadow Pulse. A Void Sentinel casts 46086 on reset, which triggers 46087 every 3s for 3750 shadow
// within radius index 13 = 10y. 1250 damage per second standing next to it is why the wiki tells ranged
// to handle Sentinels - and why melee DPS are steered off them entirely here. The Sentinel's own tank
// is exempt: somebody has to eat Void Blast (46161, 9426 plus -35% haste).
constexpr float MURU_PULSE_RADIUS = 10.0f;
constexpr float MURU_PULSE_CLEAR_TO = 14.0f;

constexpr uint32 MURU_FLEE_CANDIDATES = 5;

// Wide on purpose. Focus fire only converges if every DPS bot can SEE the same add set - a bot whose
// search misses the add everyone else picked will settle on a different "best" and the raid splits. The
// two wave spawn points are ~55y and ~40y from M'uru on opposite bearings, so a bot on the far side can
// be ~85y from the far group. Well inside the ~533y grid-search clamp, and this is not on the multiplier
// hot path - only the two lowest-ranked actions read it.
constexpr float MURU_ADD_PICKUP_RANGE = 100.0f;

// ADD KILL ORDER — the wiki's, verbatim, per two runs of live feedback.
//
//   Ranged DPS: Void Sentinel > Void Spawn > Fury Mage > Berserker > M'uru
//   Melee DPS:  Fury Mage > Berserker > M'uru
//
// Rev 2 tried weighted-health scoring instead (Sentinel weight 1.0, i.e. LAST) on the theory that killing
// a Sentinel manufactures work, since its death casts 46071 eight times. Measured over a full attempt
// that spread the raid's damage across Sentinel (870 retargets), Void Spawn (768) and Fury Mage (681)
// simultaneously - so nothing died fast enough, a second Sentinel arrived on top of the first, and the
// tanks were overwhelmed. Boss damage collapsed to 89 retargets at the same time. Scoring converges on
// "everything is half dead" when the health of several classes is falling at once.
//
// A strict class order is what makes a whole LANE land on one target at a time, which is the only thing
// that kills a Sentinel inside its 30-second respawn window. The starvation risk that scoring was built
// to avoid is answered here by the TWO LANES rather than by the ordering: melee cover Fury Mage then
// Berserker while ranged cover Sentinel then Void Spawn, so every class has an owner.
//
// Within one tier the pick is BUCKETED health, GUID tie-broken.
//
// Raw health percent was rev 3's bug and it produced exactly the reported symptom - bots visibly flipping
// targets and dealing no damage. Two Fury Mages being damaged by different bots sit within a fraction of a
// percent of each other, so the minimum flips every tick. And a flip is not free: AttackAction::Attack
// calls bot->StopMoving() and clears the motion master whenever the last movement priority is below
// MOVEMENT_COMBAT. So a melee bot that retargets every tick is FROZEN IN PLACE, never closes to melee
// range, and never swings - while the trace shows it dutifully "retargeting" hundreds of times to the same
// entry at the same rounded health.
//
// Bucketing to MURU_FOCUS_HEALTH_BUCKET makes the key stable: a flip now needs an add to cross a quarter
// boundary, which is a real change rather than damage noise. It is also self-reinforcing - the focused add
// drops a bucket first and therefore keeps the focus - while still finishing a nearly-dead add ahead of a
// healthy one, which is what plain GUID ordering would lose.
constexpr float MURU_FOCUS_HEALTH_BUCKET = 25.0f;

// Escape destinations are snapped to fixed compass sectors around the hazard, and this is LOAD-BEARING.
//
// Rev 2 computed each escape point from the bot's OWN LIVE POSITION every tick, which means the
// destination moved as the bot moved and never stabilised - so MoveTo had nothing to dedup against and
// refused every order while the previous spline was still in flight. Measured: 130 of 268 Darkness
// escapes accepted NO rung at all ("try 5"), 41 of them while the bot was standing in the zone taking
// 3000 per second that cannot be healed. From outside it looks exactly like a bot running back and forth
// doing nothing, which is what it was.
//
// Snapping the bearing to one of 16 sectors makes the destination IDENTICAL across ticks while the bot
// walks radially outward, so IsDuplicateMove holds the original order and the spline actually completes.
// Same latch-it-once principle as the Brutallus anchor. Retry rungs step to neighbouring sectors instead
// of rotating off a moving bearing.
constexpr uint32 MURU_CLEAR_SECTORS = 16;  // 22.5 degrees each

// -------------------------------------------------------------------------------------------------
// Kil'jaeden
//
// SIX THINGS IN THIS FIGHT ARE NOT WHAT THE DESIGN SPEC ORIGINALLY SAID, and every one of them was
// read out of the runtime rather than a guide. They are written here because two of them invert the
// shape of the whole strategy.
//
// 1. DARKNESS OF A THOUSAND SOULS CANNOT BE DODGED, OUTRANGED, OR OUT-HEALED. 46605 is a 750ms cast
//    (not an 8s one) that puts an 8000ms DUMMY aura on KJ; spell_kiljaeden_darkness_aura fires 45657
//    when - and only when - that aura expires NATURALLY. 45657 is 47,499 shadow at radius index 28 =
//    50000 yards, i.e. the entire map, uncapped. A level-70 raid has ~9-10k per body. There is no
//    positional answer and no healing answer.
//
//    The ONLY answer in the encounter is Shield of the Blue, and it is a flat -96% damage taken
//    (45848, MOD_DAMAGE_PERCENT_TAKEN -96), which turns 47,499 into ~1,900. That is why the drake is
//    a hard requirement rather than a convenience: without one, every Darkness is a wipe.
//
// 2. THE DRAKE DOES NOT FLY. creature_template_movement for 25653 is Ground=1, Swim=1, Flight=0, so
//    piloting is ordinary ground movement - no z math, no air legs.
//
// 3. THE PILOT IS INVULNERABLE WHILE PILOTING, which is what makes the whole design cheap.
//    spell_kiljaeden_vengeance_of_the_blue_flight_aura applies 45838 to the player on possess:
//    SPELL_AURA_SCHOOL_IMMUNITY with MiscValue 127 (ALL schools) plus SPELL_AURA_MOD_UNATTACKABLE,
//    for the full 120s. And Unit::SetCharmedBy(CHARM_TYPE_POSSESS) sets UNIT_FLAG_DISABLE_MOVE on the
//    CHARMER - so the pilot's body is frozen wherever it clicked, and does not care, because nothing
//    in the room can touch it. A pilot parked 48y from the fight is therefore free, not exposed.
//
// 4. FIRE BLOOM IS NOT A DETONATION. 45641 is a 20s aura that ticks 45642 every 2000ms - 1618 fire at
//    radius index 13 = 10y to TargetB 30 = SRC_AREA_ALLY. Ten separate hits on everyone standing near
//    the carrier, not one hit at expiry, so the carrier has to be clear for the whole twenty seconds.
//    Shield of the Blue removes it outright on apply, which is why the quarantine stands down inside a
//    Darkness window: the fix is to be in the shield, not out of the raid.
//
// 5. SHADOW SPIKE IS AN AREA HIT. 46680 is a 28s aura on KJ ticking every 3000ms; each tick picks a
//    random target within 60y and lands 45885 - 5099 shadow at radius index 14 = 8y. So the baseline
//    ranged formation has to be a genuine spread, not a loose one.
//
// 6. SHIELD ORBS ARE A RANGED-ONLY PROBLEM. 25502 orbits at z=40 on an 18y circle around KJ, i.e. ~12y
//    above the floor, so melee simply cannot reach one. Each carries 45679, which triggers 45680 every
//    500ms: 999 shadow at radius index 11 = 45y, MaxAffectedTargets 0, conditioned on "target is a
//    Player" AND "target does not have 45839" (the drake pilot exemption). Uncapped raid-wide damage
//    from something only half the raid can shoot at.
constexpr float KJ_ANCHOR_X = 1698.45f;
constexpr float KJ_ANCHOR_Y = 628.03f;
constexpr float KJ_ANCHOR_Z = 28.20f;

// 80, not the 120 used for M'uru, and the number is forced. The Eredar Twins' room sits 115y away at
// (1814, 626) and only FIVE yards below this floor (z 33.4 against 28.2), so the z band that separates
// every other Sunwell room from its neighbours does not separate these two. 2D distance is the only
// discriminator, and the platform is ~50y in radius (the orbs sit at 48y), so 80 covers the platform and
// its approach while stopping 35y short of the Twins.
constexpr float KJ_PARTICIPANT_RANGE = 80.0f;
constexpr float KJ_Z_MIN = 15.0f;
constexpr float KJ_Z_MAX = 45.0f;

// The main tank holds this point, and every other position in the fight is a fixed polar offset from
// it. Same reasoning as the Brutallus anchor, but far simpler: KJ SPAWNS here (the controller's own
// spawn point, boss at z+1.5) and he chases his victim, so a main tank who never leaves this spot is a
// boss who never leaves it either. No latching is needed because the anchor is authored, not observed.
constexpr float KJ_ANCHOR_TOLERANCE = 6.0f;

// Armageddon: 45915 is 9999 fire at radius index 40 = 9y, which is instant death at level 70. 13 to
// start moving, 17 to stop, and the gap between them is what stops the pair oscillating on the lip.
constexpr float KJ_ARMAGEDDON_RADIUS = 9.0f;
constexpr float KJ_ARMAGEDDON_CLEAR_FROM = 13.0f;
constexpr float KJ_ARMAGEDDON_CLEAR_TO = 17.0f;

// Fire Bloom quarantine. The tick radius is 10y, the ranged ring below sits at 26y, so a carrier pushed
// to 38y is 12y clear of the outermost body in the raid and clear of the melee stack by construction.
// Same shape as the Brutallus Burn quarantine, and the same trade: a quarantined bot is at the edge of
// healer range, which is worth it against 1618 every two seconds landing on the whole clump instead.
constexpr float KJ_FIRE_BLOOM_RADIUS = 10.0f;
constexpr float KJ_FIRE_BLOOM_RANGE = 38.0f;

// Shield of the Blue is a 12y persistent area aura centred on the DRAKE (radius index 32). Everything
// below is one error budget against that 12, and it MUST be added up rather than eyeballed - the first
// version of these numbers did not close, and the failure mode is silent and lethal:
//
//     worst bot-to-drake distance = outer ring + arrival tolerance + drake park tolerance
//
// The first cut was 8 + 3 + 8 = 19y against a 12y radius, so a bot on the far side of the ring from a
// drifted drake would have been OUTSIDE the shield and taken the full 47,499. Nothing would have reported
// it either: the stack action would log "seated", the drake would log "SHIELD", and the bot would simply
// be dead. Hence the coverage count logged at cast time (see KjDriveDrakeAction).
//
// As budgeted now: 6 + 2 + 2 = 10y, i.e. 2y of margin under the radius.
//
// There is a second, independent safety net worth knowing about: Effect 0 of 45848 is a PERSISTENT area
// aura, so its DynamicObject re-runs its target search every update rather than resolving once at cast.
// A bot that arrives LATE - slowed to 3.4 yd/s by Flame Dart or Shadow Spike, say - still picks the shield
// up on the way in, and it only has to be inside by the 8000ms mark when 45657 fires, against a shield
// cast at 4000ms that lasts until 9000ms. So the budget above is the guarantee and this is the slack.
//
// The rings are DELIBERATELY tight enough to be a pile. 12 slots at 3y is 1.6y of arc and at 6y is 3.1y -
// bots do not collide, and piling is the point here. The slots exist only so 22 bots do not all request
// the IDENTICAL point, which is what makes MoveTo's duplicate-move guard thrash.
constexpr float KJ_SHIELD_RADIUS = 12.0f;
constexpr float KJ_STACK_RING_INNER = 3.0f;
constexpr float KJ_STACK_RING_OUTER = 6.0f;
constexpr uint32 KJ_STACK_SLOTS_PER_RING = 12;
// 2y, and a tight tolerance is affordable here in a way it was not on Brutallus: this rule is live for an
// 8-second window rather than a whole fight, and `kj stack for darkness` returns FALSE, so re-issuing per
// tick cannot starve heals or attacks out of the queue the way the Brutallus station action did.
constexpr float KJ_STACK_TOLERANCE = 2.0f;

// When the drake casts. The aura is 8000ms and the shield lasts 5000ms with a 20000ms cooldown, so
// firing at 4000ms remaining covers 4000..9000ms against an expiry at 8000ms - a full second of margin
// on both sides, and only one cast per Darkness even though Darkness repeats every 45s.
constexpr uint32 KJ_DARKNESS_DURATION_MS = 8000;
constexpr uint32 KJ_SHIELD_LEAD_MS = 4000;
// Take the second coverage reading inside this much of the expiry - the moment 45657 actually fires, which
// is the only moment coverage decides anything. One or two samples per Darkness at a ~0.5s tick.
constexpr uint32 KJ_COVERAGE_SAMPLE_MS = 750;
constexpr uint32 KJ_SHIELD_COOLDOWN_MS = 20000;
constexpr uint32 KJ_BREATH_COOLDOWN_MS = 10000;

// The baseline ranged/healer formation: one fixed polar station each on a ring around the anchor.
//
// A REACTIVE "step away from whoever is too close" RULE WAS REJECTED, and Brutallus is why. A
// destination computed from a neighbour's live position moves as the neighbour moves, so MoveTo has
// nothing to dedup against and the pair shuttle - the same failure that made 130 of 268 M'uru Darkness
// escapes accept no move order at all. Fixed slots on a fixed ring are stable by construction and give
// a guaranteed, measurable spacing instead of an emergent one.
//
// 26y, and the ceiling is not arbitrary: every station must stay inside playerbots' SpellDistance
// (28.5) or "enemy out of spell" fires "reach spell" at ACTION_HIGH every tick and drags the bot off
// station (the Brutallus ring-radius lesson). 16 slots at 26y is 10.2y of arc between neighbours, which
// clears Shadow Spike's 8y and Fire Bloom's 10y.
constexpr float KJ_RANGED_RING = 26.0f;
constexpr uint32 KJ_RANGED_SLOTS = 16;
// 6y, not 2.5. Bots settle a couple of yards off a requested point, and a tolerance tighter than that
// makes the trigger permanently active: the action re-issues a short move every tick, the bot jitters in
// place, and because a positioning action that returns true stops the engine for that tick it ALSO
// starves every heal and attack out of the queue. That cost a whole Brutallus raid once.
constexpr float KJ_STATION_TOLERANCE = 6.0f;

// Orb handling. 4y to click - the goober's own spell reaches 20y, so this is comfortable.
constexpr float KJ_ORB_USE_RANGE = 4.0f;
constexpr float KJ_ORB_SEARCH_RANGE = 100.0f;
// 2y, and this is a TERM IN THE SHIELD BUDGET above, not a convenience. Every yard the drake is allowed to
// sit off the anchor comes straight off the margin that keeps the outermost stacked bot inside the 12y
// shield, so it cannot be loosened without shrinking KJ_STACK_RING_OUTER to match.
constexpr float KJ_DRAKE_PARK_TOLERANCE = 2.0f;

// The two breaths are TARGET_UNIT_CONE_ALLY at 13y, so unlike the shield they have to be AIMED - a cone
// cast from the middle of a pile is mostly wasted, and cast at nothing when the drake happens to be facing
// open floor.
//
// AIMING TAKES TWO CALLS, NOT ONE. Unit::SetFacingTo launches a facing SPLINE, and the unit's own
// m_orientation is not updated until that spline resolves - so a cast issued the same tick would run
// HasInArc against the OLD facing. SetOrientation is what makes the server-side arc test correct
// immediately; SetFacingTo is what makes the client see the turn. Both, in that order (SetFacingTo first,
// so its spline starts from the real old bearing), and then the cast lands the same tick.
constexpr float KJ_BREATH_CONE_RANGE = 13.0f;
// How far off the desired bearing is worth re-facing for. Inside this the drake is left alone, because
// SetFacingTo re-splines in place and would cancel a move that was still finishing.
constexpr float KJ_BREATH_FACING_EPSILON = 0.30f;  // ~17 degrees
// Aim candidates must be at least this far out or the bearing to them is meaningless - a body standing on
// top of the drake gives a random angle.
constexpr float KJ_BREATH_AIM_MIN = 2.0f;
constexpr float KJ_BREATH_AIM_MAX = 25.0f;

// Refresh the possession this long before it lapses. The drake is a 120s summon and Darkness repeats
// every 45s, so one drake covers two Darknesses and the raid must never be between drakes when one
// lands. Refreshing needs the pilot's body to WALK back to an orb - and its body is frozen by
// UNIT_FLAG_DISABLE_MOVE while possessing - so possession has to be dropped first, which is why this
// window is generous: 30s covers the ~7s walk plus a whole Darkness window interrupting it.
constexpr uint32 KJ_POSSESS_REFRESH_MS = 30000;

// Legs for the walk to an orb. The orb is a fixed world point so the destination is stable on its own;
// the ladder exists only for the usual off-mesh reason (see FELMYST_REFUGE_STEPS).
constexpr uint32 KJ_ORB_WALK_CANDIDATES = 3;

enum class SunwellSpells : uint32
{
    // Kalecgos
    SPELL_SPECTRAL_REALM                = 46021,  // on a player while in the spectral realm
    SPELL_SPECTRAL_EXHAUSTION           = 44867,  // cannot (re-)enter the rift
    SPELL_CURSE_OF_BOUNDLESS_AGONY      = 45032,
    SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR  = 45034,
    SPELL_BANISH                        = 44836,  // that boss half is done (<=1%)
    SPELL_CRAZED_RAGE                   = 44807,

    // Brutallus
    SPELL_METEOR_SLASH                  = 45150,  // stacking +75% fire taken on everyone hit
    SPELL_BURN_DOT                      = 46394,  // 60s escalating fire DoT; jumps at 2y

    // Felmyst
    SPELL_FOG_OF_CORRUPTION_CHARM       = 45717,  // infinite-duration MC; core kills on removal
    SPELL_ENCAPSULATE_CHANNEL           = 45661,  // 7s channel on a random raider
    SPELL_ENCAPSULATE_PULSE             = 45665,  // the aura that actually does the damage
    SPELL_GAS_NOVA                      = 45855,  // 100y, unavoidable, DispelType 1 = MAGIC
    SPELL_MASS_DISPEL                   = 32375,  // priest, level 70, ground-targeted

    // Eredar Twins
    SPELL_DARK_TOUCHED                  = 45347,  // -5% healing received per stack, max 20, 180s
    SPELL_FLAME_TOUCHED                 = 45348,  // 300 fire per 2s per stack, max 20, 180s
    SPELL_DARK_FLAME                    = 45345,  // 3s INERT marker; blocks both touches while up
    SPELL_CONFLAGRATION                 = 45342,  // 8y area aura; each victim re-pulses 46768
    SPELL_SHADOW_NOVA                   = 45329,  // 10y, ~3238 shadow - the Flame Touched cure
    SPELL_SHADOW_BLADES                 = 45248,  // 20y around Sacrolash every ~10s
    SPELL_PYROGENICS                    = 45230,  // SELF-buff on Alythess: +35% fire done, Magic
    SPELL_BLAZE_TRAP                    = 45246,  // what the Blaze ground object casts
    SPELL_CONFOUNDING_BLOW              = 45256,  // 6s confuse + -60% speed on her victim; NO threat drop

    // M'uru / Entropius
    SPELL_DARK_FIEND_APPEARANCE         = 45934,  // duration -1, DispelType 1 = MAGIC; strip it = despawn
    SPELL_DARK_FIEND_TRIGGER            = 45944,  // 5000 + 2000/s for 10s to EVERYTHING within 50y
    SPELL_MURU_DARKNESS                 = 45996,  // 15y, 3000/s, 20s, -100% healing received
    SPELL_MURU_DARKNESS_PRE             = 45999,  // 3s pre-effect on M'uru: the only advance warning
    SPELL_SPELL_FURY                    = 46102,  // Fury Mage self-buff, 30s - the Spellsteal target
    SPELL_BLACK_HOLE_PASSIVE            = 46228,  // the singularity ARMS by gaining this at 8s
    SPELL_BLACK_HOLE_EFFECT             = 46230,  // the singularity's 7y pull

    // Kil'jaeden
    SPELL_KJ_DARKNESS                   = 46605,  // 8s DUMMY aura on KJ; the only warning there is
    SPELL_KJ_DARKNESS_DAMAGE            = 45657,  // 47,499 at radius 50000y on that aura's expiry
    SPELL_KJ_FIRE_BLOOM                 = 45641,  // 20s; ticks 1618 into 10y of allies every 2s
    SPELL_KJ_SHADOW_SPIKE               = 46680,  // 28s aura on KJ; 8y area hit every 3s
    SPELL_KJ_ARMAGEDDON_PERIODIC        = 45921,  // spawns the 25735 markers, max 3 live
    SPELL_KJ_SUMMON_BLUE_DRAKE          = 45836,  // SUMMON 25653, 120s
    SPELL_KJ_VENGEANCE_BLUE             = 45839,  // the possess aura ON THE PILOT; 120s
    SPELL_KJ_POSSESS_DRAKE_IMMUNITY     = 45838,  // all-school immunity + unattackable on the pilot
    SPELL_KJ_SHIELD_OF_THE_BLUE         = 45848,  // -96% damage taken, 12y around the drake, 5s
    SPELL_KJ_BREATH_HASTE               = 45856,  // 13y cone, +24% haste 30s
    SPELL_KJ_BREATH_REVITALIZE          = 45860,  // 13y cone, 449 health + 449 mana per 2s for 10s
};

enum class SunwellNpcs : uint32
{
    NPC_KALECGOS_DRAGON = 24850,
    NPC_KALEC_FRIENDLY  = 24891,  // heal him or the encounter scripted-wipes
    NPC_SATHROVARR      = 24892,
    NPC_BRUTALLUS       = 24882,
    NPC_FELMYST         = 25038,
    NPC_DEMONIC_VAPOR   = 25265,  // follows the player who summoned it; kite it away
    NPC_SACROLASH       = 25165,  // shadow twin; melees, must die FIRST
    NPC_ALYTHESS        = 25166,  // fire twin; stationary caster, never melees
    // Shadow Image 25214 is deliberately absent: its template carries UNIT_FLAG_NOT_SELECTABLE
    // (33554432), so it cannot be targeted at all and there is nothing for a bot to do about it.
    // The design spec's claim that they are "killable ordinary adds if targeted" is wrong.

    NPC_MURU            = 25741,  // permanently REACT_PASSIVE; clamped to 1 HP, never actually dies
    NPC_ENTROPIUS       = 25840,  // MELEES, so unlike M'uru he needs a real tank
    NPC_DARK_FIEND      = 25744,  // IMMORTAL (1% clamp); removed only by a MAGIC dispel
    NPC_VOID_SENTINEL   = 25772,  // 3750 shadow per 3s within 10y of itself; 8 Void Spawn on death
    NPC_VOID_SPAWN      = 25824,
    NPC_SW_BERSERKER    = 25798,
    NPC_SW_FURY_MAGE    = 25799,  // Fel Fireball 4251; Spell Fury is stealable
    NPC_SINGULARITY     = 25855,  // NOT_SELECTABLE: footwork only
    NPC_ENTROPIUS_ZONE  = 25879,  // Entropius' 3y void zone, spawned under a random player

    NPC_KILJAEDEN       = 25315,
    NPC_KJ_HAND         = 25588,  // phase 1; CALL_FOR_HELP 50y, so all three pull together
    NPC_KJ_SHIELD_ORB   = 25502,  // orbits at z=40 - RANGED ONLY, melee cannot reach it
    NPC_KJ_REFLECTION   = 25708,  // four copies of ONE player per cast (MAX_TARGETS is 1)
    NPC_KJ_ARMAGEDDON   = 25735,  // the marker; the meteor lands 6s later, 9999 within 9y
    NPC_KJ_BLUE_DRAKE   = 25653,  // "Power of the Blue Flight" - the possessed drake. Ground, not air.
    NPC_KJ_FELFIRE_FIEND = 25598, // explodes for 2024 in 10y whether it is killed or reaches you
};

enum class SunwellObjects : uint32
{
    GO_SPECTRAL_RIFT = 187055,
    GO_BLAZE         = 187366,  // Alythess' ground fire; trap radius 2.5y, casts 45246

    // The four orbs, in the order instance_sunwell_plateau maps them to DATA_ORB_*, which is the order
    // boss_kiljaeden::EmpowerOrb un-flags them: orb 1 at 85%, orb 2 at 55%, orbs 3 and 4 at 25%.
    // Goobers with consumable = 0 and no cooldown, so one orb can be clicked as often as needed.
    GO_KJ_ORB_1      = 187869,
    GO_KJ_ORB_2      = 188114,
    GO_KJ_ORB_3      = 188115,
    GO_KJ_ORB_4      = 188116,
};

bool IsInSunwell(Player* bot);
bool IsInSpectralRealm(Player* bot);

// Cross-realm lookups (see .cpp comment): plain grid searches by entry.
Creature* FindKalecgosDragon(Player* bot);
Creature* FindSathrovarr(Player* bot);
Creature* FindKalecFriendly(Player* bot);

Creature* FindBrutallus(Player* bot);

// Class taunt by spell NAME (warrior/paladin/druid/DK) - shared by the Kalecgos
// Sathrovarr pickup and the Brutallus window-flip swap.
bool CastClassTaunt(Player* bot, PlayerbotAI* botAI, Unit* target);

// The two tanks, taken from the RAID's OWN assignments so the player decides who tanks what:
// Main = the Main Tank slot flag (MEMBER_FLAG_MAINTANK, which PlayerbotAI::GetMainTankGuid
// already honours), Off = the Main Assist slot flag (MEMBER_FLAG_MAINASSIST, which playerbots
// reads nowhere else - note MEMBER_FLAG_ASSISTANT is a DIFFERENT flag, "promoted to assistant").
// Each falls back to a deterministic lowest-GUID pick among alive tank bots when unassigned, so
// an unflagged raid still works. A third or later tank is None and fights as a normal melee.
enum class BrutallusTankRole
{
    None,
    Main,
    Off,
};

BrutallusTankRole GetBrutallusTankRole(Player* bot, PlayerbotAI* botAI);

// Where this bot stands: a fixed polar offset from the LATCHED anchor pair (see the comment on
// BRUTALLUS_LANE_SEPARATION), on its lane bearing - one seat per bot on the rings, melee seated
// innermost because only the inner rings are inside melee range, and rotated off-cone while
// carrying Burn or BRUTALLUS_BAIL_STACKS.
//
// Returns false - meaning "no station, use generic behaviour" - for the MAIN TANK always (he is
// frozen wherever he took the boss, and he is what the anchor is measured from), for everyone until
// the anchor is latched, and for any bot past the last seat in the cone. Stationing a bot that
// generic behaviour also wants to move is only safe because BrutallusTankHoldMultiplier vetoes the
// actions that would move it; without that veto the two fight every tick and the bot visibly
// shuttles back and forth.
//
// NEITHER assigned tank ever quarantines, and that ordering is load-bearing: testing Burn first
// sent a burned OFF TANK 100+ degrees away, abandoning lane 1 and the threat the handoff needs.
bool GetBrutallusStation(Player* bot, PlayerbotAI* botAI, Creature* boss, Position& station);

// Stack count of the Meteor Slash fire-vulnerability debuff (0 when absent).
uint32 GetBrutallusSlashStacks(Unit* unit);

Creature* FindFelmyst(Player* bot);

// The current Encapsulate victim, or null. Checked three ways because the aura's landing unit is not
// obvious from the data (45665 declares TARGET_UNIT_CASTER but is cast via a spell link): either aura
// on a group member, else the boss's channel target. Whichever fires first wins - the channel is the
// earliest signal, which matters because the damage starts one second in.
Player* FindFelmystEncapsulateVictim(Player* bot, Creature* boss);

// Does this bot need to clear an Encapsulate blast right now? Shared by the trigger and the movement
// multiplier so the two can never disagree - if they did, the multiplier would hand generic movement
// back mid-flee and the bot would shuttle in and out of the blast.
bool FelmystShouldFleeEncapsulate(Player* bot, Creature* boss);

// Straight away from the victim to FELMYST_ENCAPSULATE_FLEE. `attempt` walks the same off-mesh ladder
// as the fog refuge (see GetFelmystFogStation) - shortening the flee rather than freezing when the
// point behind the bot is not walkable.
bool GetFelmystEncapsulateFleeSpot(Player* bot, Player* victim, Position& spot, uint32 attempt = 0);

// How many raid members within Mass Dispel's 15y (this bot included) are carrying Gas Nova.
uint32 CountFelmystGasNovaAfflicted(Player* bot);

// Should this bot Mass Dispel right now? Priest, knows 32375, off cooldown, enough afflicted allies in
// radius - and NOT while it owes a repositioning move, because the cast has a cast time and playerbots
// cancels a cast-time spell the moment the bot is moving. Without that check the dispel would fight the
// fog hop and the Encapsulate flee for the same tick, and losing either of those is fatal where losing
// a dispel is merely expensive.
bool FelmystShouldMassDispel(Player* bot, PlayerbotAI* botAI, Creature* boss);

// Is the encounter in its flight phase? MOVEMENTFLAG_DISABLE_GRAVITY is the same signal the core's
// own boss_felmyst::UpdateAI gates melee on, and it is the only readable one: SetInvincibility()
// merely assigns a PRIVATE `_invincible` field on BossAI, so a module cannot see it - don't try.
//
// CALLERS MUST ALSO REQUIRE boss->IsInCombat(). She flies an idle waypoint loop with this flag set
// long before the pull, so testing the flag alone would have the raid dodging fog while players were
// still walking in.
bool IsFelmystAirborne(Creature* boss);

// Which lane is about to be (or is being) fogged: 0=top, 1=middle, 2=bottom, or -1 for "unknown,
// hold position". Read from her position against the six authored lane points, with hysteresis - see
// FELMYST_LANE_DETECT_MARGIN for why a bare nearest-point test oscillates.
int32 GetFelmystActiveLane(Creature* boss);

// Is this ground inside the given lane's fog (plus FELMYST_FOG_MARGIN)? The middle and bottom lanes
// are line segments, so this is a point-to-segment test and the end caps fall out of it for free -
// a bot already north of y 700 is safe from them at any x. The top lane is treated as everything
// east of its line because its 17 extra triggers scatter east across the whole y range.
bool IsInFelmystFogLane(float x, float y, int32 lane);

// Where this bot goes to clear the live lane: a refuge x at the bot's OWN current y, so the hop is a
// pure sideways step (shortest possible, fits the warning window, and leaves the raid spread along y
// rather than stacked). `attempt` walks the candidate ladder (0 = furthest out); the action retries
// until MoveTo accepts one. Returns false when no lane is live, when the bot is already clear, or when
// the ladder is exhausted - in the first two cases it must simply stay put, which is also what keeps
// it out of the ~10s of fog lingering behind a strafe that has already passed.
//
// FLEES TO THE SIDE THE BOT IS ALREADY ON, and that is the whole point. Sending a bot to the far side
// routes it THROUGH the fog line it is running from: rev 3 sent bots at x 1466.5 east to 1501, they
// crossed the live middle line, the destination turned out to be off-mesh so every move order was
// refused, and they froze on the line and were all charmed. Only when the bot's own side has no
// refuge inside the room (the bottom lane's west side is outside it) does this cross, and then it
// crosses within the first second of a hop that has ~7s of warning.
bool GetFelmystFogStation(Player* bot, PlayerbotAI* botAI, Creature* boss, Position& station,
                          uint32 attempt = 0);

// The Demonic Vapor chasing THIS bot, or null. Exact, not a guess: 45391 force-casts the summon on
// one random enemy, so the vapor is a TempSummon whose summoner GUID is the player it MoveFollows.
Creature* FindFelmystVaporChasingMe(Player* bot);

// An alive raid member who has been mind-controlled by Fog of Corruption, or null.
//
// THE RAID MUST KILL THEM. The charm's duration is -1 (infinite): it never expires, the victim fights
// the raid with a damage buff, and `Unit::Kill` fires only when the aura is finally removed. It is
// undispellable (DispelType 0) and immune to purge (ATTR0 carries SPELL_ATTR0_NO_IMMUNITIES), so there
// is no way to save them - which is exactly the retail mechanic. Left alone, four or five charmed bots
// wiped the raid (observed).
Player* FindFelmystCharmedRaider(Player* bot);

// Centre of mass of the alive, uncharmed raid, excluding this bot. False if there is nobody to group
// with. Charmed members are excluded deliberately - they are hostile and usually running off, so
// counting them would drag the raid after them.
bool GetFelmystRaidCentroid(Player* bot, Position& centroid);

// Is this bot stranded away from the raid with no better reason to be there? Shared by the trigger and
// the movement multiplier, same as the Encapsulate predicate, so the two cannot disagree mid-walk.
bool FelmystShouldRegroup(Player* bot, PlayerbotAI* botAI, Creature* boss);

Creature* FindSacrolash(Player* bot);
Creature* FindAlythess(Player* bot);

// THREE tank slots, resolved so the raid degrades gracefully as tanks are available:
//   Sacrolash        = the raid's Main Tank flag (MEMBER_FLAG_MAINTANK)
//   Alythess         = the raid's Main Assist flag, else the lowest-GUID spare tank
//   SacrolashRelief  = the next lowest-GUID spare tank after that
// Alythess deliberately gets the SECOND tank rather than the third, even though it is Sacrolash that
// needs two: with only two tanks in the raid that leaves both bosses held, whereas seating the pair on
// Sacrolash first would leave Alythess untanked and drop Blaze (5525 fire every ~3.8s) on a random
// clothie. The relief slot is the one that goes unfilled when the raid is short, which is the failure
// that merely costs control rather than a life. A fourth or later tank is None and fights as melee.
enum class TwinsTankRole
{
    None,
    Sacrolash,
    Alythess,
    SacrolashRelief,
};

TwinsTankRole GetTwinsTankRole(Player* bot, PlayerbotAI* botAI);

// Either of the two tanks assigned to Sacrolash. The swap is deliberately SYMMETRIC - see
// TwinsShouldReliefTaunt.
bool IsTwinsSacrolashTank(TwinsTankRole role);

// Should this bot take Sacrolash off its co-tank right now? True when this bot is one of her two tanks,
// she is currently attacking the OTHER one, and that one is confused by Confounding Blow.
//
// SYMMETRIC ON PURPOSE, AND THERE IS NO HAND-BACK. Whoever taunts becomes the holder and keeps her; the
// next Confounding Blow lands on THEM and the other tank takes her back. That alternation is how a real
// raid runs it, and it is the only version that cannot oscillate - a "primary reclaims the boss when the
// confuse ends" rule would fight this one every ~22s for the rest of the fight.
bool TwinsShouldReliefTaunt(Player* bot, PlayerbotAI* botAI);

// Has Sacrolash been dragged dangerously far from her spawn - typically by a confused tank wandering
// with her in tow? True only for the tank actually holding her, since nobody else can move her.
bool TwinsShouldFixLeash(Player* bot, PlayerbotAI* botAI);

// A point back toward Sacrolash's home position. She chases her victim, so walking her holder home walks
// HER home.
bool GetTwinsLeashSpot(Player* bot, Position& spot);

// Either twin alive and in combat, with this bot close enough to be in the fight.
bool IsTwinsEncounterActive(Player* bot);

// The player a live Conflagration cast is aimed at, or null. Read from BOTH twins' spell slots by
// spell id, which is what makes every Conflagration rule survive the empower transition for free:
// whichever sister is casting it, the id is the same - and if Alythess dies first it becomes
// SACROLASH who casts it (boss_sacrolash::DoAction), so keying off "the fire twin" would silently
// stop working in exactly the case that needs it most.
Player* FindTwinsConflagrationTarget(Player* bot);

// Nearest raid member OTHER than this bot that is currently a Conflagration bomb - either the target
// of a live cast or already carrying the aura. Both halves are needed: the cast window is when
// neighbours can still get clear before the aura is applied to them too, and the 10s aura is how long
// they must stay clear of the 1600/s pulses afterwards.
Player* FindTwinsConflagrationBomb(Player* bot);

// Am I the one about to be conflagrated, and can I still do something about it? Shared by the trigger
// and the movement multiplier so the two cannot disagree mid-flee.
bool TwinsShouldFleeConflagration(Player* bot);

// Is a bomb close enough that I need to get out of its 8y radius?
bool TwinsShouldClearConflagration(Player* bot);

// Radially outward from the raid's centre of mass. The victim is one of Sacrolash's top-6 threat, i.e.
// almost always inside the melee stack, so "away from the raid" is the only bearing that helps - and a
// radial run separates two simultaneous victims for free.
bool GetTwinsConflagrationFleeSpot(Player* bot, Position& spot, uint32 attempt = 0);

// Straight away from the bomb, each neighbour on its own bearing so the stack fans out rather than
// collapsing onto one escape point (and so no two of them run through each other).
bool GetTwinsConflagrationClearSpot(Player* bot, Player* bomb, Position& spot, uint32 attempt = 0);

// Stack count of a touch aura (0 when absent).
uint32 GetTwinsTouchStacks(Unit* unit, uint32 spellId);

// THE SINGLE POSITION AUTHORITY FOR BLAZE, and it must stay that way. Two separate actions - one that
// walks into a patch to cleanse Dark Touched and one that steps out of a patch to stop burning - would
// be the exact oscillator shape that toured Burn through the raid on Brutallus: one pushes in, the
// other pushes out, forever. Instead this function decides which of the two a bot wants, keyed on its
// STACK STATE rather than on proximity, and one action serves both.
//
// `seeking` is set true when the returned station is a patch to stand IN, false when it is a step OUT.
// Returns false when the bot wants neither.
bool GetTwinsBlazeStation(Player* bot, Position& station, bool& seeking);

// Does this bot owe any blaze footwork at all?
bool TwinsShouldWorkBlaze(Player* bot);

// Buried in Flame Touched, and far enough from Sacrolash to be missing the Shadow Blades that would
// clear it.
bool TwinsShouldSeekShadow(Player* bot);
bool GetTwinsShadowSeekSpot(Player* bot, Position& spot);

// Phase 2: Sacrolash is dead and Alythess is not. Stacking is how Shadow Nova - by then the only shadow
// in the fight - reaches the whole raid and strips Flame Touched. See TWINS_STACK_RANGE.
bool TwinsIsAlythessSoloPhase(Player* bot);
bool TwinsShouldStackOnAlythess(Player* bot);

// Which of this bot's abilities can strip a MAGIC buff off an ENEMY. Pyrogenics (45230) is a self-buff
// on Alythess granting +35% fire damage done for 15s, and DispelType 1 = Magic, so an offensive dispel
// removes it - worth roughly a quarter of her damage output.
char const* TwinsOffensiveDispelSpell(Player* bot);
bool TwinsShouldDispelPyrogenics(Player* bot, PlayerbotAI* botAI);

// Centre of mass of the alive raid, excluding this bot. False when there is nobody to measure against.
bool GetTwinsRaidCentroid(Player* bot, Position& centroid);

// Does this bot owe one of THIS fight's movements right now? The movement veto is deliberately keyed on
// this rather than on "the encounter is live": generic movement is correct for most of this fight (melee
// should chase, ranged should hold range), so suppressing it wholesale would strand every bot the
// strategy moved - the Felmyst vapor-carrier bug, which needed a whole extra action to undo. Narrowing
// the veto to the moments a move is owed means the generic way home is never taken away in the first
// place.
bool TwinsShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI);

// ---------------------------------------------------------------------------------------------
// M'uru / Entropius

Creature* FindMuru(Player* bot);
Creature* FindEntropius(Player* bot);

// True while the INSTANCE says this encounter is in progress and the bot is in the room. Both halves of
// the fight matter: M'uru is clamped to 1 HP and stops acting, then Entropius carries the rest, and the
// encounter only ends via boss_entropius::JustDied -> muru->KillSelf() - so the boss state is the only
// signal that spans both without a gap.
//
// It gates on SWP_DATA_MURU rather than on bot combat + geometry because the approach corridor's Sunwell
// Honor Guards stand 42y from M'uru at his own z, INSIDE the participant volume: fighting them satisfied
// a combat-plus-position gate, and the raid then charged the boss and shut the door on the player. See
// the body for the full account.
bool IsMuruEncounterActive(Player* bot);

// Has M'uru handed off? He gains UNIT_FLAG_NOT_SELECTABLE the instant he is clamped, 7s before
// Entropius even exists, so this is the authority for "stop trying to damage the boss you can see".
bool MuruIsHandedOff(Player* bot);
bool MuruIsEntropiusPhase(Player* bot);

// Where the 15y Darkness is, and whether it is live or about to be. M'uru never moves, and 45996 is
// cast at TARGET_DEST_CASTER, so the centre is always M'uru himself; the only real question is timing.
// Reads the live DynamicObject for 45996 and, failing that, the 45999 pre-effect aura - so a bot gets
// out during the 3s warning instead of after the first unhealable 3000 tick.
bool GetMuruDarknessCenter(Player* bot, Position& center);

// True both to LEAVE the zone and to HOLD outside it. The hold case exists because M'uru sits at the
// centre of his own Darkness, so a melee bot whose target is the boss gets dragged straight back in by
// generic movement the moment the leave case goes false - an oscillator that produces no damage and no
// escape. While the zone is live, melee with nothing but the boss to hit stay parked outside instead.
bool MuruShouldClearDarkness(Player* bot, PlayerbotAI* botAI);

// False once the bot is already clear, which is what makes the hold case hold: no destination means no
// MoveTo, while the predicate above keeps the generic movement veto in place.
bool GetMuruDarknessClearSpot(Player* bot, Position& spot, uint32 attempt = 0);

// Which of this bot's abilities can strip a MAGIC buff off an ENEMY. Promoted out of the Twins work
// (Pyrogenics) because the same three spells are the entire answer to Dark Fiends.
char const* SwpOffensiveDispelSpell(Player* bot);

// The fiend THIS bot should dispel. Deterministic: fiends are ordered by GUID and the bot indexes in by
// its own rank among the raid's dispel-capable bots, so six dispellers strip six different fiends
// instead of racing for the same one and wasting five casts on a target that is already gone.
Creature* FindMuruDarkFiend(Player* bot, PlayerbotAI* botAI);
bool MuruShouldDispelFiend(Player* bot, PlayerbotAI* botAI);

// Melee DPS only - the Sentinel's tank has to stand in the pulse. Nothing here helps a tank.
Creature* FindMuruVoidSentinel(Player* bot);
bool MuruShouldAvoidShadowPulse(Player* bot, PlayerbotAI* botAI);
bool GetMuruShadowPulseSpot(Player* bot, Position& spot, uint32 attempt = 0);

Creature* FindMuruVoidZone(Player* bot);
bool MuruShouldClearVoidZone(Player* bot, PlayerbotAI* botAI);
bool GetMuruVoidZoneClearSpot(Player* bot, Position& spot, uint32 attempt = 0);

Creature* FindMuruSingularity(Player* bot);
bool MuruShouldFleeSingularity(Player* bot, PlayerbotAI* botAI);
bool GetMuruSingularityFleeSpot(Player* bot, Position& spot, uint32 attempt = 0);

// Focus weight for an add entry. See the MURU_ADD_WEIGHT_* block for why this is scored rather than
// ordered. Returns 1.0f for anything unrecognised.
float GetMuruAddWeight(uint32 entry);

// What this bot should be attacking. EVERY DPS bot, melee and ranged alike, takes adds first and the boss
// only when nothing else is alive - the raid clears the room, then damages M'uru. The one role split left
// is that melee never take a Void Sentinel (3750 per 3s inside 10y; its tank eats that and nobody else).
// The single add is chosen by minimum weighted health percent with a GUID tie-break, so the whole raid
// derives the SAME target and focus-fires it down. Returns nullptr when there is nothing to correct.
Unit* GetMuruFocusTarget(Player* bot, PlayerbotAI* botAI);
bool MuruShouldRetarget(Player* bot, PlayerbotAI* botAI);

// An add of this tank's assigned kind that is loose on somebody who is not a tank. Main tank holds the
// blood elf waves and Entropius; the off tank holds Void Sentinels, because Void Blast (9426) landing on
// whoever also holds six elites is how a single tank dies.
Creature* FindMuruUntankedAdd(Player* bot, PlayerbotAI* botAI);
bool MuruShouldTankAdds(Player* bot, PlayerbotAI* botAI);

// Same narrow contract as the Twins veto: only while a move is actually owed.
bool MuruShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI);

// ---------------------------------------------------------------------------------------------
// Kil'jaeden

Creature* FindKiljaeden(Player* bot);

// Grid-search free, for the reason spelled out on IsMuruEncounterActive: this is the first line of
// every predicate the movement multiplier evaluates for every queued action of every bot.
bool IsKjEncounterActive(Player* bot);

// Phase 1 - at least one Hand of the Deceiver still alive. KJ himself does not exist yet (he is summoned
// only once the last Hand dies, then spends 11s untargetable in his rebirth), so nothing in the fight
// proper can be keyed off him during this window.
bool KjIsDeceiverPhase(Player* bot);

// Is a Darkness of a Thousand Souls resolving, and how long is left? `remainingMs` is the 46605 aura's
// own duration, which is the whole warning the encounter gives: 47,499 lands the instant it expires.
bool KjDarknessWindow(Player* bot, uint32& remainingMs);

// The authored anchor. Everything else in the fight is a polar offset from this.
void GetKjAnchor(Player* bot, Position& anchor);

// Main tank only: hold the anchor so the boss holds it too.
bool KjShouldHoldAnchor(Player* bot, PlayerbotAI* botAI);

// This bot's fixed slot inside the Darkness stack - two rings at 4y and 8y, so 25 bodies fit inside the
// drake's 12y shield without piling onto one point. Returns false once the bot is already in its slot,
// which is what makes the stack a HOLD rather than a shuffle (the M'uru Darkness idiom).
bool KjShouldStackForDarkness(Player* bot, PlayerbotAI* botAI);
bool GetKjDarknessStackSpot(Player* bot, Position& spot);

// The nearest live Armageddon marker, or null. Distance tests against it must be 2D: the marker is
// pushed 20y into the air and the missile's destination is offset back down to the floor.
Creature* FindKjArmageddonMarker(Player* bot);
bool KjShouldClearArmageddon(Player* bot);
bool GetKjArmageddonClearSpot(Player* bot, Position& spot, uint32 attempt = 0);

// Fire Bloom quarantine. Stands down inside a Darkness window on purpose: Shield of the Blue REMOVES
// 45641 on apply, so the cure there is to be in the stack, not out at 38y where the shield cannot reach.
bool KjShouldQuarantineFireBloom(Player* bot, PlayerbotAI* botAI);
bool GetKjFireBloomSpot(Player* bot, Position& spot, uint32 attempt = 0);

// Ranged/healer baseline formation: a fixed polar station on the KJ_RANGED_RING. Melee and tanks get
// nothing here - they belong in melee range and generic behaviour puts them there.
//
// TWO predicates, and the split is load-bearing. `KjShouldHoldStation` is "a walk is owed" and drives the
// action; `KjHasStation` is "this bot is supposed to be on the ring at all" and drives the movement VETO.
// Using the narrow one for both is what let generic ranged movement drag arrived bots straight back off
// station - see the comment in KjShouldSuppressGenericMovement.
bool KjHasStation(Player* bot, PlayerbotAI* botAI);
bool KjShouldHoldStation(Player* bot, PlayerbotAI* botAI);
bool GetKjStation(Player* bot, PlayerbotAI* botAI, Position& station);

// ---- drake piloting ----

// The drake this bot is currently possessing, or null. Possession makes the player the CHARMER, so this
// is GetCharm() filtered on entry - and it is also the authoritative "am I the pilot" test, because a
// bot that already holds a drake must keep it regardless of how the designation would rank now.
Creature* GetKjDrake(Player* bot);

// Does anybody in the raid already have a drake under control?
bool KjRaidHasDrake(Player* bot);

// The bot designated to fetch a drake: the lowest-GUID alive DPS bot, ranged preferred. Deterministic,
// so every bot in the raid derives the same answer with no shared state - and losing one DPS from 85%
// onward is the price a real raid pays for this too.
bool KjIsDesignatedPilot(Player* bot, PlayerbotAI* botAI);

// The nearest orb that boss_kiljaeden::EmpowerOrb has un-flagged, or null. Empowerment is readable
// directly off the GameObject: the encounter sets GO_FLAG_NOT_SELECTABLE on all four at reset and
// removes it one orb at a time.
GameObject* FindKjEmpoweredOrb(Player* bot);
bool KjShouldClaimOrb(Player* bot, PlayerbotAI* botAI);

// Is this pilot's possession close enough to lapsing that it should be dropped and re-taken? Never true
// inside a Darkness window - dropping there would throw away the shield the raid is about to need.
bool KjShouldRefreshPossession(Player* bot);

// How many alive raiders are inside the shield radius of the drake, out of how many there are. Logged at
// the moment the shield is cast, because the alternative is a silent lethal failure: a bot left outside a
// 12y bubble takes the full 47,499 while the stack action reports "seated" and the drake reports "SHIELD".
// This is the only line that can say the coverage budget actually held in game.
uint32 CountKjShieldCoverage(Player* bot, Creature* drake, uint32& total);

// Where to point a breath cone. `healing` picks the aim rule: Revitalize aims at the LOWEST-HEALTH raider
// in range, Haste aims at the melee stack (Kil'jaeden himself, since that is where the melee are). False
// when nothing eligible is in the KJ_BREATH_AIM_MIN..MAX band, in which case the caller casts without
// re-facing rather than skipping the cast.
bool GetKjBreathBearing(Player* bot, Creature* drake, bool healing, float& bearing);

// ---- targeting ----

// Ranged: Shield Orb > Sinister Reflection. Melee: Sinister Reflection only, because a Shield Orb
// orbits at z=40 and melee cannot reach one. Phase 1 is its own lane: the Hands.
std::vector<uint32> GetKjKillOrder(Player* bot, PlayerbotAI* botAI);

// What this bot should be on, or null when this fight has no opinion. Bucketed health within a tier with
// a GUID tie-break, exactly as M'uru - see MURU_FOCUS_HEALTH_BUCKET for why a raw percent flips every
// tick and freezes melee in place.
Unit* GetKjFocusTarget(Player* bot, PlayerbotAI* botAI);
bool KjShouldRetarget(Player* bot, PlayerbotAI* botAI);

// Same narrow contract as the Twins and M'uru vetoes: only while a move is actually owed. The one broad
// case is a bot that is PILOTING - its body carries UNIT_FLAG_DISABLE_MOVE and is pacified and silenced,
// so there is nothing for generic behaviour to do with it at all.
bool KjShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI);

// Is another alive tank BOT in the raid carrying fewer than `stacks` Meteor Slash stacks?
// Gates the handoff so a lone tank never silences himself waiting for relief that cannot come.
bool HasFresherBrutallusTank(Player* bot, PlayerbotAI* botAI, uint32 stacks);

// Per-instance, 1s-TTL snapshot of both boss halves so the hot multiplier path
// never runs a grid search per queued action.
struct KalecgosSnapshot
{
    time_t takenAt = 0;
    bool valid = false;
    float dragonPct = 100.0f;
    float demonPct = 100.0f;
    bool dragonBanished = false;
    bool demonBanished = false;
};

extern std::unordered_map<uint32, KalecgosSnapshot> kalecgosSnapshotByInstance;

// Cached snapshot for the bot's instance, refreshed via grid search when stale.
// snapshot.valid is false until both halves exist and the dragon is in combat.
KalecgosSnapshot const& GetKalecgosSnapshot(Player* bot);

// Per-instance, 1s-TTL snapshot of who is holding Brutallus. The tank-hold multiplier runs
// once per queued action, so it must never grid-search.
//
// The `anchor*` block and `combatStartMs` are NOT part of the 1s refresh - they are latched once
// per pull and carried across every refresh, and only the whole entry being erased (which happens
// when the boss leaves combat) resets them. That is what makes every station a fixed world point
// and what gives the off tank a stable opening window.
struct BrutallusSnapshot
{
    time_t takenAt = 0;
    bool valid = false;
    ObjectGuid victimGuid;
    float bossX = 0.0f;
    float bossY = 0.0f;

    // Pull time, for BRUTALLUS_OFFTANK_OPENING_MS.
    uint32 combatStartMs = 0;

    // The anchor: where the main tank was standing the first moment he held the boss in melee
    // range, and where the boss was standing at that same moment. Latched together because the
    // formation needs both an origin (the boss) and an axis (the boss->tank bearing).
    bool anchorSet = false;
    ObjectGuid anchorGuid;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorZ = 0.0f;
    float anchorBossX = 0.0f;
    float anchorBossY = 0.0f;
    float anchorAxis = 0.0f;     // boss -> main tank bearing: lane 0, and the cone's rest position
    float anchorRadius = 0.0f;   // boss -> main tank distance: the band both tanks stand on
    float laneSign = 1.0f;       // +1 / -1: which way lane 1 rotates off the axis (LOS-probed once)

    // Settle tracking for the latch (see BRUTALLUS_ANCHOR_SETTLE_MS): the tank must hold still in
    // melee range, not merely arrive there.
    ObjectGuid settleGuid;
    uint32 settleSince = 0;

    // Burn isolation zone per (lane, is-outward-column), index = lane * 2 + (outward ? 1 : 0).
    //
    // LOS-PROBED and remapped at latch, because a zone can be inside rock: laneSign only ever proved
    // that LANE 1's side is open, and there are three more zones. Bots sent into the corridor wall
    // path to wherever they can reach, out of their healers' line of sight, and Burn (~50k over 60s)
    // kills them long before it expires - which is what happened to the group that "died very
    // quickly". A blocked outward zone falls back to the gap zone, which is open by construction
    // because both lanes are.
    float burnBearing[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float burnRange[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Last time ANY bot that could actually see the boss found him in combat. This is what makes
    // the entry's lifetime a property of the ENCOUNTER rather than of whichever bot happened to
    // ask - see BRUTALLUS_SNAPSHOT_KEEP_MS.
    uint32 lastSeenMs = 0;
};

// A veto must never outlive the encounter or reach a bot that isn't in it: a silenced bot looks
// exactly like a broken one (parked, answering "I am already attacking", cleared only by a
// manual relog). Every freeze is gated on the bot being this close to the boss.
constexpr float BRUTALLUS_PARTICIPANT_RANGE = 60.0f;

// How long the per-instance entry survives with nobody able to see the boss in combat.
//
// This exists because the entry is SHARED RAID STATE - the latched anchor every station is
// measured from, and the pull timestamp the off tank's opening window runs off - so anything that
// erases it mid-fight re-anchors the whole raid on a new axis and re-arms the opening window.
// Two things were doing exactly that:
//   * the out-of-combat cleanup action, which fires for ANY bot not in combat - and a DEAD bot is
//     not in combat, so the first casualty wiped the formation for everyone;
//   * this getter erasing when its own caller could not find the boss - a corpse released to the
//     graveyard is far past the 250y search, so that bot's read deleted a live encounter's state.
// Observed as the off tank walking to a brand-new lane bearing mid-fight, the zones shifting under
// everyone, and the tank swap needing 5 stacks because the off tank's no-damage window kept
// restarting and he never built the threat the handoff spends.
constexpr uint32 BRUTALLUS_SNAPSHOT_KEEP_MS = 15000;

extern std::unordered_map<uint32, BrutallusSnapshot> brutallusSnapshotByInstance;

BrutallusSnapshot const& GetBrutallusSnapshot(Player* bot);

}  // namespace SunwellPlateauHelpers

#endif
