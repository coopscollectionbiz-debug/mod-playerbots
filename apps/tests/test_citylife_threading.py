"""Thread-ownership contracts for CityLife (source checks; they do not run the server)."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]


def body(source, signature):
    start = source.index(signature)
    open_brace = source.index("{", start)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace:index + 1]
    raise AssertionError(f"unterminated body for {signature}")


class CityLifeThreadingTests(unittest.TestCase):
    def read(self, path):
        return (ROOT / path).read_text(encoding="utf-8")

    def test_zone_hook_only_queues(self):
        hooks = self.read("src/Script/Playerbots.cpp")
        zone_hook = body(hooks, "void OnPlayerUpdateZone(")
        self.assertIn("QueueCityLifeRequest(player, newZone)", zone_hook)
        self.assertNotIn("PopulateCityForPlayer", zone_hook)

    def test_population_runs_only_from_world_thread_drain(self):
        callers = []
        for path in (ROOT / "src").rglob("*.cpp"):
            for match in re.finditer(r"PopulateCityForPlayer\(", path.read_text(encoding="utf-8")):
                callers.append(path.name)
        # Definition plus the single call in ProcessCityLifeRequests.
        self.assertEqual(sorted(callers), ["RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr.cpp"])
        manager = self.read("src/Bot/RandomPlayerbotMgr.cpp")
        drain = body(manager, "void RandomPlayerbotMgr::ProcessCityLifeRequests(")
        self.assertIn("_cityLifeRequests.Take()", drain)
        self.assertIn("ObjectAccessor::FindPlayer(", drain)
        self.assertIn("PopulateCityForPlayer(player, request.zone)", drain)
        self.assertIn("LFG_STATE_NONE", drain)

    def test_drain_is_called_from_world_update(self):
        hooks = self.read("src/Script/Playerbots.cpp")
        world_update = body(hooks, "void OnUpdate(uint32 diff) override")
        self.assertIn("sRandomPlayerbotMgr.ProcessCityLifeRequests(diff);", world_update)

    def test_map_thread_actions_do_not_read_lfg_manager(self):
        action = self.read("src/Ai/World/Rpg/Action/NewRpgAction.cpp")
        for signature in ("bool NewRpgCityLifeAction::Execute(Event", "bool NewRpgCityLifeAction::ExecuteNatural()"):
            if signature not in action:
                continue
            with self.subTest(function=signature):
                self.assertNotIn("sLFGMgr", body(action, signature))

    def test_queue_holds_lock_only_for_push_and_swap(self):
        queue = self.read("src/Ai/World/Rpg/CityLifeRequestQueue.h")
        self.assertEqual(queue.count("std::lock_guard<std::mutex>"), 2)
        self.assertIn("requests.swap(_pending);", queue)
        self.assertIn("Capacity", queue)


if __name__ == "__main__":
    unittest.main(verbosity=2)
