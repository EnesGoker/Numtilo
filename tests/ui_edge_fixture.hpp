#pragma once

// External, current-App preparation for roadmap 11.2/11.3. This header is not linked
// into the product. Its controlled request groups bypass native callbacks and
// AccessibilityBridge::takeActions; they are App-dispatch evidence only.
#include "app/App.hpp"
#include "app/GameSession.hpp"
#include "app/ScoreCoordinator.hpp"
#include "app/SettingsStore.hpp"
#include "app/ui/Accessibility.hpp"
#include "app/ui/Scene.hpp"

#include <SFML/Window/Event.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace numtilo_edge {

inline constexpr std::string_view kPlayingBatch = "e11-02-playing-batch";
inline constexpr std::string_view kWinBatch = "e11-02-win-batch";
inline constexpr std::string_view kLifecycle = "e11-03-lifecycle";
inline constexpr std::array<std::string_view, 15> kCases{kPlayingBatch,
                                                         kWinBatch,
                                                         kLifecycle,
                                                         "e11-04-win-active-close",
                                                         "e11-04-win-settled-close",
                                                         "e11-04-terminal-active-close",
                                                         "e11-04-terminal-settled-close",
                                                         "e11-05-new-before-close",
                                                         "e11-05-menu-before-close",
                                                         "e11-05-close-before-new",
                                                         "e11-05-close-before-menu",
                                                         "e11-06-new-recovery-close",
                                                         "e11-06-menu-recovery-close",
                                                         "e11-06-new-persistent-close",
                                                         "e11-06-menu-persistent-close"};

struct SourceReference {
    std::string_view name;
    std::filesystem::path path;
    // Embedded in the generated bindings, so a changed source cannot be silently
    // accepted by editing a preparation sidecar before the future checkpoint.
    std::string_view bytes;
};

struct Preparation {
    std::filesystem::path projectRoot;
    std::filesystem::path output;
    std::array<SourceReference, 3> sources;
    std::string_view winMoves;
};

inline std::string readBytes(const std::filesystem::path &path,
                             const std::size_t maximumBytes = 1024 * 1024,
                             const std::optional<std::size_t> expectedSize = std::nullopt) {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)))
        throw std::runtime_error("Fixture input must be an ordinary file: " + path.string());
    const auto size = std::filesystem::file_size(path);
    if (size > maximumBytes || (expectedSize && size != *expectedSize))
        throw std::runtime_error("Fixture source size changed or exceeds its bound: " +
                                 path.string());
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read fixture source: " + path.string());
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(size));
    if (file.gcount() != static_cast<std::streamsize>(size))
        throw std::runtime_error("Fixture source changed during read: " + path.string());
    char tail;
    if (file.get(tail) || !file.eof())
        throw std::runtime_error("Fixture source changed during read: " + path.string());
    return bytes;
}

inline void writeEvidence(const std::filesystem::path &path, const std::string_view bytes) {
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open isolated fixture evidence: " + path.string());
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.flush();
    if (!file)
        throw std::runtime_error("Isolated fixture evidence write failed: " + path.string());
    file.close();
    if (!file)
        throw std::runtime_error("Isolated fixture evidence close failed: " + path.string());
}

inline bool below(const std::filesystem::path &path, const std::filesystem::path &root) {
    auto candidate = path.begin();
    for (auto parent = root.begin(); parent != root.end(); ++parent, ++candidate)
        if (candidate == path.end() || *candidate != *parent)
            return false;
    return candidate != path.end();
}

inline void requireOrdinaryDirectoryChain(const std::filesystem::path &path) {
    if (!path.is_absolute() || path.lexically_normal() != path)
        throw std::runtime_error("Fixture paths must be absolute and normalized");
    std::filesystem::path current;
    for (const auto &part : path) {
        current /= part;
        const auto status = std::filesystem::symlink_status(current);
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status))
            throw std::runtime_error("Fixture directory must be ordinary: " + current.string());
    }
}

inline const char *sceneName(const app::ui::SceneId scene) {
    switch (scene) {
    case app::ui::SceneId::Splash:
        return "Splash";
    case app::ui::SceneId::HighScores:
        return "HighScores";
    case app::ui::SceneId::Playing:
        return "Playing";
    case app::ui::SceneId::Win:
        return "Win";
    case app::ui::SceneId::GameOver:
        return "GameOver";
    }
    return "Unknown";
}

inline const char *commandName(const app::ui::SceneCommand command) {
    switch (command) {
    case app::ui::SceneCommand::None:
        return "None";
    case app::ui::SceneCommand::StartGame:
        return "StartGame";
    case app::ui::SceneCommand::ShowHighScores:
        return "ShowHighScores";
    case app::ui::SceneCommand::ShowSplash:
        return "ShowSplash";
    case app::ui::SceneCommand::RestartGame:
        return "RestartGame";
    case app::ui::SceneCommand::ToggleSound:
        return "ToggleSound";
    case app::ui::SceneCommand::ContinueGame:
        return "ContinueGame";
    }
    return "Unknown";
}

inline nlohmann::json eventDetails(const sf::Event &event) {
    nlohmann::json result = {{"type", static_cast<int>(event.type)}};
    switch (event.type) {
    case sf::Event::KeyPressed:
    case sf::Event::KeyReleased:
        result["name"] = event.type == sf::Event::KeyPressed ? "KeyPressed" : "KeyReleased";
        result["key"] = static_cast<int>(event.key.code);
        result["modifiers"] = {event.key.alt, event.key.control, event.key.shift, event.key.system};
        break;
    case sf::Event::MouseButtonPressed:
    case sf::Event::MouseButtonReleased:
        result["name"] = event.type == sf::Event::MouseButtonPressed ? "MouseButtonPressed"
                                                                     : "MouseButtonReleased";
        result["button"] = static_cast<int>(event.mouseButton.button);
        result["pixel"] = {event.mouseButton.x, event.mouseButton.y};
        break;
    case sf::Event::LostFocus:
        result["name"] = "LostFocus";
        break;
    case sf::Event::GainedFocus:
        result["name"] = "GainedFocus";
        break;
    case sf::Event::Closed:
        result["name"] = "Closed";
        break;
    case sf::Event::TextEntered:
        result["name"] = "TextEntered";
        result["unicode"] = event.text.unicode;
        break;
    default:
        result["name"] = "Other";
        break;
    }
    return result;
}

inline bool observedEvent(const sf::Event &event) {
    return event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased ||
           event.type == sf::Event::MouseButtonPressed ||
           event.type == sf::Event::MouseButtonReleased || event.type == sf::Event::LostFocus ||
           event.type == sf::Event::GainedFocus || event.type == sf::Event::Closed ||
           event.type == sf::Event::TextEntered;
}

inline nlohmann::json snapshotDetails(const app::ui::AccessibilitySnapshot &snapshot) {
    nlohmann::json nodes = nlohmann::json::array();
    for (const auto &node : snapshot.nodes) {
        nlohmann::json actions = nlohmann::json::array();
        for (const auto action : node.actions)
            actions.push_back(static_cast<int>(action));
        nodes.push_back({{"id", node.id},
                         {"role", static_cast<int>(node.role)},
                         {"name", node.name},
                         {"value", node.value},
                         {"focused", node.focused},
                         {"enabled", node.enabled},
                         {"actions", actions}});
    }
    return {{"generation", snapshot.generation},
            {"round", snapshot.roundId},
            {"scene_name", snapshot.sceneName},
            {"focused_ids_scope", "App-semantic-snapshot-not-native-AX-focus"},
            {"modal", snapshot.modal},
            {"nodes", nodes}};
}

struct Runtime {
    static constexpr std::size_t kMaxTraceRows = 8192;
    std::string caseId;
    std::string cause{"fixture-setup"};
    std::filesystem::path runOutput;
    std::filesystem::path profile;
    std::string_view winMoves;
    nlohmann::json trace = nlohmann::json::array();
    nlohmann::json checks = nlohmann::json::array();
    std::optional<nlohmann::json> lastSnapshot;
    std::size_t accepted{0};
    std::size_t rejected{0};
    std::vector<sf::Event> events;
    std::size_t nextEvent{0};
    std::size_t closedEvents{0};
    bool providerWasAttached{false};
    bool providerAttached{false};
    std::size_t drainedAfterDetach{0};
    std::optional<bool> publishAfterDetach;
    std::optional<numtilo_core::Direction> finalDirection;
    nlohmann::json finalMoveState;
    nlohmann::json closedState;
    std::optional<app::ui::AccessibilitySnapshot> closedSnapshot;
    nlohmann::json endState;
    nlohmann::json scoreCheckpoints = nlohmann::json::array();
    nlohmann::json settingsCheckpoints = nlohmann::json::array();
    nlohmann::json storageAttempts = nlohmann::json::array();
    nlohmann::json diskReload;
    std::string baselineScoreBytes;
    std::string baselineSettingsBytes;
    std::string failureRound;
    bool failTargetedSave{false};
    bool truncated{false};
    bool controlledComplete{false};

    bool lifecycle() const {
        return caseId == kLifecycle;
    }

    bool requestBatch() const {
        return caseId == kPlayingBatch || caseId == kWinBatch;
    }

    bool eventLoop() const {
        return !lifecycle() && !requestBatch();
    }

    bool finalWin() const {
        return caseId.starts_with("e11-04-win-");
    }

    bool failureCase() const {
        return caseId.starts_with("e11-06-");
    }

    bool navigationFirst() const {
        return failureCase() || caseId.ends_with("-before-close");
    }

    bool newNavigation() const {
        return caseId.find("-new-") != std::string::npos || caseId.ends_with("-new");
    }

    bool recoveredClose() const {
        return failureCase() && caseId.find("-recovery-") != std::string::npos;
    }

    void flush() const {
        const auto report =
            nlohmann::json{{"schema_version", 1},
                           {"case", caseId},
                           {"scope", lifecycle()   ? "native-lifecycle-recording"
                                     : eventLoop() ? "controlled-current-App-event-loop"
                                                   : "controlled-current-App-dispatch"},
                           {"native_callback_or_takeActions_proof", false},
                           {"controlled_events_are_OS_events", false},
                           {"truncated", truncated},
                           {"rows", trace}};
        writeEvidence(runOutput / "trace.json", report.dump(2) + '\n');
    }

    void record(const std::string_view stage, nlohmann::json state,
                const sf::Event *event = nullptr, const app::ui::SceneAction *action = nullptr,
                const app::ui::AccessibilityAction *request = nullptr) {
        if (stage == "ax-accepted")
            ++accepted;
        else if (stage == "ax-rejected")
            ++rejected;
        if (trace.size() == kMaxTraceRows) {
            truncated = true;
            flush();
            return;
        }
        state["sequence"] = trace.size();
        state["stage"] = stage;
        state["cause"] = cause;
        if (event)
            state["event"] = eventDetails(*event);
        if (action) {
            state["command"] = commandName(action->command);
            if (action->direction)
                state["direction"] = static_cast<int>(*action->direction);
        }
        if (request)
            state["request"] = {{"kind", static_cast<int>(request->kind)},
                                {"target", request->targetId},
                                {"value", request->value},
                                {"generation", request->generation},
                                {"round", request->roundId}};
        trace.push_back(std::move(state));
        flush();
    }

    void snapshot(const app::ui::AccessibilitySnapshot &snapshot, nlohmann::json state) {
        const auto details = snapshotDetails(snapshot);
        if (lastSnapshot && *lastSnapshot == details)
            return;
        lastSnapshot = details;
        state["semantic"] = details;
        record("semantic-snapshot", std::move(state));
    }

    void require(const bool condition, const std::string_view description) {
        checks.push_back({{"name", description}, {"passed", condition}});
        if (!condition)
            throw std::runtime_error("Controlled App check failed: " + std::string(description));
    }
};

inline Runtime &runtime() {
    static Runtime instance;
    return instance;
}

inline nlohmann::json completedEntries(const numtilo_core::ScoreManager &scores) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto &entry : scores.completedGames())
        result.push_back({{"round", entry.gameId},
                          {"score", entry.score},
                          {"player", entry.playerName},
                          {"played_at", entry.playedAtUtc}});
    return result;
}

inline void recordScoreCheckpoint(const app::ScoreCheckpointResult &result,
                                  const app::GameSession &session,
                                  const numtilo_core::ScoreManager &scores) {
    auto &fixture = runtime();
    nlohmann::json snapshot = {{"round", session.gameId()},
                               {"player", session.playerName()},
                               {"score", session.game().getScore()},
                               {"terminal", session.game().isGameOver()},
                               {"pending_completion", session.shouldRecordFinalScore()},
                               {"best", scores.bestScore()},
                               {"completed", completedEntries(scores)},
                               {"dirty", scores.isDirty()},
                               {"round_completed", result.roundCompleted},
                               {"save_attempted", result.saveAttempted},
                               {"save_succeeded", result.saveSucceeded},
                               {"save_status", static_cast<int>(scores.lastSaveStatus())},
                               {"failure_reported", result.failureToReport.has_value()}};
    fixture.scoreCheckpoints.push_back(snapshot);
    fixture.record("score-checkpoint", std::move(snapshot));
}

inline void recordSettingsCheckpoint(const std::string_view stage,
                                     const app::SettingsStore &settings) {
    auto &fixture = runtime();
    nlohmann::json snapshot = {{"sound_enabled", settings.soundEnabled()},
                               {"dirty", settings.isDirty()},
                               {"needs_migration", settings.needsMigration()},
                               {"save_status", static_cast<int>(settings.lastSaveStatus())}};
    fixture.settingsCheckpoints.push_back({{"stage", stage}, {"state", snapshot}});
    fixture.record(stage, std::move(snapshot));
}

class ControlledScoreStorage final : public numtilo_core::ScoreStorage {
  public:
    explicit ControlledScoreStorage(std::shared_ptr<numtilo_core::ScoreStorage> delegate)
        : delegate_(delegate ? std::move(delegate) : numtilo_core::makeScoreStorage()) {
    }

    numtilo_core::FileSaveStatus save(const std::filesystem::path &path,
                                      const std::string &serialized,
                                      const std::optional<std::string> &expectedSource,
                                      const bool backupLegacy) override {
        auto &fixture = runtime();
        fixture.require(path == fixture.profile / "scores.json",
                        "Injected storage stays inside the isolated score file");
        bool targeted = false;
        const auto document = nlohmann::json::parse(serialized);
        for (const auto &entry : document.at("completed_games"))
            if (entry.at("game_id") == fixture.failureRound)
                targeted = true;
        const bool injected = targeted && fixture.failTargetedSave;
        const auto status = injected
                                ? numtilo_core::FileSaveStatus::IoError
                                : delegate_->save(path, serialized, expectedSource, backupLegacy);
        nlohmann::json attempt = {
            {"path", path.string()},         {"targeted", targeted},
            {"injected_IoError", injected},  {"serialized", serialized},
            {"backup_legacy", backupLegacy}, {"status", static_cast<int>(status)}};
        attempt["expected_source"] =
            expectedSource ? nlohmann::json(*expectedSource) : nlohmann::json(nullptr);
        fixture.storageAttempts.push_back(attempt);
        fixture.record("score-storage-attempt", std::move(attempt));
        return status;
    }

  private:
    std::shared_ptr<numtilo_core::ScoreStorage> delegate_;
};

inline std::shared_ptr<numtilo_core::ScoreStorage>
scoreStorage(std::shared_ptr<numtilo_core::ScoreStorage> delegate) {
    if (!runtime().failureCase())
        return delegate;
    return std::make_shared<ControlledScoreStorage>(std::move(delegate));
}

inline void seedSession(app::GameSession &session) {
    auto &fixture = runtime();
    const bool win = fixture.caseId == kWinBatch || fixture.finalWin();
    session.resetGame(win ? 0 : fixture.eventLoop() ? 9 : 112);
    session.setPlayerName("Çağrı");
    if (!win && !fixture.eventLoop())
        return;
    if (win) {
        const auto count = fixture.winMoves.size() - (fixture.eventLoop() ? 1U : 0U);
        for (std::size_t index = 0; index < fixture.winMoves.size(); ++index) {
            const auto move = fixture.winMoves[index];
            const auto direction = move == 'L'   ? numtilo_core::Direction::Left
                                   : move == 'R' ? numtilo_core::Direction::Right
                                   : move == 'U' ? numtilo_core::Direction::Up
                                                 : numtilo_core::Direction::Down;
            if (index < count)
                session.applyMove(direction);
            else
                fixture.finalDirection = direction;
        }
    } else {
        constexpr std::array directions{numtilo_core::Direction::Left, numtilo_core::Direction::Up,
                                        numtilo_core::Direction::Right,
                                        numtilo_core::Direction::Down};
        app::GameSession probe(9);
        std::size_t moves = 0;
        while (moves < 5000 && !probe.game().isGameOver())
            probe.applyMove(directions[moves++ % directions.size()]);
        fixture.require(probe.game().isGameOver() && moves > 0,
                        "Bounded public seed-nine replay reaches a real terminal round");
        for (std::size_t index = 0; index + 1 < moves; ++index)
            session.applyMove(directions[index % directions.size()]);
        fixture.finalDirection = directions[(moves - 1) % directions.size()];
    }
    if (fixture.eventLoop())
        fixture.require(!session.game().hasWon() && !session.game().isGameOver() &&
                            fixture.finalDirection.has_value(),
                        "Public prefix is unfinished before its real final App move");
    else
        fixture.require(session.game().hasWon() && !session.game().isGameOver(),
                        "Versioned public seed-zero replay creates an unfinished win");
}

inline app::ui::AccessibilityAction press(const app::ui::AccessibilitySnapshot &snapshot,
                                          const std::string_view target) {
    return {std::string(target),
            app::ui::AccessibilityActionKind::Press,
            {},
            snapshot.generation,
            snapshot.roundId};
}

inline sf::Event keyEvent(const sf::Event::EventType type, const sf::Keyboard::Key key) {
    sf::Event event{};
    event.type = type;
    event.key.code = key;
    return event;
}

inline sf::Event closeEvent() {
    sf::Event event{};
    event.type = sf::Event::Closed;
    return event;
}

template <typename State, typename Dispatch, typename Playing>
void prepareEventLoop(State state, Dispatch dispatch, Playing &playing) {
    auto &fixture = runtime();
    fixture.require(fixture.eventLoop() && fixture.finalDirection.has_value(),
                    "Controlled event-loop mode has a genuine final direction");
    const auto prefix = state();
    fixture.require(prefix.at("scene_id") == static_cast<int>(app::ui::SceneId::Playing) &&
                        !prefix.at("won").template get<bool>() &&
                        !prefix.at("terminal").template get<bool>(),
                    "No outcome modal exists before the chosen final App move");
    fixture.baselineScoreBytes = readBytes(fixture.profile / "scores.json");
    fixture.baselineSettingsBytes = readBytes(fixture.profile / "settings.json");
    if (fixture.failureCase()) {
        fixture.failureRound = prefix.at("round").template get<std::string>();
        fixture.failTargetedSave = true;
    }
    fixture.cause = "controlled-real-final-move";
    dispatch(app::ui::SceneAction{app::ui::SceneCommand::None, fixture.finalDirection});
    fixture.finalMoveState = state();
    const auto &final = fixture.finalMoveState;
    fixture.require(final.at("scene_id") == static_cast<int>(app::ui::SceneId::Playing) &&
                        final.at("round") == prefix.at("round") &&
                        final.at("grid") != prefix.at("grid") && final.at("player") == "Çağrı",
                    "Final changed move uses actual App dispatch and preserves original identity");
    fixture.require(fixture.finalWin() ? final.at("won").template get<bool>() &&
                                             !final.at("terminal").template get<bool>()
                                       : final.at("terminal").template get<bool>(),
                    "Real final move reaches the selected unfinished win or terminal state");
    fixture.require(playing.hasActiveAnimations(),
                    "Actual Playing.onMove starts the final move animation");
    if (fixture.caseId.find("-settled-") != std::string::npos) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            playing.tickVisuals();
            if (!playing.hasActiveAnimations())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        fixture.require(!playing.hasActiveAnimations(),
                        "Public animation clock settles within its explicit two-second bound");
        fixture.require(state().at("scene_id") == static_cast<int>(app::ui::SceneId::Playing),
                        "Settling clocks does not fabricate a modal before Closed");
    }
    const auto navigation = keyEvent(
        sf::Event::KeyPressed, fixture.newNavigation() ? sf::Keyboard::N : sf::Keyboard::Escape);
    if (fixture.caseId.starts_with("e11-04-")) {
        fixture.events = {closeEvent(), keyEvent(sf::Event::KeyPressed, sf::Keyboard::N),
                          keyEvent(sf::Event::KeyReleased, sf::Keyboard::N),
                          keyEvent(sf::Event::KeyPressed, sf::Keyboard::Left)};
    } else if (fixture.navigationFirst()) {
        fixture.events = {navigation, closeEvent(),
                          keyEvent(sf::Event::KeyReleased, navigation.key.code),
                          keyEvent(sf::Event::KeyPressed, sf::Keyboard::Left),
                          keyEvent(sf::Event::KeyReleased, sf::Keyboard::Left)};
    } else {
        fixture.events = {closeEvent(), navigation,
                          keyEvent(sf::Event::KeyReleased, navigation.key.code),
                          keyEvent(sf::Event::KeyPressed, sf::Keyboard::Left),
                          keyEvent(sf::Event::KeyReleased, sf::Keyboard::Left)};
    }
    fixture.cause = "controlled-event-vector";
    nlohmann::json group = nlohmann::json::array();
    for (const auto &event : fixture.events)
        group.push_back(eventDetails(event));
    auto prepared = state();
    prepared["events"] = group;
    fixture.record("controlled-event-group-prepared", std::move(prepared));
}

inline bool pollControlledEvent(sf::Event &event) {
    auto &fixture = runtime();
    if (fixture.nextEvent == fixture.events.size())
        throw std::runtime_error("Controlled queue exhausted before the real App Closed branch");
    event = fixture.events[fixture.nextEvent++];
    if (event.type == sf::Event::Closed && fixture.recoveredClose())
        fixture.failTargetedSave = false;
    return true;
}

inline void observeClosed(const nlohmann::json &state,
                          const app::ui::AccessibilitySnapshot &snapshot) {
    auto &fixture = runtime();
    ++fixture.closedEvents;
    fixture.closedState = state;
    fixture.closedSnapshot = snapshot;
    if (!fixture.eventLoop())
        return;
    fixture.require(app::ui::isValidAccessibilitySnapshot(snapshot),
                    "The pre-close snapshot is valid before the real provider detaches");
    if (fixture.caseId.starts_with("e11-04-")) {
        const bool expectedActive = fixture.caseId.find("-active-") != std::string::npos;
        fixture.require(state.at("animations").get<bool>() == expectedActive,
                        expectedActive
                            ? "Closed arrives while the actual animation remains active"
                            : "Closed arrives immediately after measured animation settlement");
        fixture.require(state.at("scene_id") == static_cast<int>(app::ui::SceneId::Playing),
                        "Closed reaches real App priority branch before outcome routing");
    }
}

inline void verifyEventLoop(const nlohmann::json &state,
                            app::ui::AccessibilityBridge &accessibility) {
    auto &fixture = runtime();
    fixture.endState = state;
    const auto &original = fixture.finalMoveState;
    fixture.require(
        fixture.closedEvents == 1 && !fixture.truncated,
        "Real App Closed branch is handled exactly once with complete bounded evidence");
    const bool navigated = fixture.navigationFirst();
    const std::size_t expectedPolled = navigated ? 2 : 1;
    fixture.require(
        fixture.nextEvent == expectedPolled && fixture.events.size() - fixture.nextEvent >= 3,
        "Closed breaks real event polling without consuming navigation/release/direction tail");
    fixture.require(fixture.closedSnapshot.has_value(),
                    "Real Closed captured its valid current snapshot");
    fixture.publishAfterDetach = accessibility.publish(*fixture.closedSnapshot);
    fixture.require(
        fixture.providerWasAttached && !accessibility.attached() &&
            fixture.drainedAfterDetach == 0 && accessibility.takeActions().empty() &&
            !*fixture.publishAfterDetach,
        "Actual attached provider detaches and rejects publication with an empty drain");
    fixture.require(state.at("sound_enabled") == false &&
                        readBytes(fixture.profile / "settings.json") ==
                            fixture.baselineSettingsBytes,
                    "Navigation and close preserve the existing isolated Sound snapshot");
    const auto expectedScene = navigated && !fixture.newNavigation() ? app::ui::SceneId::Splash
                                                                     : app::ui::SceneId::Playing;
    fixture.require(state.at("scene_id") == static_cast<int>(expectedScene),
                    "Only navigation accepted before Closed determines the final scene");
    fixture.require(navigated && fixture.newNavigation()
                        ? state.at("round") != original.at("round") && state.at("score") == 0
                        : state.at("round") == original.at("round") &&
                              state.at("score") == original.at("score"),
                    "Closed-first preserves old round; accepted New alone replaces it");
    fixture.require(state.at("player") == original.at("player") &&
                        (navigated && fixture.newNavigation()
                             ? state.at("grid") != original.at("grid")
                             : state.at("grid") == original.at("grid")),
                    "Player remains original; only accepted New replaces the final board");
    fixture.require(original.at("best") == original.at("score") &&
                        state.at("best") == original.at("best"),
                    "The fresh profile retains the exact final best across navigation and Close");
    const auto expectedCheckpoints = navigated ? 4U : 3U;
    fixture.require(
        fixture.scoreCheckpoints.size() == expectedCheckpoints,
        "Startup, final move, accepted navigation and Closed checkpoint order is exact");
    fixture.require(
        fixture.scoreCheckpoints.at(1).at("round") == original.at("round") &&
            fixture.scoreCheckpoints.at(1).at("score") == original.at("score") &&
            fixture.scoreCheckpoints.back().at("round") == state.at("round") &&
            (!navigated || fixture.scoreCheckpoints.at(2).at("round") == original.at("round")),
        "Final and navigation checkpoints retain the old round before Close sees the accepted "
        "state");
    fixture.require(fixture.settingsCheckpoints.size() == (navigated ? 6U : 4U),
                    "Settings are checkpointed only at startup, accepted navigation and Closed");
    std::size_t firstCompletion = 0;
    for (const auto &checkpoint : fixture.scoreCheckpoints)
        if (checkpoint.at("round_completed").get<bool>())
            ++firstCompletion;
    fixture.require(
        firstCompletion == (fixture.finalWin() ? 0U : 1U),
        "Final move acknowledges terminal completion once; Close/retries never duplicate it");
    const auto &memory = state.at("completed");
    fixture.require(memory.size() == (fixture.finalWin() ? 0U : 1U),
                    "Unfinished win has no history; terminal memory has exactly one completion");
    if (!fixture.finalWin())
        fixture.require(memory.at(0).at("round") == original.at("round") &&
                            memory.at(0).at("player") == original.at("player") &&
                            memory.at(0).at("score") == original.at("score"),
                        "Completion retains original terminal ID/player/score across navigation");
    numtilo_core::ScoreManager reopened(fixture.profile / "scores.json");
    app::SettingsStore reopenedSettings(fixture.profile / "settings.json");
    fixture.require(reopened.load() && reopenedSettings.load(),
                    "Normal production loaders reopen actual isolated score and settings files");
    fixture.diskReload = {
        {"best", reopened.bestScore()},
        {"completed", completedEntries(reopened)},
        {"score_load_status", static_cast<int>(reopened.lastLoadStatus())},
        {"sound_enabled", reopenedSettings.soundEnabled()},
        {"settings_load_status", static_cast<int>(reopenedSettings.lastLoadStatus())}};
    if (fixture.failureCase()) {
        nlohmann::json targeted = nlohmann::json::array();
        for (const auto &attempt : fixture.storageAttempts)
            if (attempt.at("targeted").get<bool>())
                targeted.push_back(attempt);
        fixture.require(
            targeted.size() == 3 && targeted.at(0).at("injected_IoError") == true &&
                targeted.at(1).at("injected_IoError") == true,
            "Terminal and navigation saves fail; real Closed makes the third targeted attempt");
        for (const auto &attempt : targeted)
            fixture.require(attempt.at("serialized") == targeted.at(0).at("serialized") &&
                                attempt.at("expected_source") == fixture.baselineScoreBytes &&
                                attempt.at("backup_legacy") == false,
                            "Retry payload, timestamp and expected source remain byte-identical");
        fixture.require(
            fixture.scoreCheckpoints.at(1).at("round_completed") == true &&
                fixture.scoreCheckpoints.at(1).at("save_succeeded") == false &&
                fixture.scoreCheckpoints.at(2).at("save_succeeded") == false &&
                fixture.scoreCheckpoints.back().at("save_succeeded") == fixture.recoveredClose(),
            "Completion acknowledges once while save outcome follows failure/recovery schedule");
        if (!fixture.recoveredClose()) {
            fixture.require(readBytes(fixture.profile / "scores.json") ==
                                    fixture.baselineScoreBytes &&
                                reopened.completedGames().empty() &&
                                fixture.scoreCheckpoints.back().at("dirty") == true &&
                                targeted.back().at("injected_IoError") == true,
                            "Persistent failure preserves baseline bytes and dirty memory; "
                            "completion is not durable");
        } else
            fixture.require(
                targeted.back().at("status") ==
                        static_cast<int>(numtilo_core::FileSaveStatus::Saved) &&
                    fixture.scoreCheckpoints.back().at("dirty") == false &&
                    completedEntries(reopened) == memory &&
                    reopened.bestScore() == state.at("best"),
                "Recovered close delegates real save and reopens exactly the original completion");
    } else
        fixture.require(completedEntries(reopened) == memory &&
                            reopened.bestScore() == state.at("best"),
                        "Successful close reopens exact best/history without a fabricated "
                        "new-round completion");
    fixture.require(!reopenedSettings.soundEnabled(),
                    "Reopened Sound remains the accepted baseline");
    fixture.controlledComplete = true;
}

template <typename Update, typename Snapshot, typename State, typename Route>
void runControlledBatch(Update update, Snapshot snapshot, State state, Route route) {
    auto &fixture = runtime();
    using app::ui::SceneId;
    update();
    const auto before = state();
    const auto initial = snapshot();
    fixture.snapshot(initial, before);
    fixture.require(initial.roundId == before.at("round").template get<std::string>(),
                    "Initial request tags refer to the actual current round");
    fixture.require(fixture.accepted == 0 && fixture.rejected == 0,
                    "Controlled group starts without another dispatched request");
    fixture.cause = "controlled-old-tag-group";

    if (fixture.caseId == kPlayingBatch) {
        fixture.require(before.at("scene_id") == static_cast<int>(SceneId::Playing),
                        "Playing batch starts in Playing");
        const std::array group{press(initial, "new"), press(initial, "new"),
                               press(initial, "sound"), press(initial, "menu")};
        for (const auto &request : group)
            route(request);
        const auto after = state();
        fixture.require(fixture.accepted == 1 && fixture.rejected == 3,
                        "One New accepted and all three old-tag requests rejected");
        fixture.require(after.at("scene_id") == static_cast<int>(SceneId::Playing),
                        "Rejected old Menu cannot leave Playing");
        fixture.require(after.at("round") != before.at("round") &&
                            after.at("generation") == initial.generation + 1,
                        "Exactly one accepted New changes round and scene visit");
        fixture.require(after.at("score") == 0 && after.at("best") == before.at("best") &&
                            after.at("completed") == before.at("completed") &&
                            after.at("sound_enabled") == before.at("sound_enabled"),
                        "Old-tag tail cannot change score records or Sound");
        fixture.cause = "controlled-fresh-current-request";
        route(press(snapshot(), "sound"));
        const auto fresh = state();
        fixture.require(fixture.accepted == 2 && fixture.rejected == 3 &&
                            fresh.at("sound_enabled") != after.at("sound_enabled") &&
                            fresh.at("round") == after.at("round"),
                        "Fresh current-tag Sound remains usable and toggles once");
        route(press(snapshot(), "new"));
        const auto newer = state();
        fixture.require(fixture.accepted == 3 && fixture.rejected == 3 &&
                            newer.at("round") != fresh.at("round") &&
                            newer.at("generation") ==
                                fresh.at("generation").template get<std::uint64_t>() + 1,
                        "Fresh current-tag New remains usable after rejected same-ID New");
    } else {
        fixture.require(fixture.caseId == kWinBatch &&
                            before.at("scene_id") == static_cast<int>(SceneId::Win),
                        "Current GameplayOutcome enters genuine Win before the group");
        const std::array group{press(initial, "continue"), press(initial, "new")};
        for (const auto &request : group)
            route(request);
        const auto after = state();
        fixture.require(fixture.accepted == 1 && fixture.rejected == 1,
                        "Continue accepted and old Win-tag New rejected");
        fixture.require(after.at("scene_id") == static_cast<int>(SceneId::Playing) &&
                            after.at("generation") == initial.generation + 1 &&
                            after.at("round") == before.at("round") &&
                            after.at("grid") == before.at("grid") &&
                            after.at("score") == before.at("score"),
                        "Continue changes scene visit while retaining round, grid and score");
        fixture.require(after.at("best") == before.at("best") &&
                            after.at("completed") == before.at("completed") &&
                            after.at("sound_enabled") == before.at("sound_enabled"),
                        "Rejected old Win New cannot change records or Sound");
        fixture.cause = "controlled-fresh-current-request";
        route(press(snapshot(), "new"));
        const auto fresh = state();
        fixture.require(fixture.accepted == 2 && fixture.rejected == 1 &&
                            fresh.at("scene_id") == static_cast<int>(SceneId::Playing) &&
                            fresh.at("round") != after.at("round") && fresh.at("score") == 0 &&
                            fresh.at("generation") ==
                                after.at("generation").template get<std::uint64_t>() + 1,
                        "Fresh Playing-tag New works after Continue");
    }
    fixture.require(!fixture.truncated, "Controlled trace is complete");
    fixture.controlledComplete = true;
}

inline int runMain(const int argc, char **argv, const Preparation &preparation) {
    auto &fixture = runtime();
    for (int index = 1; index < argc; ++index) {
        const std::string_view option{argv[index]};
        if (option == "--help") {
            std::cout << "--case CASE --run-output /absolute/fresh/build/descendant\nCases:\n";
            for (const auto name : kCases)
                std::cout << name << '\n';
            return 0;
        }
        if (index + 1 == argc)
            throw std::runtime_error("Each runtime option requires a value");
        const std::string value{argv[++index]};
        if (option == "--case" && fixture.caseId.empty())
            fixture.caseId = value;
        else if (option == "--run-output" && fixture.runOutput.empty())
            fixture.runOutput = value;
        else
            throw std::runtime_error("Unknown or repeated runtime option");
    }
    if (std::find(kCases.begin(), kCases.end(), fixture.caseId) == kCases.end() ||
        fixture.runOutput.empty())
        throw std::runtime_error("Explicit --case and fresh --run-output are required; no launch");

    requireOrdinaryDirectoryChain(preparation.projectRoot / "build");
    requireOrdinaryDirectoryChain(preparation.output);
    requireOrdinaryDirectoryChain(fixture.runOutput.parent_path());
    if (!fixture.runOutput.is_absolute() ||
        fixture.runOutput.lexically_normal() != fixture.runOutput ||
        !below(preparation.output, preparation.projectRoot / "build") ||
        !below(fixture.runOutput, preparation.projectRoot / "build") ||
        std::filesystem::exists(std::filesystem::symlink_status(fixture.runOutput)))
        throw std::runtime_error("Runtime output must be a fresh ordinary build descendant");
    for (const auto &source : preparation.sources) {
        if (readBytes(source.path, source.bytes.size(), source.bytes.size()) != source.bytes)
            throw std::runtime_error(
                "Current source changed; regenerate/recompile before launch: " +
                std::string(source.name));
    }
    if (!std::filesystem::create_directory(fixture.runOutput))
        throw std::runtime_error("Cannot reserve fresh runtime output");
    fixture.profile = fixture.runOutput / "profile";
    const auto legacy = fixture.runOutput / "legacy-empty";
    if (!std::filesystem::create_directory(fixture.profile) ||
        !std::filesystem::create_directory(legacy))
        throw std::runtime_error("Cannot create ordinary isolated profile directories");
    fixture.winMoves = preparation.winMoves;
    if (fixture.eventLoop()) {
        app::SettingsStore initialSettings(fixture.profile / "settings.json");
        if (!initialSettings.load() || !initialSettings.setSoundEnabled(false) ||
            !initialSettings.save())
            throw std::runtime_error("Cannot create an ordinary isolated Sound baseline");
    }
    app::RunConfig config;
    config.userDataOverride = fixture.profile;
    config.assetOverride = preparation.projectRoot / "assets";
    config.legacyDataOverride = legacy;
    config.frameLimit = 60;
    const auto preparationReport =
        nlohmann::json::parse(readBytes(preparation.output / "preparation.json"));
    int result = 1;
    std::string error;
    try {
        result = app::run(config);
    } catch (const std::exception &failure) {
        error = failure.what();
    }
    const bool passed = !fixture.lifecycle() && result == 0 && fixture.controlledComplete &&
                        !fixture.truncated && error.empty();
    const auto status = fixture.lifecycle() && result == 0 && error.empty()
                            ? "recorded-pending-native-review"
                        : passed ? fixture.eventLoop() ? "passed-controlled-App-event-loop"
                                                       : "passed-controlled-App-dispatch"
                                 : "failed";
    fixture.flush();
    const auto report = nlohmann::json{
        {"schema_version", 1},
        {"case", fixture.caseId},
        {"status", status},
        {"exit_code", result},
        {"error", error},
        {"profile", fixture.profile.string()},
        {"assets", config.assetOverride->string()},
        {"legacy", legacy.string()},
        {"checks", fixture.checks},
        {"accepted", fixture.accepted},
        {"rejected", fixture.rejected},
        {"trace_truncated", fixture.truncated},
        {"score_checkpoints", fixture.scoreCheckpoints},
        {"settings_checkpoints", fixture.settingsCheckpoints},
        {"storage_attempts", fixture.storageAttempts},
        {"disk_reload", fixture.diskReload},
        {"controlled_events_polled", fixture.nextEvent},
        {"controlled_events_remaining", fixture.events.size() - fixture.nextEvent},
        {"closed_events", fixture.closedEvents},
        {"provider_was_attached", fixture.providerWasAttached},
        {"provider_attached_after_close", fixture.providerAttached},
        {"drained_after_detach", fixture.drainedAfterDetach},
        {"publish_after_detach", fixture.publishAfterDetach
                                     ? nlohmann::json(*fixture.publishAfterDetach)
                                     : nlohmann::json(nullptr)},
        {"final_move_state", fixture.finalMoveState},
        {"closed_state", fixture.closedState},
        {"end_state", fixture.endState},
        {"retained_native_callback_injected", false},
        {"initially_populated_native_queue_flush_proof", false},
        {"controlled_events_are_OS_events", false},
        {"prepared_source_hashes", preparationReport.at("source_hashes")},
        {"source_byte_preflight_scope", preparationReport.at("source_byte_preflight_scope")},
        {"linked_object_freshness",
         "Requires fresh product build then fixture regeneration/recompilation; "
         "not certified by three source-byte matches"},
        {"source_normalization", preparationReport.at("source_normalization")},
        {"controlled_cleanup_is_OS_Closed_proof", false},
        {"native_callback_or_takeActions_proof", false}};
    writeEvidence(fixture.runOutput / "result.json", report.dump(2) + '\n');
    writeEvidence(fixture.runOutput / "exit.txt", std::to_string(result) + '\n');
    if (!error.empty())
        std::cerr << error << '\n';
    return result;
}

} // namespace numtilo_edge
