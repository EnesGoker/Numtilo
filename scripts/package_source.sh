#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -P -- "$(dirname "$0")/.." && pwd -P)"

fail() {
  printf 'Source package refused: %s\n' "$1" >&2
  exit 1
}

valid_output_name() {
  local name="$1"
  case "$name" in
    ""|.zip|-*|*/*|*\\*|*..*|*:*|*'<'*|*'>'*|*'"'*|*'|'*|*'?'*|*'*'*) return 1 ;;
  esac
  [[ "$name" == *.zip && "$name" != *[[:cntrl:]]* ]]
}

[[ $# -le 1 ]] || fail "Expected at most one ZIP basename argument."
if [[ $# -eq 1 ]]; then
  valid_output_name "$1" || fail "Output must be a safe .zip basename."
  [[ ! -e "$project_root/$1" && ! -L "$project_root/$1" ]] || fail "Output already exists."
fi

for required_cmd in python3 mktemp mv; do
  if ! command -v "$required_cmd" >/dev/null 2>&1; then
    fail "Missing required command: $required_cmd"
  fi
done

cd "$project_root"
umask 077

tmp_dir=""
tmp_identity=""
manage_temporary_directory() {
  python3 - "$project_root" "$tmp_dir" "$1" "$tmp_identity" "${2-}" <<'PY'
import os
import re
import shutil
import stat
import sys
import time
import zipfile

root, temporary, operation, recorded, version = sys.argv[1:]
if not all(hasattr(os, name) for name in ("O_DIRECTORY", "O_NOFOLLOW")):
    raise SystemExit("Source-package temporary handling requires no-follow POSIX operations.")
flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW


def open_root(path):
    if not os.path.isabs(path) or os.path.normpath(path) != path:
        raise ValueError("Expected a physical absolute project root.")
    descriptor = os.open(os.path.sep, flags)
    try:
        for component in path.split(os.path.sep)[1:]:
            child = os.open(component, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


root_fd = temporary_fd = None
try:
    name = os.path.basename(temporary)
    if os.path.dirname(temporary) != root or not name.startswith(".numtilo-source-"):
        raise ValueError("Temporary directory is outside its owned root.")
    root_fd = open_root(root)
    root_status = os.fstat(root_fd)
    temporary_fd = os.open(name, flags, dir_fd=root_fd)
    temporary_status = os.fstat(temporary_fd)
    mode = stat.S_IMODE(temporary_status.st_mode)
    if (temporary_status.st_uid != os.geteuid() or mode != 0o700
            or temporary_status.st_dev != root_status.st_dev):
        raise ValueError("Temporary directory is not owned, private and on the output filesystem.")
    identity = (root_status.st_dev, root_status.st_ino, temporary_status.st_dev,
                temporary_status.st_ino, temporary_status.st_uid, mode)
    if operation == "record":
        print(":".join(map(str, identity)))
    elif operation in {"cleanup", "archive"}:
        if identity != tuple(int(value) for value in recorded.split(":")):
            raise ValueError("Temporary directory identity changed; operation refused.")
        if operation == "cleanup":
            if not getattr(shutil.rmtree, "avoids_symlink_attacks", False):
                raise ValueError("Descriptor-safe recursive cleanup is unavailable.")
            # Anchor traversal to the initially owned inode, even if its root name moves.
            os.fchdir(temporary_fd)
            for child in os.listdir(temporary_fd):
                status = os.stat(child, dir_fd=temporary_fd, follow_symlinks=False)
                if stat.S_ISDIR(status.st_mode):
                    shutil.rmtree(child)
                else:
                    os.unlink(child, dir_fd=temporary_fd)
            named = os.stat(name, dir_fd=root_fd, follow_symlinks=False)
            if (named.st_dev, named.st_ino) != (temporary_status.st_dev, temporary_status.st_ino):
                raise ValueError("Temporary directory name changed; final removal refused.")
            os.rmdir(name, dir_fd=root_fd)
        else:
            if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version, flags=re.ASCII):
                raise ValueError("Archive version must follow the existing X.Y.Z contract.")

            def fingerprint(status):
                return (status.st_dev, status.st_ino, status.st_mode, status.st_nlink,
                        status.st_size, status.st_mtime_ns, status.st_ctime_ns)

            def zip_info(path, status, directory=False):
                info = zipfile.ZipInfo(path + ("/" if directory else ""),
                                       date_time=time.localtime(status.st_mtime)[:6])
                info.create_system = 3
                mode = (stat.S_IFDIR | 0o755) if directory else (
                    stat.S_IFREG | (0o755 if status.st_mode & 0o111 else 0o644))
                info.external_attr = (mode << 16) | (0x10 if directory else 0)
                info.compress_type = zipfile.ZIP_STORED if directory else zipfile.ZIP_DEFLATED
                info.file_size = 0 if directory else status.st_size
                return info

            def add_directory(archive, descriptor, path):
                status = os.fstat(descriptor)
                archive.writestr(zip_info(path, status, directory=True), b"")
                for child in sorted(os.listdir(descriptor)):
                    if child in {".", ".."} or "/" in child or "\\" in child:
                        raise ValueError("Invalid staged archive component.")
                    before = os.stat(child, dir_fd=descriptor, follow_symlinks=False)
                    if stat.S_ISDIR(before.st_mode):
                        child_fd = os.open(child, flags, dir_fd=descriptor)
                        try:
                            if fingerprint(os.fstat(child_fd)) != fingerprint(before):
                                raise ValueError("Staged directory changed before archiving.")
                            add_directory(archive, child_fd, path + "/" + child)
                        finally:
                            os.close(child_fd)
                    elif stat.S_ISREG(before.st_mode) and before.st_nlink == 1:
                        child_fd = os.open(child, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                                           dir_fd=descriptor)
                        try:
                            opened = os.fstat(child_fd)
                            if fingerprint(opened) != fingerprint(before):
                                raise ValueError("Staged file changed before archiving.")
                            with archive.open(zip_info(path + "/" + child, opened), "w") as output:
                                while True:
                                    data = os.read(child_fd, 1024 * 1024)
                                    if not data:
                                        break
                                    output.write(data)
                            named = os.stat(child, dir_fd=descriptor, follow_symlinks=False)
                            if (fingerprint(os.fstat(child_fd)) != fingerprint(opened)
                                    or fingerprint(named) != fingerprint(opened)):
                                raise ValueError("Staged file changed during archiving.")
                        finally:
                            os.close(child_fd)
                    else:
                        raise ValueError("Staged archive input must be normal, non-hardlinked files/directories.")

            package_name = "Numtilo-" + version
            package_fd = os.open(package_name, flags, dir_fd=temporary_fd)
            try:
                archive_fd = os.open("archive.zip", os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                     0o600, dir_fd=temporary_fd)
                with os.fdopen(archive_fd, "w+b") as destination:
                    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                        # Explicit ZipInfo fields omit OS xattrs; zipfile sets UTF-8/EFS for Unicode names.
                        add_directory(archive, package_fd, package_name)
            finally:
                os.close(package_fd)
    else:
        raise ValueError("Unknown temporary-directory operation.")
except (OSError, ValueError, NotImplementedError) as error:
    raise SystemExit(f"Source-package temporary handling refused: {error}")
finally:
    for descriptor in (temporary_fd, root_fd):
        if descriptor is not None:
            os.close(descriptor)
PY
}

cleanup() {
  local exit_status=$?
  trap - EXIT
  # Only this invocation's mktemp directory may be removed, never a caller path.
  if [[ -n "$tmp_dir" && "$tmp_dir" == "$project_root"/.numtilo-source-* &&
        -d "$tmp_dir" && ! -L "$tmp_dir" ]]; then
    if [[ -n "$tmp_identity" ]]; then
      manage_temporary_directory cleanup || printf 'Could not remove verified source-package temporary directory.\n' >&2
    else
      printf 'Temporary directory identity was not recorded; cleanup refused.\n' >&2
    fi
  fi
  exit "$exit_status"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

# Keep the private ZIP and its final destination on the same filesystem.
tmp_dir="$(mktemp -d "$project_root/.numtilo-source-XXXXXXXX")"
[[ -d "$tmp_dir" && ! -L "$tmp_dir" ]] || fail "Could not create an ordinary temporary directory."
tmp_identity="$(manage_temporary_directory record)"

# The selector validates VERSION through its guarded tracked-file reader. Do not
# read the original VERSION with a shell redirection that could follow a link.
version="$(python3 "$project_root/scripts/stage_source.py" \
  --source-root "$project_root" --destination "$tmp_dir/source" \
  --manifest "$tmp_dir/source.numtilo-manifest.json" --archive-layout shell)"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Selector returned an invalid X.Y.Z version."
output_name="${1-Numtilo-${version}-source.zip}"
valid_output_name "$output_name" || fail "Output must be a safe .zip basename."
[[ ! -e "$project_root/$output_name" && ! -L "$project_root/$output_name" ]] || fail "Output already exists."

# This rename stays entirely inside the new private temporary directory.
mv "$tmp_dir/source" "$tmp_dir/Numtilo-${version}"

# Use the standard ZIP writer: it preserves Unicode names with EFS metadata and
# reads only the private selector output, without OS xattrs or ZIPOPT options.
manage_temporary_directory archive "$version"

verified_version="$(python3 -B "$project_root/scripts/verify_source_package.py" \
  --manifest "$tmp_dir/source.numtilo-manifest.json" --archive "$tmp_dir/archive.zip")"
[[ "$verified_version" == "$version" ]] || fail "ZIP verification returned an unexpected version."

# Exact-path hard-link publication cannot replace an existing file, directory or
# symlink, including one created after the early output checks.
python3 - "$project_root" "$tmp_dir" "$output_name" "$tmp_identity" <<'PY'
import os
import stat
import sys
import unicodedata

root, temporary, output, recorded = sys.argv[1:]
if (not output or output == ".zip" or not output.endswith(".zip")
        or output.startswith("-") or ".." in output
        or any(char in '/\\:<>"|?*' for char in output)
        or any(unicodedata.category(char) == "Cc" for char in output)):
    raise SystemExit("Source package refused: unsafe output basename.")

if not all(hasattr(os, name) for name in ("O_DIRECTORY", "O_NOFOLLOW", "O_NONBLOCK")):
    raise SystemExit("Source package refused: no-follow POSIX file operations are required.")

directory_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW


def open_absolute_directory(path):
    if not os.path.isabs(path) or os.path.normpath(path) != path:
        raise ValueError("Expected a physical absolute project root.")
    descriptor = os.open(os.path.sep, directory_flags)
    try:
        for component in path.split(os.path.sep)[1:]:
            child = os.open(component, directory_flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


root_fd = temporary_fd = archive_fd = None
try:
    temporary_name = os.path.basename(temporary)
    if os.path.dirname(temporary) != root or not temporary_name.startswith(".numtilo-source-"):
        raise ValueError("Temporary directory is outside its owned root.")
    root_fd = open_absolute_directory(root)
    root_status = os.fstat(root_fd)
    temporary_fd = os.open(temporary_name, directory_flags, dir_fd=root_fd)
    temporary_status = os.fstat(temporary_fd)
    if (temporary_status.st_uid != os.geteuid()
            or stat.S_IMODE(temporary_status.st_mode) != 0o700
            or temporary_status.st_dev != root_status.st_dev):
        raise ValueError("Temporary directory must be owned, private and on the output filesystem.")
    identity = (root_status.st_dev, root_status.st_ino, temporary_status.st_dev,
                temporary_status.st_ino, temporary_status.st_uid,
                stat.S_IMODE(temporary_status.st_mode))
    if identity != tuple(int(value) for value in recorded.split(":")):
        raise ValueError("Temporary directory identity changed before publication.")

    archive_fd = os.open("archive.zip", os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=temporary_fd)
    archive_status = os.fstat(archive_fd)
    if (not stat.S_ISREG(archive_status.st_mode) or archive_status.st_nlink != 1
            or archive_status.st_uid != os.geteuid()
            or archive_status.st_dev != root_status.st_dev):
        raise ValueError("Temporary archive must be an owned, singly linked regular file.")
    named_status = os.stat("archive.zip", dir_fd=temporary_fd, follow_symlinks=False)
    if (named_status.st_dev, named_status.st_ino) != (archive_status.st_dev, archive_status.st_ino):
        raise ValueError("Temporary archive changed before publication.")

    os.link("archive.zip", output, src_dir_fd=temporary_fd, dst_dir_fd=root_fd,
            follow_symlinks=False)
except (OSError, ValueError, NotImplementedError) as error:
    raise SystemExit(f"Source package refused: {error}")
finally:
    for descriptor in (archive_fd, temporary_fd, root_fd):
        if descriptor is not None:
            os.close(descriptor)
PY

printf 'Created source package: %s/%s\n' "$project_root" "$output_name"
