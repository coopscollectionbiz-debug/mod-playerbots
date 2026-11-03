"""Offline integration contracts; these do not execute the server's object/movement layer."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT.parents[1]


class CityLifeWiringTests(unittest.TestCase):
    def read(self, path):
        return (ROOT / path).read_text(encoding="utf-8")

    def natural_action(self):
        source = self.read("src/Ai/World/Rpg/Action/NewRpgAction.cpp")
        return source.split("bool NewRpgCityLifeAction::ExecuteNatural()", 1)[1].split(
            "bool NewRpgCityLifeAction::Execute(Event", 1)[0]

    def test_gameplay_protections(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        eligibility = manager.split("bool CityLifeMgr::Eligible", 1)[1].split("bool CityLifeMgr::Hidden", 1)[0]
        for guard in ("GetGroup()", "IsInCombat()", "isDead()", "InBattleground()",
                      "InBattlegroundQueue()", "IsBeingTeleported()", "IsInFlight()",
                      "IsDungeon()", "HasGameClientMaster()", "LFG_STATE_NONE", "GetSocialPauseUntil()"):
            with self.subTest(guard=guard):
                self.assertIn(guard, eligibility)
        action = self.natural_action()
        for guard in ("GetGroup()", "HasGameClientMaster()", "IsInCombat()"):
            self.assertIn(guard, action)
        # Dungeon Finder release moved off the map thread (LFGMgr is world-thread data).
        self.assertNotIn("sLFGMgr", action)
        refresh = manager.split("void CityLifeMgr::RefreshPopulation", 1)[1].split("void CityLifeMgr::Admit", 1)[0]
        self.assertIn("LFG_STATE_NONE", refresh)

    def test_no_random_recovery_teleport_or_transactions_in_natural_action(self):
        action = self.natural_action()
        for operation in ("TeleportTo(", "MoveRandomNear(", "MoveFarTo(",
                          "InteractWithNpcOrGameObjectForQuest(", "ChooseNpcOrGameObjectToInteract("):
            self.assertNotIn(operation, action)
        self.assertIn("CityLifePolicy::TargetInRange", action)
        self.assertIn("CityLifePolicy::CoolingDown", action)

    def test_movement_wait_precedes_new_destination(self):
        action = self.natural_action()
        self.assertLess(action.index("bot->isMoving()"), action.index("AI_VALUE(GuidVector"))
        self.assertLess(action.index("IsWaitingForLastMove"), action.index("PathGenerator path"))
        self.assertLess(action.index("else if (data->MoveStart)"), action.index("PathGenerator path"))
        stationary = action.split("IsWaitingForLastMove", 1)[1].split("if (CityLifePolicy::CoolingDown", 1)[0]
        self.assertIn("return true;", stationary)  # consumes the turn: no lower-priority grind fallback

    def test_world_ownership_and_bounded_work(self):
        hooks = self.read("src/Script/Playerbots.cpp")
        self.assertIn("CityLifeMgr::Instance().Update(diff);", hooks)
        world = (CORE / "src/server/game/World/World.cpp").read_text(encoding="utf-8")
        self.assertLess(world.index("sMapMgr->Update(diff)"), world.index("sScriptMgr->OnWorldUpdate(diff)"))
        maps = (CORE / "src/server/game/Maps/MapMgr.cpp").read_text(encoding="utf-8")
        self.assertIn("m_updater.wait();", maps)
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        self.assertIn("CityLifePolicy::CandidateBudget", manager)
        self.assertIn("CityLifePolicy::PlacementBudget", manager)
        self.assertIn("CityLifePolicy::AdmissionInterval", manager)
        self.assertIn("_requests.exchange", manager)
        self.assertEqual(manager.count("bot->TeleportTo("), 1)
        self.assertNotIn("Database.Query", manager)

    def test_reservations_and_invisible_geometry_revalidation(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        self.assertIn("assigned[index * 2 + bot->GetTeamId()]", manager)
        self.assertLess(manager.index("state.Managed = true"), manager.index("bot->TeleportTo("))
        self.assertIn("ArrivalTimeout", manager)
        self.assertIn("HaveAtClient(source)", manager)
        self.assertIn("GetViewpoint()", manager)
        self.assertIn("CanReachPositionAndGetValidCoords", manager)
        self.assertIn("IsInWater", manager)

    def test_feature_and_diagnostics_default_off(self):
        config = self.read("src/PlayerbotAIConfig.cpp")
        self.assertIn('"AiPlayerbot.CityLife.NaturalActivation", false', config)
        self.assertIn('"AiPlayerbot.CityLife.Diagnostics", false', config)
        dist = self.read("conf/playerbots.conf.dist")
        self.assertIn("AiPlayerbot.CityLife.NaturalActivation = 0", dist)
        self.assertIn("AiPlayerbot.CityLife.Diagnostics = 0", dist)

    def test_failed_arrivals_consume_budget_and_cannot_repeat_immediately(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        transfer = manager.index("bot->TeleportTo(")
        self.assertLess(manager.index("work.Attempt(initial);"), transfer)
        self.assertIn("_arrivalFailures.count(guid.GetRawValue())", manager)
        self.assertIn("_arrivalFailures.size() >= CityLifePolicy::FailureCacheLimit", manager)
        self.assertIn("_placementFailures.count", manager)
        self.assertIn("_arrivalFailures[guid.GetRawValue()] = started;", manager[transfer:])
        self.assertIn("city->Arrived && !bot->IsBeingTeleported()", manager)

    def test_empty_cities_only_suspend_and_maintain_leases(self):
        action = self.natural_action()
        suspension = action.split("if (!CityLifeMgr::Instance().IsObserved", 1)[1].split(
            "if (botAI->GetSocialPauseUntil()", 1)[0]
        self.assertIn("Schedule.Suspended = true", suspension)
        self.assertIn("return true;", suspension)
        self.assertNotIn("Move", suspension)
        ai = self.read("src/Bot/PlayerbotAI.cpp")
        self.assertIn("activityType == ALL_ACTIVITY && !allowed", ai)
        self.assertIn("city->Schedule.Suspended = true;", ai)

    def test_two_stages_coalesce_entry_and_stop_on_departure(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        self.assertIn("CityLifePolicy::ActivationInterval", manager)
        self.assertIn("_activation[key].Observe((_activated & (1u << key)) != 0, started)", manager)
        self.assertIn("activation.Initial(started, sPlayerbotAIConfig.CityLifeActivationDurationMs)", manager)
        self.assertIn("work.Allows(initial, maintenanceDue", manager)
        self.assertIn("CityLifePolicy::NextCity(_cursor)", manager)
        self.assertIn("if (_activated)", manager)
        config = self.read("src/PlayerbotAIConfig.cpp")
        for option in ("ActivationBudget", "ActivationDurationMs", "MaintenanceBudget", "ActivationMaxTickMs"):
            self.assertIn(f'"AiPlayerbot.CityLife.{option}"', config)
        self.assertIn('"AiPlayerbot.CityLife.MaintenanceBudget", legacyCityLifeBudget', config)

    def test_fast_slices_reuse_guid_snapshot_and_consume_candidates_once(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        process = manager.split("void CityLifeMgr::Process()", 1)[1].split(
            "void CityLifeMgr::RefreshPopulation", 1)[0]
        admission = manager.split("void CityLifeMgr::Admit", 1)[1]
        self.assertEqual(manager.count("GetPlayerBotsBegin()"), 1)
        self.assertNotIn("GetPlayerBotsBegin", admission)
        self.assertIn("if (refreshed)", process)
        self.assertIn("if (!previous && _activated)", process)
        self.assertIn("candidates.pop_back();", admission)
        self.assertIn("ObjectAccessor::FindPlayer(guid)", admission)
        header = self.read("src/Ai/World/Rpg/CityLifeMgr.h")
        self.assertIn("std::array<std::vector<ObjectGuid>", header)
        self.assertNotIn("std::vector<Player*> _", header)
        self.assertIn("!bot->IsInWorld() && !city", manager)  # in-transit CityLife is not discarded
        self.assertIn("--_assigned[key];", admission)  # failed transfer releases cached reservation

    def test_nearby_ranking_and_real_time_pressure_preserve_hidden_placement(self):
        manager = self.read("src/Ai/World/Rpg/CityLifeMgr.cpp")
        for guard in ("CityLifePolicy::PlacementScore", "CityLifePolicy::InForwardCone",
                      "CityLifePolicy::DestinationBudget", "CityLifePolicy::SliceTimeBudget",
                      "sWorldUpdateTime.GetLastUpdateTime()", "CityLifePolicy::PressureBudget"):
            self.assertIn(guard, manager)
        self.assertNotIn("EnsureGridLoaded", manager)
        self.assertNotIn("LoadGrid(", manager)
        self.assertNotIn("AddPlayerBot(", manager)
        self.assertNotIn("OnBotLogin(", manager)
        self.assertIn("GetCreatureBySpawnIdStore()", manager)


if __name__ == "__main__":
    unittest.main(verbosity=2)
