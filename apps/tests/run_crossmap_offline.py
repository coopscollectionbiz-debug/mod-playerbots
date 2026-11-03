"""Build/run standalone tests with MSVC ASan; no server or database is started.

The inbox header is production code. ModelBot tests dispatch scenarios, not the entire AI.
The remote operation class is extracted verbatim and tested with a stub manager/operation base.
"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from test_owner_thread_dispatch import function

ROOT = Path(__file__).resolve().parents[2]


def vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    install = subprocess.run([str(vswhere), "-latest", "-property", "installationPath"],
                             capture_output=True, text=True, check=True).stdout.strip()
    return Path(install) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", help="Run one named harness instead of the complete suite")
    args = parser.parse_args()
    tests = ["owner_thread_inbox_test", "owner_thread_dispatch_test"]
    # Run existing local suites when present without requiring unrelated untracked test files.
    tests += [name for name in ("citylife_policy_test", "citylife_request_queue_test", "encounter_state_map_test")
              if (ROOT / "apps/tests" / (name + ".cpp")).is_file()]
    with tempfile.TemporaryDirectory(prefix="crossmap-offline-") as directory:
        temp = Path(directory)
        remote = (ROOT / "src/Bot/Cmd/PlayerbotCommandServer.cpp").read_text(encoding="utf-8")
        start = remote.index("    class RemoteCommandOperation")
        end = remote.index("\n    std::string HandleRemoteCommandOnWorldThread", start)
        source = '''#include <future>
#include <chrono>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <iostream>
class PlayerbotOperation {
public:
    virtual ~PlayerbotOperation() = default;
    virtual bool Execute() = 0;
    virtual std::string GetName() const = 0;
};
struct RandomPlayerbotMgr {
    static RandomPlayerbotMgr& instance() { static RandomPlayerbotMgr mgr; return mgr; }
    std::string HandleRemoteCommand(std::string request) {
        if (request == "throw") throw std::runtime_error("expected");
        return request + " reply";
    }
};
''' + remote[start:end] + '''
struct PlayerbotWorldThreadProcessor {
    bool reject = false;
    bool hold = false;
    std::unique_ptr<PlayerbotOperation> pending;
    static PlayerbotWorldThreadProcessor& instance() { static PlayerbotWorldThreadProcessor queue; return queue; }
    bool QueueOperation(std::unique_ptr<PlayerbotOperation> operation) {
        if (reject) return false;
        if (hold) { pending = std::move(operation); return true; }
        std::thread world([operation = std::move(operation)] { operation->Execute(); });
        world.join();
        return true;
    }
};
''' + remote[end:remote.index('\n}\n\nvoid session(', end)] + '''
int main() {
    auto reply = std::make_shared<std::promise<std::string>>();
    auto result = reply->get_future();
    RemoteCommandOperation op("request", reply);
    reply.reset();
    std::thread world([&] { op.Execute(); });
    if (result.get() != "request reply") return 1;
    world.join();
    auto abandoned = std::make_shared<std::promise<std::string>>();
    { auto disconnected = abandoned->get_future(); }
    RemoteCommandOperation late("late", abandoned);
    if (!late.Execute()) return 2;
    auto error = std::make_shared<std::promise<std::string>>();
    auto errorResult = error->get_future();
    RemoteCommandOperation failure("throw", error);
    if (failure.Execute() || errorResult.get() != "error") return 3;
    if (HandleRemoteCommandOnWorldThread("dispatch") != "dispatch reply") return 4;
    auto& queue = PlayerbotWorldThreadProcessor::instance();
    queue.reject = true;
    if (HandleRemoteCommandOnWorldThread("busy") != "busy") return 5;
    queue.reject = false;
    queue.hold = true;
    if (HandleRemoteCommandOnWorldThread("late") != "timeout") return 6;
    if (!queue.pending->Execute()) return 7;
    std::cout << "PASS: production remote dispatch, reply, busy, timeout, abandoned waiter, exception\\n";
}
'''
        (temp / "remote_operation_test.cpp").write_text(source, encoding="utf-8")
        tests.append("remote_operation_test")
        # Execute the real world-batch function against a registry that deletes/replaces its AI.
        lifetime = '''#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include <iostream>
using uint64 = std::uint64_t;
using ObjectGuid = int;
struct Player { ObjectGuid GetGUID() const { return 1; } };
struct PlayerbotAI;
std::unique_ptr<PlayerbotAI> live;
Player player;
struct ObjectAccessor {
    static Player* FindConnectedPlayer(ObjectGuid) { return live ? &player : nullptr; }
};
#define GET_PLAYERBOT_AI(player) live.get()
int executions = 0;
struct PlayerbotAI {
    struct OwnerThreadWork { bool logout; bool replace; };
    Player* bot = &player;
    uint64 ownerThreadToken = 1;
    std::atomic<unsigned> worldThreadBatches{1};
    void ExecuteOwnerThreadWork(OwnerThreadWork& work);
    void ExecuteWorldThreadBatch(uint64 token, std::vector<OwnerThreadWork>& work);
};
void PlayerbotAI::ExecuteOwnerThreadWork(OwnerThreadWork& work) {
    ++executions;
    bool replace = work.replace;
    if (work.logout) live.reset();
    if (replace) { live = std::make_unique<PlayerbotAI>(); live->ownerThreadToken = 2; }
}
void PlayerbotAI::ExecuteWorldThreadBatch(uint64 token, std::vector<OwnerThreadWork>& work)
''' + function("void PlayerbotAI::ExecuteWorldThreadBatch") + '''
int main() {
    for (bool replace : {false, true}) {
        executions = 0;
        live = std::make_unique<PlayerbotAI>();
        std::vector<PlayerbotAI::OwnerThreadWork> work{{true, replace}, {false, false}};
        live->ExecuteWorldThreadBatch(1, work);
        if (executions != 1) return 1;
        if (replace && live->worldThreadBatches != 1) return 2;
        live.reset();
    }
    std::cout << "PASS: production world batch stops after AI deletion/replacement\\n";
}
'''
        (temp / "production_lifetime_test.cpp").write_text(lifetime, encoding="utf-8")
        tests.append("production_lifetime_test")
        map_source = (ROOT.parents[1] / "src/server/game/Maps/Map.cpp").read_text(encoding="utf-8")
        marker = '''#include <thread>
#include <array>
#include <atomic>
#include <iostream>
struct Map {
    static Map const* GetUpdatingMapOfCurrentThread();
    struct UpdateScope {
        Map const* _previous;
        explicit UpdateScope(Map const* map);
        ~UpdateScope();
    };
};
''' + map_source[map_source.index("namespace\n{\n    thread_local Map const*"):
                map_source.index("ZoneDynamicInfo::ZoneDynamicInfo")] + '''
int main() {
    std::atomic<int> errors{0};
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        Map first, second;
        for (int i = 0; i < 10000; ++i) {
            if (Map::GetUpdatingMapOfCurrentThread()) ++errors;
            {
                Map::UpdateScope outer(&first);
                if (Map::GetUpdatingMapOfCurrentThread() != &first) ++errors;
                { Map::UpdateScope inner(&second);
                  if (Map::GetUpdatingMapOfCurrentThread() != &second) ++errors; }
                if (Map::GetUpdatingMapOfCurrentThread() != &first) ++errors;
            }
            if (Map::GetUpdatingMapOfCurrentThread()) ++errors;
        }
    });
    for (auto& worker : workers) worker.join();
    if (errors) return 1;
    std::cout << "PASS: production map TLS marker, 8 threads, 80000 nested scopes\\n";
}
'''
        (temp / "production_map_scope_test.cpp").write_text(marker, encoding="utf-8")
        tests.append("production_map_scope_test")
        drain = (ROOT / "apps/tests/owner_thread_drain_fixture.h").read_text(encoding="utf-8")
        drain += "\nbool PlayerbotAI::RunsOnOwnerThread() const\n" + function("bool PlayerbotAI::RunsOnOwnerThread")
        drain += "\nvoid PlayerbotAI::DrainOwnerThreadWork()\n" + function("void PlayerbotAI::DrainOwnerThreadWork")
        (temp / "production_drain_test.cpp").write_text(drain, encoding="utf-8")
        tests.append("production_drain_test")
        if args.only:
            if args.only not in tests:
                parser.error("unknown harness: " + args.only)
            tests = [args.only]
        environment = os.environ.copy()
        environment["ASAN_OPTIONS"] = "halt_on_error=1:detect_leaks=0"
        for name in tests:
            source = (temp / (name + ".cpp") if name.startswith(("remote_", "production_"))
                      else ROOT / "apps/tests" / (name + ".cpp"))
            includes = [ROOT / "src/Bot", ROOT / "src/Util", ROOT / "src/Ai/World/Rpg"]
            flags = ' '.join('/I"%s"' % path for path in includes)
            script = temp / "run.bat"
            exe = temp / (name + ".exe")
            script.write_text('@call "%s" >nul\n@cl /nologo /std:c++20 /EHsc /W4 /WX /Od /Zi '
                              '/fsanitize=address %s /Fo"%s" /Fe"%s" /Fd"%s" "%s"\n'
                              '@if errorlevel 1 exit /b 1\n@"%s"\n' %
                              (vcvars(), flags, temp / (name + ".obj"), exe,
                               temp / (name + ".pdb"), source, exe))
            result = subprocess.run(["cmd", "/d", "/c", str(script)], capture_output=True, text=True,
                                    cwd=temp, env=environment)
            print(name, result.stdout, result.stderr, flush=True)
            if result.returncode:
                return result.returncode
        if "encounter_state_map_test" not in tests:
            return 0
        # Existing intentional use-after-free must be caught, proving ASan instrumentation is active.
        script.write_text('@call "%s" >nul\n@"%s" --negative-control\n' %
                          (vcvars(), temp / "encounter_state_map_test.exe"))
        negative = subprocess.run(["cmd", "/d", "/c", str(script)],
                                  capture_output=True, text=True, cwd=temp, env=environment)
        if negative.returncode == 0 or "heap-use-after-free" not in negative.stdout + negative.stderr:
            print("FAIL: ASan negative control", negative.stdout, negative.stderr)
            return 1
        print("PASS: ASan negative control detected heap-use-after-free")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
