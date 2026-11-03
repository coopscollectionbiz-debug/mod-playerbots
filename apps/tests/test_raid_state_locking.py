"""Raid helper shared-state contracts (source checks; the containers themselves are exercised by
apps/tests/encounter_state_map_test.cpp). These encode the ownership rules the handles rely on."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
RAIDS = ("Hyjal", "Mag", "SSC", "TK", "ZA", "BT", "Kara")
GLOBAL = re.compile(r"^\s*(?:extern\s+)?(?:static\s+)?std::unordered_(?:map|set|multimap)<.*>\s+[A-Za-z_]+;", re.M)
LOCKED = re.compile(r"^\s*(?:extern\s+)?Encounter(?:State|SharedState)(?:Map|Set)<.*>\s+([A-Za-z_]+);", re.M)
HANDLE_MAP = re.compile(r"^\s*(?:extern\s+)?EncounterStateMap<.*>\s+([A-Za-z_]+);", re.M)
SHARED = re.compile(r"^\s*(?:extern\s+)?EncounterSharedStateMap<.*>\s+([A-Za-z_]+);", re.M)
# Containers that other players' actions read or erase (passers, main tank, core handlers).
CROSS_PLAYER = {"intendedLineup", "lastCoreInInventoryTime", "shahrazTankStep"}


def raid_files(raid, suffix):
    return sorted((ROOT / "src/Ai/Raid" / raid).rglob("*" + suffix))


def all_text(suffix):
    return "".join(p.read_text(encoding="utf-8") for raid in RAIDS for p in raid_files(raid, suffix))


class RaidStateLockingTests(unittest.TestCase):
    def test_no_unlocked_namespace_scope_containers(self):
        for raid in RAIDS:
            for path in raid_files(raid, ".h") + raid_files(raid, ".cpp"):
                source = path.read_text(encoding="utf-8")
                # Only unindented (namespace-scope) declarations are shared across instances.
                shared = [m.group(0) for m in GLOBAL.finditer(source) if not m.group(0).startswith((" ", "\t"))]
                with self.subTest(path=str(path.relative_to(ROOT))):
                    self.assertEqual(shared, [])

    def test_every_locked_declaration_has_a_definition(self):
        for raid in RAIDS:
            headers = "".join(p.read_text(encoding="utf-8") for p in raid_files(raid, ".h"))
            sources = "".join(p.read_text(encoding="utf-8") for p in raid_files(raid, ".cpp"))
            declared = set(LOCKED.findall(headers))
            defined = set(LOCKED.findall(sources))
            with self.subTest(raid=raid):
                self.assertTrue(declared)
                self.assertEqual(declared, defined)

    def test_cross_player_containers_never_hand_out_handles(self):
        declared = set(SHARED.findall(all_text(".h")))
        self.assertEqual(declared, CROSS_PLAYER)

    def test_handle_maps_are_only_keyed_by_owned_keys(self):
        # Handles are safe only for keys owned by the executing map thread: an instance id or the
        # executing bot's own GUID. Any other player's GUID must go through EncounterSharedStateMap.
        names = set(HANDLE_MAP.findall(all_text(".h")))
        access = re.compile(r"\b(" + "|".join(sorted(names)) + r")\b\s*(?:\.\s*\w+\s*\(|\[)\s*([^;\])]*)")
        offenders = []
        for raid in RAIDS:
            for path in raid_files(raid, ".cpp"):
                for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                    for match in access.finditer(line):
                        key = match.group(2)
                        for owner in re.findall(r"(\w+)->GetGUID\(\)", key):
                            if owner != "bot":
                                offenders.append(f"{path.name}:{number} {match.group(1)}[{key.strip()}]")
        self.assertEqual(offenders, [])

    def test_spell_listeners_check_the_raid_map_before_writing(self):
        for path, map_id in (("Hyjal/Util/HyjalScripts.cpp", "HYJAL_MAP_ID"),
                             ("Mag/Util/MagScripts.cpp", "MAG_MAP_ID"),
                             ("TK/Util/TKScripts.cpp", "TK_MAP_ID")):
            source = (ROOT / "src/Ai/Raid" / path).read_text(encoding="utf-8")
            with self.subTest(path=path):
                self.assertIn(f"caster->GetMapId() != {map_id}", source)
                self.assertLess(source.index(f"caster->GetMapId() != {map_id}"), source.index("GetInstanceId()"))

    def test_cleanup_triggers_run_only_inside_the_raid_map(self):
        for path, guard in (("SSC/SSCTriggers.cpp", "bot->GetMapId() == SSC_MAP_ID"),
                            ("BT/BTTriggers.cpp", "bot->GetMapId() == BLACK_TEMPLE_MAP_ID"),
                            ("Kara/KaraTriggers.cpp", "bot->GetMapId() == KARA_MAP_ID")):
            with self.subTest(path=path):
                self.assertIn(guard, (ROOT / "src/Ai/Raid" / path).read_text(encoding="utf-8"))
        # Hyjal's reset trigger is also active outside Hyjal (!IsEncounterInProgress is true off-map), so a
        # bot there erases archimondeAirBurstTargets[0]. That is safe only because nothing writes key 0:
        # the Air Burst listener checks HYJAL_MAP_ID (test_spell_listeners_check_the_raid_map_before_writing).
        hyjal = (ROOT / "src/Ai/Raid/Hyjal/HyjalActions.cpp").read_text(encoding="utf-8")
        reset = hyjal.split("bool HyjalResetEncounterStatesAction::Execute", 1)[1].split("\n}", 1)[0]
        self.assertIn("archimondeAirBurstTargets.erase(bot->GetInstanceId())", reset)
        self.assertEqual(reset.count("archimondeAirBurstTargets"), 1)
        for raid, path in (("Mag", "Mag/MagTriggers.cpp"), ("TK", "TK/TKTriggers.cpp"),
                           ("ZA", "ZA/ZATriggers.cpp")):
            source = (ROOT / "src/Ai/Raid" / path).read_text(encoding="utf-8")
            trigger = source.split("NoEncounterInProgressTrigger::IsActive()", 1)[1].split("\n}", 1)[0]
            with self.subTest(raid=raid):
                self.assertIn("IsMechanicTrackerBot(bot,", trigger)
        helpers = (ROOT / "src/Util/EncounterHelpers.cpp").read_text(encoding="utf-8")
        tracker = helpers.split("bool IsMechanicTrackerBot", 1)[1].split("\n}", 1)[0]
        self.assertIn("member->GetMapId() != mapId", tracker)

    def test_containers_never_expose_std_iterators(self):
        header = (ROOT / "src/Util/EncounterStateMap.h").read_text(encoding="utf-8")
        self.assertNotIn("begin()", header)
        self.assertNotIn("std::function", header)
        self.assertNotIn("Container::iterator", header)
        shared = header.split("class EncounterSharedStateMap", 1)[1].split("class EncounterStateSet", 1)[0]
        self.assertNotIn("Handle", shared)
        self.assertNotIn("operator[]", shared)
        self.assertNotIn("&>", shared)


if __name__ == "__main__":
    unittest.main(verbosity=2)
