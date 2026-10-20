#!/usr/bin/env python3
"""Verify a native Numtilo runtime without opening or changing the user's profile.

The game performs read-only --check-runtime probes. A separate guarded worker
performs persistence checks under a newly created, sentinel-marked sandbox.
All subprocess output and the final JSON report remain in --output.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata


MARKER = ".numtilo-acceptance"
MARKER_BYTES = b"numtilo-acceptance-v1\n"
RESOURCES = {
    "fonts/Inter-Variable.ttf",
    "icons/numtilo.png",
    "sounds/slide.wav",
    "sounds/merge.wav",
    "sounds/spawn.wav",
    "sounds/game_over.wav",
    "sounds/high_score.wav",
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def same_path(left, right):
    """Compare existing paths by identity, including macOS Unicode spelling."""
    a, b = Path(left), Path(right)
    try:
        return a.samefile(b)
    except (OSError, ValueError):
        if a.name == b.name:
            try:
                return a.parent.samefile(b.parent)
            except (OSError, ValueError):
                pass
        left_text = os.path.normcase(os.path.abspath(a))
        right_text = os.path.normcase(os.path.abspath(b))
        if platform.system() == "Darwin":
            left_text = unicodedata.normalize("NFC", left_text)
            right_text = unicodedata.normalize("NFC", right_text)
        return left_text == right_text


def inside(path, parent):
    resolved = Path(path).resolve()
    return any(same_path(candidate, parent) for candidate in (resolved, *resolved.parents))


def bundle_for(executable):
    for parent in executable.parents:
        if parent.suffix.lower() == ".app":
            return parent
    return None


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Verification:
    def __init__(self, args):
        self.args = args
        self.executable = args.executable.resolve(strict=True)
        self.worker = args.worker.resolve(strict=True)
        args.output.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix="platform-", dir=args.output.resolve()))
        (self.root / MARKER).write_bytes(MARKER_BYTES)
        self.report = {
            "version": 1,
            "success": False,
            "platform": platform.system(),
            "architecture": platform.machine(),
            "mode": args.mode,
            "executable": str(self.executable),
            "worker": str(self.worker),
            "evidence_directory": str(self.root),
            "scope": {
                "user_profile_loaded": False,
                "user_profile_written": False,
                "environment_modified": False,
                "runtime_asset_validation": "binary_read",
                "gui_rendering_tested": False,
                "clean_machine_tested": False,
                "native_platform": platform.system(),
            },
            "cases": [],
        }
        self.case_number = 0

    def case(self, name, function):
        self.case_number += 1
        directory = self.root / (str(self.case_number).zfill(2) + "-" + name)
        directory.mkdir()
        started = time.monotonic()
        entry = {"name": name, "success": False, "evidence_directory": str(directory)}
        self.report["cases"].append(entry)
        try:
            details = function(directory)
            entry["details"] = details
            entry["success"] = True
            print("PASS " + name, flush=True)
            return details
        except Exception as error:
            entry["error"] = str(error)
            print("FAIL " + name + ": " + str(error), file=sys.stderr, flush=True)
            raise
        finally:
            entry["elapsed_seconds"] = round(time.monotonic() - started, 3)
            self.save_report()

    def command(self, command, cwd, evidence, stem="process", expected=0):
        require(Path(cwd).is_dir(), "Subprocess CWD must be an existing fixture directory")
        command = [str(argument) for argument in command]
        record = {"command": command, "cwd": str(cwd)}
        try:
            result = subprocess.run(command, cwd=cwd, capture_output=True, timeout=60)
            record["returncode"] = result.returncode
            (evidence / (stem + ".stdout")).write_bytes(result.stdout)
            (evidence / (stem + ".stderr")).write_bytes(result.stderr)
            require(result.returncode == expected,
                    "Expected exit " + str(expected) + ", received " + str(result.returncode)
                    + "; see " + str(evidence / (stem + ".stderr")))
            return result
        except subprocess.TimeoutExpired as error:
            record["timed_out"] = True
            (evidence / (stem + ".stdout")).write_bytes(error.stdout or b"")
            (evidence / (stem + ".stderr")).write_bytes(error.stderr or b"")
            raise
        finally:
            (evidence / (stem + ".command.json")).write_text(
                json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def runtime(self, executable, cwd, evidence, success=True):
        result = self.command([executable, "--check-runtime"], cwd, evidence,
                              expected=0 if success else 1)
        value = json.loads(result.stdout.decode("utf-8"))
        require(value.get("version") == 1, "Unsupported runtime report schema")
        require(value.get("success") is success, "Runtime success flag disagrees with exit")
        checks = value.get("checks", {})
        require(checks.get("user_profile_opened") is False, "Runtime opened a user profile")
        require(checks.get("user_profile_written") is False, "Runtime wrote a user profile")
        require(checks.get("asset_validation") == "binary_read", "Unexpected asset probe scope")
        user_data = value.get("user_data")
        require(isinstance(user_data, dict), "Native user-data selection failed")
        require(user_data.get("source") == "platform_user_data", "User-data path is not native")
        for key in ("directory", "scores_file", "settings_file"):
            require(isinstance(user_data.get(key), str) and Path(user_data[key]).is_absolute(),
                    "User-data " + key + " must be absolute")
        require(same_path(user_data["scores_file"], Path(user_data["directory"]) / "scores.json"),
                "Score path is outside the selected profile")
        require(same_path(user_data["settings_file"], Path(user_data["directory"]) / "settings.json"),
                "Settings path is outside the selected profile")
        if success:
            require(value.get("errors") == [], "Successful runtime contains errors")
            assets = value.get("assets", {})
            require(assets.get("source") in ("bundle_resources", "executable_directory"),
                    "Assets were not selected from the native runtime")
            require(Path(assets["directory"]).is_absolute(), "Asset directory is not absolute")
            resources = value.get("resources", [])
            require(len(resources) == len(RESOURCES), "Runtime resource count differs")
            require({item.get("relative_path") for item in resources} == RESOURCES,
                    "Runtime asset catalog differs")
            for item in resources:
                require(item.get("readable") is True and isinstance(item.get("bytes"), int)
                        and item["bytes"] > 0, "Unreadable or empty runtime resource")
                require(same_path(item["path"], Path(assets["directory"]) / item["relative_path"]),
                        "Resource path does not match the selected asset root")
        return value

    def clone_runtime(self, target, baseline):
        target.mkdir()
        bundle = bundle_for(self.executable)
        if bundle:
            destination = target / bundle.name
            shutil.copytree(bundle, destination, symlinks=True)
            return destination / self.executable.relative_to(bundle)
        destination = target / self.executable.name
        shutil.copy2(self.executable, destination)
        shutil.copytree(Path(baseline["assets"]["directory"]), target / "assets", symlinks=True)
        # CMake's installed flat runtimes put DLLs/shared libraries next to the game.
        # Copy only runtime companions, never the surrounding build/project tree.
        for child in self.executable.parent.iterdir():
            lowered = child.name.lower()
            if child != self.executable and (lowered.endswith((".dll", ".dylib"))
                                            or ".so" in lowered):
                if child.is_symlink():
                    (target / child.name).symlink_to(os.readlink(child))
                elif child.is_file():
                    shutil.copy2(child, target / child.name)
        return destination

    def native_paths(self):
        def check(directory):
            first_cwd, second_cwd = directory / "foreign-a", directory / "foreign-b"
            first_cwd.mkdir()
            second_cwd.mkdir()
            first = self.runtime(self.executable, first_cwd, directory / "first")
            second = self.runtime(self.executable, second_cwd, directory / "second")
            for key in ("directory", "scores_file", "settings_file"):
                require(same_path(first["user_data"][key], second["user_data"][key]),
                        "User-data selection depends on CWD")
            require(same_path(first["assets"]["directory"], second["assets"]["directory"]),
                    "Asset selection depends on CWD")
            require(first["assets"]["source"] == second["assets"]["source"],
                    "Asset source depends on CWD")
            return first
        return self.case("native-paths-foreign-cwd", self.prepare_evidence(check, "first", "second"))

    @staticmethod
    def prepare_evidence(function, *names):
        def prepared(directory):
            for name in names:
                (directory / name).mkdir()
            return function(directory)
        return prepared

    def relocated(self, baseline):
        def check(directory):
            executable = self.clone_runtime(directory / "Unicode İ oyun 目录", baseline)
            cwd = directory / "foreign-cwd"
            cwd.mkdir()
            value = self.runtime(executable, cwd, directory)
            require(inside(value["assets"]["directory"], executable.parent.parent.parent
                           if bundle_for(executable) else executable.parent),
                    "Relocated runtime still reads assets outside its copied layout")
            require(not same_path(value["assets"]["directory"], baseline["assets"]["directory"]),
                    "Relocated runtime uses original assets")
            require(same_path(value["user_data"]["directory"], baseline["user_data"]["directory"]),
                    "Relocation changed native user-data selection")
            original_sizes = {item["relative_path"]: item["bytes"] for item in baseline["resources"]}
            require({item["relative_path"]: item["bytes"] for item in value["resources"]}
                    == original_sizes, "Relocated resource bytes differ")
            for item in value["resources"]:
                require(digest(Path(item["path"]))
                        == digest(Path(baseline["assets"]["directory"]) / item["relative_path"]),
                        "Copied runtime resource content differs")
            return value
        return self.case("unicode-runtime-relocation", check)

    def missing_font(self, baseline):
        def check(directory):
            executable = self.clone_runtime(directory / "copied-runtime", baseline)
            bundle = bundle_for(executable)
            assets = bundle / "Contents/Resources/assets" if bundle else executable.parent / "assets"
            font = assets / "fonts/Inter-Variable.ttf"
            require(font.is_file(), "Copied font missing before fault injection")
            font.unlink()
            cwd = directory / "foreign-cwd"
            cwd.mkdir()
            shutil.copytree(Path(baseline["assets"]["directory"]), cwd / "assets")
            value = self.runtime(executable, cwd, directory, success=False)
            require(any(item.get("domain") == "resource" and item.get("code") == "missing"
                        and same_path(item.get("path", ""), font)
                        for item in value.get("errors", [])), "Missing font lacks selected full-path error")
            require(same_path(value["assets"]["directory"], assets),
                    "Missing font changed the selected root")
            require(any(item["relative_path"] == "fonts/Inter-Variable.ttf"
                        and item["readable"] is False for item in value["resources"]),
                    "Missing font was silently read from another location")
            return value
        return self.case("missing-font-no-cwd-fallback", check)

    def missing_icon(self, baseline):
        def check(directory):
            executable = self.clone_runtime(directory / "copied-runtime", baseline)
            bundle = bundle_for(executable)
            assets = bundle / "Contents/Resources/assets" if bundle else executable.parent / "assets"
            icon = assets / "icons/numtilo.png"
            require(icon.is_file(), "Copied window icon missing before fault injection")
            icon.unlink()
            cwd = directory / "foreign-cwd"
            cwd.mkdir()
            shutil.copytree(Path(baseline["assets"]["directory"]), cwd / "assets")
            value = self.runtime(executable, cwd, directory, success=False)
            require(any(item.get("domain") == "resource" and item.get("code") == "missing"
                        and same_path(item.get("path", ""), icon)
                        for item in value.get("errors", [])), "Missing icon lacks selected full-path error")
            require(same_path(value["assets"]["directory"], assets),
                    "Missing icon changed the selected root")
            resources = value["resources"]
            require(len(resources) == len(RESOURCES)
                    and {item["relative_path"] for item in resources} == RESOURCES,
                    "Missing icon changed the runtime resource catalog")
            require(any(item["relative_path"] == "icons/numtilo.png"
                        and item["readable"] is False and item["bytes"] == 0
                        for item in resources), "Missing icon was silently read from another location")
            return value
        return self.case("missing-icon-no-cwd-fallback", check)

    def worker_process(self, phase, sandbox, profile_dir, legacy_dir, evidence):
        result = self.command([self.worker, "--sandbox", sandbox, "--profile", profile_dir,
                               "--legacy", legacy_dir, "--phase", phase], evidence, evidence)
        value = json.loads(result.stdout.decode("utf-8"))
        require(value.get("version") == 1 and value.get("phase") == phase
                and value.get("success") is True, "Worker phase/schema failed")
        for key, path in (("profile_dir", profile_dir), ("scores_file", profile_dir / "scores.json"),
                          ("settings_file", profile_dir / "settings.json")):
            require(same_path(value.get(key, ""), path), "Worker escaped its explicit " + key)
        require(value.get("best_score") == 60000 and value.get("sound_enabled") is False,
                "Worker score/settings state differs")
        require(value.get("accept_round_count") == 1, "Acceptance round was not recorded exactly once")
        return value

    @staticmethod
    def legacy_fixture(directory, malformed=False):
        directory.mkdir()
        games = {"scores": [
            {"score": 128, "played_at": "2026-01-01T10:00:00Z", "player_name": "Ada"},
            {"score": 512, "played_at": "2026-01-02T10:00:00Z", "player_name": "İpek"},
        ]}
        scores = b'{"scores":[{"score":-1,"played_at":"invalid"}]}\n' if malformed else (
            json.dumps(games, indent=2, ensure_ascii=False).replace("\n", "\r\n") + "\r\n").encode("utf-8")
        settings = b'{"sound_enabled": true}\r\n'
        (directory / "scores.json").write_bytes(scores)
        (directory / "settings.json").write_bytes(settings)
        return scores, settings

    def persistence(self):
        def check(directory):
            sandbox = directory / "sandbox"
            sandbox.mkdir()
            (sandbox / MARKER).write_bytes(MARKER_BYTES)
            profile_dir, legacy_dir = sandbox / "profile İ 目录", sandbox / "legacy Ş 用户"
            source_score, source_settings = self.legacy_fixture(legacy_dir)
            write_dir, retry_dir, reopen_dir, authoritative_dir = (
                directory / name for name in ("write", "write-again", "reopen", "canonical-authority"))
            for child in (write_dir, retry_dir, reopen_dir, authoritative_dir):
                child.mkdir()
            written = self.worker_process("write", sandbox, profile_dir, legacy_dir, write_dir)
            require(written.get("completed_count") == 3 and written.get("score_dirty") is False,
                    "Successful write did not persist the exact three-round snapshot")
            require((profile_dir / "scores.json.bak").read_bytes() == source_score,
                    "Migrated score backup does not preserve source bytes")
            require((profile_dir / "settings.json.bak").read_bytes() == source_settings,
                    "Migrated settings backup does not preserve source bytes")
            before = {child.name: child.read_bytes() for child in profile_dir.iterdir() if child.is_file()}
            retried = self.worker_process("write", sandbox, profile_dir, legacy_dir, retry_dir)
            require(retried.get("completed_count") == 3 and retried.get("score_dirty") is False,
                    "Repeated write duplicated the accepted round or left dirty memory")
            require(before == {child.name: child.read_bytes() for child in profile_dir.iterdir()
                               if child.is_file()}, "Idempotent write retry changed canonical bytes")
            reopened = self.worker_process("reopen", sandbox, profile_dir, legacy_dir, reopen_dir)
            require(reopened.get("completed_count") == 3 and reopened.get("score_dirty") is False,
                    "Reopen added a duplicate round or dirtied the snapshot")
            after = {child.name: child.read_bytes() for child in profile_dir.iterdir() if child.is_file()}
            require(before == after, "Clean reopen changed profile bytes")
            require((legacy_dir / "scores.json").read_bytes() == source_score
                    and (legacy_dir / "settings.json").read_bytes() == source_settings,
                    "Import changed legacy source bytes")
            changed_score = b'{"scores":[{"score":100000,"played_at":"2026-10-06","player_name":"Later legacy"}]}\n'
            changed_settings = b'{"sound_enabled":true}\n'
            (legacy_dir / "scores.json").write_bytes(changed_score)
            (legacy_dir / "settings.json").write_bytes(changed_settings)
            authoritative = self.worker_process("reopen", sandbox, profile_dir, legacy_dir, authoritative_dir)
            require(authoritative.get("completed_count") == 3,
                    "Later legacy source replaced authoritative canonical history")
            require(before == {child.name: child.read_bytes() for child in profile_dir.iterdir()
                               if child.is_file()}, "Later legacy source changed canonical bytes")
            require((legacy_dir / "scores.json").read_bytes() == changed_score
                    and (legacy_dir / "settings.json").read_bytes() == changed_settings,
                    "Canonical reopen changed the deliberately updated legacy fixture")
            return {"write": written, "write_again": retried, "reopen": reopened,
                    "canonical_authority": authoritative, "profile_bytes_unchanged_on_reopen": True}
        self.case("persistence-write-reopen", check)

        def blocked_and_recover(directory):
            sandbox = directory / "sandbox"
            sandbox.mkdir()
            (sandbox / MARKER).write_bytes(MARKER_BYTES)
            profile_dir, legacy_dir = sandbox / "profile İ 目录", sandbox / "legacy Ş 用户"
            bad_score, source_settings = self.legacy_fixture(legacy_dir, malformed=True)
            blocked_dir, recover_dir, reopen_dir = (directory / name for name in ("blocked", "recover", "reopen"))
            for child in (blocked_dir, recover_dir, reopen_dir):
                child.mkdir()
            blocked = self.worker_process("blocked", sandbox, profile_dir, legacy_dir, blocked_dir)
            require(blocked.get("completed_count") == 1 and blocked.get("score_dirty") is True,
                    "Blocked score snapshot was lost or marked clean")
            require(blocked.get("score_save_status") == "WriteBlocked"
                    and blocked.get("scores_file_exists") is False,
                    "Malformed source did not block score publication")
            require(not (profile_dir / "scores.json").exists()
                    and not (profile_dir / "scores.json.bak").exists(),
                    "Malformed source created an empty score target/backup")
            require((legacy_dir / "scores.json").read_bytes() == bad_score,
                    "Blocked import modified malformed source")
            require((profile_dir / "settings.json.bak").read_bytes() == source_settings,
                    "Score failure prevented independent settings migration")
            valid = {"scores": [
                {"score": 128, "played_at": "2026-01-01T10:00:00Z", "player_name": "Ada"},
                {"score": 512, "played_at": "2026-01-02T10:00:00Z", "player_name": "İpek"},
            ]}
            restored = (json.dumps(valid, ensure_ascii=False) + "\n").encode("utf-8")
            (legacy_dir / "scores.json").write_bytes(restored)
            recovered = self.worker_process("recover", sandbox, profile_dir, legacy_dir, recover_dir)
            require(recovered.get("completed_count") == 3 and recovered.get("score_dirty") is False,
                    "Fresh process did not recover the protected score import")
            require((profile_dir / "scores.json.bak").read_bytes() == restored,
                    "Recovery backup differs from repaired source")
            before = (profile_dir / "scores.json").read_bytes()
            reopened = self.worker_process("reopen", sandbox, profile_dir, legacy_dir, reopen_dir)
            require(reopened.get("completed_count") == 3 and before == (profile_dir / "scores.json").read_bytes(),
                    "Recovered reopen modified or duplicated history")
            require((legacy_dir / "scores.json").read_bytes() == restored,
                    "Recovery modified the repaired source")
            return {"blocked": blocked, "recover": recovered, "reopen": reopened}
        self.case("protected-source-fresh-process-recovery", blocked_and_recover)

    def launch_services(self, baseline):
        require(platform.system() == "Darwin", "--launch-services requires macOS")
        bundle = bundle_for(self.executable)
        require(bundle is not None, "--launch-services requires a .app executable")
        def check(directory):
            stdout, stderr = directory / "application.stdout", directory / "application.stderr"
            self.command(["/usr/bin/open", "-n", "-W", "--stdout", stdout, "--stderr", stderr,
                          "-a", bundle, "--args", "--check-runtime"], directory, directory, "open")
            value = json.loads(stdout.read_text(encoding="utf-8"))
            require(value.get("success") is True and value.get("version") == 1,
                    "LaunchServices runtime check failed")
            require(value.get("checks") == baseline["checks"], "LaunchServices changed probe scope")
            require(same_path(value["assets"]["directory"], baseline["assets"]["directory"])
                    and same_path(value["user_data"]["directory"], baseline["user_data"]["directory"]),
                    "LaunchServices changed native runtime paths")
            return {"launch_method": "LaunchServices open -n -W", "runtime": value,
                    "physical_finder_interaction": False, "gui_rendering_tested": False}
        self.case("launch-services-read-only-probe", check)

    def save_report(self):
        (self.root / "report.json").write_text(
            json.dumps(self.report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def runtime_dependencies(self):
        def check(directory):
            cmake = shutil.which("cmake")
            require(cmake is not None, "CMake is required to audit installed runtime dependencies")
            script = Path(__file__).resolve().parents[1] / "cmake/RuntimeDependencies.cmake"
            try:
                self.command([cmake, "-DNUMTILO_RUNTIME_EXECUTABLE=" + str(self.executable),
                              "-DPython3_EXECUTABLE=" + sys.executable,
                              "-P", script], directory, directory, "runtime-dependencies")
            except Exception:
                for stream in ("stdout", "stderr"):
                    evidence = directory / ("runtime-dependencies." + stream)
                    if evidence.is_file():
                        output = evidence.read_text(encoding="utf-8", errors="replace")
                        if output:
                            print("Runtime dependency " + stream + ":\n" + output,
                                  file=sys.stderr, flush=True)
                raise
            return {"scanner": "CMake GET_RUNTIME_DEPENDENCIES",
                    "scope": "installed Windows/Linux runtime and transitive shared libraries"}
        self.case("installed-runtime-dependencies", check)

    def run(self):
        try:
            if self.args.mode == "installed" and platform.system() in ("Windows", "Linux"):
                self.runtime_dependencies()
            baseline = self.native_paths()
            self.relocated(baseline)
            self.missing_font(baseline)
            self.missing_icon(baseline)
            self.persistence()
            if self.args.launch_services:
                self.launch_services(baseline)
            self.report["success"] = True
        except Exception as error:
            self.report["error"] = str(error)
        finally:
            self.save_report()
            print("Report: " + str(self.root / "report.json"), flush=True)
        return 0 if self.report["success"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--worker", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("build", "installed"), required=True)
    parser.add_argument("--launch-services", action="store_true",
                        help="Also run a read-only macOS .app diagnostic via LaunchServices")
    args = parser.parse_args()
    try:
        require(args.executable.is_file(), "Game executable does not exist")
        require(args.worker.is_file(), "Acceptance worker does not exist")
        return Verification(args).run()
    except Exception as error:
        print("Platform verification could not start: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
