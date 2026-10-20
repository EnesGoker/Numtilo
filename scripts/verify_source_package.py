#!/usr/bin/env python3
"""Verify final source ZIPs against the trusted, successfully staged snapshot."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
import re
import stat
import struct
import sys
import unicodedata
import zipfile
import zlib

sys.dont_write_bytecode = True
import stage_source as staging


class VerificationError(Exception):
    """A source archive or its snapshot cannot safely be accepted."""


MAX_MANIFEST = 16 * 1024 * 1024
MAX_FILES = 100000
MAX_FILE_SIZE = 1024 * 1024 * 1024
MAX_TOTAL_SIZE = 4 * MAX_FILE_SIZE
MAX_ARCHIVE_SIZE = 8 * MAX_FILE_SIZE
MAX_CENTRAL_SIZE = 128 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise VerificationError(message)


@contextmanager
def secure_file(value, manifest=False):
    path = staging.absolute_path(value)
    with staging.absolute_directory(path.parent) as parent:
        with staging.source_file(parent, path.name) as (descriptor, identity):
            info = os.fstat(descriptor)
            if manifest:
                require(info.st_uid == os.geteuid() and not info.st_mode & 0o022,
                        "Manifest must be owned by this user and not writable by others.")
            with os.fdopen(os.dup(descriptor), "rb") as stream:
                yield stream, info.st_size
            require(staging.fingerprint(os.fstat(descriptor)) == identity,
                    "Package input changed while verifying: " + str(path))
            with staging.absolute_directory(path.parent) as current:
                linked = os.stat(path.name, dir_fd=current, follow_symlinks=False)
                require(staging.fingerprint(linked) == identity,
                        "Package input path changed while verifying: " + str(path))


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def invalid_constant(value):
    raise VerificationError("Invalid JSON constant: " + value)


def portable_name(name, spellings):
    require(isinstance(name, str), "Package path must be a string.")
    staging.validate_name(name)
    parts = name.split("/")
    require(len(name.encode("utf-8")) <= 4096
            and all(len(part.encode("utf-8")) <= 255 for part in parts),
            "Package path exceeds portable length bounds: " + name)
    require(not any(part.casefold().startswith("._") or part.casefold() == "__macosx"
                    for part in parts), "Forbidden metadata path: " + name)
    for end in range(1, len(parts) + 1):
        prefix = "/".join(parts[:end])
        key = unicodedata.normalize("NFC", prefix).casefold()
        previous = spellings.setdefault(key, prefix)
        require(previous == prefix, "Case/Unicode-colliding package path: " + name)


def read_manifest(path):
    with secure_file(path, manifest=True) as (stream, size):
        require(0 < size <= MAX_MANIFEST, "Manifest size exceeds the supported bounds.")
        data = stream.read(MAX_MANIFEST + 1)
    manifest = json.loads(data.decode("utf-8"), object_pairs_hook=unique_object,
                          parse_constant=invalid_constant)
    require(isinstance(manifest, dict)
            and set(manifest) == {"schema", "version", "root", "files"},
            "Invalid manifest fields.")
    require(type(manifest["schema"]) is int and manifest["schema"] == 1,
            "Unsupported manifest schema.")
    version = manifest["version"]
    require(isinstance(version, str) and staging.VERSION_PATTERN.fullmatch(version),
            "Invalid manifest version.")
    require(manifest["root"] in {"Numtilo-" + version + "-Source", "Numtilo-" + version},
            "Invalid archive root.")
    files = manifest["files"]
    require(isinstance(files, dict) and 0 < len(files) <= MAX_FILES,
            "Invalid manifest file count.")
    spellings, directories, total = {}, set(), 0
    for name, entry in files.items():
        portable_name(name, spellings)
        require(isinstance(entry, dict) and set(entry) == {"sha256", "size", "mode"},
                "Invalid manifest file fields: " + name)
        require(isinstance(entry["sha256"], str)
                and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]),
                "Invalid manifest SHA256: " + name)
        require(type(entry["size"]) is int and 0 <= entry["size"] <= MAX_FILE_SIZE,
                "Invalid manifest file size: " + name)
        require(type(entry["mode"]) is int and entry["mode"] in {0o644, 0o755},
                "Invalid manifest file mode: " + name)
        total += entry["size"]
        parts = name.split("/")
        directories.update("/".join(parts[:end]) for end in range(1, len(parts)))
    require(total <= MAX_TOTAL_SIZE, "Manifest total size exceeds the supported bounds.")
    require(not directories.intersection(files), "Manifest has a file/directory collision.")
    require(staging.REQUIRED_FILES.issubset(files), "Manifest omits required source files.")
    return manifest, directories


def check_extra(extra, raw_name, name, central, zip64=b""):
    seen = set()
    while extra:
        require(len(extra) >= 4, "Truncated ZIP extra field: " + name)
        kind, size = struct.unpack_from("<HH", extra)
        require(kind not in seen and len(extra) >= 4 + size, "Invalid ZIP extra field: " + name)
        seen.add(kind)
        value, extra = extra[4:4 + size], extra[4 + size:]
        if kind == 0x0001:  # Only the fields required by header sentinels, in order.
            require(bool(zip64) and value == zip64, "Invalid or unused ZIP64 extra: " + name)
        elif kind == 0x5455:  # Extended timestamps; central records may carry only mtime.
            require(size >= 1 and not value[0] & ~7, "Invalid timestamp extra: " + name)
            count = (1 if value[0] & 1 else 0) if central else bin(value[0]).count("1")
            require(size == 1 + 4 * count, "Invalid timestamp length: " + name)
        elif kind == 0x7075:  # Unicode path must not introduce an alternate spelling.
            require(size >= 5 and value[0] == 1
                    and struct.unpack_from("<I", value, 1)[0] == zlib.crc32(raw_name)
                    and value[5:].decode("utf-8") == name,
                    "Invalid Unicode path extra: " + name)
        elif kind == 0x7875:  # CPack's Unix UID/GID field, not extended attributes.
            require(size >= 5 and value[0] == 1 and 1 <= value[1] <= 8,
                    "Invalid Unix identity extra: " + name)
            end = 2 + value[1]
            require(end < size and 1 <= value[end] <= 8 and end + 1 + value[end] == size,
                    "Invalid Unix identity length: " + name)
        else:
            raise VerificationError("Unsupported ZIP metadata extra: " + hex(kind) + " in " + name)
    require(not zip64 or 0x0001 in seen, "Missing ZIP64 extra: " + name)


def check_zip_bounds(stream, size, max_entries):
    require(22 <= size <= MAX_ARCHIVE_SIZE, "ZIP size exceeds the supported bounds.")
    stream.seek(max(0, size - 65557))
    tail = stream.read(65557)
    offset = tail.rfind(b"PK\x05\x06")
    require(offset >= 0 and len(tail) - offset >= 22, "ZIP end record is missing.")
    end = struct.unpack_from("<4s4H2IH", tail, offset)
    require(end[1] == end[2] == 0 and end[3] == end[4]
            and offset + 22 + end[7] == len(tail), "Invalid or multi-disk ZIP end record.")
    count, central_size, central_offset = end[4:7]
    record_start = size - len(tail) + offset
    if count == 0xffff or central_size == 0xffffffff or central_offset == 0xffffffff:
        end_offset = size - len(tail) + offset
        require(end_offset >= 20, "ZIP64 locator is missing.")
        stream.seek(end_offset - 20)
        locator = struct.unpack("<4sIQI", stream.read(20))
        require(locator[0] == b"PK\x06\x07" and locator[1] == 0 and locator[3] == 1
                and locator[2] + 56 <= end_offset - 20, "Invalid ZIP64 locator.")
        stream.seek(locator[2])
        record = struct.unpack("<4sQ2H2I4Q", stream.read(56))
        require(record[0] == b"PK\x06\x06" and record[1] == 44
                and locator[2] + 56 == end_offset - 20
                and record[4] == record[5] == 0 and record[6] == record[7],
                "Invalid ZIP64 end record.")
        count, central_size, central_offset = record[7:10]
        record_start = locator[2]
    require(0 < count <= max_entries and central_size <= MAX_CENTRAL_SIZE
            and central_offset + central_size == record_start,
            "ZIP directory exceeds supported bounds or contains unsupported records.")
    stream.seek(0)
    return count, central_offset, central_size


def check_central_header(stream, position, info):
    stream.seek(position)
    header = stream.read(46)
    require(len(header) == 46, "Truncated ZIP central header.")
    values = struct.unpack("<4s6H3I5H2I", header)
    require(values[0] == b"PK\x01\x02" and values[3] == info.flag_bits
            and values[4] == info.compress_type and values[7] == info.CRC
            and values[12] == 0 and values[15] == info.external_attr,
            "Invalid ZIP central header.")
    raw_name, extra = stream.read(values[10]), stream.read(values[11])
    name = raw_name.decode("utf-8" if info.flag_bits & 0x800 else "cp437")
    require(name == info.orig_filename and extra == info.extra, "ZIP central record mismatch.")
    expected = b""
    for value, actual in ((values[9], info.file_size), (values[8], info.compress_size),
                          (values[16], info.header_offset)):
        if value == 0xffffffff:
            expected += struct.pack("<Q", actual)
        else:
            require(value == actual, "ZIP central size/offset mismatch.")
    require(values[13] in {0, 0xffff}, "Multi-disk ZIP member is unsupported.")
    if values[13] == 0xffff:
        expected += struct.pack("<I", 0)
    check_extra(extra, raw_name, name, central=True, zip64=expected)
    return stream.tell()


def check_local_header(stream, info):
    stream.seek(info.header_offset)
    header = stream.read(30)
    require(len(header) == 30, "Truncated ZIP local header.")
    values = struct.unpack("<4s5H3I2H", header)
    require(values[0] == b"PK\x03\x04" and values[2] == info.flag_bits
            and values[3] == info.compress_type, "ZIP local/central header mismatch.")
    raw_name = stream.read(values[-2])
    extra = stream.read(values[-1])
    require(len(raw_name) == values[-2] and len(extra) == values[-1], "Truncated ZIP header data.")
    name = raw_name.decode("utf-8" if info.flag_bits & 0x800 else "cp437")
    require(name == info.orig_filename, "ZIP local/central path mismatch.")
    descriptor = bool(info.flag_bits & 8)
    require(values[6] in ({0, info.CRC} if descriptor else {info.CRC}),
            "ZIP local/central CRC mismatch.")
    expected = b""
    for value, actual in ((values[8], info.file_size), (values[7], info.compress_size)):
        if value == 0xffffffff:
            expected += struct.pack("<Q", actual)
        else:
            require(value in ({0, actual} if descriptor else {actual}),
                    "ZIP local/central size mismatch.")
    check_extra(extra, raw_name, name, central=False, zip64=expected)
    data_start = stream.tell()
    end = data_start + info.compress_size
    if descriptor:
        stream.seek(end)
        first = stream.read(4)
        if first == b"PK\x07\x08":
            first = stream.read(4)
            end += 4
        require(len(first) == 4 and struct.unpack("<I", first)[0] == info.CRC,
                "ZIP data descriptor CRC mismatch.")
        fmt = "<2Q" if expected else "<2I"
        size = struct.calcsize(fmt)
        require(struct.unpack(fmt, stream.read(size)) == (info.compress_size, info.file_size),
                "ZIP data descriptor size mismatch.")
        end += 4 + size
    return info.header_offset, end, data_start


def check_compressed_payload(stream, info, data_start):
    # ZipFile truncates output to the central size. Also check the actual raw
    # stream, so appended compressed data or extra decompressed bytes cannot hide.
    if info.compress_type == zipfile.ZIP_STORED:
        require(info.compress_size == info.file_size, "Stored ZIP size mismatch.")
        return
    stream.seek(data_start)
    decoder, remaining, produced = zlib.decompressobj(-15), info.compress_size, 0
    while remaining:
        data = stream.read(min(65536, remaining))
        require(bool(data), "Truncated ZIP compressed payload.")
        remaining -= len(data)
        while data:
            output = decoder.decompress(data, min(1024 * 1024, info.file_size - produced + 1))
            produced += len(output)
            require(produced <= info.file_size, "ZIP payload exceeds its declared size.")
            require(not decoder.unused_data, "ZIP payload contains trailing compressed data.")
            data = decoder.unconsumed_tail
        require(not decoder.eof or not remaining, "ZIP payload contains unused compressed bytes.")
    require(decoder.eof and produced == info.file_size, "ZIP compressed stream size/end mismatch.")


def verify_archive(path, manifest, directories):
    files, root = manifest["files"], manifest["root"]
    with secure_file(path) as (stream, size):
        count, central, central_size = check_zip_bounds(stream, size, len(files) + len(directories) + 1)
        with zipfile.ZipFile(stream) as archive:
            require(not archive.comment, "ZIP archive comments are unsupported metadata.")
            members = archive.infolist()
            require(len(members) == count, "ZIP entry count mismatch.")
            seen, spellings, found, spans = set(), {}, set(), []
            position = central
            for info in members:
                name = info.orig_filename
                require(name == info.filename and not info.comment, "Invalid ZIP member name/comment.")
                directory = name.endswith("/")
                canonical = name[:-1] if directory else name
                require(canonical not in seen, "Duplicate ZIP member: " + name)
                seen.add(canonical)
                portable_name(canonical, spellings)
                require(canonical == root or canonical.startswith(root + "/"), "Wrong ZIP root: " + name)
                relative = canonical[len(root) + 1:] if canonical != root else ""
                require(info.create_system == 3 and info.flag_bits & ~0x0808 == 0
                        and info.compress_type in {zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED},
                        "Unsupported ZIP creator, flags or compression: " + name)
                mode = info.external_attr >> 16
                if directory:
                    require((not relative or relative in directories) and mode == (stat.S_IFDIR | 0o755)
                            and info.file_size == 0,
                            "Unexpected ZIP directory/type/mode: " + name)
                    expected = None
                else:
                    require(relative in files, "Unexpected ZIP file: " + name)
                    expected = files[relative]
                    require(mode == (stat.S_IFREG | expected["mode"])
                            and info.file_size == expected["size"], "ZIP file type/mode/size mismatch: " + name)
                    found.add(relative)
                position = check_central_header(stream, position, info)
                start, end, data_start = check_local_header(stream, info)
                spans.append((start, end))
                check_compressed_payload(stream, info, data_start)
                digest, count, version_data = hashlib.sha256(), 0, bytearray()
                with archive.open(info) as member:
                    while True:
                        chunk = member.read(min(1024 * 1024, info.file_size - count + 1))
                        if not chunk:
                            break
                        count += len(chunk)
                        require(count <= info.file_size, "ZIP member exceeds expected size: " + name)
                        digest.update(chunk)
                        if relative == "VERSION":
                            require(count <= 4096, "Archived VERSION is too large.")
                            version_data.extend(chunk)
                require(count == info.file_size, "Truncated ZIP member: " + name)
                if expected:
                    require(digest.hexdigest() == expected["sha256"], "ZIP SHA256 mismatch: " + name)
                if relative == "VERSION":
                    require(version_data.decode("ascii").strip(" \t\r\n") == manifest["version"],
                            "Archived VERSION differs from the staged snapshot version.")
            require(found == set(files), "ZIP omits expected source files.")
            require(position == central + central_size, "ZIP central directory has unused bytes.")
            end = 0
            for start, next_end in sorted(spans):
                require(start == end and next_end >= start, "ZIP has overlapping or unused local bytes.")
                end = next_end
            require(end == central, "ZIP has a prefix or unused data before its directory.")


def verify(manifest, archives):
    try:
        staging.require_secure_platform()
        require(bool(archives), "At least one source ZIP is required.")
        snapshot, directories = read_manifest(manifest)
        for path in archives:
            verify_archive(path, snapshot, directories)
        return snapshot["version"]
    except (staging.StageError, OSError, ValueError, TypeError, struct.error,
            zipfile.BadZipFile, RuntimeError, EOFError, zlib.error) as error:
        raise VerificationError(str(error)) from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, help="Trusted absolute staging manifest path")
    parser.add_argument("--archive", action="append", required=True, help="Final absolute source ZIP path")
    args = parser.parse_args()
    try:
        version = verify(args.manifest, args.archive)
    except VerificationError as error:
        print("Source package verification refused: " + str(error), file=sys.stderr)
        return 1
    print(version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
