#!/usr/bin/env python3
"""Build and audit a fresh headless tree; retain every command and its evidence.

Requires CMake >= 3.24, a Makefiles/Ninja generator, and a GNU/Clang native
toolchain. Dependency scans use the actual compile database with -M (including
system headers), rather than assuming an empty search result proves isolation.
No existing build directory or installed dependency prefix is modified.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata


BACKENDS = (
    "game_core", "app_logic", "app_paths", "asset_resolver", "asset_locations",
    "user_data_paths", "app_settings", "profile_migration", "runtime_check",
)
DESKTOP_SOURCES = (
    "src/app/main.cpp", "src/app/App.cpp", "src/app/App.hpp",
    "src/app/SoundManager.cpp", "src/app/SoundManager.hpp", "src/app/ui/",
    "tests/platform_acceptance_worker.cpp",
)
# These existing contracts contain only standard/core declarations, despite
# their UI directory. Their complete compiler-reported dependency closure is
# still audited; no runtime UI header or SFML/framework dependency is exempt.
HEADLESS_UI_CONTRACTS = frozenset({
    "src/app/ui/Scene.hpp", "src/app/ui/GameplayOutcome.hpp",
})
FORBIDDEN = re.compile(
    r"(?i)(?:\bSFML::|\bfmt::|(?:^|[/\\])(?:SFML|fmt|AL|OpenAL)(?:[/\\]|$)"
    r"|\b(?:lib)?sfml[-.](?:audio|graphics|network|system|window)\b"
    r"|\b(?:lib)?fmt(?:d)?(?:\.(?:a|so|dylib|lib|dll)|[-:]|$)"
    r"|\b(?:lib)?openal(?:32)?(?:\b|_)"
    r"|/(?:AppKit|Cocoa|OpenGL|CoreAudio|AudioToolbox|AudioUnit|Metal)\.framework(?:/|$|[\";\s])"
    r"|-(?:weak_)?framework[ ,;=]+(?:AppKit|Cocoa|OpenGL|CoreAudio|AudioToolbox|AudioUnit|Metal)\b)")
SYMBOLS = re.compile(r"\b(?:sf|fmt)::|_Z[NK]*(?:2sf|3fmt)|\b_?(?:al|alc)[A-Z]\w*")


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def path_key(path):
    value = os.path.normcase(str(Path(path).resolve()))
    return unicodedata.normalize("NFC", value) if sys.platform == "darwin" else value


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def make_dependencies(text):
    """Read the first Make dependency rule, including escaped spaces and dollars."""
    logical = text.replace("\\\r\n", "").replace("\\\n", "")
    match = re.search(r"(?<!\\):\s", logical)
    require(match is not None, "Compiler dependency output has no Make rule")
    body = logical[match.end():].split("\n", 1)[0]
    values, word, escaped = [], [], False
    for char in body:
        if escaped:
            word.append(char)
            escaped = False
        elif char == "\\":
            escaped = True
        elif char.isspace():
            if word:
                values.append("".join(word).replace("$$", "$"))
                word = []
        else:
            word.append(char)
    require(not escaped, "Truncated compiler dependency output")
    if word:
        values.append("".join(word).replace("$$", "$"))
    require(values, "Compiler returned an empty dependency rule")
    return values


def dependency_command(arguments, destination):
    """Preserve language, includes and defines; replace only compile/output modes."""
    result, index = [], 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in ("-o", "-MF", "-MT", "-MQ", "-MJ"):
            require(index + 1 < len(arguments), "Truncated compiler output option")
            index += 2
            continue
        if argument in ("-c", "-MD", "-MMD", "-MP", "-M", "-MM", "-MG"):
            index += 1
            continue
        if index and any(argument.startswith(flag) and argument != flag
                         for flag in ("-o", "-MF", "-MT", "-MQ", "-MJ")):
            index += 1
            continue
        require(not argument.startswith("@"),
                "Response-file compile commands are not supported by the header audit")
        result.append(argument)
        index += 1
    return result + ["-M", "-MF", str(destination), "-MT", "headless-audit"]


class Verification:
    def __init__(self, args):
        self.args = args
        self.source = args.source.resolve(strict=True)
        self.guard = Path(__file__).resolve().parents[1] / "cmake/VerifyHeadlessDependencies.cmake"
        require(self.guard.is_file(), "Headless dependency guard is missing")
        args.output.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix="headless-", dir=args.output.resolve()))
        self.build = self.root / "build"
        self.evidence = self.root / "evidence"
        self.evidence.mkdir()
        self.report = {
            "schema_version": 1, "success": False,
            "platform": platform.system(), "architecture": platform.machine(),
            "source": str(self.source), "build": str(self.build),
            "evidence_directory": str(self.root), "configuration": args.config,
            "build_testing": not args.no_tests,
            "scope": {
                "fresh_build_tree": True, "fresh_dependency_downloads_required": False,
                "desktop_gui_tested": False, "other_native_platforms_tested": False,
                "system_framework_transitive_dependencies_inspected": False,
                "tests_disabled_final_executable_link_tested": False,
            }, "stages": [],
            "inputs": {"cmake": args.cmake, "toolchain": str(args.toolchain) if args.toolchain else None,
                       "dependency_prefix": str(args.dependency_prefix) if args.dependency_prefix else None,
                       "extra_cmake_arguments": args.cmake_arg,
                       "guard_sha256": hashlib.sha256(self.guard.read_bytes()).hexdigest(),
                       "verifier_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        }
        self.report["inputs"]["source_build_files"] = {
            name: hashlib.sha256((self.source / name).read_bytes()).hexdigest()
            for name in ("CMakeLists.txt", "VERSION", "vcpkg.json") if (self.source / name).is_file()
        }
        self.save()

    def save(self):
        write_json(self.root / "report.json", self.report)

    def stage(self, name, action):
        entry = {"name": name, "success": False}
        self.report["stages"].append(entry)
        started = time.monotonic()
        print("RUN " + name, flush=True)
        try:
            entry["details"] = action()
            entry["success"] = True
            print("PASS " + name, flush=True)
        except Exception as error:
            entry["error"] = str(error)
            raise
        finally:
            entry["elapsed_seconds"] = round(time.monotonic() - started, 3)
            self.save()

    def command(self, arguments, name, cwd=None, timeout=None):
        arguments = [str(argument) for argument in arguments]
        record = {"arguments": arguments, "cwd": str(cwd or self.source)}
        try:
            result = subprocess.run(arguments, cwd=cwd or self.source, capture_output=True,
                                    timeout=timeout or self.args.timeout)
            record["returncode"] = result.returncode
            (self.evidence / (name + ".stdout")).write_bytes(result.stdout)
            (self.evidence / (name + ".stderr")).write_bytes(result.stderr)
            require(result.returncode == 0,
                    f"Command exited {result.returncode}; see {self.evidence / (name + '.stderr')}"
                    f" and {self.evidence / (name + '.stdout')}")
            return result.stdout.decode("utf-8", errors="replace")
        except subprocess.TimeoutExpired as error:
            record["timed_out"] = True
            (self.evidence / (name + ".stdout")).write_bytes(error.stdout or b"")
            (self.evidence / (name + ".stderr")).write_bytes(error.stderr or b"")
            raise
        finally:
            write_json(self.evidence / (name + ".command.json"), record)

    def allowed_text(self, value, context):
        # A checkout name such as sfml-2048 must not be mistaken for a dependency.
        value = unicodedata.normalize("NFC", str(value))
        for prefix in sorted((str(self.source), str(self.build)), key=len, reverse=True):
            value = value.replace(unicodedata.normalize("NFC", prefix), "<project>")
        require(not FORBIDDEN.search(value), "Desktop dependency in " + context + ": " + value[:400])

    def source_allowed(self, path, context):
        key, base = path_key(path), path_key(self.source)
        if key.startswith(base + os.sep):
            relative = key[len(base) + 1:].replace(os.sep, "/")
            require(relative in HEADLESS_UI_CONTRACTS or
                    not any(relative == name or (name.endswith("/") and relative.startswith(name))
                            for name in DESKTOP_SOURCES), "Desktop source in " + context + ": " + relative)
        self.allowed_text(path, context)
        self.allowed_text(Path(path).resolve(), context + " (resolved path)")

    def configure(self):
        query = self.build / ".cmake/api/v1/query/client-headless-audit"
        query.mkdir(parents=True)
        for kind in ("codemodel-v2", "cache-v2", "toolchains-v1"):
            (query / kind).touch()
        command = [self.args.cmake, "-S", self.source, "-B", self.build]
        if self.args.generator:
            command += ["-G", self.args.generator]
        if self.args.toolchain:
            command += ["-DCMAKE_TOOLCHAIN_FILE=" + str(self.args.toolchain.resolve(strict=True))]
        if self.args.dependency_prefix:
            command += ["-DCMAKE_PREFIX_PATH=" + str(self.args.dependency_prefix.resolve(strict=True))]
        if self.args.triplet:
            command += ["-DVCPKG_TARGET_TRIPLET=" + self.args.triplet]
        command += self.args.cmake_arg
        if self.args.toolchain and "vcpkg" in str(self.args.toolchain).lower():
            command += ["-DVCPKG_INSTALLED_DIR=" + str(self.build / "vcpkg_installed")]
        command += ["-DNUMTILO_BUILD_APP=OFF",
                    "-DBUILD_TESTING=" + ("OFF" if self.args.no_tests else "ON"),
                    "-DCMAKE_BUILD_TYPE=" + self.args.config,
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    "-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=" + str(self.guard),
                    "-DHEADLESS_AUDIT_DIR=" + str(self.evidence)]
        self.command(command, "configure")
        reply = self.build / ".cmake/api/v1/reply"
        indices = sorted(reply.glob("index-*.json"))
        require(indices, "CMake did not return File API evidence")
        index = read_json(indices[-1])
        self.cmake = index["cmake"]
        require("Makefiles" in self.cmake["generator"]["name"] or
                "Ninja" in self.cmake["generator"]["name"],
                "Full header audit requires a Makefiles or Ninja generator")
        client = index["reply"]["client-headless-audit"]
        require(all("jsonFile" in client[kind]
                    for kind in ("codemodel-v2", "cache-v2", "toolchains-v1")),
                "Incomplete File API response")
        self.cache = {item["name"]: item["value"] for item in
                      read_json(reply / client["cache-v2"]["jsonFile"])["entries"]}
        require(self.cache.get("NUMTILO_BUILD_APP") == "OFF", "Application option is not OFF")
        require(self.cache.get("BUILD_TESTING") == ("OFF" if self.args.no_tests else "ON"),
                "Test option disagrees with requested audit")
        snapshot = read_json(self.evidence / "target-properties.json")
        require(snapshot.get("cross_compiling") is False,
                "Runtime acceptance requires evidence of a native toolchain")
        toolchains = read_json(reply / client["toolchains-v1"]["jsonFile"])["toolchains"]
        require(all(item.get("compiler", {}).get("id") in ("GNU", "Clang", "AppleClang")
                    for item in toolchains if item["language"] in ("CXX", "OBJCXX")),
                "Full header audit currently supports GNU/Clang compilers")
        model = read_json(reply / client["codemodel-v2"]["jsonFile"])
        configurations = model["configurations"]
        selected = [item for item in configurations if item["name"] == self.args.config]
        require(len(selected) == 1, "File API does not contain requested configuration")
        self.targets = {item["name"]: read_json(reply / item["jsonFile"])
                        for item in selected[0]["targets"]}
        self.directories = [read_json(reply / item["jsonFile"])
                            for item in selected[0]["directories"]]
        return {"version": self.cmake["version"]["string"],
                "generator": self.cmake["generator"]["name"],
                "file_api_index": str(indices[-1]), "toolchains": toolchains}

    def graph(self):
        events = [json.loads(line) for line in
                  (self.evidence / "dependency-events.jsonl").read_text(encoding="utf-8").splitlines()]
        require(any(item.get("event") == "loaded" for item in events), "Dependency guard was not loaded")
        for item in events:
            if item.get("event") == "request":
                require(item["name"].lower() not in ("sfml", "fmt", "openal", "python", "python3"),
                        "Forbidden package discovery was recorded")
                require(not (self.args.no_tests and item["name"].lower() == "catch2"),
                        "Catch2 discovery occurred with tests disabled")
        properties = read_json(self.evidence / "target-properties.json")
        require(properties["schema_version"] == 1 and not properties["missing_targets"],
                "Incomplete target property snapshot")
        require(properties.get("unresolved_imported_targets") == [],
                "Imported targets outside the audit scope cannot be inspected: " +
                str(properties.get("unresolved_imported_targets")))
        for name in properties["buildsystem_targets"] + properties["imported_targets"]:
            self.allowed_text(name, "declared target")
            require(not (self.args.no_tests and "catch2" in name.lower()),
                    "Catch2 declaration retained with tests disabled")
        for name, target in properties["targets"].items():
            self.allowed_text(name, "target property closure")
            self.allowed_text(json.dumps(target["properties"]), "link/include/imported properties of " + name)
            for value in target["properties"].values():
                if isinstance(value, str):
                    for item in value.split(";"):
                        if Path(item).is_absolute():
                            self.allowed_text(Path(item).resolve(), "resolved property path of " + name)
            require(not (self.args.no_tests and "catch2" in name.lower()),
                    "Catch2 target retained with tests disabled")
        names = set(self.targets)
        require(set(BACKENDS).issubset(names), "One or more backend targets are missing")
        require(not names.intersection({"numtilo", "numtilo_assets", "platform_acceptance_worker",
                                        "package", "package_source"}), "Desktop targets in headless graph")
        require(("core_unit_tests" in names) is not self.args.no_tests, "Unit target gate differs")
        self.expected_sources = set()
        for name, target in self.targets.items():
            self.allowed_text(name, "generated target")
            require(not (self.args.no_tests and "catch" in name.lower()), "Test library in backend-only graph")
            if name in BACKENDS:
                require(target["type"] == "STATIC_LIBRARY", name + " is not STATIC")
            for source in target.get("sources", []):
                path = Path(source["path"])
                path = path if path.is_absolute() else self.source / path
                self.source_allowed(path, "generated source of " + name)
                if "compileGroupIndex" in source:
                    self.expected_sources.add(path_key(path))
            for group in target.get("compileGroups", []):
                for include in group.get("includes", []):
                    self.allowed_text(include["path"], "include directory of " + name)
                for fragment in group.get("compileCommandFragments", []):
                    self.allowed_text(fragment["fragment"], "compile flags of " + name)
            for fragment in target.get("link", {}).get("commandFragments", []):
                self.allowed_text(fragment["fragment"], "generated link of " + name)
        require(self.expected_sources, "Generated graph contains no backend translation units")
        for directory in self.directories:
            for installer in directory.get("installers", []):
                require(installer.get("component") != "Runtime", "Runtime install component in headless graph")
        for pattern in ("CPack*Config.cmake", "CleanInstalledAssets*.cmake*", "FixupMacBundle*.cmake*", "assets"):
            require(not list(self.build.glob(pattern)), "Desktop/package output in fresh headless build: " + pattern)
        for key, value in self.cache.items():
            require(not re.match(r"(?i)^(SFML_|fmt_|OPENAL_|Python3_)", key),
                    "Desktop package cache entry: " + key)
        packages = []
        if self.args.toolchain and "vcpkg" in str(self.args.toolchain).lower():
            require(self.cache.get("VCPKG_MANIFEST_NO_DEFAULT_FEATURES") in ("ON", "TRUE", "1"),
                    "vcpkg default features were not disabled")
            require(self.cache.get("VCPKG_MANIFEST_FEATURES", "") == ("" if self.args.no_tests else "tests"),
                    "vcpkg features differ from the headless contract")
            installed = Path(self.cache.get("VCPKG_INSTALLED_DIR", str(self.build / "vcpkg_installed")))
            status = installed / "vcpkg/status"
            require(status.is_file(), "Actual vcpkg installed-package status is missing")
            for paragraph in status.read_text(encoding="utf-8").split("\n\n"):
                fields = dict(line.split(": ", 1) for line in paragraph.splitlines() if ": " in line)
                if fields.get("Status") == "install ok installed":
                    packages.append(fields)
            require(packages, "vcpkg reported no installed dependencies")
            for package in packages:
                require(package["Package"] not in ("sfml", "fmt", "openal", "openal-soft", "catch2") or
                        (package["Package"] == "catch2" and not self.args.no_tests),
                        "Forbidden installed vcpkg port: " + package["Package"])
            require(any(item["Package"] == "nlohmann-json" for item in packages), "JSON port missing")
            require(any(item["Package"] == "catch2" for item in packages) is not self.args.no_tests,
                    "Catch2 installed-port gate differs")
        return {"backend_targets": list(BACKENDS), "compiled_translation_units": len(self.expected_sources),
                "all_generated_targets": sorted(names), "property_closure_targets": sorted(properties["targets"]),
                "dependency_requests": events, "installed_vcpkg_packages": packages}

    def build_all(self):
        self.command([self.args.cmake, "--build", self.build, "--config", self.args.config,
                      "--parallel", self.args.jobs, "--verbose"], "build")
        for pattern in ("CPack*Config.cmake", "CleanInstalledAssets*.cmake*", "FixupMacBundle*.cmake*", "assets"):
            require(not list(self.build.glob(pattern)), "Desktop/package output after headless build: " + pattern)
        return {"default_target_built": True}

    def headers(self):
        database = read_json(self.build / "compile_commands.json")
        selected, records = {}, []
        for entry in database:
            cwd = Path(entry["directory"])
            arguments = entry.get("arguments") or shlex.split(entry["command"])
            self.allowed_text(" ".join(arguments), "compile database")
            source = Path(entry["file"])
            source = source if source.is_absolute() else cwd / source
            key = path_key(source)
            if key not in self.expected_sources:
                continue
            # The compiler's -o is relative to its recorded working directory.
            # CMake 4's optional output field may instead be build-root-relative.
            require("-o" in arguments, "Compile command has no object output")
            output = arguments[arguments.index("-o") + 1]
            output = Path(output)
            output = output if output.is_absolute() else cwd / output
            if not output.is_file():
                continue  # Multi-config databases also contain unbuilt configurations.
            require(key not in selected, "Ambiguous built compile commands for " + str(source))
            selected[key] = (entry, arguments, source, output)
        require(set(selected) == self.expected_sources, "Missing built compile commands for: " +
                ", ".join(sorted(self.expected_sources - set(selected))))
        directory = self.evidence / "headers"
        directory.mkdir()
        for number, key in enumerate(sorted(selected), 1):
            entry, arguments, source, output = selected[key]
            depfile = directory / (str(number).zfill(3) + ".d")
            self.command(dependency_command(arguments, depfile), "header-" + str(number).zfill(3),
                         cwd=Path(entry["directory"]))
            require(depfile.is_file(), "Compiler did not produce dependency evidence")
            headers = make_dependencies(depfile.read_text(encoding="utf-8"))
            resolved = [Path(value) if Path(value).is_absolute() else Path(entry["directory"]) / value
                        for value in headers]
            require(key in {path_key(value) for value in resolved}, "Dependency scan does not contain its source")
            require(len(resolved) > 1, "Dependency scan contains no headers for " + str(source))
            for header in resolved:
                self.source_allowed(header, "actual compiler dependency of " + str(source))
            records.append({"source": str(source), "object": str(output), "dependency_file": str(depfile),
                            "dependency_count": len(resolved), "headers": [str(value) for value in resolved]})
        write_json(directory / "index.json", records)
        return {"translation_units": len(records), "includes_system_headers": True,
                "dependency_records": sum(item["dependency_count"] for item in records),
                "index": str(directory / "index.json")}

    def artifacts(self, name):
        values = self.targets[name].get("artifacts", [])
        require(values, "Generated target has no artifact: " + name)
        paths = [Path(item["path"]) for item in values]
        paths = [path if path.is_absolute() else self.build / path for path in paths]
        require(all(path.is_file() for path in paths), "Built artifact is missing: " + name)
        return paths

    def symbols(self):
        nm = shutil.which("llvm-nm") or shutil.which("nm")
        require(nm, "Undefined-symbol audit requires llvm-nm or nm")
        records = []
        for name in BACKENDS:
            for archive in self.artifacts(name):
                symbols = self.command([nm, "-C", "-u", archive], "symbols-" + name)
                require(not SYMBOLS.search(symbols), "Desktop undefined symbol in " + name)
                records.append({"target": name, "archive": str(archive)})
        return {"archives": records, "tool": nm}

    def tests(self):
        ctest = self.cmake["paths"]["ctest"]
        base = [ctest, "--test-dir", self.build, "-C", self.args.config]
        inventory = json.loads(self.command(base + ["--show-only=json-v1"], "test-inventory"))
        tests = inventory["tests"]
        if self.args.no_tests:
            require(not tests, "CTest registrations retained with tests disabled")
            return {"registered_tests": [], "executed": False}
        require(len(tests) == 1 and tests[0]["name"] == "core_unit_tests", "Unexpected or empty headless test index")
        require(tests[0].get("command") and
                path_key(tests[0]["command"][0]) == path_key(self.artifacts("core_unit_tests")[0]),
                "CTest command does not execute the audited unit-test artifact")
        labels = next((item["value"] for item in tests[0].get("properties", []) if item["name"] == "LABELS"), [])
        require(labels == ["unit"], "Headless unit label differs")
        output = self.command(base + ["--output-on-failure", "--verbose", "--no-tests=error", "-L", "^unit$"], "tests")
        totals = re.search(r"All tests passed \((\d+) assertions in (\d+) test cases\)", output)
        return {"registered_tests": ["core_unit_tests"], "executed": True,
                "catch2_totals": {"assertions": int(totals.group(1)), "cases": int(totals.group(2))}
                if totals else None}

    def runtime(self):
        if self.args.no_tests:
            return {"applicable": False, "reason": "Backend-only build has no final executable"}
        executable = self.artifacts("core_unit_tests")[0]
        libraries, inspected = [], []
        if sys.platform == "darwin":
            otool = shutil.which("otool")
            require(otool, "Runtime dependency audit requires otool")
            pending, seen = [(executable, [])], set()
            while pending:
                binary, inherited_rpaths = pending.pop()
                if path_key(binary) in seen:
                    continue
                seen.add(path_key(binary))
                listing = self.command([otool, "-L", binary], "runtime-" + str(len(seen)))
                dependencies = [match.group(1).strip() for line in listing.splitlines()[1:]
                                if (match := re.match(r"\s+(.+?) \(compatibility version", line))]
                require(dependencies, "No runtime dependency records for " + str(binary))
                inspected.append(str(binary))
                load = self.command([otool, "-l", binary], "runtime-load-" + str(len(seen)))
                identities = re.findall(r"cmd LC_ID_DYLIB\s+cmdsize \d+\s+name (.+?) \(offset", load)
                dependencies = [value for value in dependencies if value not in identities]
                rpaths = re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.+?) \(offset", load)
                def expand(value):
                    return value.replace("@loader_path", str(binary.parent)).replace("@executable_path", str(executable.parent))
                search_paths = [expand(value) for value in rpaths] + inherited_rpaths
                for dependency in dependencies:
                    self.allowed_text(dependency, "runtime dependency")
                    libraries.append({"image": str(binary), "dependency": dependency})
                    if dependency.startswith(("/System/Library/", "/usr/lib/", "/Library/Apple/System/")):
                        continue  # macOS system libraries may exist only in the dyld cache.
                    candidates = ([Path(root) / dependency[len("@rpath/"):]
                                   for root in search_paths] if dependency.startswith("@rpath/")
                                  else [Path(expand(dependency))])
                    resolved = next((path.resolve() for path in candidates if path.is_file()), None)
                    require(resolved is not None, "Unresolved non-system runtime dependency: " + dependency)
                    self.allowed_text(resolved, "resolved runtime dependency")
                    if path_key(resolved) != path_key(binary):
                        pending.append((resolved, search_paths))
        elif sys.platform.startswith("linux"):
            ldd = shutil.which("ldd")
            require(ldd, "Runtime dependency audit requires ldd")
            listing = self.command([ldd, executable], "runtime")
            require("not found" not in listing, "Unresolved runtime dependency")
            for line in listing.splitlines():
                if line.strip():
                    self.allowed_text(line, "runtime dependency")
                    libraries.append({"image": str(executable), "dependency": line.strip()})
            inspected.append(str(executable))
        else:
            raise AssertionError("Runtime inspection currently supports native macOS and Linux")
        require(libraries, "Runtime audit produced no library evidence")
        write_json(self.evidence / "runtime-dependencies.json", libraries)
        return {"applicable": True, "executable": str(executable), "inspected_images": inspected,
                "dependencies": libraries}

    def run(self):
        for name, action in (("configure", self.configure), ("dependency-and-target-graph", self.graph),
                             ("build", self.build_all), ("actual-headers", self.headers),
                             ("archive-symbols", self.symbols), ("unit-tests", self.tests),
                             ("runtime-dependencies", self.runtime)):
            self.stage(name, action)
        self.report["success"] = True
        self.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("."))
    parser.add_argument("--output", type=Path, default=Path("build/headless-acceptance"))
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--no-tests", action="store_true", help="Audit backend libraries with BUILD_TESTING=OFF")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--generator")
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--dependency-prefix", type=Path)
    parser.add_argument("--triplet")
    parser.add_argument("--cmake-arg", action="append", default=[], help="Extra configure argument; use --cmake-arg=-DNAME=value")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--timeout", type=int, default=1800, help="Per-command timeout in seconds")
    args = parser.parse_args()
    if args.jobs <= 0 or args.timeout <= 0:
        parser.error("Jobs and timeout must be positive")
    verification = None
    try:
        verification = Verification(args)
        verification.run()
        print("PASS headless acceptance: " + str(verification.root / "report.json"), flush=True)
        return 0
    except Exception as error:
        print("FAIL headless acceptance: " + str(error), file=sys.stderr, flush=True)
        if verification:
            verification.report["error"] = str(error)
            verification.save()
            print("Evidence: " + str(verification.root / "report.json"), file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
