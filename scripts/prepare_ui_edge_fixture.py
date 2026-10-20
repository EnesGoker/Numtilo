#!/usr/bin/env python3
"""Prepare/compile isolated current-App step 11.2/11.3 fixtures; never launch them.

The two E11-02 cases call App's current routeAccessible for a controlled request
vector, bypassing bridge callbacks/takeActions. E11-03 records the real App loop
for a separately authorized native checkpoint; preparation cannot pass it.
E11-04/05/06 use controlled events in App's unchanged routing/Closed loop body,
with real public final moves/clocks and a scoped production storage delegate.
Only the existing local Makefiles numtilo compile/link metadata is supported.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shlex
import shutil
import stat
import subprocess
import sys


CASES = ("e11-02-playing-batch", "e11-02-win-batch", "e11-03-lifecycle",
         "e11-04-win-active-close", "e11-04-win-settled-close",
         "e11-04-terminal-active-close", "e11-04-terminal-settled-close",
         "e11-05-new-before-close", "e11-05-menu-before-close",
         "e11-05-close-before-new", "e11-05-close-before-menu",
         "e11-06-new-recovery-close", "e11-06-menu-recovery-close",
         "e11-06-new-persistent-close", "e11-06-menu-persistent-close")
ROOT = Path(__file__).absolute().parent.parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def ordinary_directory_chain(path):
    """Inspect each lexical ancestor without resolve() hiding a symlink."""
    if not path.is_absolute() or Path(os.path.normpath(path)) != path:
        raise ValueError(f"Path must be absolute and normalized: {path}")
    current = Path(path.anchor)
    for component in path.parts[1:]:
        current /= component
        mode = current.lstat().st_mode
        if stat.S_ISLNK(mode) or not stat.S_ISDIR(mode):
            raise ValueError(f"Directory must be ordinary, without symlinks: {current}")


def input_path(value):
    path = Path(value)
    if ".." in path.parts:
        raise ValueError("Parent traversal is not allowed in fixture paths")
    return path if path.is_absolute() else ROOT / path


def build_descendant(path):
    try:
        relative = path.relative_to(ROOT / "build")
    except ValueError as error:
        raise ValueError(f"Fixture paths must stay below project build/: {path}") from error
    if not relative.parts:
        raise ValueError("The project build/ root is not a fixture output")


def reserve_output(path):
    build_descendant(path)
    ordinary_directory_chain(path.parent)
    if os.path.lexists(path):
        raise ValueError(f"Preparation output must be fresh: {path}")
    path.mkdir()
    ordinary_directory_chain(path)


def ordinary_file(path, read=True):
    ordinary_directory_chain(path.parent)
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError(f"File must be ordinary, without a symlink: {path}")
    return path.read_bytes() if read else None


def cpp_string(value):
    return json.dumps(str(value), ensure_ascii=False)


def cpp_raw(data, label):
    delimiter = "EDGE" + digest(data)[:10]
    content = data.decode("utf-8")
    if ")" + delimiter + '"' in content:
        raise ValueError(f"Raw-string delimiter collides with {label}")
    return f'R"{delimiter}({content}){delimiter}"'


class AppPatches:
    def __init__(self, source):
        self.original = source
        self.current = source
        self.patches = []

    def replace(self, old, new, label, count=1):
        actual = self.current.count(old)
        if actual != count:
            raise ValueError(f"Current App anchor changed ({label}): {actual}, expected {count}")
        self.current = self.current.replace(old, new)
        self.patches.append((old, new, label, count))

    def normalized(self):
        result = self.current
        for old, new, label, count in reversed(self.patches):
            if result.count(new) != count:
                raise ValueError(f"Cannot uniquely reverse fixture patch: {label}")
            result = result.replace(new, old)
        if result != self.original:
            raise ValueError("Fixture normalization did not restore exact current App bytes")
        return result


def prepare_app(source):
    patches = AppPatches(source)
    patches.replace("namespace {\n", '#include "tests/ui_edge_fixture.hpp"\n'
                    '#include "edge_fixture_bindings.hpp"\n\nnamespace {\n',
                    "external-only fixture headers")
    patches.replace("    GameSession session;",
                    "    GameSession session;\n    numtilo_edge::seedSession(session);",
                    "public deterministic session setup")
    patches.replace("    numtilo_core::ScoreManager scoreManager(dataPaths.scoresFile(),\n"
                    "                                            storageAfterProfileImport(migration.scores));",
                    "    numtilo_core::ScoreManager scoreManager(dataPaths.scoresFile(),\n"
                    "        numtilo_edge::scoreStorage(storageAfterProfileImport(migration.scores)));",
                    "isolated targeted failure delegate in existing constructor")
    patches.replace("        const auto result = scoreCoordinator.checkpoint(session);",
                    "        const auto result = scoreCoordinator.checkpoint(session);\n"
                    "        numtilo_edge::recordScoreCheckpoint(result, session, scoreManager);",
                    "real checkpoint result and snapshot observation")
    patches.replace("checkpointSettings();",
                    'numtilo_edge::recordSettingsCheckpoint("settings-checkpoint-before", settings);\n'
                    '        checkpointSettings();\n'
                    '        numtilo_edge::recordSettingsCheckpoint("settings-checkpoint-after", settings);',
                    "unchanged settings checkpoint observation", 4)
    patches.replace("    SceneId scene = SceneId::Splash;\n    splashScene.onEnter();",
                    "    SceneId scene = SceneId::Playing;\n    playingScene.onEnter();",
                    "initial actual Playing scene")
    trace = r'''
    const auto edgeState = [&] {
        nlohmann::json completed = nlohmann::json::array();
        for (const auto &entry : scoreManager.completedGames())
            completed.push_back({{"round", entry.gameId}, {"score", entry.score},
                                 {"player", entry.playerName}, {"played_at", entry.playedAtUtc}});
        return nlohmann::json{
            {"scene", numtilo_edge::sceneName(scene)}, {"scene_id", static_cast<int>(scene)},
            {"generation", accessibilityGeneration}, {"round", session.gameId()},
            {"grid", session.game().getGrid()}, {"score", session.game().getScore()},
            {"best", scoreManager.bestScore()}, {"completed", completed},
            {"won", session.game().hasWon()}, {"terminal", session.game().isGameOver()},
            {"animations", playingScene.hasActiveAnimations()},
            {"player", session.playerName()}, {"sound_enabled", soundManager.isEnabled()},
            {"provider_attached", numtilo_edge::runtime().providerAttached},
            {"controlled_events_polled", numtilo_edge::runtime().nextEvent},
            {"controlled_events_remaining", numtilo_edge::runtime().events.size() - numtilo_edge::runtime().nextEvent}};
    };
    const auto edgeRecord = [&](const std::string_view stage, const sf::Event *event = nullptr,
                                const ui::SceneAction *action = nullptr,
                                const ui::AccessibilityAction *request = nullptr) {
        numtilo_edge::runtime().record(stage, edgeState(), event, action, request);
    };
    edgeRecord("ready");
'''
    patches.replace("    const auto updateGameplayScene = [&] {",
                    trace + "    const auto updateGameplayScene = [&] {",
                    "bounded App-state trace")
    patches.replace("            beginScenePresentation(scene, previousScene);\n        }\n    };",
                    "            beginScenePresentation(scene, previousScene);\n"
                    '            edgeRecord("outcome-transition");\n        }\n    };',
                    "real GameplayOutcome transition trace")
    patches.replace("    const auto dispatchAction = [&](const ui::SceneAction &action) {\n",
                    "    const auto dispatchAction = [&](const ui::SceneAction &action) {\n"
                    "        if (action.direction || action.command != SceneCommand::None)\n"
                    '            edgeRecord("command-before", nullptr, &action);\n',
                    "current command input trace")
    patches.replace("        if (command == SceneCommand::ShowSplash) {\n"
                    "            playingScene.resetVisualEffects();\n        }\n    };",
                    "        if (command == SceneCommand::ShowSplash) {\n"
                    "            playingScene.resetVisualEffects();\n        }\n"
                    "        if (action.direction || action.command != SceneCommand::None)\n"
                    '            edgeRecord("command-after", nullptr, &action);\n    };',
                    "current command result trace")
    patches.replace("    const auto accessibilitySnapshot = [&] {",
                    "    numtilo_edge::runtime().providerWasAttached = accessibility.supported() && accessibility.attached();\n"
                    "    numtilo_edge::runtime().providerAttached = accessibility.attached();\n"
                    '    edgeRecord("provider-initial-state");\n'
                    "    const auto accessibilitySnapshot = [&] {", "actual provider attachment observation")
    patches.replace("    const auto routeAccessible = [&](const ui::AccessibilityAction &request) {\n",
                    "    const auto routeAccessible = [&](const ui::AccessibilityAction &request) {\n"
                    "        if (numtilo_edge::runtime().lifecycle())\n"
                    '            numtilo_edge::runtime().cause = "native-request-drain";\n'
                    '        edgeRecord("ax-request", nullptr, nullptr, &request);\n',
                    "real routeAccessible input trace")
    patches.replace("        if (!ui::acceptsAccessibilityAction(current, request))\n            return;",
                    "        if (!ui::acceptsAccessibilityAction(current, request)) {\n"
                    '            edgeRecord("ax-rejected", nullptr, nullptr, &request);\n'
                    "            return;\n        }\n"
                    '        edgeRecord("ax-accepted", nullptr, nullptr, &request);',
                    "real request revalidation outcome")
    patches.replace("            focusAccessible(request.targetId);\n            return;",
                    "            focusAccessible(request.targetId);\n"
                    '            edgeRecord("ax-focus-applied", nullptr, nullptr, &request);\n'
                    "            return;", "semantic focus request trace")
    patches.replace("            }\n            return;\n        }\n        ui::SceneAction action;",
                    "            }\n"
                    '            edgeRecord("ax-value-applied", nullptr, nullptr, &request);\n'
                    "            return;\n        }\n        ui::SceneAction action;",
                    "desired-value request trace")
    patches.replace("        dispatchAccessibleAction(action);\n    };",
                    "        dispatchAccessibleAction(action);\n"
                    '        edgeRecord("ax-result", nullptr, &action, &request);\n    };',
                    "real accessible dispatch result")
    preparation = r'''
    const auto edgePublish = [&](const ui::AccessibilitySnapshot &snapshot) {
        // These focused IDs are App semantics, not observed native AX focus.
        numtilo_edge::runtime().snapshot(snapshot, edgeState());
        return accessibility.publish(snapshot);
    };
    if (numtilo_edge::runtime().requestBatch()) {
        numtilo_edge::runControlledBatch(updateGameplayScene, accessibilitySnapshot,
                                     edgeState, routeAccessible);
        numtilo_edge::runtime().cause = "fixture-stop";
        edgeRecord("fixture-stop-before-detach");
        accessibility.detach();
        numtilo_edge::runtime().providerAttached = accessibility.attached();
        window.close();
        // Deliberate fixture cleanup, never an OS Closed/checkpoint assertion.
        edgeRecord("fixture-stop-returned");
        return 0;
    }
    if (numtilo_edge::runtime().eventLoop())
        numtilo_edge::prepareEventLoop(edgeState, dispatchAction, playingScene);
    const auto edgePollEvent = [&](sf::Event &event) {
        if (!numtilo_edge::runtime().eventLoop())
            return window.pollEvent(event);
        playingScene.tickVisuals();
        return numtilo_edge::pollControlledEvent(event);
    };
'''
    patches.replace("    while (window.isOpen()) {", preparation + "    while (window.isOpen()) {",
                    "explicit controlled batch or unmodified native loop")
    patches.replace("        while (window.pollEvent(event)) {\n",
                    "        while (edgePollEvent(event)) {\n"
                    '            numtilo_edge::runtime().cause = numtilo_edge::runtime().eventLoop() ? "controlled-event-vector" : "sfml-event";\n'
                    "            const bool edgeObserved = numtilo_edge::observedEvent(event);\n"
                    '            if (edgeObserved) edgeRecord("sfml-queued", &event);\n',
                    "native SFML event observation")
    patches.replace("            if (event.type == sf::Event::Closed) {\n",
                    "            if (event.type == sf::Event::Closed) {\n"
                    "                if (numtilo_edge::runtime().eventLoop()) playingScene.tickVisuals();\n"
                    "                numtilo_edge::observeClosed(edgeState(), accessibilitySnapshot());\n"
                    '                edgeRecord("closed-before-checkpoint", &event);\n',
                    "real Closed priority branch entry")
    patches.replace("                accessibility.detach();\n                window.close();",
                    '                edgeRecord("closed-after-checkpoint", &event);\n'
                    "                accessibility.detach();\n"
                    "                numtilo_edge::runtime().providerAttached = accessibility.attached();\n"
                    "                numtilo_edge::runtime().drainedAfterDetach = accessibility.takeActions().size();\n"
                    '                edgeRecord("closed-after-detach", &event);\n'
                    "                window.close();", "actual detach and untouched-tail observation")
    patches.replace("            if (!inputGuard.accepts(event)) {\n                continue;\n            }",
                    "            if (!inputGuard.accepts(event)) {\n"
                    '                if (edgeObserved) edgeRecord("sfml-guard-rejected", &event);\n'
                    "                continue;\n            }", "guard rejection observation")
    patches.replace("            dispatchAction(action);\n        }",
                    '            if (edgeObserved) edgeRecord("sfml-routed", &event, &action);\n'
                    "            dispatchAction(action);\n"
                    '            if (edgeObserved) edgeRecord("sfml-result", &event, &action);\n        }',
                    "native input routing result")
    patches.replace("accessibility.publish(accessibilitySnapshot());",
                    "edgePublish(accessibilitySnapshot());", "unchanged native publish with semantic trace", 3)
    patches.replace("    return 0;\n}\n\n} // namespace app",
                    "    if (numtilo_edge::runtime().eventLoop())\n"
                    "        numtilo_edge::verifyEventLoop(edgeState(), accessibility);\n"
                    '    numtilo_edge::runtime().cause = numtilo_edge::runtime().eventLoop() ? "controlled-App-loop-return" : "native-loop-return";\n'
                    '    edgeRecord("app-returned");\n    return 0;\n}\n\n} // namespace app',
                    "normal native loop return trace")
    normalized = patches.normalized()
    main = r'''

int main(int argc, char **argv) {
    try {
        return numtilo_edge::runMain(argc, argv, numtilo_edge::kPreparation);
    } catch (const std::exception &error) {
        std::cerr << "UI edge fixture refused: " << error.what() << '\n';
        return 2;
    }
}
'''
    return patches.current + main, normalized, [item[2] for item in patches.patches]


def current_commands(build, output):
    compile_bytes = ordinary_file(build / "compile_commands.json")
    link_path = build / "CMakeFiles/numtilo.dir/link.txt"
    link_bytes = ordinary_file(link_path)
    entries = json.loads(compile_bytes)
    matches = [entry for entry in entries
               if Path(entry["file"]).absolute() == ROOT / "src/app/App.cpp"]
    if len(matches) != 1:
        raise ValueError("Expected one current numtilo App.cpp compile entry")
    entry = matches[0]
    directory = input_path(entry["directory"])
    if directory != build:
        raise ValueError("Only current local numtilo Makefiles metadata is supported")
    command = list(entry["arguments"]) if "arguments" in entry else shlex.split(entry["command"])
    if command.count("-o") != 1 or command.count("-c") != 1:
        raise ValueError("Unexpected App compiler command format")
    original_object = command[command.index("-o") + 1]
    source_operand = command.index("-c") + 1
    if Path(command[source_operand]).absolute() != ROOT / "src/app/App.cpp":
        raise ValueError("App compile entry source operand differs from current source")
    fixture_object = output / "app_edge_fixture.o"
    compile_command = list(command)
    compile_command[compile_command.index("-o") + 1] = str(fixture_object)
    compile_command[source_operand] = str(output / "app_edge_fixture.cpp")
    # Root contains VERSION; on case-insensitive macOS an -I root would shadow
    # libc++'s <version>. Only quoted fixture includes may search the project root.
    compile_command += ["-Werror", "-iquote", str(ROOT)]
    link = shlex.split(link_bytes.decode("utf-8"))
    main_objects = [index for index, arg in enumerate(link)
                    if arg.endswith("CMakeFiles/numtilo.dir/src/app/main.cpp.o")]
    app_objects = [index for index, arg in enumerate(link)
                   if arg == original_object]
    if len(main_objects) != 1 or len(app_objects) != 1 or link.count("-o") != 1:
        raise ValueError("Cannot uniquely replace current App/main objects in numtilo link command")
    for index, arg in enumerate(link):
        if arg.endswith(".o") or arg.endswith(".a"):
            ordinary_file(Path(arg) if Path(arg).is_absolute() else build / arg, read=False)
    link[main_objects[0]] = str(fixture_object)
    link.pop(app_objects[0])
    link[link.index("-o") + 1] = str(output / "ui_edge_fixture")
    return compile_command, link, {
        "build_directory": str(build),
        "compile_metadata": str(build / "compile_commands.json"),
        "compile_metadata_sha256": digest(compile_bytes),
        "link_metadata": str(link_path),
        "link_metadata_sha256": digest(link_bytes),
        "original_compile_command": command,
        "fixture_compile_command": compile_command,
        "fixture_link_command": link,
    }


def prepare(args):
    ordinary_directory_chain(ROOT)
    ordinary_directory_chain(ROOT / "build")
    build = input_path(args.build_dir)
    output = input_path(args.output)
    build_descendant(build)
    build_descendant(output)
    ordinary_directory_chain(build)
    ordinary_directory_chain(output.parent)
    if os.path.lexists(output):
        raise ValueError(f"Preparation output must be fresh: {output}")
    source_paths = (ROOT / "src/app/App.cpp", ROOT / "tests/ui_acceptance.cpp",
                    ROOT / "tests/ui_edge_fixture.hpp")
    source_bytes = tuple(ordinary_file(path) for path in source_paths)
    declaration = re.search(r"constexpr\s+(?:char|std::string_view)\s+kFirstWinMoves(?:\[\])?\s*=\s*(.*?);",
                            source_bytes[1].decode("utf-8"), re.DOTALL)
    if not declaration:
        raise ValueError("Current maintained first-win replay declaration is missing")
    expression = declaration.group(1)
    moves = "".join(re.findall(r'"([DLUR]+)"', expression))
    if not moves or re.sub(r'"[DLUR]+"|\s+', "", expression):
        raise ValueError("Current maintained first-win replay has an unexpected expression")
    generated, normalized, patch_labels = prepare_app(source_bytes[0].decode("utf-8"))
    normalized_bytes = normalized.encode("utf-8")
    if normalized_bytes != source_bytes[0]:
        raise ValueError("Current App bytes did not survive fixture normalization")
    compile_command, link_command, command_metadata = current_commands(build, output)
    bindings = "#pragma once\n\nnamespace numtilo_edge {\ninline const Preparation kPreparation{\n"
    bindings += f"    {cpp_string(ROOT)}, {cpp_string(output)},\n"
    bindings += "    std::array<SourceReference, 3>{{\n"
    for path, data in zip(source_paths, source_bytes):
        bindings += f"        {{{cpp_string(path.relative_to(ROOT))}, {cpp_string(path)}, {cpp_raw(data, str(path))}}},\n"
    bindings += f"    }}}}, {cpp_string(moves)}\n}};\n}} // namespace numtilo_edge\n"
    metadata = {
        "schema_version": 1,
        "status": "prepared",
        "launches": 0,
        "compiled": False,
        "cases": list(CASES),
        "controlled_scope": "Real current App routeAccessible/dispatchAction with immutable request-vector groups; native callbacks/takeActions bypassed",
        "lifecycle_scope": "Unmodified native App loop with bounded event/semantic observations; actual OS lifecycle and native AX focus await approved checkpoint",
        "event_loop_scope": "Current App routing/Closed body with fixture-controlled bounded event vector; no actual OS input/Closed claim",
        "failure_scope": "IoError injection only for original terminal completion payload; ordinary/recovered writes delegate to production storage; no OS permissions/power-loss claim",
        "retained_native_callback_injected": False,
        "initially_populated_native_queue_flush_proof": False,
        "lifecycle_auto_pass": False,
        "controlled_cleanup_is_OS_Closed_proof": False,
        "runtime_requires": ["explicit --case", "fresh ordinary --run-output below project build/", "exact source-byte preflight before app::run"],
        "source_byte_preflight_scope": [str(path.relative_to(ROOT)) for path in source_paths],
        "linked_object_freshness": "Caller must rebuild numtilo before preparing/compiling and again before regenerating for the checkpoint; source-byte preflight covers only the three named sources",
        "source_hashes": {str(path.relative_to(ROOT)): digest(data) for path, data in zip(source_paths, source_bytes)},
        "source_normalization": {
            "exact_match": True, "app_sha256": digest(source_bytes[0]),
            "normalized_sha256": digest(normalized_bytes), "patches": patch_labels,
        },
        "generated_source_sha256": digest(generated.encode("utf-8")),
        "bindings_sha256": digest(bindings.encode("utf-8")),
        "replay_moves": len(moves),
        "replay_sha256": digest(moves.encode("ascii")),
        "preparer_sha256": digest(ordinary_file(Path(__file__).absolute())),
        "commands": command_metadata,
        "executable": str(output / "ui_edge_fixture"),
    }
    reserve_output(output)
    (output / "runs").mkdir()
    (output / "app_edge_fixture.cpp").write_bytes(generated.encode("utf-8"))
    (output / "edge_fixture_bindings.hpp").write_bytes(bindings.encode("utf-8"))
    (output / "normalized_App.cpp").write_bytes(normalized_bytes)
    report = output / "preparation.json"
    report.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    if args.compile:
        with (output / "compile.log").open("w") as log:
            subprocess.run(compile_command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
            subprocess.run(link_command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        metadata["compiled"] = True
        metadata["executable_sha256"] = digest(ordinary_file(output / "ui_edge_fixture"))
        if sys.platform == "darwin":
            name = "NumtiloEdge112Lifecycle"
            contents = output / f"{name}.app" / "Contents"
            (contents / "MacOS").mkdir(parents=True)
            shutil.copy2(output / "ui_edge_fixture", contents / "MacOS" / name)
            with (contents / "Info.plist").open("wb") as info:
                plistlib.dump({"CFBundleDisplayName": name, "CFBundleName": name,
                              "CFBundleExecutable": name,
                              "CFBundleIdentifier": "local.codex.numtilo-edge112.lifecycle",
                              "CFBundleVersion": "1", "CFBundlePackageType": "APPL",
                              "NSHighResolutionCapable": True}, info)
            metadata["lifecycle_bundle"] = str(contents.parent)
        report.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"status": "prepared", "compiled": metadata["compiled"], "launches": 0,
                      "preparation": str(report), "executable": metadata["executable"],
                      "cases": list(CASES)}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, help="Existing freshly built numtilo Makefiles directory below build/")
    parser.add_argument("--output", required=True, help="Fresh output directory; parent must already be ordinary and below build/")
    parser.add_argument("--compile", action="store_true", help="Compile/link the generated fixture, never launch it")
    args = parser.parse_args()
    try:
        prepare(args)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"UI edge preparation refused: {error}\n")


if __name__ == "__main__":
    main()
