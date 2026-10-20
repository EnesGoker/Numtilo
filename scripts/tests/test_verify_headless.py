"""Native integration controls for the headless acceptance oracle.

Run: python3 -m unittest discover -s scripts/tests -v
Each control configures a new real CMake fixture. The header control builds
objects before proving that a forbidden header hidden behind -isystem is caught.
"""

import json
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BACKENDS = (
    "game_core", "app_logic", "app_paths", "asset_resolver", "asset_locations",
    "user_data_paths", "app_settings", "profile_migration", "runtime_check",
)


@unittest.skipUnless(sys.platform in ("darwin", "linux") and shutil.which("cmake")
                     and shutil.which("c++") and (shutil.which("ninja") or shutil.which("make")),
                     "Native GNU/Clang CMake fixture toolchain is required")
class HeadlessIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="headless-controls-")
        self.directory = Path(self.temporary.name)
        self.source = self.directory / "source with spaces"
        self.source.mkdir()
        self.configure = ["cmake_minimum_required(VERSION 3.24)",
                          "project(headless_control LANGUAGES CXX)", "enable_testing()"]
        for name in BACKENDS:
            (self.source / (name + ".cpp")).write_text(
                "#include <cstddef>\nint " + name + "_value() { return sizeof(int); }\n", encoding="utf-8")
            self.configure.append("add_library(" + name + " STATIC " + name + ".cpp)")

    def tearDown(self):
        self.temporary.cleanup()

    def run_fixture(self, extra_arguments=(), generator=None):
        (self.source / "CMakeLists.txt").write_text("\n".join(self.configure) + "\n", encoding="utf-8")
        output = self.directory / "reports"
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/verify_headless.py"), "--source", str(self.source),
             "--output", str(output), "--config", "Release", "--no-tests", "--generator",
             generator or ("Ninja" if shutil.which("ninja") else "Unix Makefiles"),
             *extra_arguments],
            capture_output=True, text=True, timeout=120)
        reports = list(output.glob("*/report.json"))
        self.assertEqual(len(reports), 1, result.stdout + result.stderr)
        return result, json.loads(reports[0].read_text(encoding="utf-8"))

    def test_clean_backend_positive_control(self):
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(report["success"])
        stages = {entry["name"]: entry for entry in report["stages"]}
        self.assertEqual(stages["actual-headers"]["details"]["translation_units"], 9)
        self.assertFalse(stages["runtime-dependencies"]["details"]["applicable"])

    @unittest.skipUnless(shutil.which("make"), "Makefiles object-directory fixture requires make")
    def test_nested_dependency_objects_use_the_actual_compiler_output(self):
        child = self.source / "nested/dependency"
        child.mkdir(parents=True)
        (child / "probe.cpp").write_text(
            "#include <cstddef>\nint nested_probe() { return sizeof(int); }\n", encoding="utf-8")
        (child / "CMakeLists.txt").write_text(
            "add_library(native_nested_helper STATIC probe.cpp)\n", encoding="utf-8")
        self.configure += [
            'add_subdirectory(nested/dependency "${CMAKE_CURRENT_BINARY_DIR}/_deps/native-probe-build")',
            "target_link_libraries(game_core PUBLIC native_nested_helper)"]
        result, report = self.run_fixture(generator="Unix Makefiles")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        stages = {entry["name"]: entry for entry in report["stages"]}
        self.assertEqual(stages["actual-headers"]["details"]["translation_units"], 10)
        database = json.loads((Path(report["build"]) / "compile_commands.json").read_text(
            encoding="utf-8"))
        entry = next(item for item in database if Path(item["file"]).name == "probe.cpp")
        cwd = Path(entry["directory"])
        self.assertNotEqual(cwd.resolve(), Path(report["build"]).resolve())
        arguments = entry.get("arguments") or shlex.split(entry["command"])
        actual_output = Path(arguments[arguments.index("-o") + 1])
        actual_output = actual_output if actual_output.is_absolute() else cwd / actual_output
        self.assertTrue(actual_output.is_file())
        records = json.loads(Path(stages["actual-headers"]["details"]["index"]).read_text(
            encoding="utf-8"))
        audited = next(item for item in records if Path(item["source"]).name == "probe.cpp")
        self.assertEqual(Path(audited["object"]).resolve(), actual_output.resolve())

    def add_pure_ui_contracts(self):
        # Use the real contracts and their core headers rather than a neutral
        # stand-in, so future forbidden transitive includes also fail this control.
        for name in ("app/ui/Scene.hpp", "app/ui/GameplayOutcome.hpp", "core/Board.hpp",
                     "core/Game.hpp", "core/GameState.hpp", "core/Tile.hpp"):
            destination = self.source / "src" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "src" / name, destination)
        source = self.source / "game_core.cpp"
        source.write_text('#include "app/ui/GameplayOutcome.hpp"\n' + source.read_text(
            encoding="utf-8"), encoding="utf-8")
        self.configure += [
            'target_include_directories(game_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")',
            "target_compile_features(game_core PRIVATE cxx_std_20)"]

    def test_real_pure_ui_contract_headers_pass_the_full_dependency_scan(self):
        self.add_pure_ui_contracts()
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        stage = next(entry for entry in report["stages"] if entry["name"] == "actual-headers")
        records = json.loads(Path(stage["details"]["index"]).read_text(encoding="utf-8"))
        game = next(item for item in records if Path(item["source"]).name == "game_core.cpp")
        source_root = self.source.resolve()
        headers = {Path(value).resolve().relative_to(source_root).as_posix()
                   for value in game["headers"]
                   if Path(value).resolve().is_relative_to(source_root)}
        self.assertIn("src/app/ui/Scene.hpp", headers)
        self.assertIn("src/app/ui/GameplayOutcome.hpp", headers)
        self.assertIn("src/core/Game.hpp", headers)

    def test_pure_ui_contract_exception_does_not_hide_transitive_sfml_headers(self):
        self.add_pure_ui_contracts()
        include = self.source / "external/include/SFML"
        include.mkdir(parents=True)
        (include / "Probe.hpp").write_text("#pragma once\n", encoding="utf-8")
        scene = self.source / "src/app/ui/Scene.hpp"
        scene.write_text('#include <SFML/Probe.hpp>\n' + scene.read_text(encoding="utf-8"),
                         encoding="utf-8")
        self.configure.append(
            'target_include_directories(game_core SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/include")')
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertTrue(next(entry for entry in report["stages"] if entry["name"] == "build")["success"])
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        self.assertIn("SFML/Probe.hpp", report["error"])

    def test_pure_ui_contract_exception_does_not_allow_neighboring_runtime_headers(self):
        self.add_pure_ui_contracts()
        runtime = self.source / "src/app/ui/PlayingScene.hpp"
        runtime.write_text("#pragma once\n", encoding="utf-8")
        scene = self.source / "src/app/ui/Scene.hpp"
        scene.write_text('#include "app/ui/PlayingScene.hpp"\n' + scene.read_text(
            encoding="utf-8"), encoding="utf-8")
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertTrue(next(entry for entry in report["stages"] if entry["name"] == "build")["success"])
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        self.assertIn("src/app/ui/PlayingScene.hpp", report["error"])

    def test_forbidden_find_package_is_rejected_even_if_quiet(self):
        self.configure.append("find_package(sFmL QUIET)")
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertFalse(report["success"])
        self.assertEqual(report["stages"][-1]["name"], "configure")
        errors = Path(report["evidence_directory"]) / "evidence/configure.stderr"
        self.assertIn("request for sFmL", errors.read_text(encoding="utf-8"))

    def test_forbidden_fetchcontent_is_rejected_before_population(self):
        self.configure += ["include(FetchContent)",
                           "FetchContent_Declare(FmT SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/missing)",
                           "FetchContent_MakeAvailable(FmT)"]
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "configure")
        errors = Path(report["evidence_directory"]) / "evidence/configure.stderr"
        self.assertIn("FETCHCONTENT_MAKEAVAILABLE_SERIAL", errors.read_text(encoding="utf-8"))

    def test_tests_off_rejects_catch_discovery(self):
        self.configure.append("find_package(Catch2 QUIET)")
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "configure")

    def test_installed_openal_soft_is_rejected_before_build(self):
        toolchain = self.directory / "vcpkg-status-fixture.cmake"
        toolchain.write_text(
            'set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON CACHE BOOL "" FORCE)\n'
            'set(VCPKG_MANIFEST_FEATURES "" CACHE STRING "" FORCE)\n'
            'if (DEFINED VCPKG_INSTALLED_DIR)\n'
            '  file(MAKE_DIRECTORY "${VCPKG_INSTALLED_DIR}/vcpkg")\n'
            '  file(WRITE "${VCPKG_INSTALLED_DIR}/vcpkg/status" '
            '"Package: nlohmann-json\\nArchitecture: fixture-native\\nStatus: install ok installed\\n\\n'
            'Package: openal-soft\\nArchitecture: fixture-native\\nStatus: install ok installed\\n\\n")\n'
            'endif()\n', encoding="utf-8")
        result, report = self.run_fixture(["--toolchain", str(toolchain)])
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(report["stages"][-1]["name"], "dependency-and-target-graph")
        self.assertIn("Forbidden installed vcpkg port: openal-soft", report["error"])
        self.assertFalse(any(stage["name"] == "build" for stage in report["stages"]))
        self.assertFalse(list(Path(report["build"]).glob("**/libgame_core.a")))

    def test_explicit_cross_toolchain_is_rejected_even_for_same_host_os(self):
        result, report = self.run_fixture(
            ["--cmake-arg=-DCMAKE_SYSTEM_NAME=" + platform.system()])
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "configure")
        self.assertIn("native toolchain", report["error"])

    def test_hidden_imported_static_dependency_is_rejected_before_build(self):
        self.configure += ["add_library(neutral_payload STATIC IMPORTED)",
                           'set_target_properties(neutral_payload PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/external/libsfml-system.a")',
                           "add_library(neutral_bridge INTERFACE IMPORTED)",
                           'set_target_properties(neutral_bridge PROPERTIES INTERFACE_LINK_LIBRARIES "$<$<CONFIG:Release>:neutral_payload>")',
                           "target_link_libraries(game_core PUBLIC neutral_bridge)"]
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "dependency-and-target-graph")
        self.assertIn("Desktop dependency", report["error"])

    def test_audio_framework_link_flag_is_rejected_with_tests_off(self):
        self.configure.append('target_link_libraries(game_core PRIVATE "-framework CoreAudio")')
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "dependency-and-target-graph")

    def test_scoped_imported_target_fails_when_its_properties_cannot_be_inspected(self):
        child = self.source / "child"
        child.mkdir()
        (child / "CMakeLists.txt").write_text(
            'add_library(scoped_payload STATIC IMPORTED)\n'
            'set_target_properties(scoped_payload PROPERTIES IMPORTED_LOCATION /hidden/libsfml-system.a)\n',
            encoding="utf-8")
        self.configure.append("add_subdirectory(child)")
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "dependency-and-target-graph")
        self.assertIn("outside the audit scope", report["error"])

    def test_new_backend_helper_cannot_hide_inline_only_desktop_headers(self):
        include = self.source / "external/include/SFML"
        include.mkdir(parents=True)
        (include / "Probe.hpp").write_text("#pragma once\n", encoding="utf-8")
        source = self.source / "helper.cpp"
        source.write_text('#include <SFML/Probe.hpp>\nint helper() { return 0; }\n', encoding="utf-8")
        self.configure += ["add_library(new_helper STATIC helper.cpp)",
                           'target_include_directories(new_helper SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/include")',
                           "target_link_libraries(game_core PUBLIC new_helper)"]
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertTrue(next(entry for entry in report["stages"] if entry["name"] == "build")["success"])
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        self.assertIn("helper.cpp", report["error"])

    def test_system_include_header_is_rejected_after_real_build(self):
        include = self.source / "external/include/SFML"
        include.mkdir(parents=True)
        (include / "Probe.hpp").write_text("#pragma once\n", encoding="utf-8")
        source = self.source / "game_core.cpp"
        source.write_text('#include <SFML/Probe.hpp>\n' + source.read_text(encoding="utf-8"), encoding="utf-8")
        self.configure.append('target_include_directories(game_core SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/include")')
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertTrue(next(entry for entry in report["stages"] if entry["name"] == "build")["success"])
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        self.assertIn("SFML/Probe.hpp", report["error"])

    def test_neutral_header_symlink_cannot_hide_forbidden_header(self):
        include = self.source / "external/include"
        include.mkdir(parents=True)
        actual = self.directory / "hidden/SFML/Probe.hpp"
        actual.parent.mkdir(parents=True)
        actual.write_text("#pragma once\n", encoding="utf-8")
        (include / "Neutral.hpp").symlink_to(actual)
        source = self.source / "game_core.cpp"
        source.write_text('#include <Neutral.hpp>\n' + source.read_text(encoding="utf-8"), encoding="utf-8")
        self.configure.append('target_include_directories(game_core SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/include")')
        result, report = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        # Compilers may report the canonical path directly or retain the symlink name.
        self.assertIn("Desktop dependency in actual compiler dependency", report["error"])
        self.assertIn(str(actual.resolve()), report["error"])

    def test_missing_compiler_dependency_evidence_is_rejected(self):
        wrapper = self.directory / "compiler-wrapper"
        wrapper.write_text(
            "#!" + sys.executable + "\nimport os, sys\n"
            "if '-M' in sys.argv:\n"
            "    from pathlib import Path\n"
            "    Path(sys.argv[sys.argv.index('-MF') + 1]).write_text('headless-audit: \\n')\n"
            "    sys.exit(0)\n"
            "os.execv(" + repr(shutil.which("c++")) + ", [" + repr(shutil.which("c++")) + "] + sys.argv[1:])\n",
            encoding="utf-8")
        wrapper.chmod(0o755)
        result, report = self.run_fixture(["--cmake-arg=-DCMAKE_CXX_COMPILER=" + str(wrapper)])
        self.assertEqual(result.returncode, 1)
        self.assertTrue(next(entry for entry in report["stages"] if entry["name"] == "build")["success"])
        self.assertEqual(report["stages"][-1]["name"], "actual-headers")
        self.assertIn("empty dependency rule", report["error"])


if __name__ == "__main__":
    unittest.main()
