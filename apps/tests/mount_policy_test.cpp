// Standalone tests for PlayerbotMountPolicy (no core headers needed). Build and run:
//   g++ -std=c++20 -Wall -Wextra -Werror -I src/Bot/Factory apps/tests/mount_policy_test.cpp
//       src/Bot/Factory/PlayerbotMountPolicy.cpp -o /tmp/mount_policy_test && /tmp/mount_policy_test

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

#include "PlayerbotMountPolicy.h"

using namespace PlayerbotMountPolicy;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        ++checks;                                                       \
        if (!(cond))                                                    \
        {                                                               \
            ++failures;                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

static bool Has(std::vector<uint32_t> const& v, uint32_t id) { return std::find(v.begin(), v.end(), id) != v.end(); }

// The 12 races and their class lists on this server (stock + UAC custom combos).
struct RaceClasses
{
    uint8_t race;
    std::vector<uint8_t> classes;
};
static std::vector<RaceClasses> const kCombos = {
    {RACE_ID_HUMAN, {1, 2, 4, 5, 6, 8, 9}},
    {RACE_ID_DWARF, {1, 2, 3, 4, 5, 6, 7, 9}},
    {RACE_ID_NIGHTELF, {1, 3, 4, 5, 6, 11}},
    {RACE_ID_GNOME, {1, 4, 6, 8, 9}},
    {RACE_ID_DRAENEI, {1, 2, 3, 5, 6, 7, 8}},
    {RACE_ID_WORGEN, {1, 2, 3, 4, 5, 6, 8, 9, 11}},
    {RACE_ID_ORC, {1, 3, 4, 6, 7, 9}},
    {RACE_ID_TROLL, {1, 3, 4, 5, 6, 7, 8}},
    {RACE_ID_TAUREN, {1, 2, 3, 6, 7, 11}},
    {RACE_ID_UNDEAD, {1, 2, 4, 5, 6, 8, 9}},
    {RACE_ID_BLOODELF, {1, 2, 3, 4, 5, 6, 8, 9}},
    {RACE_ID_GOBLIN, {1, 3, 4, 5, 6, 7, 8, 9}},
};

static void TestTable()
{
    std::set<uint32_t> seen;
    for (RaceClasses const& rc : kCombos)
    {
        RacialMounts const* m = GetRacialMounts(rc.race);
        CHECK(m != nullptr);
        if (!m)
            continue;
        CHECK(!m->slow.empty());
        CHECK(!m->fast.empty());
        for (uint32_t id : m->slow)
        {
            CHECK(!IsForeignRacialMount(rc.race, id));
            CHECK(IsRacialVendorMount(id));
        }
        for (uint32_t id : m->fast)
            CHECK(!IsForeignRacialMount(rc.race, id));
        // no spell shared between two races (Running Wild appears in both of Worgen's own tiers only)
        std::set<uint32_t> own(m->slow.begin(), m->slow.end());
        own.insert(m->fast.begin(), m->fast.end());
        for (uint32_t id : own)
        {
            CHECK(!seen.count(id));
            seen.insert(id);
        }
        // fast list has no duplicates
        std::set<uint32_t> fastSet(m->fast.begin(), m->fast.end());
        CHECK(fastSet.size() == m->fast.size());
    }
    CHECK(GetRacialMounts(0) == nullptr);
    CHECK(GetRacialMounts(13) == nullptr);

    // Blood Elf fix: 33660 Swift Pink Hawkstrider is 100% -> fast list; 35025 once.
    RacialMounts const* be = GetRacialMounts(RACE_ID_BLOODELF);
    CHECK(Has(be->fast, 33660) && !Has(be->slow, 33660));
    CHECK(std::count(be->fast.begin(), be->fast.end(), 35025u) == 1);

    CHECK(GetRacialMounts(RACE_ID_WORGEN)->slow == std::vector<uint32_t>{MOUNT_RUNNING_WILD});
    CHECK(GetRacialMounts(RACE_ID_GOBLIN)->slow == std::vector<uint32_t>{MOUNT_GOBLIN_TRIKE});
    CHECK(GetRacialMounts(RACE_ID_GOBLIN)->fast == std::vector<uint32_t>{MOUNT_GOBLIN_TURBO_TRIKE});
}

static void TestForeign()
{
    // The production cases from the 2026-11-09 audit.
    CHECK(IsForeignRacialMount(RACE_ID_WORGEN, 34406));  // Brown Elekk
    CHECK(IsForeignRacialMount(RACE_ID_WORGEN, 10793));  // Striped Nightsaber
    CHECK(IsForeignRacialMount(RACE_ID_WORGEN, 470));    // Black Stallion
    CHECK(IsForeignRacialMount(RACE_ID_WORGEN, 6898));   // White Ram
    CHECK(IsForeignRacialMount(RACE_ID_WORGEN, 23225));  // Swift Green Mechanostrider
    CHECK(IsForeignRacialMount(RACE_ID_GOBLIN, 6654));   // Brown Wolf
    CHECK(IsForeignRacialMount(RACE_ID_GOBLIN, 35025));  // Swift Green Hawkstrider
    CHECK(IsForeignRacialMount(RACE_ID_DWARF, 35713));   // Great Blue Elekk
    CHECK(IsForeignRacialMount(RACE_ID_UNDEAD, 33660));
    CHECK(IsForeignRacialMount(RACE_ID_HUMAN, MOUNT_RUNNING_WILD));
    CHECK(IsForeignRacialMount(RACE_ID_ORC, MOUNT_GOBLIN_TRIKE));

    // Never foreign: class, flying, special/reward mounts.
    for (RaceClasses const& rc : kCombos)
    {
        for (uint32_t id : {13819u, 23214u, 34769u, 34767u, 5784u, 23161u, 48778u, 54729u, 32235u, 32244u, 32292u,
                            32297u, 55164u, 59791u, 61425u, 103195u})
            CHECK(!IsForeignRacialMount(rc.race, id));
        std::vector<uint32_t> foreign = GetForeignRacialMounts(rc.race);
        RacialMounts const* own = GetRacialMounts(rc.race);
        for (uint32_t id : foreign)
        {
            CHECK(!Has(own->slow, id) && !Has(own->fast, id));
            CHECK(IsRacialVendorMount(id));
        }
        CHECK(std::set<uint32_t>(foreign.begin(), foreign.end()).size() == foreign.size());
    }
    // 65 distinct racial spells; Worgen foreign = all but Running Wild.
    CHECK(GetForeignRacialMounts(RACE_ID_WORGEN).size() == 64);
}

static void TestClass()
{
    CHECK(IsClassMount(CLASS_ID_PALADIN, 13819) && IsClassMount(CLASS_ID_PALADIN, 34767));
    CHECK(IsClassMount(CLASS_ID_WARLOCK, 5784) && IsClassMount(CLASS_ID_WARLOCK, 23161));
    CHECK(!IsClassMount(CLASS_ID_WARLOCK, 7584));  // "Increased Gun", the old IP id
    CHECK(IsClassMount(CLASS_ID_DEATH_KNIGHT, 48778));
    CHECK(!IsClassMount(CLASS_ID_PALADIN, 5784));
    CHECK(!IsClassMount(1, 13819));
}

static void TestRanking()
{
    // Foreign racial mounts are dropped entirely.
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        std::vector<uint32_t> r = RankGroundMounts(RACE_ID_WORGEN, 1, {34406, MOUNT_RUNNING_WILD, 10793}, roll);
        CHECK(r == std::vector<uint32_t>{MOUNT_RUNNING_WILD});
    }
    // Only foreign -> nothing to ride (caller falls back to the next speed bucket / items).
    CHECK(RankGroundMounts(RACE_ID_GOBLIN, 1, {6654, 17463}, 5).empty());

    // Paladin: class mount first; race-appropriate spell first among same tier.
    CHECK(RankGroundMounts(RACE_ID_HUMAN, CLASS_ID_PALADIN, {23227, 34769, 13819}, 0) ==
          (std::vector<uint32_t>{13819, 34769, 23227}));
    CHECK(RankGroundMounts(RACE_ID_BLOODELF, CLASS_ID_PALADIN, {13819, 34769, 35025}, 0) ==
          (std::vector<uint32_t>{34769, 13819, 35025}));
    CHECK(RankGroundMounts(RACE_ID_TAUREN, CLASS_ID_PALADIN, {23249, 34767}, 3).front() == 34767);

    // Warlock / Death Knight: class first, every roll.
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        CHECK(RankGroundMounts(RACE_ID_GNOME, CLASS_ID_WARLOCK, {23225, 23161}, roll).front() == 23161);
        CHECK(RankGroundMounts(RACE_ID_GOBLIN, CLASS_ID_WARLOCK, {MOUNT_GOBLIN_TURBO_TRIKE, 23161}, roll).front() ==
              23161);
        CHECK(RankGroundMounts(RACE_ID_ORC, CLASS_ID_DEATH_KNIGHT, {23250, 48778}, roll).front() == 48778);
        // Worgen Warlock/DK: class mount preferred; Running Wild kept as fallback.
        std::vector<uint32_t> w = RankGroundMounts(RACE_ID_WORGEN, CLASS_ID_WARLOCK, {MOUNT_RUNNING_WILD, 23161}, roll);
        CHECK(w == (std::vector<uint32_t>{23161, MOUNT_RUNNING_WILD}));
    }

    // Worgen Paladin: 70% class mount, 30% Running Wild; both always present.
    int classFirst = 0;
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        std::vector<uint32_t> r =
            RankGroundMounts(RACE_ID_WORGEN, CLASS_ID_PALADIN, {MOUNT_RUNNING_WILD, 23214, 34406}, roll);
        CHECK(r.size() == 2 && Has(r, 23214) && Has(r, MOUNT_RUNNING_WILD));
        if (r.front() == 23214)
            ++classFirst;
    }
    CHECK(classFirst == static_cast<int>(WORGEN_PALADIN_CLASS_MOUNT_PERCENT));
    // Worgen Paladin with no class mount in the bucket: Running Wild.
    CHECK(RankGroundMounts(RACE_ID_WORGEN, CLASS_ID_PALADIN, {MOUNT_RUNNING_WILD}, 99) ==
          std::vector<uint32_t>{MOUNT_RUNNING_WILD});

    // Non-class Worgen / Goblin: own racial first, special mounts after.
    CHECK(RankGroundMounts(RACE_ID_WORGEN, 4, {59791, MOUNT_RUNNING_WILD}, 0).front() == MOUNT_RUNNING_WILD);
    CHECK(RankGroundMounts(RACE_ID_GOBLIN, 1, {59791, MOUNT_GOBLIN_TRIKE}, 1).front() == MOUNT_GOBLIN_TRIKE);
    // Special mount still reachable when no racial/class mount is in the bucket.
    CHECK(RankGroundMounts(RACE_ID_GOBLIN, 1, {59791}, 1) == std::vector<uint32_t>{59791});

    // Every own racial mount reachable as first choice across rolls (variety preserved).
    std::set<uint32_t> firsts;
    for (uint32_t roll = 0; roll < 100; ++roll)
        firsts.insert(RankGroundMounts(RACE_ID_HUMAN, 1, {470, 6648, 458, 472}, roll).front());
    CHECK(firsts.size() == 4);

    // Duplicates in the input are collapsed; output never contains a spell twice.
    std::vector<uint32_t> d = RankGroundMounts(RACE_ID_HUMAN, CLASS_ID_PALADIN, {13819, 13819, 470, 470}, 7);
    CHECK(d.size() == 2);

    // Every race/class combo: output is a subset of input with no foreign racial mount.
    for (RaceClasses const& rc : kCombos)
        for (uint8_t cls : rc.classes)
        {
            std::vector<uint32_t> all;
            for (RaceClasses const& other : kCombos)
            {
                RacialMounts const* m = GetRacialMounts(other.race);
                all.insert(all.end(), m->slow.begin(), m->slow.end());
                all.insert(all.end(), m->fast.begin(), m->fast.end());
            }
            all.insert(all.end(), {13819, 23214, 34769, 34767, 5784, 23161, 48778, 59791});
            for (uint32_t roll : {0u, 42u, 69u, 70u, 99u})
            {
                std::vector<uint32_t> r = RankGroundMounts(rc.race, cls, all, roll);
                for (uint32_t id : r)
                {
                    CHECK(Has(all, id));
                    CHECK(!IsForeignRacialMount(rc.race, id));
                }
                // a class with a class mount ranks one first (Worgen Paladin may lead with Running Wild)
                if (cls == CLASS_ID_PALADIN || cls == CLASS_ID_WARLOCK || cls == CLASS_ID_DEATH_KNIGHT)
                    CHECK(IsClassMount(cls, r.front()) ||
                          (rc.race == RACE_ID_WORGEN && cls == CLASS_ID_PALADIN && r.front() == MOUNT_RUNNING_WILD));
                else
                    CHECK(Categorize(rc.race, cls, r.front()) == MountCategory::OwnRacial);
            }
        }
}

int main()
{
    TestTable();
    TestForeign();
    TestClass();
    TestRanking();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
