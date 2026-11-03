"""Battleground queue demand, bracket and uptime checks.

The production BattlegroundInfo struct (src/Bot/RandomPlayerbotMgr.h) and core getMSTimeDiff
(src/common/Utilities/Timer.h) are extracted verbatim, compiled with minimal stubs by MSVC into a
temporary directory and run. Source contracts cover the parts that need a live BattlegroundQueue.

    py -3 -B apps/tests/test_bg_queue_policy.py
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT.parents[1]


def extract_block(source, start_pattern):
    start = re.search(start_pattern, source).start()
    open_brace = source.index("{", start)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError("unterminated block")


HARNESS = r"""
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <vector>
typedef std::uint32_t uint32;
typedef std::uint8_t uint8;
enum TeamId { TEAM_ALLIANCE = 0, TEAM_HORDE = 1, TEAM_NEUTRAL = 2 };

%(timer)s

%(info)s;

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL line %%d: %%s\n", __LINE__, #expr); ++failures; } } while (0)

int main()
{
    // A human queue opens TeamSize slots per active queue for bots to fill.
    {
        BattlegroundInfo info;
        info.activeBgQueue = 1;
        CHECK(info.DemandedBgSlotsPerTeam(10, TEAM_ALLIANCE) == 10);
        CHECK(info.DemandedBgSlotsPerTeam(10, TEAM_HORDE) == 10);
    }
    // Instances with a real player always demand their full capacity.
    {
        BattlegroundInfo info;
        info.bgPlayerInstances = {7};
        info.bgInstanceCapacity[7] = 10;
        CHECK(info.DemandedBgSlotsPerTeam(10, TEAM_HORDE) == 10);
    }
    // Bot-only instances: only the auto-join target counts at full capacity. Others only count
    // the bots they already have, so they cannot sustain extra demand (no runaway).
    {
        BattlegroundInfo info;
        info.bgAutoJoinTarget = 1;
        for (uint32 instance : {1u, 2u, 3u})
            info.bgInstanceCapacity[instance] = 10;
        info.bgInstanceBots[1] = {9, 8};
        info.bgInstanceBots[2] = {7, 6};
        info.bgInstanceBots[3] = {5, 4};
        uint32 alliance = info.DemandedBgSlotsPerTeam(10, TEAM_ALLIANCE);
        CHECK(alliance == 10 + 7 + 5);
        // With every instance already holding its bots, demand is not above supply plus the target's gap.
        uint32 allianceBots = 9 + 7 + 5;
        CHECK(alliance - allianceBots == 10 - 9);
        // The old formula demanded TeamSize * instances = 30 and kept spawning bot-only games.
        CHECK(alliance < 10u * 3u);
    }
    // No queues and no instances: no demand.
    {
        BattlegroundInfo info;
        CHECK(info.DemandedBgSlotsPerTeam(10, TEAM_ALLIANCE) == 0);
    }

    // Uptime: GetInGameTime() is login uptime in ms; the join delay is 120 000 ms.
    {
        uint32 const delay = 120 * 1000;
        uint32 login = 1790000000u;              // ~20.7 days of uptime
        CHECK(getMSTimeDiff(login, login + 130000u) >= delay);
        CHECK(getMSTimeDiff(login, login + 60000u) < delay);
        // Across the uint32 wrap at 49.7 days.
        uint32 lateLogin = 0xFFFFFFFFu - 50000u;
        CHECK(getMSTimeDiff(lateLogin, 80000u) >= delay);
        CHECK(getMSTimeDiff(lateLogin, 10000u) < delay);
        // The old check compared epoch seconds with uptime ms and went negative here.
        long long epochNow = 1793000000LL;
        CHECK((epochNow - static_cast<long long>(1800000000u)) < 120);
    }

    if (failures)
        return 1;
    std::printf("PASS: battleground queue demand and uptime checks\n");
    return 0;
}
"""


def vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    install = subprocess.run([str(vswhere), "-latest", "-property", "installationPath"],
                             capture_output=True, text=True, check=True).stdout.strip()
    return Path(install) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"


class BattlegroundQueuePolicyTests(unittest.TestCase):
    def test_demand_and_uptime_compiled(self):
        header = (ROOT / "src/Bot/RandomPlayerbotMgr.h").read_text(encoding="utf-8")
        info = extract_block(header, r"struct BattlegroundInfo\b")
        timer_source = (CORE / "src/common/Utilities/Timer.h").read_text(encoding="utf-8")
        timer = extract_block(timer_source, r"inline uint32 getMSTimeDiff\(uint32 oldMSTime, uint32 newMSTime\)")
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "bg_queue_policy_test.cpp"
            source.write_text(HARNESS % {"timer": timer, "info": info}, encoding="utf-8")
            script = Path(temp) / "build.cmd"
            exe = Path(temp) / "bg_queue_policy_test.exe"
            script.write_text('@call "%s" >nul 2>nul\r\n@cl /nologo /std:c++20 /EHsc /W4 /WX "%s" /Fo"%s\\\\" /Fe"%s"'
                              ' >"%s\\build.txt" || (type "%s\\build.txt" & exit /b 1)\r\n@"%s"\r\n'
                              % (vcvars(), source, temp, exe, temp, temp, exe))
            result = subprocess.run(["cmd", "/d", "/c", str(script)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_join_actions_use_demand_and_uptime_ms(self):
        action = (ROOT / "src/Ai/Base/Actions/BattleGroundJoinAction.cpp").read_text(encoding="utf-8")
        self.assertEqual(action.count("DemandedBgSlotsPerTeam(TeamSize, teamId)"), 2)
        self.assertNotIn("TeamSize * (activeBgQueue + bgInstanceCount)", action)
        self.assertNotIn("time(nullptr) - bot->GetInGameTime()", action)
        self.assertIn("getMSTimeDiff(bot->GetInGameTime()", action)

    def test_brackets_come_from_queue_or_instance(self):
        manager = (ROOT / "src/Bot/RandomPlayerbotMgr.cpp").read_text(encoding="utf-8")
        check = extract_block(manager, r"void RandomPlayerbotMgr::CheckBgQueue\(\)")
        self.assertIn("auto getQueueBracket", check)
        self.assertEqual(check.count("getQueueBracket(player, bg, queueType, queueTypeId, mapId)"), 1)
        self.assertEqual(check.count("getQueueBracket(bot, bg, queueType, queueTypeId, mapId)"), 1)
        self.assertNotIn("GetBattlegroundBracketByLevel(mapId, player->GetLevel())", check)
        self.assertNotIn("GetBattlegroundBracketByLevel(mapId, bot->GetLevel())", check)
        self.assertIn("bgAutoJoinTarget = minCount", check)


if __name__ == "__main__":
    unittest.main(verbosity=2)
