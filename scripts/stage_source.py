#!/usr/bin/env python3
"""Stage tracked working-tree files for Numtilo source packages.

Only packaging requires Git/Python. No recursive working-tree copy or fallback
is permitted. POSIX descriptor-relative, no-follow APIs are required; platforms
without them fail closed instead of silently following filesystem links.
"""

import argparse
from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import unicodedata


class StageError(Exception):
    """A source package cannot safely be prepared."""


REQUIRED_FILES = frozenset({
    "CMakeLists.txt", "CMakePresets.json", "VERSION", "vcpkg.json",
    "LICENSE", "THIRD_PARTY_NOTICES.md", "README.md", "CHANGELOG.md",
    "scripts/package_source.sh", "scripts/stage_source.py",
    "scripts/verify_source_package.py", "cmake/VerifySourcePackage.cmake.in",
    "cmake/StageSourcePackage.cmake.in", "cmake/MacOSXBundleInfo.plist.in",
    "cmake/CleanInstalledAssets.cmake.in", "cmake/FixupMacBundle.cmake.in",
    "cmake/SyncAssets.cmake", "cmake/VerifyHeadlessDependencies.cmake",
    "assets/fonts/Inter-Variable.ttf", "assets/fonts/LICENSE-Inter.txt",
    "assets/sounds/slide.wav", "assets/sounds/merge.wav",
    "assets/sounds/spawn.wav", "assets/sounds/game_over.wav",
    "assets/sounds/high_score.wav",
    "artwork/numtilo-logo-original.png",
    "assets/icons/numtilo.png",
    "packaging/macos/Numtilo.icns",
    "packaging/windows/Numtilo.ico",
    "packaging/windows/Numtilo.rc.in",
    "packaging/linux/icons/16x16/numtilo.png",
    "packaging/linux/icons/24x24/numtilo.png",
    "packaging/linux/icons/32x32/numtilo.png",
    "packaging/linux/icons/48x48/numtilo.png",
    "packaging/linux/icons/64x64/numtilo.png",
    "packaging/linux/icons/128x128/numtilo.png",
    "packaging/linux/icons/256x256/numtilo.png",
    "packaging/linux/icons/512x512/numtilo.png",
})
PRIVATE_NAMES = frozenset({
    "scores.json", "settings.json", "cmakeuserpresets.json",
    ".ds_store", ".env", ".netrc", "_netrc", ".npmrc", ".pypirc",
    "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519",
    "credentials", "credentials.json",
    "cmakecache.txt", "cmake_install.cmake", "compile_commands.json",
    "cpackconfig.cmake", "cpacksourceconfig.cmake", "sha256sums.txt",
    "numtilo", "sfml_2048", "2048", ".numtilo-acceptance",
})
PRIVATE_DIRECTORIES = frozenset({
    ".git", ".ssh", ".aws", ".azure", ".gnupg", "__pycache__",
    "_cpack_packages", "cmakefiles", "vcpkg_installed", ".vcpkg",
    "__macosx", "node_modules",
})
OUTPUT_SUFFIXES = (
    ".bak", ".bak.gz", ".orig", ".rej", ".swp", ".swo", "~", ".log",
    ".zip", ".7z", ".tar", ".tgz", ".txz", ".tbz2", ".tar.gz",
    ".tar.xz", ".tar.bz2", ".pyc", ".pyo", ".o", ".obj",
    ".exe", ".dll", ".dylib", ".so", ".a", ".lib", ".pdb",
    ".pem", ".key", ".p12", ".pfx",
)
GIT_LOCATION_OVERRIDES = (
    "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR",
)
VERSION_PATTERN = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+\Z", re.ASCII)


@dataclass(frozen=True)
class Entry:
    name: str
    mode: int


def require_secure_platform():
    required = (os.open, os.stat, os.mkdir)
    if (os.name != "posix"
            or not hasattr(os, "O_NOFOLLOW")
            or not hasattr(os, "O_DIRECTORY")
            or not all(call in os.supports_dir_fd for call in required)
            or os.listdir not in os.supports_fd
            or os.stat not in os.supports_follow_symlinks):
        raise StageError("Source packaging requires POSIX no-follow/dir_fd APIs; "
                         "this platform is not supported safely.")


def absolute_path(value):
    path = Path(value)
    if not path.is_absolute() or ".." in path.parts:
        raise StageError("Source and staging paths must be absolute without '..'.")
    return path


def directory_flags():
    return os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, "O_CLOEXEC", 0)


@contextmanager
def absolute_directory(path):
    """Open every absolute directory component without following a link."""
    descriptor = os.open(path.anchor, directory_flags())
    try:
        for component in path.parts[1:]:
            child = os.open(component, directory_flags(), dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        yield descriptor
    finally:
        os.close(descriptor)


@contextmanager
def parent_directory(root_descriptor, parts, create=False):
    descriptor = os.dup(root_descriptor)
    try:
        for component in parts:
            if create:
                try:
                    os.mkdir(component, 0o755, dir_fd=descriptor)
                except FileExistsError:
                    pass
            child = os.open(component, directory_flags(), dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
            if create:
                os.fchmod(descriptor, 0o755)
        yield descriptor
    finally:
        os.close(descriptor)


def fingerprint(value):
    return (value.st_dev, value.st_ino, value.st_mode, value.st_nlink,
            value.st_size, value.st_mtime_ns, value.st_ctime_ns)


def regular_single_link(value, name):
    if not stat.S_ISREG(value.st_mode) or value.st_nlink != 1:
        raise StageError("Selected source must be a regular, non-hardlinked file: " + name)


@contextmanager
def source_file(root_descriptor, name, expected=None):
    parts = name.split("/")
    with parent_directory(root_descriptor, parts[:-1]) as parent:
        before = os.stat(parts[-1], dir_fd=parent, follow_symlinks=False)
        regular_single_link(before, name)
        if expected is not None and fingerprint(before) != expected:
            raise StageError("Selected source changed during staging: " + name)
        flags = (os.O_RDONLY | os.O_NOFOLLOW | getattr(os, "O_CLOEXEC", 0)
                 | getattr(os, "O_NONBLOCK", 0))
        descriptor = os.open(parts[-1], flags, dir_fd=parent)
        try:
            opened = os.fstat(descriptor)
            regular_single_link(opened, name)
            if fingerprint(opened) != fingerprint(before):
                raise StageError("Selected source changed while opening: " + name)
            yield descriptor, fingerprint(opened)
            after = os.fstat(descriptor)
            linked = os.stat(parts[-1], dir_fd=parent, follow_symlinks=False)
            if fingerprint(after) != fingerprint(opened) or fingerprint(linked) != fingerprint(opened):
                raise StageError("Selected source changed while reading: " + name)
        finally:
            os.close(descriptor)


def validate_name(name):
    if (not name or str(PurePosixPath(name)) != name
            or PurePosixPath(name).is_absolute() or "\\" in name):
        raise StageError("Invalid tracked source path: " + repr(name))
    parts = name.split("/")
    for part in parts:
        if (not part or part in {".", ".."} or part.endswith((" ", "."))
                or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in part)
                or any(c in '<>:"|?*' for c in part)):
            raise StageError("Non-portable tracked source path: " + repr(name))
        base = part.split(".", 1)[0].upper()
        if base in {"CON", "PRN", "AUX", "NUL"} or re.fullmatch(r"(COM|LPT)[1-9]", base):
            raise StageError("Reserved tracked source path: " + repr(name))
    lowered = [part.casefold() for part in parts]
    leaf = lowered[-1]
    if (any(part in PRIVATE_DIRECTORIES for part in lowered)
            or any(part == "build" or part.startswith("cmake-build-")
                   or part.endswith(".app") or part.startswith(".numtilo-source-")
                   for part in lowered[:-1])
            or lowered[0] in {"dist", "out"}
            or leaf in PRIVATE_NAMES or leaf.startswith((".env.", "._"))
            or leaf.startswith(("scores.json.", "settings.json."))
            or ".bak." in leaf or leaf.endswith(OUTPUT_SUFFIXES)):
        raise StageError("Forbidden private/generated tracked path: " + name)


def git_records(root):
    for name in GIT_LOCATION_OVERRIDES:
        if name in os.environ:
            raise StageError("Unset Git location override before source packaging: " + name)
    git = shutil.which("git")
    if not git:
        raise StageError("Git is required for source packaging; no directory-scan fallback is allowed.")
    environment = dict(os.environ, GIT_OPTIONAL_LOCKS="0")
    def command(arguments):
        result = subprocess.run(
            [git, "--no-optional-locks", "-C", str(root), *arguments],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=environment, timeout=30, check=False)
        if result.returncode:
            raise StageError("Git source selection failed; a real checkout is required: "
                             + os.fsdecode(result.stderr).strip())
        return result.stdout
    top = os.fsdecode(command(["rev-parse", "--show-toplevel"])).rstrip("\r\n")
    if not top or Path(os.path.abspath(top)) != root:
        raise StageError("Git top-level must be the exact source root, not a parent repository.")
    return command(["ls-files", "--stage", "--full-name", "-z"])


def select_entries(records):
    if not records or not records.endswith(b"\0"):
        raise StageError("Git returned an empty or invalid tracked file list.")
    entries = []
    seen = set()
    spellings = {}
    for record in records[:-1].split(b"\0"):
        try:
            header, raw_name = record.split(b"\t", 1)
            raw_mode, object_id, stage = header.split(b" ")
        except ValueError as error:
            raise StageError("Invalid Git index record.") from error
        if stage != b"0" or raw_mode not in {b"100644", b"100755"}:
            raise StageError("Conflicts, symlinks, submodules or unsupported Git modes: "
                             + repr(os.fsdecode(raw_name)))
        if not re.fullmatch(b"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", object_id):
            raise StageError("Invalid Git object identifier.")
        name = os.fsdecode(raw_name)
        validate_name(name)
        if name in seen:
            raise StageError("Duplicate tracked source path: " + name)
        seen.add(name)
        parts = name.split("/")
        for end in range(1, len(parts) + 1):
            prefix = "/".join(parts[:end])
            portable = unicodedata.normalize("NFC", prefix).casefold()
            previous = spellings.setdefault(portable, prefix)
            if previous != prefix:
                raise StageError("Case/Unicode-colliding tracked paths: " + previous + " / " + prefix)
        entries.append(Entry(name, 0o755 if raw_mode == b"100755" else 0o644))
    missing = sorted(REQUIRED_FILES - seen)
    if missing:
        raise StageError("Required source files are not Git-tracked (not added automatically): "
                         + ", ".join(missing))
    for name in seen:
        parts = name.split("/")
        if any("/".join(parts[:end]) in seen for end in range(1, len(parts))):
            raise StageError("Tracked file/directory collision: " + name)
    return sorted(entries, key=lambda entry: entry.name)


def read_version(root_descriptor, snapshot):
    with source_file(root_descriptor, "VERSION", snapshot) as (descriptor, _):
        data = os.read(descriptor, 4097)
    if len(data) > 4096:
        raise StageError("VERSION is too large.")
    try:
        version = data.decode("ascii").strip(" \t\r\n")
    except UnicodeDecodeError as error:
        raise StageError("VERSION must contain an ASCII X.Y.Z version.") from error
    if not VERSION_PATTERN.fullmatch(version):
        raise StageError("VERSION must follow the existing X.Y.Z contract.")
    return version


@contextmanager
def destination_directory(destination, source, entries):
    # CPack legitimately stages below source/build/_CPack_Packages. It is safe
    # only because input is a fixed file list, never a recursive directory scan.
    if destination == source or destination in source.parents:
        raise StageError("Staging must not contain or replace the source root.")
    for entry in entries:
        selected = source / entry.name
        if selected == destination or destination in selected.parents:
            raise StageError("Staging overlaps selected source files: " + entry.name)
    with absolute_directory(destination.parent) as parent:
        try:
            os.mkdir(destination.name, 0o755, dir_fd=parent)
        except FileExistsError:
            pass
        descriptor = os.open(destination.name, directory_flags(), dir_fd=parent)
        try:
            info = os.fstat(descriptor)
            if info.st_uid != os.geteuid() or info.st_mode & 0o022:
                raise StageError("Staging must be owned by this user and not writable by others.")
            if os.listdir(descriptor):
                raise StageError("Staging must be empty; existing content is never cleaned or merged.")
            yield descriptor
        finally:
            os.close(descriptor)


def write_all(descriptor, data):
    remaining = memoryview(data)
    while remaining:
        written = os.write(descriptor, remaining)
        if written <= 0:
            raise StageError("Short write while staging source.")
        remaining = remaining[written:]


def copy_entry(source_descriptor, destination_descriptor, entry, snapshot):
    digest = hashlib.sha256()
    size = 0
    with source_file(source_descriptor, entry.name, snapshot) as (input_descriptor, _):
        with parent_directory(destination_descriptor, entry.name.split("/")[:-1], create=True) as parent:
            flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
            output = os.open(entry.name.split("/")[-1], flags, 0o600, dir_fd=parent)
            try:
                while True:
                    data = os.read(input_descriptor, 1024 * 1024)
                    if not data:
                        break
                    write_all(output, data)
                    digest.update(data)
                    size += len(data)
                os.fchmod(output, entry.mode)
                os.fsync(output)
            finally:
                os.close(output)
    return {"sha256": digest.hexdigest(), "size": size, "mode": entry.mode}


def write_manifest(path, destination, version, files, archive_layout):
    # Keep verification state outside the archive tree, in CPack's owned work
    # area. Never overwrite an old file, follow a link or write a caller path.
    path = absolute_path(path)
    if (path.parent != destination.parent
            or path.name != destination.name + ".numtilo-manifest.json"):
        raise StageError("Manifest must be the staging directory's named sibling.")
    suffix = "-Source" if archive_layout == "cpack" else ""
    data = json.dumps({"schema": 1, "version": version,
                       "root": "Numtilo-" + version + suffix, "files": files},
                      ensure_ascii=True, sort_keys=True).encode("utf-8") + b"\n"
    with absolute_directory(path.parent) as parent:
        info = os.fstat(parent)
        if info.st_uid != os.geteuid() or info.st_mode & 0o022:
            raise StageError("Manifest parent must be owned and not writable by others.")
        descriptor = os.open(path.name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=parent)
        try:
            write_all(descriptor, data)
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
        os.fsync(parent)


def stage(source, destination, expected_version=None, manifest=None, archive_layout="cpack"):
    require_secure_platform()
    source = absolute_path(source)
    destination = absolute_path(destination)
    if archive_layout not in {"cpack", "shell"}:
        raise StageError("Unsupported source archive layout.")
    if expected_version is not None and not VERSION_PATTERN.fullmatch(expected_version):
        raise StageError("Configured package version is invalid.")
    with absolute_directory(source) as source_descriptor:
        source_identity = os.fstat(source_descriptor)
        records = git_records(source)
        entries = select_entries(records)
        snapshots = {}
        # Validate every selected leaf/ancestor before reading source contents.
        for entry in entries:
            with source_file(source_descriptor, entry.name) as (_, snapshot):
                snapshots[entry.name] = snapshot
        version = read_version(source_descriptor, snapshots["VERSION"])
        if expected_version is not None and version != expected_version:
            raise StageError("VERSION differs from the configured package version; reconfigure first.")
        with destination_directory(destination, source, entries) as destination_descriptor:
            files = {}
            for entry in entries:
                files[entry.name] = copy_entry(
                    source_descriptor, destination_descriptor, entry, snapshots[entry.name])
            for entry in entries:
                with source_file(source_descriptor, entry.name, snapshots[entry.name]):
                    pass
            if git_records(source) != records:
                raise StageError("Git index changed during source staging.")
            with absolute_directory(source) as current:
                current_identity = os.fstat(current)
                if ((current_identity.st_dev, current_identity.st_ino)
                        != (source_identity.st_dev, source_identity.st_ino)):
                    raise StageError("Source root changed during staging.")
            os.fsync(destination_descriptor)
            if manifest is not None:
                write_manifest(manifest, destination, version, files, archive_layout)
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, help="Physical absolute Git checkout root")
    parser.add_argument("--destination", required=True, help="New or empty, owned physical staging directory")
    parser.add_argument("--expected-version", help="Reject a VERSION differing from CPack configuration")
    parser.add_argument("--manifest", help="Exclusive sibling file for the staged content manifest")
    parser.add_argument("--archive-layout", choices=("cpack", "shell"), default="cpack",
                        help="Expected source ZIP root recorded in the manifest")
    args = parser.parse_args()
    try:
        version = stage(args.source_root, args.destination, args.expected_version,
                        args.manifest, args.archive_layout)
    except (StageError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("Source staging refused: " + str(error), file=sys.stderr)
        return 1
    print(version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
