"""Portable configuration controls using the real CMake evidence provider.

Run: python3 -m unittest discover -s scripts/tests -p test_check_headless_configuration.py -v
Fixtures configure tiny C++ targets without building or downloading dependencies.
Installed-package controls use vcpkg's status format, with no platform mocks.
"""

import json
from pathlib import Path, PureWindowsPath
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from check_headless_configuration import ConfigurationCheck  # noqa: E402
from verify_headless import BACKENDS  # noqa: E402


@unittest.skipUnless(shutil.which("cmake"), "CMake with a native C++ toolchain is required")
class HeadlessConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="headless-configuration-")
        self.directory = Path(self.temporary.name)
        self.source = self.directory / "source with spaces"
        self.source.mkdir()
        self.build = self.directory / "build with spaces"
        self.evidence = self.directory / "evidence with spaces"
        self.testing = True

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, testing=True, extra_lines=(), extra_arguments=()):
        self.testing = testing
        lines = ["cmake_minimum_required(VERSION 3.24)", "project(configuration_control LANGUAGES CXX)"]
        for name in BACKENDS:
            (self.source / (name + ".cpp")).write_text("int " + name + "_value() { return 1; }\n", encoding="utf-8")
            lines.append("add_library(" + name + " STATIC " + name + ".cpp)")
        lines += ["add_library(neutral_payload STATIC IMPORTED)",
                  'set_target_properties(neutral_payload PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/external/libneutral.a")',
                  "add_library(safe::payload ALIAS neutral_payload)",
                  "add_library(neutral_bridge INTERFACE IMPORTED)",
                  'set_target_properties(neutral_bridge PROPERTIES INTERFACE_LINK_LIBRARIES "$<$<CONFIG:Release>:safe::payload>")',
                  "target_link_libraries(game_core PRIVATE neutral_bridge)"]
        if testing:
            (self.source / "unit.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            lines.append("add_executable(core_unit_tests unit.cpp)")
        lines.extend(extra_lines)
        (self.source / "CMakeLists.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        result = subprocess.run(
            ["cmake", "-S", str(self.source), "-B", str(self.build),
             "-DNUMTILO_BUILD_APP=OFF", "-DBUILD_TESTING=" + ("ON" if testing else "OFF"),
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=" + str(ROOT / "cmake/VerifyHeadlessDependencies.cmake"),
             "-DHEADLESS_AUDIT_DIR=" + str(self.evidence), *extra_arguments],
            capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def check(self):
        return ConfigurationCheck(self.build, self.evidence, not self.testing).run()

    def assert_failure(self, text, stage=None):
        report = self.check()
        self.assertFalse(report["success"], report)
        self.assertIn(text, report["error"])
        if stage:
            self.assertEqual(report["stages"][-1]["name"], stage)
        return report

    def snapshot(self):
        return json.loads((self.evidence / "target-properties.json").read_text(encoding="utf-8"))

    def write_snapshot(self, snapshot):
        (self.evidence / "target-properties.json").write_text(json.dumps(snapshot), encoding="utf-8")

    def set_cache(self, name, value, kind="STRING"):
        path = self.build / "CMakeCache.txt"
        lines = [line for line in path.read_text(encoding="utf-8").splitlines()
                 if not line.startswith(name + ":")]
        lines.append(name + ":" + kind + "=" + value)
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def installed_fixture(self, testing=True):
        self.configure(testing)
        installed = self.build / "vcpkg_installed"
        self.set_cache("VCPKG_MANIFEST_DIR", self.source.as_posix(), "PATH")
        self.set_cache("VCPKG_INSTALLED_DIR", installed.as_posix(), "PATH")
        self.set_cache("VCPKG_MANIFEST_NO_DEFAULT_FEATURES", "ON", "BOOL")
        self.set_cache("VCPKG_MANIFEST_FEATURES", "tests" if testing else "")
        self.set_cache("VCPKG_TARGET_TRIPLET", "fixture-native")
        status = installed / "vcpkg/status"
        status.parent.mkdir(parents=True)
        packages = ["vcpkg-cmake", "nlohmann-json"] + (["catch2"] if testing else [])
        status.write_text("".join("Package: " + name + "\nArchitecture: fixture-native\n"
                                  "Status: install ok installed\n\n" for name in packages), encoding="utf-8")
        return status

    def test_real_provider_accepts_tests_on_and_off(self):
        for testing in (True, False):
            with self.subTest(testing=testing):
                self.configure(testing)
                report = self.check()
                self.assertTrue(report["success"], report)
                self.assertFalse(report["scope"]["compiler_headers_checked"])
                self.assertFalse(report["scope"]["runtime_libraries_checked"])
                graph = next(stage["details"] for stage in report["stages"]
                             if stage["name"] == "target-property-closure")
                self.assertIn("safe::payload", graph["property_closure_targets"])
                self.assertEqual("core_unit_tests" in graph["buildsystem_targets"], testing)

    def test_windows_path_spellings_preserve_project_prefix_masking(self):
        check = ConfigurationCheck(self.build, self.evidence)
        # PureWindowsPath exercises the actual Windows spelling on every host;
        # it does not alter the running platform or replace filesystem APIs.
        check.source = PureWindowsPath("C:/workspace/SFML/2048")
        check.build = check.source / "build"
        for value in (str(check.source / "src"), (check.source / "src").as_posix(),
                      str(check.build / "Release"), (check.build / "Release").as_posix()):
            check.allowed_text(value, "project path")
        with self.assertRaisesRegex(AssertionError, "Desktop dependency"):
            check.allowed_text("C:/external/SFML/include", "external path")

    @unittest.skipUnless(shutil.which("ninja"), "Ninja is required for the multi-config control")
    def test_multi_config_configuration_needs_no_compile_database(self):
        self.configure(extra_arguments=["-G", "Ninja Multi-Config"])
        self.assertFalse((self.build / "compile_commands.json").exists())
        self.assertTrue(self.check()["success"])

    def test_guard_load_and_forbidden_discovery_are_verified(self):
        self.configure(False)
        path = self.evidence / "dependency-events.jsonl"
        original = path.read_text(encoding="utf-8")
        path.write_text("", encoding="utf-8")
        self.assert_failure("guard was not loaded", "dependency-provider")
        for package in ("sFmL", "Python3", "Catch2"):
            with self.subTest(package=package):
                request = {"schema_version": 1, "event": "request", "method": "FIND_PACKAGE",
                           "name": package, "arguments": "QUIET", "build_testing": False, "rejected": False}
                path.write_text(original + json.dumps(request) + "\n", encoding="utf-8")
                self.assert_failure("Forbidden package discovery", "dependency-provider")

    def test_missing_and_corrupt_evidence_produce_failure_reports(self):
        self.configure()
        path = self.evidence / "target-properties.json"
        original = path.read_text(encoding="utf-8")
        for content in (None, "{broken json"):
            with self.subTest(content=content):
                if content is None:
                    path.unlink()
                else:
                    path.write_text(content, encoding="utf-8")
                output = self.directory / "reports/check.json"
                result = subprocess.run(
                    [sys.executable, str(ROOT / "scripts/check_headless_configuration.py"),
                     "--build", str(self.build), "--evidence", str(self.evidence), "--output", str(output)],
                    capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                report = json.loads(output.read_text(encoding="utf-8"))
                self.assertFalse(report["success"])
                self.assertEqual(report["stages"][-1]["name"], "dependency-provider")
                path.write_text(original, encoding="utf-8")
        self.set_cache("NUMTILO_BUILD_APP", "ON", "BOOL")
        self.assert_failure("Application option is not OFF", "cmake-cache")

    def test_hidden_imported_dependency_is_rejected_before_build(self):
        self.configure(extra_lines=[
            'set_target_properties(neutral_payload PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/external/libsfml-system.a")'])
        self.assert_failure("Desktop dependency", "target-property-closure")
        self.assertFalse(list(self.build.glob("**/libgame_core.a")))

    def test_incomplete_property_closure_and_native_evidence_are_rejected(self):
        self.configure()
        original = self.snapshot()
        changes = (
            (lambda data: data["targets"].pop("safe::payload"), "Missing linked target"),
            (lambda data: data["targets"]["game_core"]["properties"].pop("IMPORTED_IMPLIB_RELEASE"),
             "Incomplete configuration-specific properties"),
            (lambda data: data.update(unresolved_imported_targets=["scoped_payload"]), "outside the audit scope"),
            (lambda data: data.update(cross_compiling=True), "native toolchain"),
            (lambda data: data.update(build_dir=str(self.directory / "unrelated build")), "Evidence path differs"),
        )
        for change, expected in changes:
            with self.subTest(expected=expected):
                data = json.loads(json.dumps(original))
                change(data)
                self.write_snapshot(data)
                self.assert_failure(expected)
        self.write_snapshot(original)
        self.set_cache("BUILD_TESTING", "OFF", "BOOL")
        self.assert_failure("Test option disagrees", "cmake-cache")

    def test_vcpkg_installed_state_and_feature_bypasses(self):
        status = self.installed_fixture()
        self.assertTrue(self.check()["success"])
        original = status.read_text(encoding="utf-8")
        controls = (
            ("VCPKG_MANIFEST_NO_DEFAULT_FEATURES", "OFF", "default features"),
            ("VCPKG_MANIFEST_FEATURES", "desktop;tests", "features differ"),
            ("VCPKG_INSTALL_OPTIONS", "--X-FEATURE=desktop", "installer option"),
            ("VCPKG_INSTALL_OPTIONS", "@options.rsp", "installer option"),
        )
        for name, value, expected in controls:
            with self.subTest(name=name, value=value):
                self.set_cache(name, value)
                self.assert_failure(expected, "vcpkg-installed-state")
                self.set_cache(name, "ON" if name == "VCPKG_MANIFEST_NO_DEFAULT_FEATURES"
                               else "tests" if name == "VCPKG_MANIFEST_FEATURES" else "")
        for package in ("fmt", "openal-soft"):
            with self.subTest(package=package):
                status.write_text(original + "Package: " + package + "\nArchitecture: fixture-native\n"
                                  "Status: install ok installed\n\n", encoding="utf-8")
                self.assert_failure("Forbidden installed vcpkg port", "vcpkg-installed-state")
        status.write_text(original.replace("Architecture: fixture-native", "Architecture: wrong-triplet"), encoding="utf-8")
        self.assert_failure("target triplet", "vcpkg-installed-state")
        status.unlink()
        self.assert_failure("status", "vcpkg-installed-state")

    def test_tests_off_cannot_retain_installed_catch2(self):
        status = self.installed_fixture(False)
        self.assertTrue(self.check()["success"])
        with status.open("a", encoding="utf-8") as stream:
            stream.write("Package: catch2\nArchitecture: fixture-native\nStatus: install ok installed\n\n")
        self.assert_failure("Catch2 installed-port gate differs", "vcpkg-installed-state")

    def test_runtime_installer_and_desktop_outputs_are_rejected(self):
        self.configure(extra_lines=['install(FILES game_core.cpp DESTINATION . COMPONENT Runtime)'])
        self.assert_failure("Runtime install component", "generated-desktop-outputs")
        child = self.source / "child"
        child.mkdir()
        (child / "payload.txt").write_text("desktop installer payload", encoding="utf-8")
        (child / "CMakeLists.txt").write_text("install(FILES payload.txt DESTINATION . COMPONENT Runtime)\n", encoding="utf-8")
        self.configure(extra_lines=["add_subdirectory(child)"])
        self.assert_failure("Runtime install component", "generated-desktop-outputs")
        shutil.rmtree(self.build / "child")
        self.configure()
        for relative in ("assets", "Release/assets", "CPackConfig.cmake", "numtilo",
                         "Release/numtilo.exe", "numtilo.app", "Release/Numtilo.exe",
                         "Numtilo.app", "sfml_2048-fixture.zip",
                         "sfml-2048-fixture.zip"):
            with self.subTest(relative=relative):
                path = self.build / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("unexpected desktop output", encoding="utf-8")
                self.assert_failure("Desktop/package output", "generated-desktop-outputs")
                path.unlink()


if __name__ == "__main__":
    unittest.main()
