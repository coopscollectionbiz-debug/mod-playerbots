"""Compile-only check for CityLife translation units (MSVC, Windows).

Uses the flags from an existing CMake-generated modules.vcxproj and writes objects only to a
temporary directory; the build tree is read, never modified. Exit code 0 means all units compiled.

    python apps/tests/compile_citylife.py [--build-dir C:\\Build] [--config RelWithDebInfo]
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
UNITS = (
    "src/Ai/World/Rpg/CityLifeMgr.cpp",
    "src/Ai/World/Rpg/NewRpgInfo.cpp",
    "src/Ai/World/Rpg/Action/NewRpgAction.cpp",
    "src/Ai/World/Rpg/Action/NewRpgBaseAction.cpp",
    "src/Mgr/Travel/TravelMgr.cpp",
    "src/Bot/PlayerbotAI.cpp",
    "src/Bot/RandomPlayerbotMgr.cpp",
    "src/PlayerbotAIConfig.cpp",
    "src/Script/Playerbots.cpp",
)


def compile_flags(build_dir, config):
    project = (Path(build_dir) / "modules" / "modules.vcxproj").read_text(encoding="utf-8")
    start = project.index("'%s|x64'\">" % config)
    block = project[start:project.index("</ClCompile>", start)]

    def tag(name):
        match = re.search(r"<%s>(.*?)</%s>" % (name, name), block, re.S)
        return match.group(1).strip() if match else ""

    def items(name):
        return [i for i in tag(name).split(";") if i and not i.startswith("%(")]

    flags = ["/nologo", "/c", "/std:c++20", "/EHsc", "/MD", "/Od", "/Zc:preprocessor", "/W3",
             "/we4263", "/we4264", "/external:W0"]
    flags += ["/wd" + w for w in items("DisableSpecificWarnings")]
    flags.append(tag("AdditionalOptions").replace("%(AdditionalOptions)", "").strip())
    flags += ['/D"%s"' % d.replace('"', '\\"') for d in items("PreprocessorDefinitions")]
    flags += ['/I"%s"' % i for i in items("AdditionalIncludeDirectories")]
    return flags


def vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    install = subprocess.run([str(vswhere), "-latest", "-property", "installationPath"],
                             capture_output=True, text=True, check=True).stdout.strip()
    return Path(install) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default=r"C:\Build")
    parser.add_argument("--config", default="RelWithDebInfo")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="citylife-compile-") as temp:
        temp = Path(temp)
        rsp = temp / "flags.rsp"
        rsp.write_text("\n".join(compile_flags(args.build_dir, args.config)), encoding="utf-8")
        failed = []
        for unit in UNITS:
            source = ROOT / unit
            script = temp / "compile.bat"
            script.write_text('@call "%s" >nul\r\n@cl @"%s" /Fo"%s\\\\" "%s"\r\n' % (vcvars(), rsp, temp, source))
            result = subprocess.run(["cmd", "/d", "/c", str(script)], capture_output=True, text=True)
            errors = [line for line in result.stdout.splitlines() if " error " in line or "fatal error" in line]
            print("%-50s %s" % (unit, "ok" if result.returncode == 0 else "FAILED"))
            for line in errors:
                print("    " + line.strip())
            if result.returncode != 0:
                failed.append(unit)
    print("%d/%d units compiled" % (len(UNITS) - len(failed), len(UNITS)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
