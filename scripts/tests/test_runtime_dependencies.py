"""Portable Runtime packaging policy controls using CMake's real regex engine.

Run: python3 -B -m unittest discover -s scripts/tests -p test_runtime_dependencies.py -v
All files are synthetic and temporary. The dependency scanner is replaced only
inside these fixtures; these tests do not claim native PE/ELF scanning or game
execution on Windows/Linux. Native installed-package checks remain CI's job.
"""

from contextlib import redirect_stderr
import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "cmake/RuntimeDependencies.cmake"


def bracket(value):
    return "[==[" + str(value) + "]==]"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required")
class RuntimeDependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="numtilo-runtime-policy-")
        self.directory = Path(self.temporary.name).resolve()
        self.package = self.directory / "package with spaces"
        self.package.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def run_cmake(self, lines):
        script = self.directory / "control.cmake"
        script.write_text("\n".join([
            "cmake_minimum_required(VERSION 3.24)", *lines, "",
        ]), encoding="utf-8")
        return subprocess.run(
            ["cmake", "-P", str(script)], cwd=self.directory,
            capture_output=True, text=True, timeout=15)

    def assert_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_failure(self, result, message):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout + result.stderr)

    def check_filters(self, system, cases, phase="pre"):
        lines = [
            "set(CMAKE_SYSTEM_NAME " + system + ")",
            "include(" + bracket(MODULE.as_posix()) + ")",
        ]
        for value, expected in cases:
            lines += ["set(value " + bracket(value) + ")", "set(excluded FALSE)"]
            if phase == "pre":
                # CMake lowercases Windows import names before PRE matching.
                if system == "Windows":
                    lines.append("string(TOLOWER \"${value}\" value)")
                lines += [
                    "foreach(pattern IN LISTS NUMTILO_RUNTIME_PRE_EXCLUDE_REGEXES)",
                    "  if(value MATCHES \"${pattern}\")",
                    "    set(excluded TRUE)", "  endif()", "endforeach()",
                ]
            else:
                lines += [
                    "foreach(pattern IN LISTS NUMTILO_RUNTIME_POST_EXCLUDE_REGEXES)",
                    "  if(value MATCHES \"${pattern}\")",
                    "    set(excluded TRUE)", "  endif()", "endforeach()",
                    # GET_RUNTIME_DEPENDENCIES applies POST_INCLUDE first.
                    "foreach(pattern IN LISTS NUMTILO_RUNTIME_POST_INCLUDE_REGEXES)",
                    "  if(value MATCHES \"${pattern}\")",
                    "    set(excluded FALSE)", "  endif()", "endforeach()",
                ]
            lines += [
                "if(NOT excluded STREQUAL " + ("TRUE" if expected else "FALSE") + ")",
                "  message(FATAL_ERROR \"Unexpected filter result: ${value} (${excluded})\")",
                "endif()",
            ]
        self.assert_success(self.run_cmake(lines))

    def write_files(self, names):
        for name in names:
            path = self.package / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic fixture; not an executable or shared library\n")

    def verifier(self, system="Linux", files=(), resolved=(), unresolved=(), conflicts=(),
                 conflict_paths=None):
        executable = self.package / ("Numtilo.exe" if system == "Windows" else "numtilo")
        executable.write_bytes(b"synthetic fixture executable; never launched\n")
        self.write_files(files)
        paths = [str(value) if Path(value).is_absolute() else str(self.package / value)
                 for value in resolved]
        marker = self.directory / "scanner-called"
        lines = [
            "set(CMAKE_SYSTEM_NAME " + system + ")",
            "set(Python3_EXECUTABLE " + bracket(Path(sys.executable).as_posix()) + ")",
            "set(fixture_resolved " + bracket(";".join(paths)) + ")",
            "set(fixture_unresolved " + bracket(";".join(unresolved)) + ")",
            "set(fixture_conflicts " + bracket(";".join(conflicts)) + ")",
            "set(fixture_package " + bracket(self.package.as_posix()) + ")",
        ]
        for name, candidates in (conflict_paths or {}).items():
            candidate_paths = [str(value) if Path(value).is_absolute()
                               else str(self.package / value) for value in candidates]
            lines.append("set(fixture_conflict_" + name + " "
                         + bracket(";".join(candidate_paths)) + ")")
        lines += [
            # CMake preserves its built-in command as _file when overridden.
            # A macro preserves built-in file() output variables in the caller.
            "macro(file operation)",
            "  if(\"${operation}\" STREQUAL \"GET_RUNTIME_DEPENDENCIES\")",
            "    cmake_parse_arguments(scan \"\"",
            "      \"RESOLVED_DEPENDENCIES_VAR;UNRESOLVED_DEPENDENCIES_VAR;CONFLICTING_DEPENDENCIES_PREFIX\"",
            "      \"\" ${ARGN})",
            "    set(${scan_RESOLVED_DEPENDENCIES_VAR} \"${fixture_resolved}\")",
            "    set(${scan_UNRESOLVED_DEPENDENCIES_VAR} \"${fixture_unresolved}\")",
            "    set(${scan_CONFLICTING_DEPENDENCIES_PREFIX}_FILENAMES \"${fixture_conflicts}\")",
            "    foreach(conflict_name IN LISTS fixture_conflicts)",
            "      set(${scan_CONFLICTING_DEPENDENCIES_PREFIX}_${conflict_name}",
            "          \"${fixture_conflict_${conflict_name}}\")",
            "    endforeach()",
            "    _file(WRITE " + bracket(marker.as_posix()) + " \"called\")",
            "  else()",
            "    if(\"${operation}\" STREQUAL \"GLOB_RECURSE\")",
            "      set(fixture_glob_args ${ARGN})",
            "      list(POP_FRONT fixture_glob_args)",
            "      foreach(item IN LISTS fixture_glob_args)",
            "        if(NOT item STREQUAL \"LIST_DIRECTORIES\" AND",
            "           NOT item STREQUAL \"FALSE\" AND NOT item STREQUAL \"TRUE\")",
            "          if(NOT item STREQUAL \"${fixture_package}/*\")",
            "            message(FATAL_ERROR \"Fixture refuses glob outside its owned package: ${item}\")",
            "          endif()",
            "        endif()",
            "      endforeach()",
            "    endif()",
            "    _file(${operation} ${ARGN})",
            "  endif()", "endmacro()",
            "include(" + bracket(MODULE.as_posix()) + ")",
            "numtilo_verify_runtime_dependencies(" + bracket(executable.as_posix()) + ")",
        ]
        result = self.run_cmake(lines)
        return result, marker.exists()

    def test_windows_system_names_and_third_party_names(self):
        blocked = ("KERNEL32.DLL", "user32.dll", "gdi32.dll", "opengl32.dll",
                   "shell32.dll", "ntdll.dll", "ucrtbase.dll", "hvsifiletrust.dll",
                   "api-ms-win-core-file-l1-1-0.dll", "ext-ms-win-test.dll")
        retained = ("sfml-graphics-2.dll", "sfml-audio-2.dll", "OpenAL32.dll",
                    "msvcp140.dll", "vcruntime140_1.dll", "concrt140.dll",
                    "kernel32-helper.dll", "user32-plugin.dll")
        self.check_filters("Windows", [(name, True) for name in blocked]
                           + [(name, False) for name in retained])

    def test_windows_system_paths_and_redistributable_priority(self):
        self.check_filters("Windows", [
            (r"C:\Windows\System32/kernel32.dll", True),
            (r"C:\WINDOWS\sYsTeM32\UsEr32.DlL", True),
            ("C:/Windows/SYSWOW64/GDI32.DLL", True),
            (r"C:\Windows\SysWOW64\opengl32.dll", True),
            ("C:/vcpkg/installed/x64-windows/bin/sfml-audio-2.dll", False),
            (r"C:\package\OpenAL32.dll", False),
            (r"C:\Windows\System32\MSVCP140.dll", False),
            (r"C:\Windows\System32/vcruntime140_1.dll", False),
            ("C:/Windows/System32/concrt140.dll", False),
            (r"C:\Windows\System32/OpenAL32.dll", False),
            ("C:/Windows/System32/SFML-AUDIO-2.DLL", False),
            (r"C:\Windows\System32\sfml-audio-d-2.dll", False),
        ], phase="post")

    def test_linux_glibc_and_loader_names_only(self):
        blocked = ("libc.so.6", "libm.so.6", "libmvec.so.1", "libpthread.so.0", "libdl.so.2",
                   "librt.so.1", "libresolv.so.2", "ld-linux-x86-64.so.2",
                   "ld-linux-aarch64.so.1", "libc-2.31.so", "ld-2.31.so")
        retained = ("libopenal.so.1", "libsfml-audio.so.2.6", "libX11.so.6",
                    "libstdc++.so.6", "libgcc_s.so.1", "libcap.so.2",
                    "libcrypto.so.3", "libcairo.so.2", "libmylibrary.so.1",
                    "libnss_systemd.so.2", "libnss_ldap.so.2")
        self.check_filters("Linux", [(name, True) for name in blocked]
                           + [(name, False) for name in retained])

    def test_linux_usr_lib_third_party_paths_are_not_blanket_excluded(self):
        self.check_filters("Linux", [
            ("/lib/x86_64-linux-gnu/libopenal.so.1", False),
            ("/lib64/libX11.so.6", False),
            ("/usr/lib/x86_64-linux-gnu/libstdc++.so.6", False),
            ("/usr/lib/libsfml-graphics.so.2.6", False),
        ], phase="post")

    def test_linux_static_sfml_openal_do_not_require_shared_copies(self):
        result, scanned = self.verifier()
        self.assert_success(result)
        self.assertTrue(scanned)

    def test_linux_shared_libraries_and_internal_soname_links_are_kept(self):
        self.write_files(["libopenal.so.1.2", "libsfml-audio.so.2.6", "libX11.so.6"])
        try:
            (self.package / "libopenal.so.1").symlink_to("libopenal.so.1.2")
        except OSError as error:
            self.skipTest("Creating symlinks is unavailable: " + str(error))
        result, scanned = self.verifier(
            resolved=["libopenal.so.1", "libsfml-audio.so.2.6", "libX11.so.6"])
        self.assert_success(result)
        self.assertTrue(scanned)

    def test_windows_shared_third_party_and_msvc_runtime_are_kept(self):
        names = ["sfml-graphics-2.dll", "sfml-audio-2.dll", "OpenAL32.dll",
                 "msvcp140.dll", "vcruntime140_1.dll"]
        result, scanned = self.verifier("Windows", files=names, resolved=names)
        self.assert_success(result)
        self.assertTrue(scanned)

    def test_windows_nested_system_dll_inventory_is_rejected(self):
        result, _ = self.verifier("Windows", files=["nested/KeRnEl32.DLL"])
        self.assert_failure(result, "OS runtime library must not be packaged")

    def test_linux_core_library_inventory_is_rejected_in_all_locations(self):
        for name in ("libc.so.6", "lib/libm.so.6", "lib64/ld-linux-x86-64.so.2",
                     "usr/lib/libc-2.31.so"):
            with self.subTest(name=name):
                self.write_files([name])
                result, _ = self.verifier()
                self.assert_failure(result, "OS runtime library must not be packaged")
                (self.package / name).unlink()

    def test_unresolved_and_conflicting_dependencies_fail(self):
        result, scanned = self.verifier(unresolved=["libopenal.so.1"])
        self.assert_failure(result, "Unresolved Runtime dependencies")
        self.assertTrue(scanned)
        result, scanned = self.verifier("Windows", conflicts=["OpenAL32.dll"])
        self.assert_failure(result, "Conflicting Runtime dependencies")
        self.assertTrue(scanned)

    def test_missing_required_third_party_copy_fails(self):
        result, _ = self.verifier(resolved=["libopenal.so.1"])
        self.assert_failure(result, "Required Runtime library is missing")

    def test_external_resolution_requires_packaged_companion(self):
        external = self.directory / "host libraries/libopenal.so.1"
        external.parent.mkdir()
        external.write_bytes(b"synthetic host library\n")
        result, _ = self.verifier(resolved=[external])
        self.assert_failure(result, "Required Runtime library is missing")
        result, scanned = self.verifier(files=[external.name], resolved=[external])
        self.assert_success(result)
        self.assertTrue(scanned)

    def test_dangling_required_library_symlink_fails(self):
        try:
            (self.package / "libopenal.so.1").symlink_to("missing-openal.so.1")
        except OSError as error:
            self.skipTest("Creating symlinks is unavailable: " + str(error))
        result, _ = self.verifier(resolved=["libopenal.so.1"])
        self.assert_failure(result, "Required Runtime library is missing")

    def test_external_required_library_symlink_fails(self):
        external = self.directory / "host-openal.so.1"
        external.write_bytes(b"synthetic external library\n")
        try:
            (self.package / "libopenal.so.1").symlink_to(external)
        except OSError as error:
            self.skipTest("Creating symlinks is unavailable: " + str(error))
        result, _ = self.verifier(resolved=["libopenal.so.1"])
        self.assert_failure(result, "Runtime library points outside the package")

    def test_required_library_link_through_external_directory_fails(self):
        external = self.directory / "host-libraries"
        external.mkdir()
        (external / "target.so.1").write_bytes(b"synthetic external library\n")
        (self.package / "links").symlink_to(external, target_is_directory=True)
        link = self.package / "libopenal.so.1"
        link.symlink_to(Path("links") / "target.so.1")
        self.assertTrue(link.is_file(), "The fixture must create a usable file symlink")
        self.assertEqual(link.resolve(strict=True), (external / "target.so.1").resolve(strict=True))
        result, _ = self.verifier(resolved=["libopenal.so.1"])
        self.assert_failure(result, "Runtime library points outside the package")

    def test_required_library_symlink_loop_fails(self):
        (self.package / "libopenal.so.1").symlink_to("loop.so.1")
        (self.package / "loop.so.1").symlink_to("libopenal.so.1")
        result, _ = self.verifier(resolved=["libopenal.so.1"])
        self.assert_failure(result, "Runtime library")

    def test_linux_identical_conflict_copies_are_accepted(self):
        name = "libX11.so.6"
        self.write_files([name])
        host = self.directory / name
        host.write_bytes((self.package / name).read_bytes())
        result, scanned = self.verifier(
            conflicts=[name], conflict_paths={name: [name, host]})
        self.assert_success(result)
        self.assertTrue(scanned)

    def test_linux_different_conflict_copies_fail(self):
        name = "libX11.so.6"
        self.write_files([name])
        host = self.directory / name
        host.write_bytes(b"different synthetic library content\n")
        result, _ = self.verifier(conflicts=[name], conflict_paths={name: [name, host]})
        self.assert_failure(result, "Conflicting Runtime dependencies")

    def test_linux_missing_conflict_companion_fails(self):
        name = "libX11.so.6"
        host = self.directory / name
        host.write_bytes(b"synthetic host library\n")
        result, _ = self.verifier(conflicts=[name], conflict_paths={name: [host]})
        self.assert_failure(result, "Required Runtime library is missing")

    def test_linux_conflict_companion_external_symlink_fails(self):
        name = "libX11.so.6"
        host = self.directory / name
        host.write_bytes(b"synthetic host library\n")
        (self.package / name).symlink_to(host)
        result, _ = self.verifier(conflicts=[name], conflict_paths={name: [name, host]})
        self.assert_failure(result, "Runtime library points outside the package")

    def test_linux_empty_conflict_candidates_fail(self):
        name = "libX11.so.6"
        self.write_files([name])
        result, _ = self.verifier(conflicts=[name])
        self.assert_failure(result, "Conflicting Runtime dependencies")

    def test_dependency_failure_prints_cmake_diagnostic(self):
        spec = importlib.util.spec_from_file_location(
            "runtime_platform_verification", ROOT / "scripts/verify_platform.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        verification = module.Verification.__new__(module.Verification)
        verification.executable = self.package / "numtilo"
        verification.case = lambda name, action: action(self.directory)

        def failed_command(*args):
            (self.directory / "runtime-dependencies.stderr").write_text(
                "synthetic CMake failure detail\n", encoding="utf-8")
            raise AssertionError("synthetic command failed")

        output = io.StringIO()
        with patch.object(module.shutil, "which", return_value="cmake"), \
                patch.object(verification, "command", side_effect=failed_command) as command, \
                redirect_stderr(output):
            with self.assertRaisesRegex(AssertionError, "synthetic command failed"):
                verification.runtime_dependencies()
        self.assertIn("synthetic CMake failure detail", output.getvalue())
        self.assertIn("-DPython3_EXECUTABLE=" + sys.executable, command.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
