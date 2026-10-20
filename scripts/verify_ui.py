#!/usr/bin/env python3
"""Run the opt-in graphical worker and validate its portable JSON/PNG evidence."""

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib


SCENES = {"Splash", "HighScores", "Playing", "Win", "GameOver"}
CONTROLS = {
    "splash-start", "splash-scores", "scores-back", "playing-new", "playing-menu",
    "playing-sound", "win-continue", "win-new", "over-replay", "over-menu",
}
INPUT_EDGES = {
    "E11-01-enter", "E11-01-new", "E11-01-direction", "E11-01-text", "E11-01-blur",
}
CASES = {
    "splash-normal", "splash-long-turkish", "playing-normal", "playing-wide",
    "win-wide", "gameover-wide", "tiles-and-cards", "motion-start", "motion-end",
    "reveal-start", "reveal-end", "name-focus", "name-empty-error", "name-18-limit",
    "playing-sound-off", *(f"scores-{rows}" for rows in range(6)),
    *(f"control-{owner}-{state}" for owner in CONTROLS for state in ("focus", "hover", "pressed")),
}


def check_png(path, width, height):
    """Validate PNG framing, dimensions and every chunk CRC without image libraries."""
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError(f"Oversized PNG: {path.name}")
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"Invalid PNG signature: {path.name}")
    offset, first, image_data, ended = 8, True, bytearray(), False
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError(f"Truncated PNG: {path.name}")
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 12 + length
        if end > len(data):
            raise ValueError(f"Truncated PNG chunk: {path.name}")
        payload = data[offset + 8:offset + 8 + length]
        checksum = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if zlib.crc32(kind + payload) & 0xFFFFFFFF != checksum:
            raise ValueError(f"PNG checksum mismatch: {path.name}")
        if first:
            if kind != b"IHDR" or length != 13 or struct.unpack_from(">II", payload) != (width, height):
                raise ValueError(f"PNG dimensions/header mismatch: {path.name}")
            if payload[8:] != bytes((8, 6, 0, 0, 0)):
                raise ValueError(f"Unsupported PNG encoding: {path.name}")
            first = False
        elif kind == b"IHDR":
            raise ValueError(f"Duplicate PNG header: {path.name}")
        if kind == b"IDAT":
            image_data.extend(payload)
        if kind == b"IEND":
            if length or end != len(data):
                raise ValueError(f"Invalid PNG ending: {path.name}")
            ended = True
        offset = end
    if not ended or not image_data:
        raise ValueError(f"Incomplete PNG: {path.name}")
    stride, expected = width * 4 + 1, height * (width * 4 + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(image_data, expected + 1)
    if len(raw) != expected or not decoder.eof or decoder.unused_data or any(raw[row * stride] > 4 for row in range(height)):
        raise ValueError(f"Invalid PNG pixel stream: {path.name}")


def verify(report, output, font):
    if not isinstance(report, dict):
        raise ValueError("Report must be a JSON object")
    if report.get("schema_version") != 1 or report.get("status") != "passed":
        raise ValueError("Worker did not report a successful schema-1 run")
    if type(report.get("checks")) is not int or report["checks"] < 100:
        raise ValueError("Missing acceptance assertions")
    if len(report.get("scenes", [])) != 5 or set(report.get("scenes", [])) != SCENES or report.get("densities") != [1, 2]:
        raise ValueError("Incomplete scene/density matrix")
    if Path(report.get("font", "")).resolve() != font:
        raise ValueError("Worker used a different font")
    controls = report.get("controls", [])
    if not isinstance(controls, list) or not all(isinstance(c, dict) and type(c.get("density")) is int for c in controls) or len(controls) != 20 or {(c.get("name"), c.get("density")) for c in controls} != {
        (name, density) for name in CONTROLS for density in (1, 2)
    }:
        raise ValueError("Incomplete or duplicated input-control matrix")
    visual = report.get("visual_controls", [])
    if not isinstance(visual, list) or len(visual) != 20 or not all(isinstance(c, dict) for c in visual) or {(c.get("name"), c.get("density")) for c in visual} != {(name, density) for name in CONTROLS for density in (1, 2)}:
        raise ValueError("Incomplete or duplicated visual-control matrix")
    for c in visual:
        if type(c.get("density")) is not int or c.get("focus_outline") is not True or c.get("hover_palette") is not True or c.get("pressed_geometry") != ("unchanged-row" if c["name"] == "playing-sound" else "0.98"):
            raise ValueError("Incomplete focus/hover/press visual acceptance")
    ink = report.get("validation_ink", [])
    if not isinstance(ink, list) or len(ink) != 2 or not all(isinstance(c, dict) and type(c.get("density")) is int and type(c.get("error_pixels")) is int and c["error_pixels"] >= 4 * c["density"] for c in ink) or {c["density"] for c in ink} != {1, 2}:
        raise ValueError("Incomplete visible validation-ink acceptance")
    outcome = report.get("outcomes", {})
    if outcome.get("legal_first_win_score") != 21196 or outcome.get("new_round_win") is not True or outcome.get("terminal_priority") is not True:
        raise ValueError("Incomplete round/outcome acceptance")
    semantics = report.get("semantics", {})
    if semantics.get("valid_scene_snapshots") != 5 or any(semantics.get(key) is not True for key in ("stale_requests_rejected", "bulk_name_guards", "held_key_guard")):
        raise ValueError("Incomplete semantic guard acceptance")
    input_edges = report.get("input_edges", [])
    if not isinstance(input_edges, list) or len(input_edges) != len(INPUT_EDGES) or not all(isinstance(c, dict) for c in input_edges) or {c.get("id") for c in input_edges} != INPUT_EDGES:
        raise ValueError("Incomplete or duplicated keyboard-edge matrix")
    for edge in input_edges:
        if edge.get("status") != "passed" or edge.get("layer") != "controlled-guard-scenes" or type(edge.get("checks")) is not int or edge["checks"] < 1:
            raise ValueError("Incomplete controlled keyboard-edge acceptance")
    artifacts = report.get("images", [])
    expected = {f"{case}-{density}x.png" for case in CASES for density in (1, 2)}
    if not isinstance(artifacts, list) or not all(isinstance(entry, dict) for entry in artifacts) or len(artifacts) != len(expected) or {entry.get("path") for entry in artifacts} != expected:
        raise ValueError("Incomplete or duplicated render artifacts")
    for entry in artifacts:
        filename, density = entry["path"], entry.get("density")
        if type(density) is not int or density not in (1, 2) or not filename.endswith(f"-{density}x.png") or (entry.get("width"), entry.get("height")) != (920 * density, 660 * density):
            raise ValueError(f"Invalid artifact dimensions: {filename}")
        path = output / filename
        if path.is_symlink() or path.resolve().parent != output or not path.is_file():
            raise ValueError(f"Artifact escaped output directory or is missing: {filename}")
        check_png(path, entry["width"], entry["height"])
    if not isinstance(report.get("scope"), str) or not report["scope"]:
        raise ValueError("Missing acceptance scope")
    return len(artifacts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", type=Path, required=True)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    worker, font, output = args.worker.resolve(), args.font.resolve(), args.output.resolve()
    try:
        if not worker.is_file() or not os.access(worker, os.X_OK):
            raise ValueError("Worker is missing or not executable")
        if not font.is_file() or not os.access(font, os.R_OK):
            raise ValueError("Font is missing or unreadable")
        output.mkdir(parents=True, exist_ok=True)
        report_path = output / "ui-acceptance.json"
        for filename in {"ui-acceptance.json", "worker.log", *(f"{case}-{density}x.png" for case in CASES for density in (1, 2))}:
            if (output / filename).is_symlink():
                raise ValueError(f"Output path is a symlink: {filename}")
        previous = report_path.stat().st_mtime_ns if report_path.exists() else None
        with (output / "worker.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(
                [str(worker), "--font", str(font), "--output", str(output)],
                stdout=log, stderr=subprocess.STDOUT, timeout=90, check=False,
            )
        if result.returncode:
            raise ValueError(f"Worker exited {result.returncode}; see {output / 'worker.log'}")
        if not report_path.is_file() or report_path.is_symlink() or report_path.stat().st_mtime_ns == previous:
            raise ValueError("Worker did not write a fresh report")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        count = verify(report, output, font)
        print(f"UI acceptance passed: {report['checks']} checks, {count} PNGs, five scenes at 1x/2x")
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired, zlib.error) as error:
        print(f"UI acceptance failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
