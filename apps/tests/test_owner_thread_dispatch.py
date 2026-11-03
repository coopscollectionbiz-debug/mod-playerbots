"""Offline source contracts supplement the production inbox concurrency harness."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT.parents[1]
AI = (ROOT / "src/Bot/PlayerbotAI.cpp").read_text(encoding="utf-8")


def function(name):
    start = AI.index(name)
    start = AI.index("{", start)
    depth = 1
    end = start + 1
    while depth:
        depth += (AI[end] == "{") - (AI[end] == "}")
        end += 1
    return AI[start:end]


class DispatchContracts(unittest.TestCase):
    def test_both_command_entries_dispatch(self):
        ref = function("void PlayerbotAI::HandleCommand(uint32 type, std::string const&")
        self.assertIn("&fromPlayer", ref)
        ptr = function("void PlayerbotAI::HandleCommand(uint32 type, std::string const text, Player*")
        self.assertIn("RunsOnOwnerThread()", ptr)
        self.assertIn("work.sender = fromPlayer->GetGUID()", ptr)
        self.assertIn("QueueForOwnerThread", ptr)
        self.assertIn("!RunsOnOwnerThread() || IsLifecycleCommand(text)", ptr)
        update = function("void PlayerbotAI::UpdateAI(uint32")
        self.assertLess(update.index("DrainOwnerThreadWork()"), update.index("if (bot->GetSession()->IsLoggingOut())"))

    def test_execution_security_and_batch1(self):
        body = function("void PlayerbotAI::HandleCommandOnOwnerThread")
        self.assertLess(body.index("PLAYERBOT_SECURITY_ALLOW_ALL"), body.index('filtered.substr(0, 6) == "debug "'))
        self.assertIn("CheckLevelFor(PLAYERBOT_SECURITY_INVITE", body)
        self.assertIn("requiredLevel, type", body)
        queued = function("void PlayerbotAI::HandleCommands()")
        self.assertIn("ObjectAccessor::FindPlayer(it->GetOwnerGuid())", queued)
        self.assertIn("CheckLevelFor(it->GetRequiredLevel()", queued)

    def test_packet_copies_and_owner_handlers(self):
        for name in ("HandleBotOutgoingPacket", "HandleMasterIncomingPacket", "HandleMasterOutgoingPacket"):
            body = function("void PlayerbotAI::" + name + "(WorldPacket const&")
            self.assertIn("RunsOnOwnerThread()", body)
            self.assertIn("work.packet = packet", body)
            self.assertIn("QueueForOwnerThread", body)

    def test_world_batch_reresolves_after_each_item(self):
        body = function("void PlayerbotAI::ExecuteWorldThreadBatch")
        loop = body[body.index("for (OwnerThreadWork& item") :]
        self.assertIn("ObjectAccessor::FindConnectedPlayer(guid)", loop)
        self.assertIn("ai->ownerThreadToken != token", loop)
        self.assertIn("ai->ExecuteOwnerThreadWork(item)", loop)
        self.assertNotIn("\n        ExecuteOwnerThreadWork(item)", loop)
        compound = function("void PlayerbotAI::HandleCommandOnOwnerThread")
        self.assertIn("ai->ownerThreadToken != token || !sender", compound)

    def test_drain_order_teleport_and_overflow(self):
        drain = function("void PlayerbotAI::DrainOwnerThreadWork")
        self.assertIn("ownerThreadDraining = true", drain)
        self.assertIn("~DrainGuard()", drain)
        self.assertIn("!bot->IsBeingTeleported()", drain)
        self.assertIn("IsLifecycleCommand(work[i].text)", drain)
        self.assertIn("std::make_move_iterator(work.begin() + i)", drain)
        self.assertIn("owner-thread inbox full", AI)
        self.assertIn("world queue rejected owner-thread batch", AI)

    def test_map_scope_all_update_paths(self):
        for name in ("MapMgr", "MapUpdater", "MapInstanced"):
            text = (CORE / f"src/server/game/Maps/{name}.cpp").read_text(encoding="utf-8")
            self.assertIn("Map::UpdateScope scope", text)
        text = (CORE / "src/server/game/Maps/Map.cpp").read_text(encoding="utf-8")
        self.assertIn("thread_local Map const* tUpdatingMap", text)
        self.assertIn("tUpdatingMap = _previous", text)

    def test_remote_marshals_copied_request(self):
        text = (ROOT / "src/Bot/Cmd/PlayerbotCommandServer.cpp").read_text(encoding="utf-8")
        self.assertIn("std::make_unique<RemoteCommandOperation>(request, response)", text)
        self.assertIn("std::shared_ptr<std::promise<std::string>> _response", text)
        self.assertIn('return "busy"', text)
        self.assertIn('return "timeout"', text)
        self.assertIn('_response->set_value("error")', text)
        session = text[text.index("void session(") : text.index("void server(")]
        self.assertNotIn("RandomPlayerbotMgr", session)


if __name__ == "__main__":
    unittest.main()
