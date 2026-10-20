#!/usr/bin/env python3
"""Check an existing native headless CMake configuration on any CI platform.

Configure with cmake/VerifyHeadlessDependencies.cmake in
CMAKE_PROJECT_TOP_LEVEL_INCLUDES and an absolute HEADLESS_AUDIT_DIR. This check
reads the actual cache and that provider's evidence; it does not build, execute
tests, inspect compiler headers, scan symbols, or audit runtime libraries.
"""

import argparse
import json
from pathlib import Path
import re
import sys
import unicodedata

from verify_headless import BACKENDS, FORBIDDEN, path_key, read_json, require, write_json


BASE_PROPERTIES = (
    "TYPE", "IMPORTED", "ALIASED_TARGET", "LINK_LIBRARIES",
    "INTERFACE_LINK_LIBRARIES", "INCLUDE_DIRECTORIES", "INTERFACE_INCLUDE_DIRECTORIES",
    "INTERFACE_SYSTEM_INCLUDE_DIRECTORIES", "LINK_OPTIONS", "INTERFACE_LINK_OPTIONS",
    "IMPORTED_CONFIGURATIONS", "IMPORTED_LOCATION", "IMPORTED_IMPLIB", "IMPORTED_SONAME",
    "IMPORTED_NO_SONAME", "IMPORTED_LINK_INTERFACE_LIBRARIES", "IMPORTED_LINK_DEPENDENT_LIBRARIES",
)
CONFIG_PROPERTIES = (
    "IMPORTED_LOCATION", "IMPORTED_IMPLIB", "IMPORTED_SONAME", "IMPORTED_NO_SONAME",
    "IMPORTED_LINK_INTERFACE_LIBRARIES", "IMPORTED_LINK_DEPENDENT_LIBRARIES", "MAP_IMPORTED_CONFIG",
)
LINK_PROPERTY = re.compile(
    r"^(?:LINK_LIBRARIES|INTERFACE_LINK_LIBRARIES|IMPORTED_LINK_INTERFACE_LIBRARIES|"
    r"IMPORTED_LINK_DEPENDENT_LIBRARIES)(?:_|$)"
)
DESKTOP_TARGETS = {
    "numtilo", "numtilo_assets", "platform_acceptance_worker", "package", "package_source",
}
TRUE = {"1", "ON", "YES", "TRUE", "Y"}
FALSE = {"0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND", ""}


def cache_entries(path):
    """Read CMake's typed cache, preserving values containing spaces or equals."""
    entries = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not line.strip() or line.startswith(("#", "//")):
            continue
        match = re.fullmatch(r'(?:"([^"]+)"|([^:=]+)):([^=]+)=(.*)', line)
        require(match is not None, "Malformed CMake cache entry: " + line[:120])
        name = match[1] or match[2]
        require(name not in entries, "Duplicate CMake cache entry: " + name)
        entries[name] = match[4]
    require(entries, "CMake cache is empty")
    return entries


def cmake_bool(value, name):
    require(isinstance(value, str), "Missing CMake boolean: " + name)
    normalized = value.upper()
    require(normalized in TRUE | FALSE, "Unrecognized CMake boolean for " + name + ": " + value)
    return normalized in TRUE


def string_list(value, context):
    require(isinstance(value, list) and all(isinstance(item, str) and item for item in value),
            "Invalid target list: " + context)
    require(len(value) == len(set(value)), "Duplicate target in " + context)
    return set(value)


class ConfigurationCheck:
    def __init__(self, build, evidence, no_tests=False):
        self.build = Path(build).resolve()
        self.evidence = Path(evidence).resolve()
        self.testing = not no_tests
        self.report = {
            "schema_version": 1, "success": False, "build": str(self.build),
            "evidence_directory": str(self.evidence), "build_testing": self.testing,
            "scope": {
                "configuration_and_provider_evidence_checked": True,
                "built_artifacts_checked": False, "compiler_headers_checked": False,
                "archive_symbols_checked": False, "runtime_libraries_checked": False,
                "unit_tests_executed": False,
            }, "stages": [],
        }

    def allowed_text(self, value, context):
        text = unicodedata.normalize("NFC", str(value)).replace("\\", "/")
        for prefix in sorted((str(self.source), str(self.build)), key=len, reverse=True):
            prefix = unicodedata.normalize("NFC", prefix).replace("\\", "/")
            if sys.platform == "win32":
                text = re.sub(re.escape(prefix), "<project>", text, flags=re.IGNORECASE)
            else:
                text = text.replace(prefix, "<project>")
        require(not FORBIDDEN.search(text), "Desktop dependency in " + context + ": " + text[:400])

    def same_path(self, value, expected, context):
        require(isinstance(value, str) and value and path_key(value) == path_key(expected),
                "Evidence path differs from the configured " + context)

    def cache(self):
        self.values = cache_entries(self.build / "CMakeCache.txt")
        require(not cmake_bool(self.values.get("NUMTILO_BUILD_APP"), "NUMTILO_BUILD_APP"),
                "Application option is not OFF")
        require(cmake_bool(self.values.get("BUILD_TESTING"), "BUILD_TESTING") == self.testing,
                "Test option disagrees with the requested check")
        require(self.values.get("CMAKE_HOME_DIRECTORY"), "CMake source directory is missing")
        self.source = Path(self.values["CMAKE_HOME_DIRECTORY"]).resolve(strict=True)
        require(self.source.is_dir(), "Configured source directory is not a directory")
        self.same_path(self.values.get("CMAKE_CACHEFILE_DIR"), self.build, "build directory")
        self.same_path(self.values.get("HEADLESS_AUDIT_DIR"), self.evidence, "evidence directory")
        for name in self.values:
            require(not re.match(r"(?i)^(?:SFML_|fmt_|OPENAL_|Python3?_)", name),
                    "Desktop package cache entry: " + name)
        return {"source": str(self.source), "generator": self.values.get("CMAKE_GENERATOR"),
                "application": "OFF", "build_testing": self.testing}

    def metadata(self, item, context):
        require(isinstance(item, dict) and item.get("schema_version") == 1,
                "Unsupported or missing provider schema in " + context)
        self.same_path(item.get("source_dir"), self.source, "source directory in " + context)
        self.same_path(item.get("build_dir"), self.build, "build directory in " + context)
        require(item.get("build_testing") is self.testing, "Provider test gate differs in " + context)
        require(item.get("cross_compiling") is False, "Provider must prove a native toolchain in " + context)
        require(isinstance(item.get("system_name"), str) and item["system_name"] and
                item.get("system_name") == item.get("host_system_name"),
                "Provider system/host evidence differs in " + context)

    def provider(self):
        events = [json.loads(line) for line in
                  (self.evidence / "dependency-events.jsonl").read_text(encoding="utf-8").splitlines()
                  if line.strip()]
        require(events and isinstance(events[0], dict) and events[0].get("event") == "loaded",
                "Dependency guard was not loaded")
        loaded = events[0]
        self.metadata(loaded, "guard load event")
        guard = Path(__file__).resolve().parents[1] / "cmake/VerifyHeadlessDependencies.cmake"
        self.same_path(loaded.get("guard_path"), guard, "dependency guard")
        includes = self.values.get("CMAKE_PROJECT_TOP_LEVEL_INCLUDES", "").split(";")
        require(any(value and path_key(value) == path_key(guard) for value in includes),
                "Dependency guard is absent from the configured top-level includes")
        for item in events[1:]:
            require(isinstance(item, dict) and item.get("schema_version") == 1 and
                    item.get("event") == "request" and isinstance(item.get("name"), str) and item["name"] and
                    isinstance(item.get("arguments"), str),
                    "Invalid dependency request event")
            require(item.get("build_testing") is self.testing and item.get("rejected") is False,
                    "Rejected or inconsistent dependency request")
            require(item.get("method") in ("FIND_PACKAGE", "FETCHCONTENT_MAKEAVAILABLE_SERIAL"),
                    "Unknown dependency provider method")
            name = item["name"].lower()
            require(name not in ("sfml", "fmt", "openal", "python", "python3") and
                    not (not self.testing and name == "catch2"),
                    "Forbidden package discovery was recorded: " + item["name"])
        snapshot = read_json(self.evidence / "target-properties.json")
        self.metadata(snapshot, "target snapshot")
        for name in ("system_name", "host_system_name", "system_processor", "host_system_processor"):
            require(name in loaded and snapshot.get(name) == loaded[name],
                    "Provider platform metadata differs: " + name)
        self.snapshot = snapshot
        return {"guard": str(guard), "system": loaded["system_name"], "dependency_requests": events[1:]}

    def graph(self):
        snapshot = self.snapshot
        require(snapshot.get("missing_targets") == [], "Incomplete target property snapshot")
        require(snapshot.get("unresolved_imported_targets") == [],
                "Imported targets outside the audit scope cannot be inspected")
        built = string_list(snapshot.get("buildsystem_targets"), "buildsystem_targets")
        imported = string_list(snapshot.get("imported_targets"), "imported_targets")
        requested = string_list(snapshot.get("requested_targets"), "requested_targets")
        expected = set(BACKENDS) | ({"core_unit_tests"} if self.testing else set())
        require(requested == expected, "Provider requested-target inventory differs")
        require(expected <= built, "One or more headless targets are missing from the buildsystem")
        require(("core_unit_tests" in built) == self.testing, "Unit target gate differs")
        targets = snapshot.get("targets")
        require(isinstance(targets, dict) and (expected | imported) <= set(targets),
                "Incomplete imported/backend target property closure")
        names = built | imported | set(targets)
        require(not names & DESKTOP_TARGETS, "Desktop/package target in headless graph")
        for name in names:
            self.allowed_text(name, "declared target")
            require(self.testing or "catch" not in name.lower(), "Test library retained with tests disabled")
        for name, target in targets.items():
            require(isinstance(target, dict) and {"name", "type", "imported", "alias_of", "properties"} <= set(target) and
                    target.get("name") == name and
                    type(target.get("imported")) is bool and isinstance(target.get("type"), str),
                    "Invalid target snapshot: " + name)
            properties = target.get("properties")
            require(isinstance(properties, dict) and set(BASE_PROPERTIES) <= set(properties),
                    "Incomplete target properties: " + name)
            require(all(value is None or isinstance(value, str) for value in properties.values()),
                    "Invalid target property value: " + name)
            configurations = {"DEBUG", "RELEASE", "RELWITHDEBINFO", "MINSIZEREL"}
            for value in (self.values.get("CMAKE_BUILD_TYPE", ""),
                          self.values.get("CMAKE_CONFIGURATION_TYPES", ""),
                          properties["IMPORTED_CONFIGURATIONS"] or ""):
                configurations.update(item.upper() for item in value.split(";") if item)
            required = {prefix + "_" + config for prefix in CONFIG_PROPERTIES for config in configurations}
            require(required <= set(properties), "Incomplete configuration-specific properties: " + name)
            require(properties["TYPE"] == target["type"] and
                    cmake_bool(properties["IMPORTED"], "IMPORTED of " + name) == target["imported"] and
                    properties["ALIASED_TARGET"] == target.get("alias_of"),
                    "Target metadata/property disagreement: " + name)
            if name in BACKENDS:
                require(target["type"] == "STATIC_LIBRARY" and not target["imported"] and
                        target.get("alias_of") is None, name + " is not a built static backend")
            if name in imported:
                require(target["imported"], "Declared imported target is not imported: " + name)
            if name == "core_unit_tests":
                require(self.testing and target["type"] == "EXECUTABLE" and not target["imported"],
                        "Unit-test target is not a built executable")
            require(target.get("alias_of") is None or target["alias_of"] in targets,
                    "Missing alias target in property closure: " + name)
            for prop, value in properties.items():
                self.allowed_text(prop, "property name of " + name)
                if value is None:
                    continue
                self.allowed_text(value, "link/include/imported properties of " + name)
                for item in value.split(";"):
                    if Path(item).is_absolute():
                        self.allowed_text(Path(item).resolve(), "resolved property path of " + name)
                if LINK_PROPERTY.match(prop):
                    # Mirror the provider's raw-genex token scan and check known
                    # declared targets plus namespaced references for missing evidence.
                    for token in re.findall(r"[A-Za-z0-9_.:+-]+", value):
                        candidate = token
                        while candidate:
                            if candidate in names or re.match(r"^[A-Za-z0-9_.+-]+::[A-Za-z0-9_.:+-]+$", candidate):
                                require(candidate in targets, "Missing linked target in property closure: " + candidate)
                                break
                            if ":" not in candidate:
                                break
                            candidate = candidate.split(":", 1)[1]
        return {"backend_targets": list(BACKENDS), "buildsystem_targets": sorted(built),
                "property_closure_targets": sorted(targets)}

    def vcpkg(self):
        values = self.values
        active = ("vcpkg" in values.get("CMAKE_TOOLCHAIN_FILE", "").lower() or
                  any(values.get(name) for name in ("Z_VCPKG_ROOT_DIR", "_VCPKG_INSTALLED_DIR", "VCPKG_MANIFEST_DIR")))
        if not active:
            return {"active": False}
        require(cmake_bool(values.get("VCPKG_MANIFEST_NO_DEFAULT_FEATURES"), "VCPKG_MANIFEST_NO_DEFAULT_FEATURES"),
                "vcpkg default features were not disabled")
        require(values.get("VCPKG_MANIFEST_FEATURES") == ("tests" if self.testing else ""),
                "vcpkg features differ from the headless contract")
        for option in values.get("VCPKG_INSTALL_OPTIONS", "").split(";"):
            require(not re.match(r"(?i)^(?:--(?:x-)?(?:feature|no-default-features)(?:=|$)|@)", option),
                    "vcpkg installer option bypasses managed feature selection: " + option)
        installed = Path(values.get("VCPKG_INSTALLED_DIR") or values.get("_VCPKG_INSTALLED_DIR") or
                         str(self.build / "vcpkg_installed"))
        require(installed.is_absolute(), "vcpkg installed directory must be absolute")
        if values.get("_VCPKG_INSTALLED_DIR"):
            self.same_path(values["_VCPKG_INSTALLED_DIR"], installed, "vcpkg installed directory")
        status = installed / "vcpkg/status"
        packages = []
        for paragraph in re.split(r"\n\s*\n", status.read_text(encoding="utf-8")):
            fields = dict(line.split(": ", 1) for line in paragraph.splitlines() if ": " in line)
            if fields.get("Status") == "install ok installed":
                require(fields.get("Package"), "Installed vcpkg record has no package name")
                packages.append(fields)
        require(packages, "vcpkg reported no installed dependencies")
        names = {item["Package"].lower() for item in packages}
        require(not names & {"sfml", "fmt", "openal", "openal-soft"}, "Forbidden installed vcpkg port")
        require("nlohmann-json" in names, "JSON port missing")
        require(("catch2" in names) == self.testing, "Catch2 installed-port gate differs")
        triplet = values.get("VCPKG_TARGET_TRIPLET")
        if triplet:
            target_names = {item["Package"].lower() for item in packages if item.get("Architecture") == triplet}
            require("nlohmann-json" in target_names and ("catch2" in target_names) == self.testing,
                    "Required vcpkg ports are missing for the target triplet")
        return {"active": True, "status": str(status), "installed_packages": packages}

    def generated_files(self):
        install = self.build / "cmake_install.cmake"
        require(install.is_file(), "Generated CMake install script is missing")
        # The project's Runtime component must disappear; dependency development
        # installers may legitimately remain in a headless build.
        installers = sorted(self.build.rglob("cmake_install.cmake"))
        for script in installers:
            content = script.read_text(encoding="utf-8")
            require(not re.search(r'(?i)CMAKE_INSTALL_COMPONENT\s+STREQUAL\s+"Runtime"', content),
                    "Runtime install component in headless configuration: " + str(script))
            self.allowed_text(content, "generated install script " + str(script))
        roots = {self.build}
        configurations = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}
        configurations.update(value for value in self.values.get("CMAKE_CONFIGURATION_TYPES", "").split(";") if value)
        roots.update(self.build / name for name in configurations)
        patterns = ("CPack*Config.cmake", "CleanInstalledAssets*.cmake*", "FixupMacBundle*.cmake*",
                    "assets", "_CPack_Packages", "numtilo*", "Numtilo*", "sfml_2048*",
                    "sfml-2048*.zip", "sfml-2048*.tar*")
        for root in roots:
            for pattern in patterns:
                # Visual Studio's top-level solution is build metadata, not a desktop output.
                matches = [path for path in root.glob(pattern)
                           if not (root == self.build and
                                   self.values.get("CMAKE_GENERATOR", "").startswith("Visual Studio ") and
                                   path.name == "numtilo.sln" and
                                   path.is_file() and not path.is_symlink())]
                require(not matches, "Desktop/package output in headless build: " + str(root / pattern))
        return {"install_scripts_checked": [str(script) for script in installers],
                "desktop_output_directories_checked": sorted(map(str, roots))}

    def run(self):
        try:
            for name, action in (("cmake-cache", self.cache), ("dependency-provider", self.provider),
                                 ("target-property-closure", self.graph), ("vcpkg-installed-state", self.vcpkg),
                                 ("generated-desktop-outputs", self.generated_files)):
                stage = {"name": name, "success": False}
                self.report["stages"].append(stage)
                try:
                    stage["details"] = action()
                    stage["success"] = True
                except Exception as error:
                    stage["error"] = str(error)
                    raise
            self.report["success"] = True
        except Exception as error:
            self.report["error"] = str(error)
        return self.report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True, help="HEADLESS_AUDIT_DIR used during configuration")
    parser.add_argument("--no-tests", action="store_true", help="Expect BUILD_TESTING=OFF")
    parser.add_argument("--output", type=Path, help="Write the check report, including failures, as JSON")
    args = parser.parse_args(argv)
    report = ConfigurationCheck(args.build, args.evidence, args.no_tests).run()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_json(args.output, report)
    if report["success"]:
        print("PASS headless configuration" + (": " + str(args.output) if args.output else ""))
        return 0
    print("FAIL headless configuration: " + report["error"], file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
