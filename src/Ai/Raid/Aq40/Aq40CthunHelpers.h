/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40CTHUNHELPERS_H
#define PLAYERBOTS_AQ40CTHUNHELPERS_H

#include <cmath>
#include <string>

#include "Common.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

namespace TempleOfAhnQirajHelpers
{

// InstanceScript data ids from temple_of_ahnqiraj.h (script-local, so hardcoded; printed at strategy
// load so a drift shows in the log rather than as a silently dead encounter gate). The Eye and the
// body are both BossAI(creature, DATA_CTHUN): one boss slot, IN_PROGRESS across both phases.
constexpr uint32 AQT_DATA_CTHUN = 9;
constexpr uint32 AQT_DATA_EYE_OF_CTHUN = 18;

constexpr uint32 NPC_CTHUN_CLAW_TENTACLE = 15725;
constexpr uint32 NPC_CTHUN_EYE_TENTACLE = 15726;
constexpr uint32 NPC_CTHUN_GIANT_CLAW_TENTACLE = 15728;
constexpr uint32 NPC_CTHUN_GIANT_EYE_TENTACLE = 15334;
constexpr uint32 NPC_CTHUN_FLESH_TENTACLE = 15802;

// 2s cast, chains up to 60 targets, each jump within 10y of the previous victim (magic damage
// class, Spell::SearchChainTargets). Nobody within 10y of the target = it hits only the target.
constexpr uint32 SPELL_CTHUN_GREEN_BEAM = 26134;
// On the Eye (permanent duration) for the whole Dark Glare phase, wind-up included.
constexpr uint32 SPELL_CTHUN_FREEZE_ANIM = 16245;
// -100% damage taken, all schools. Removed while C'Thun is weakened.
constexpr uint32 SPELL_CTHUN_CARAPACE = 26156;
// Stomach DoT, +1 stack every 5s, removed on exit.
constexpr uint32 SPELL_CTHUN_DIGESTIVE_ACID = 26476;
// Stomach grab: stun + MOUNTED aura for 3.8s before the teleport. Never cancel it.
constexpr uint32 SPELL_CTHUN_MOUTH_TENTACLE = 26332;

// Client-side area trigger: bots never send it themselves (no client), so the exit action sends
// CMSG_AREATRIGGER for them. The handler checks the bot is within 5y of this point.
constexpr uint32 AT_CTHUN_STOMACH_EXIT = 4033;
constexpr float CTHUN_STOMACH_EXIT_X = -8546.2f;
constexpr float CTHUN_STOMACH_EXIT_Y = 1987.7f;
constexpr float CTHUN_STOMACH_EXIT_Z = -96.5f;

// ---- tuning: round numbers with obvious slack, not derived margins ----
// Stomach exit: stack N costs 149*N per 5s tick. Unhealed, 7 stacks is ~4.2k cumulative; with a
// healer in the stomach alongside, bots stay to 12. Low health always leaves.
constexpr uint32 CTHUN_ACID_EXIT_STACKS = 7;
constexpr uint32 CTHUN_ACID_EXIT_STACKS_HEALED = 12;
constexpr float CTHUN_STOMACH_EXIT_HEALTH_PCT = 35.0f;
constexpr float CTHUN_EXIT_REACHED = 3.0f;       // inside the 5y trigger
constexpr uint32 CTHUN_EXIT_HOLD_MS = 4000;      // script re-checks our position 3s after the trigger
constexpr float CTHUN_STOMACH_RADIUS = 100.0f;   // "near the stomach", 2D from the exit point
// The stomach sits directly UNDER an ordinary trash hall (Vekniss/Brainwasher packs, floor z ~-3 to 0)
// that is inside CTHUN_STOMACH_RADIUS, so "z < 0" alone matched it and the landing-hover teleport
// dropped bots ~95y onto the stomach floor. Swallowed players arrive at z -70; the hall is above -3.
constexpr float CTHUN_STOMACH_CEILING_Z = -40.0f;
// Server-side bots get no client gravity: the swallow teleport lands at z ~-70 over a floor near
// -97; that landing is the only hover we correct (see GetCthunAirborneState).
// Stomach landing hover: swallowed players arrive at z -70 over a floor near -97.
constexpr float CTHUN_STOMACH_HOVER_Z = -90.0f;
constexpr float CTHUN_STOMACH_FLOOR_MIN_Z = -110.0f;
constexpr float CTHUN_GROUND_SEARCH = 150.0f;
// Flesh Tentacles sit directly below the body (~198y 3D), so the count search is wide.
constexpr float CTHUN_FLESH_SEARCH_RANGE = 250.0f;
// Stomach close-in: pathfinding-based reach fails in the stomach, so bots are walked into range of
// a Flesh Tentacle (combat reach 15: melee needs ~<17y, casters ~<45y).
constexpr float CTHUN_STOMACH_MELEE_RANGE = 10.0f;
constexpr float CTHUN_STOMACH_RANGED_RANGE = 28.0f;

// Green Beam chain jump: chainSource->IsWithinDist(target, 10) ADDS both object sizes (~1.5 each),
// so the effective jump is ~13y centre-to-centre.
constexpr float CTHUN_BEAM_CLEAR_RANGE = 14.0f;
constexpr float CTHUN_BEAM_HOLD_RANGE = 20.0f;   // don't walk to a slot near a beam in flight

constexpr float CTHUN_GLARE_HALF_WIDTH = 5.0f;     // beam half-width, yards
constexpr float CTHUN_GLARE_PAD = 3.0f;            // extra yards on the edge
constexpr float CTHUN_GLARE_SWEEP_RATE = float(M_PI) / 35.0f;  // rad/s: 180 deg over 35s
constexpr float CTHUN_GLARE_MOVE_SECONDS = 4.0f;   // start dodging this long before it can hit
constexpr float CTHUN_GLARE_SAFE_SECONDS = 9.0f;   // dodge to this far ahead of the beam
constexpr uint32 CTHUN_GLARE_CAST_PAD_MS = 1000;   // cast-safety veto slack
constexpr float CTHUN_GLARE_MAX_RANGE = 100.0f;    // only bots in the room
constexpr uint32 CTHUN_GLARE_SECTORS = 64;

// FIXED spots, spaced BY CONSTRUCTION past the ~13y effective chain jump (IsWithinDist(10) adds both
// object sizes); no disperse. Ranged (Eye phase): two full interleaved circles around the Eye (~13
// per ring -> ~15y between neighbours on A, ~15.6y to the nearest B spot, 15y outside the melee
// ring); B stays inside the ~46.5y Eye casting range.
constexpr float CTHUN_RANGED_RING_A = 30.5f;
constexpr float CTHUN_RANGED_RING_B = 43.5f;
// Melee (tanks included), Eye phase only: 6 spots on a ring in the Eye's melee reach (~17y from its
// centre, combat reach 15; chord 15.5y) plus the centre, 2 bots per spot so a Green Beam chain can
// jump once (to the partner) and stop. The partner stands 1y tangentially off the spot. Overflow
// bots (rank >= 14) take the half-steps between ring spots.
constexpr float CTHUN_MELEE_RING = 15.5f;
constexpr uint32 CTHUN_MELEE_RING_SPOTS = 6;
constexpr uint32 CTHUN_MELEE_PER_SPOT = 2;
constexpr float CTHUN_MELEE_PAIR_OFFSET = 1.0f;
// During the opening melee stage at +-90 deg off the puller's lane (which runs along the ring
// reference through the centre), 35y out, pairs spread 12y apart along that circle.
constexpr float CTHUN_MELEE_STAGE_RADIUS = 35.0f;
constexpr float CTHUN_MELEE_STAGE_SPACING = 12.0f;
constexpr float CTHUN_SLOT_TOLERANCE = 1.5f;
// The script fires exactly 3 Green Beams at the engager (RepeatCounter < 3 -> casts at 3/6/9s, 2s
// cast); the first random beam begins 5s after the 3rd cast started. The opening ends when the 3rd
// has landed.
constexpr uint32 CTHUN_OPENING_PULLER_BEAMS = 3;
constexpr float CTHUN_PULLER_RADIUS = 25.0f;     // puller's far-side hold, from the Eye
// No-go zone around the opening puller for everyone else, for the whole opening.
// Chain reach is ~13y centre-to-centre; these keep a few yards of slack without making the raid
// back away and wait at the pull (25y did).
constexpr float CTHUN_PULLER_AVOID_RANGE = 16.0f;  // inside -> retreat to ~21y
constexpr float CTHUN_PULLER_HOLD_RANGE = 20.0f;   // inside -> don't advance

constexpr float CTHUN_MELEE_ADD_RANGE = 8.0f;     // non-tank melee only take a claw that is on them
// Ranged hold a fixed spot and never close range, so they only pick adds they can hit from it.
constexpr float CTHUN_RANGED_ADD_RANGE = 36.0f;
constexpr float CTHUN_RANGED_CLAW_RANGE = 25.0f;  // ranged help with claws this close to them
constexpr float CTHUN_SEARCH_RANGE = 100.0f;     // grid searches, always issued FROM the bot
constexpr uint32 CTHUN_SNAPSHOT_TTL_MS = 500;

enum class CthunPhase : uint8
{
    None = 0,  // not engaged, or the ~3.5s Eye -> body transition
    Eye,
    Body
};

// Per-instance half-second snapshot. Returned BY VALUE: the cache is mutex-guarded and shared
// across map-update threads.
struct CthunSnapshot
{
    uint32 takenAtMs = 0;
    bool valid = false;
    CthunPhase phase = CthunPhase::None;
    bool weakened = false;  // Body phase with both Flesh Tentacles dead (the script's own rule)
    uint32 fleshAlive = 0;  // living Flesh Tentacles (stomach)
    bool glare = false;     // Dark Glare phase (wind-up included)
    float beamAngle = 0.0f; // Eye's facing while glare
    float centerX = 0.0f;   // Eye and body are co-located
    float centerY = 0.0f;
    float centerZ = 0.0f;
};

// ---- encounter state ----
bool IsCthunEncounterActive(Player* bot);
CthunSnapshot GetCthunSnapshot(Player* bot);
bool IsInCthunStomach(Player* bot);
void CthunEraseTrackers(Player* bot);

// ---- stomach ----
bool CthunShouldExitStomach(Player* bot);  // owed and not already holding after a sent trigger
// Why the bot owes a stomach exit ("acid N" / "no flesh (weakened)"), or "" when it does not.
std::string CthunExitReason(Player* bot);
enum class CthunAirborne : uint8
{
    None = 0,
    Hovering,   // stomach landing hover (z > CTHUN_STOMACH_HOVER_Z): teleport down
    StaleFall   // on the ground but MOVEMENTFLAG_FALLING* still set: scrub the flags
};
// A bot never sends the landing opcode, so a fall's FALLING flag sticks and isMoving() stays true
// (every cast-time spell then fails). groundZ is filled for logging.
CthunAirborne GetCthunAirborneState(Player* bot, PlayerbotAI* botAI, float& groundZ);
// Where to walk to get into range of the stomach target (healers: the nearest Flesh Tentacle);
// false when already in range, leaving, or unable to move.
bool GetCthunStomachApproachSpot(Player* bot, PlayerbotAI* botAI, Position& spot);
bool IsCthunExitHolding(Player* bot);
void CthunStartExitHold(Player* bot);

// ---- Dark Glare ----
// Worst-case seconds until the Dark Glare beam can reach the bot (beam sweeping toward it); 999
// when there is no glare / not applicable.
float CthunGlareSecondsToHit(Player* bot);
bool GetCthunGlareDodgeSpot(Player* bot, Position& spot);
bool CthunShouldDodgeGlare(Player* bot);

// ---- Green Beam ----
// True while the bot is the unit target of any Green Beam being cast.
bool IsCthunBeamTarget(Player* bot);
// What this bot must step away from: a beam target within CTHUN_BEAM_CLEAR_RANGE, or - if the bot
// IS a beam target - its nearest living room-side raid member within that range. nullptr = clear.
Unit* GetCthunBeamThreat(Player* bot);
bool CthunShouldClearBeam(Player* bot);
bool GetCthunBeamClearSpot(Player* bot, Position& spot, uint32 attempt = 0);

// ---- opening ----
// The Eye's first four Green Beams all go to the engager. Until the first beam at someone else (or
// the puller dies / the Eye phase ends), that bot drags them to the side opposite the raid.
bool IsCthunOpeningPuller(Player* bot);
// The opening runs (entry exists and not over). Melee wait it out where they are.
bool IsCthunOpeningActive(Player* bot);
bool GetCthunPullerSpot(Player* bot, Position& spot);  // false once arrived (hold)
// The opening puller (never this bot) while the opening runs, if within range (2D); else nullptr.
Player* GetCthunOpeningPullerWithin(Player* bot, float range);

// ---- fixed spots ----
// Ranged role for spots: healer or ranged, never a tank.
bool IsCthunRangedRole(Player* member);
// Walk back to the bot's FIXED spot whenever off it (melee: the opening staging point first).
bool CthunShouldFormUp(Player* bot, PlayerbotAI* botAI);
bool GetCthunFormUpSpot(Player* bot, PlayerbotAI* botAI, Position& spot);

// ---- targeting ----
Unit* GetCthunAttackTarget(Player* bot, PlayerbotAI* botAI);
bool IsCthunHoldingFire(Player* bot, PlayerbotAI* botAI);
bool CthunShouldRetarget(Player* bot, PlayerbotAI* botAI);

// Shared by the triggers and the movement multiplier so they cannot disagree.
bool CthunHazardMoveOwed(Player* bot);
// Mounted (e.g. rode in with the master) and nowhere to go right now: dismount so the bot can fight.
// The Mouth Tentacle grab is also a MOUNTED aura (26332) and is left alone.
bool CthunShouldDismount(Player* bot, PlayerbotAI* botAI);
bool CthunShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI);

}  // namespace TempleOfAhnQirajHelpers

#endif
