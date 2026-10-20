#include "app/GameSession.hpp"
#include "app/ui/GameOverScene.hpp"
#include "app/ui/GameplayOutcome.hpp"
#include "app/ui/HighScoresScene.hpp"
#include "app/ui/PlayingScene.hpp"
#include "app/ui/SceneInputGuard.hpp"
#include "app/ui/SplashScene.hpp"
#include "app/ui/WinScene.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>
#include <stdexcept>
#include <thread>

namespace {
using namespace app::ui;
using Json = nlohmann::json;
int checks = 0;
std::string context;
void require(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(context + ": " + message);
}
sf::Event key(sf::Keyboard::Key code, bool release = false, bool shift = false) {
    sf::Event e{};
    e.type = release ? sf::Event::KeyReleased : sf::Event::KeyPressed;
    e.key.code = code;
    e.key.shift = shift;
    return e;
}
sf::Event pointer(sf::Vector2i p, bool release = false,
                  sf::Mouse::Button button = sf::Mouse::Left) {
    sf::Event e{};
    e.type = release ? sf::Event::MouseButtonReleased : sf::Event::MouseButtonPressed;
    e.mouseButton = {button, p.x, p.y};
    return e;
}
sf::IntRect pixels(const sf::Image &image, sf::Color color) {
    int left = image.getSize().x, top = image.getSize().y, right = -1, bottom = -1;
    for (unsigned y = 0; y < image.getSize().y; ++y)
        for (unsigned x = 0; x < image.getSize().x; ++x)
            if (image.getPixel(x, y) == color) {
                left = std::min(left, int(x));
                top = std::min(top, int(y));
                right = std::max(right, int(x));
                bottom = std::max(bottom, int(y));
            }
    return right < 0 ? sf::IntRect{} : sf::IntRect(left, top, right - left + 1, bottom - top + 1);
}
unsigned errorInk(const sf::Image &image, const sf::RenderWindow &window,
                  const AccessibilityFrame &field) {
    // Thin glyphs can have no fully covered pixel. Count visible coverage on the
    // actual error-color blend ray inside the reserved validation band instead.
    const auto first = window.mapCoordsToPixel({field.x, field.y + field.height + 4.f});
    const auto last =
        window.mapCoordsToPixel({field.x + field.width, field.y + field.height + 28.f});
    const auto foreground = theme::colors::kError, background = theme::colors::kPanel;
    unsigned count = 0;
    for (int y = first.y; y < last.y; ++y)
        for (int x = first.x; x < last.x; ++x) {
            const auto color = image.getPixel(x, y);
            const float coverage =
                (float(background.g) - color.g) / (float(background.g) - foreground.g);
            if (coverage < .2f || coverage > 1.01f)
                continue;
            const float red = background.r + (float(foreground.r) - background.r) * coverage;
            const float blue = background.b + (float(foreground.b) - background.b) * coverage;
            if (std::abs(red - color.r) <= 3.f && std::abs(blue - color.b) <= 3.f)
                ++count;
        }
    return count;
}
void setName(SplashScene &scene, const sf::RenderWindow &window, const sf::String &value) {
    scene.resetInputState();
    scene.handleEvent(key(sf::Keyboard::Tab), window);
    sf::Event text{};
    text.type = sf::Event::TextEntered;
    text.text.unicode = 8;
    for (int n = 0; n < 18; ++n)
        scene.handleEvent(text, window);
    for (auto code : value) {
        text.text.unicode = code;
        scene.handleEvent(text, window);
    }
    scene.resetInputState();
}
struct Control {
    const char *name;
    ButtonVariant variant;
    int tabs;
    bool reverse;
    SceneCommand command;
    bool playing = false, sound = false;
    std::function<SceneCommand(const sf::Event &)> event;
    std::function<void()> reset;
    std::function<void(sf::RenderTarget &)> draw;
    std::function<void(sf::Vector2f)> hover;
};
using Capture =
    std::function<sf::Image(const std::string &, const std::function<void(sf::RenderTarget &)> &)>;
void inputChecks(const std::vector<Control> &controls, sf::RenderWindow &window,
                 sf::RenderTexture &target, const sf::Font &font, const Capture &capture,
                 Json &report) {
    for (const auto &c : controls) {
        context = c.name;
        c.reset();
        target.clear(theme::colors::kBackground);
        c.draw(target);
        target.display();
        const auto rect = pixels(target.getTexture().copyToImage(),
                                 c.sound ? theme::colors::kPanel : buttonColors(c.variant).fill);
        require(rect.width > 200, "actual control fill is measurable");
        const auto first = window.mapPixelToCoords({rect.left, rect.top});
        const auto last = window.mapPixelToCoords({rect.left + rect.width, rect.top + rect.height});
        auto shape = c.sound ? createPanel(last - first)
                             : createButtonForText(
                                   sf::Text("X", font, theme::typography::kButtonSize), c.variant);
        shape.setSize(last - first);
        shape.setOrigin(0, 0);
        shape.setPosition(first);
        shape.setOutlineThickness(2.f);
        const auto normal = shape.getGlobalBounds();
        shape.setOutlineThickness(3.f);
        const auto focused = shape.getGlobalBounds();
        const float padding = c.playing ? 0.f : 4.f;
        const auto center = sf::Vector2i(rect.left + rect.width / 2, rect.top + rect.height / 2);
        const auto anchor =
            window.mapCoordsToPixel({normal.left - padding, first.y + (last.y - first.y) / 2});
        std::optional<sf::Vector2i> ring;
        for (int offset = -4; offset <= 4; ++offset) {
            const sf::Vector2i p(anchor.x + offset, anchor.y);
            const auto point = window.mapPixelToCoords(p);
            if (!containsWithPadding(normal, point, padding) &&
                containsWithPadding(focused, point, padding))
                ring = p;
        }
        require(ring.has_value(), "physical focus-only ring sample exists");
        const auto focus = [&] {
            for (int n = 0; n < c.tabs; ++n)
                c.event(key(sf::Keyboard::Tab, false, c.reverse));
        };
        const auto density = window.getSize().x / 920U;
        c.reset();
        focus();
        const auto focusedImage = capture(std::string("control-") + c.name + "-focus", c.draw);
        const sf::Vector2i outline(rect.left - int(std::ceil(2.5f * density)), center.y);
        require(focusedImage.getPixel(outline.x, outline.y) == theme::colors::kInputFocusBorder,
                "focus outline has visible ink outside the normal border");
        c.reset();
        c.hover(window.mapPixelToCoords(center));
        std::this_thread::sleep_for(std::chrono::milliseconds(140));
        const auto hoveredImage = capture(std::string("control-") + c.name + "-hover", c.draw);
        const auto hoverColor =
            c.sound ? theme::colors::kSoundOnHover : buttonColors(c.variant).hover;
        const auto hoverBounds = pixels(hoveredImage, hoverColor);
        require(c.sound ? hoverBounds.width > 0 : hoverBounds == rect,
                "settled hover uses the expected actual palette and geometry");
        c.event(pointer(center));
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        const auto heldImage = capture(std::string("control-") + c.name + "-pressed", c.draw);
        const auto heldBounds = pixels(heldImage, c.sound ? theme::colors::kPanel : hoverColor);
        require(c.sound ? heldBounds == rect
                        : std::abs(heldBounds.width - rect.width * .98f) <= 2.f * density &&
                              std::abs(heldBounds.height - rect.height * .98f) <= 2.f * density &&
                              heldBounds.width < rect.width,
                "button press visibly shrinks; Sound retains its established row geometry");
        report["visual_controls"].push_back(
            {{"name", c.name},
             {"density", density},
             {"focus_outline", true},
             {"hover_palette", true},
             {"pressed_geometry", c.sound ? "unchanged-row" : "0.98"}});
        c.reset();
        c.event(pointer(center, true));
        require(c.event(pointer(center, true)) == SceneCommand::None, "unmatched release neutral");
        for (bool beforeFocus : {false, true}) {
            c.reset();
            if (!beforeFocus)
                focus();
            c.event(pointer(beforeFocus ? center : *ring));
            if (beforeFocus)
                focus();
            require(c.event(pointer(*ring, true)) == SceneCommand::None,
                    "focus cannot expand press/release ownership");
        }
        c.reset();
        c.event(pointer(center));
        sf::Event move{};
        move.type = sf::Event::MouseMoved;
        move.mouseMove = {0, 0};
        c.event(move);
        move.mouseMove = {center.x, center.y};
        c.event(move);
        require(c.event(pointer(center, true)) == SceneCommand::None,
                "drag-off return remains cancelled");
        for (bool blur : {false, true}) {
            c.reset();
            c.event(pointer(center));
            if (blur) {
                sf::Event e{};
                e.type = sf::Event::LostFocus;
                c.event(e);
            } else
                c.reset();
            require(c.event(pointer(center, true)) == SceneCommand::None,
                    "blur/reset cancels gesture");
        }
        c.reset();
        c.event(pointer(center, false, sf::Mouse::Right));
        require(c.event(pointer(center, true)) == SceneCommand::None,
                "secondary press cannot own primary release");
        for (auto activation : {sf::Keyboard::Enter, sf::Keyboard::Space}) {
            c.reset();
            focus();
            require(c.event(key(activation)) == SceneCommand::None, "key press waits");
            require(c.event(key(activation, true)) == c.command, "focused key release activates");
            require(c.event(key(activation, true)) == SceneCommand::None,
                    "key release activates once");
        }
        c.reset();
        c.event(pointer(center));
        require(c.event(pointer(center, true)) == c.command, "fresh pointer recovers");
        c.reset();
        SceneInputGuard guard;
        require(guard.accepts(pointer(center)), "fresh scene press accepted");
        guard.onSceneChanged();
        require(!guard.accepts(pointer(center, true)), "stale scene release rejected");
        report["controls"].push_back({{"name", c.name}, {"density", window.getSize().x / 920U}});
    }
}

// Versioned legal seed-zero first-win replay; Core uses its own bounded RNG mapping.
constexpr char kFirstWinMoves[] =
    "DDLLDLLLDLDDLDUDLRLRLDLDUDRDLDLRDLDLUDRDLLDLRDLRRLDUDLDRDLLLLDDUDULULUDLLDLULLDLLDDUUDDLLDDRLU"
    "DL"
    "UDLDDLRDDDLDRRRDDLLDRDLRDRLLDLRDDDLDDUDLLRUDLDLLDDLDDDRDLLDDRDLUDLRDLRDLUUDLRDDUDLLDLRDDDLRLDL"
    "LD"
    "RLLDLDRDRDRRRDDRLDRDLRDDDDDUDLRDLDLLRDRDDLLDLDRDDLLDLLDLLDLLRDDDUDUDDUDDDLDRLDRLDLDDDLLLDULDLR"
    "LD"
    "DLRDDLLRDDLDLLDLDLUDLLDRLUDDRDLDRLRDDRRDRDLDLDDUDLDRLRRDDLDLDLRDDDDLDDLRDLRDRUDRLRLRDLUUDRLULD"
    "RR"
    "LDLRLDRDLDRDLDLRLRDLLRDLUDLLLRDLRDDUDLDLDRDRLDLRDLRDDDLRDRLDRDLDDRLDDLDLLDRDLDRDDRDLRDRRRLRRDL"
    "DD"
    "DDDDLUUDUDUDRLRLUDDRDLLDDLLDUDLULUUDDDLDDLLDULULUDRLDUDDLLUDRDDLRDLLRDLDDLRDDLLURURDLDRLDLDRUD"
    "LL"
    "UUDLRLRDRDDDRDLDDLRDLRDLRDDLLDDLRRLLDRLDRDRLRDUDLLDDRDLDDRDDDLDLRDDLLRUDDLRRDRRDDLUDLLLRLDDLLR"
    "RD"
    "DDLDDDLLLDDLDLRDDLDDLDRDDRDLDLLLDLDRLRDDLLDLDLLDDRLLRLLLRDRLDLLLLDLRLRRLLRRLRLDDLDLRLDRDLDRDLR"
    "DL"
    "LLRRDLDDDRDRDDLRRDRDLRLRDRLLRDDDLDLDRDLDLDLDLDDLRDLLLRDRDLRDDLLLLLDDRRDRLDLDRLLDRLDLRDLLLLLRDL"
    "DR"
    "DDRDLLRDDDLRDLDLLDDLLDLRLDLRDDLDDLDLRLDRDDDRDRRDLLDDRDRDLDLLDRRLRLDRRDRDLLDLDLLDRDLDDRRDLLLRRR"
    "DD"
    "LLLLLDRDDLDLDLLDLDDRDLLLLDLDLDDLLLRLRLRRDLLDRDRRLLLDLLRDDLLDRURLDLDLL";
void outcomes(const sf::Font &font, sf::RenderWindow &window, Json &report) {
    context = "legal-round outcomes";
    app::GameSession session(0U);
    session.setPlayerName("Çağrı");
    GameplayOutcome outcome;
    PlayingScene playing(font);
    WinScene win(font, 856, 596);
    GameOverScene over(font, 856, 596);
    numtilo_core::MoveResult last;
    for (char c : std::string_view(kFirstWinMoves))
        last = session.applyMove(c == 'L'   ? numtilo_core::Direction::Left
                                 : c == 'R' ? numtilo_core::Direction::Right
                                 : c == 'U' ? numtilo_core::Direction::Up
                                            : numtilo_core::Direction::Down);
    require(last.moved && session.game().hasWon() && !session.game().isGameOver(),
            "legal first-win fixture");
    const auto round = session.gameId();
    const auto grid = session.game().getGrid();
    const int score = session.game().getScore();
    playing.onMove(last);
    require(outcome.nextScene(SceneId::Playing, round, session.game(), true) == SceneId::Playing,
            "Win waits for final animation");
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (playing.hasActiveAnimations() && Clock::now() < deadline) {
        playing.tickVisuals();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(!playing.hasActiveAnimations(), "final move settles before deadline");
    require(outcome.nextScene(SceneId::Playing, round, session.game(), false) == SceneId::Win,
            "settled first Win appears");
    require(win.handleEvent(key(sf::Keyboard::Escape), window) == SceneCommand::ContinueGame,
            "actual Win Continue command");
    require(session.gameId() == round && session.game().getGrid() == grid &&
                session.game().getScore() == score,
            "Continue preserves session");
    require(outcome.nextScene(SceneId::Playing, round, session.game(), false) == SceneId::Playing,
            "same round Win is presented once");
    constexpr std::array directions{numtilo_core::Direction::Left, numtilo_core::Direction::Up,
                                    numtilo_core::Direction::Right, numtilo_core::Direction::Down};
    for (int turn = 0; turn < 5000 && !session.game().isGameOver(); ++turn)
        session.applyMove(directions[turn % 4]);
    require(session.game().isGameOver() &&
                outcome.nextScene(SceneId::Playing, round, session.game(), false) ==
                    SceneId::GameOver,
            "continued won round reaches GameOver");
    require(over.handleEvent(key(sf::Keyboard::N), window) == SceneCommand::RestartGame,
            "actual Replay command");
    session.resetGame(0U);
    require(session.gameId() != round && session.playerName() == "Çağrı" &&
                session.game().getScore() == 0,
            "Replay resets identity/score and preserves name");
    require(outcome.nextScene(SceneId::Playing, session.gameId(), session.game(), false) ==
                SceneId::Playing,
            "new round starts without a stale outcome");
    for (char c : std::string_view(kFirstWinMoves))
        session.applyMove(c == 'L'   ? numtilo_core::Direction::Left
                          : c == 'R' ? numtilo_core::Direction::Right
                          : c == 'U' ? numtilo_core::Direction::Up
                                     : numtilo_core::Direction::Down);
    require(outcome.nextScene(SceneId::Playing, session.gameId(), session.game(), false) ==
                SceneId::Win,
            "new round can present its own Win");
    numtilo_core::Game terminal(0U);
    terminal.loadState({{{1024, 1024, 4, 8}, {2, 8, 16, 32}, {4, 16, 32, 64}, {8, 32, 64, 128}}});
    terminal.applyMove(numtilo_core::Direction::Left);
    GameplayOutcome priority;
    require(terminal.hasWon() && terminal.isGameOver(),
            "controlled simultaneous winning/terminal board");
    require(priority.nextScene(SceneId::Playing, "controlled", terminal, false) == SceneId::Win &&
                priority.nextScene(SceneId::Playing, "controlled", terminal, false) ==
                    SceneId::GameOver,
            "terminal Win precedes continued GameOver");
    report["outcomes"] = {{"legal_first_win_score", score},
                          {"new_round_win", true},
                          {"terminal_priority", true},
                          {"scope", "Public session/outcome and actual modal commands; App "
                                    "dispatch and disk persistence are separate tests"}};
}
void semanticChecks(const sf::Font &font, sf::RenderWindow &window, Json &report) {
    context = "semantic guards";
    SplashScene splash(font, 856, 596);
    HighScoresScene scores(font, 856, 596);
    PlayingScene playing(font);
    WinScene win(font, 856, 596);
    GameOverScene over(font, 856, 596);
    numtilo_core::Game game(0U);
    require(splash.setAccessiblePlayerName(std::string(18, 'i')),
            "18-character bulk name accepted");
    const auto retained = splash.playerName();
    require(!splash.setAccessiblePlayerName(std::string(19, 'i')) &&
                splash.playerName() == retained,
            "over-limit bulk edit retains previous value");
    require(splash.setAccessiblePlayerName("   ") &&
                splash.activateAccessible("start") == SceneCommand::None,
            "blank accessible name cannot start");
    require(splash.setAccessiblePlayerName("你😀") &&
                splash.activateAccessible("start") == SceneCommand::None,
            "unsupported editable name remains correctable and cannot start");
    require(splash.setAccessiblePlayerName("Çağrı") &&
                splash.activateAccessible("start") == SceneCommand::StartGame,
            "valid accessible edit recovers");
    require(!splash.focusAccessible("unknown") &&
                splash.activateAccessible("unknown") == SceneCommand::None,
            "unknown action cannot navigate");
    std::vector<std::vector<AccessibilityNode>> trees{
        splash.accessibilityNodes(), scores.accessibilityNodes({}),
        playing.accessibilityNodes(game, 856, std::numeric_limits<int>::max(), "Çağrı"),
        win.accessibilityNodes(std::numeric_limits<int>::max()),
        over.accessibilityNodes(0, std::numeric_limits<int>::max())};
    for (const auto &nodes : trees) {
        AccessibilitySnapshot snapshot;
        snapshot.generation = 7;
        snapshot.roundId = "ui-round";
        snapshot.clientPixelWidth = 920;
        snapshot.clientPixelHeight = 660;
        snapshot.nodes = nodes;
        require(isValidAccessibilitySnapshot(snapshot), "scene semantic hierarchy is valid");
        const auto target = std::find_if(nodes.begin(), nodes.end(),
                                         [](const auto &node) { return node.focusable; });
        require(target != nodes.end(), "scene has a discoverable focus target");
        AccessibilityAction action{target->id, AccessibilityActionKind::Focus, "", 7, "ui-round"};
        require(acceptsAccessibilityAction(snapshot, action), "fresh tagged focus is accepted");
        --action.generation;
        require(!acceptsAccessibilityAction(snapshot, action), "stale generation rejected");
        action.generation = 7;
        action.roundId = "old-round";
        require(!acceptsAccessibilityAction(snapshot, action), "stale round rejected");
    }
    require(playing.focusAccessible("new"), "accessible New focus works");
    playing.handleEvent(key(sf::Keyboard::Space), window, 856);
    const auto move = playing.activateAccessible("left");
    require(move.direction == numtilo_core::Direction::Left, "accessible board direction accepted");
    require(playing.handleEvent(key(sf::Keyboard::Space, true), window, 856).command ==
                SceneCommand::None,
            "accepted board action cancels held New activation");
    game.loadState({{{2, 2, 0, 0}, {}, {}, {}}});
    playing.onMove(game.applyMove(numtilo_core::Direction::Left, false));
    require(!playing.activateAccessible("right").direction.has_value(),
            "animation gates accessible directions");
    const auto active = playing.accessibilityNodes(game, 856, 0, "Çağrı");
    const auto board = std::find_if(active.begin(), active.end(),
                                    [](const auto &node) { return node.id == "board"; });
    require(board != active.end() && board->customActions.empty(),
            "busy board exposes no active move actions");
    require(std::count_if(active.begin(), active.end(),
                          [](const auto &node) { return node.role == AccessibilityRole::Cell; }) ==
                16,
            "board exposes all sixteen cells including empty values");
    report["semantics"] = {{"valid_scene_snapshots", 5},
                           {"stale_requests_rejected", true},
                           {"bulk_name_guards", true},
                           {"held_key_guard", true},
                           {"scope", "Pure semantic contract; OS-native exposure is separate"}};
}
void keyboardInputEdges(const sf::Font &font, const sf::RenderWindow &window, Json &report) {
    // Controlled scene/guard composition only. GainedFocus depends on physical
    // keyboard state; release outside the app and restoration remain native evidence.
    auto runCase = [&](const char *id, const auto &body) {
        context = id;
        const int before = checks;
        report["input_edges"].push_back({{"id", id},
                                         {"status", "failed"},
                                         {"layer", "controlled-guard-scenes"},
                                         {"checks", 0}});
        auto &row = report["input_edges"].back();
        try {
            body();
        } catch (...) {
            row["checks"] = checks - before;
            throw;
        }
        row["checks"] = checks - before;
        row["status"] = "passed";
    };
    auto rejectRepeats = [](SceneInputGuard &guard, const sf::Keyboard::Key code) {
        for (int repeat = 0; repeat < 3; ++repeat)
            require(!guard.accepts(key(code)), "old held key repeats are rejected");
    };
    runCase("E11-01-enter", [&] {
        SceneInputGuard guard;
        SplashScene splash(font, 856, 596);
        PlayingScene playing(font);
        require(splash.setAccessiblePlayerName("Çağrı"), "valid name prepares Start");
        require(guard.accepts(key(sf::Keyboard::Enter)), "fresh Enter is scene-owned");
        require(splash.handleEvent(key(sf::Keyboard::Enter), window) == SceneCommand::StartGame,
                "unfocused-button Enter follows Splash's immediate Start policy");
        guard.onSceneChanged();
        splash.resetInputState();
        playing.resetInputState();
        rejectRepeats(guard, sf::Keyboard::Enter);
        require(!guard.accepts(key(sf::Keyboard::Enter, true)),
                "old Enter release cannot reach Playing");
        require(guard.accepts(key(sf::Keyboard::Enter)), "released Enter is usable again");
        require(playing.handleEvent(key(sf::Keyboard::Enter), window, 856).command ==
                    SceneCommand::RestartGame,
                "fresh Enter follows Playing's immediate New policy");
        require(guard.accepts(key(sf::Keyboard::Enter, true)), "fresh Enter release is accepted");
        require(playing.handleEvent(key(sf::Keyboard::Enter, true), window, 856).command ==
                    SceneCommand::None,
                "immediate New does not activate again on release");
    });
    runCase("E11-01-new", [&] {
        SceneInputGuard guard;
        PlayingScene playing(font);
        require(guard.accepts(key(sf::Keyboard::N)), "fresh N is scene-owned");
        require(playing.handleEvent(key(sf::Keyboard::N), window, 856).command ==
                    SceneCommand::RestartGame,
                "N supplies the existing New command");
        guard.onSceneChanged();
        playing.resetInputState();
        rejectRepeats(guard, sf::Keyboard::N);
        require(!guard.accepts(key(sf::Keyboard::N, true)), "pre-reset N release is consumed");
        require(guard.accepts(key(sf::Keyboard::N)), "released N is usable again");
        require(playing.handleEvent(key(sf::Keyboard::N), window, 856).command ==
                    SceneCommand::RestartGame,
                "fresh N still supplies New after reset");
        require(guard.accepts(key(sf::Keyboard::N, true)), "fresh N release is accepted");
        require(playing.handleEvent(key(sf::Keyboard::N, true), window, 856).command ==
                    SceneCommand::None,
                "fresh N release cannot repeat New");
    });
    runCase("E11-01-direction", [&] {
        SceneInputGuard guard;
        WinScene win(font, 856, 596);
        PlayingScene playing(font);
        require(guard.accepts(key(sf::Keyboard::Left)), "held direction belongs to Win");
        require(win.handleEvent(key(sf::Keyboard::Left), window) == SceneCommand::None,
                "Win ignores direction input");
        require(guard.accepts(key(sf::Keyboard::Escape)), "fresh Continue shortcut is usable");
        require(win.handleEvent(key(sf::Keyboard::Escape), window) == SceneCommand::ContinueGame,
                "Escape supplies Win's Continue command");
        guard.onSceneChanged();
        playing.resetInputState();
        rejectRepeats(guard, sf::Keyboard::Left);
        require(!guard.accepts(key(sf::Keyboard::Escape, true)),
                "old Continue shortcut release is consumed");
        require(!guard.accepts(key(sf::Keyboard::Left, true)),
                "old direction release cannot reach Playing");
        require(guard.accepts(key(sf::Keyboard::Left)), "released direction is usable again");
        require(playing.handleEvent(key(sf::Keyboard::Left), window, 856).direction ==
                    numtilo_core::Direction::Left,
                "fresh direction reaches Playing's existing move policy");
        require(guard.accepts(key(sf::Keyboard::Left, true)),
                "fresh direction release is accepted");
    });
    runCase("E11-01-text", [&] {
        SceneInputGuard guard;
        SplashScene splash(font, 856, 596);
        require(splash.setAccessiblePlayerName("A"), "name field starts with a known value");
        require(guard.accepts(key(sf::Keyboard::N)), "first held key is tracked");
        require(guard.accepts(key(sf::Keyboard::Left)), "second held key is tracked");
        guard.onSceneChanged();
        splash.resetInputState();
        require(splash.focusAccessible("name"), "destination name field is focused");
        sf::Event text{};
        text.type = sf::Event::TextEntered;
        text.text.unicode = 0x011f; // ğ
        auto routeText = [&] {
            if (!guard.accepts(text))
                return false;
            splash.handleEvent(text, window);
            return true;
        };
        require(!guard.accepts(key(sf::Keyboard::N)), "held N repeat stays blocked");
        require(!guard.accepts(key(sf::Keyboard::Left)), "held direction repeat stays blocked");
        require(!routeText(), "held-key text cannot reach the destination field");
        require(splash.playerName() == "A", "blocked text preserves the existing name");
        require(!guard.accepts(key(sf::Keyboard::N, true)), "first held release is consumed");
        require(!routeText(), "text remains blocked while another old key is held");
        require(splash.playerName() == "A", "partial release cannot leak text");
        require(!guard.accepts(key(sf::Keyboard::Left, true)), "last held release is consumed");
        require(routeText(), "fresh text is usable after every held key is released");
        require(splash.playerName() == "Ağ", "fresh Unicode text updates the name once");
    });
    runCase("E11-01-blur", [&] {
        SceneInputGuard guard;
        WinScene win(font, 856, 596);
        require(guard.accepts(key(sf::Keyboard::Tab)) &&
                    win.handleEvent(key(sf::Keyboard::Tab), window) == SceneCommand::None,
                "fresh Tab selects Continue without activating it");
        require(guard.accepts(key(sf::Keyboard::Tab, true)) &&
                    win.handleEvent(key(sf::Keyboard::Tab, true), window) == SceneCommand::None,
                "Tab release remains neutral");
        require(guard.accepts(key(sf::Keyboard::Enter)) &&
                    win.handleEvent(key(sf::Keyboard::Enter), window) == SceneCommand::None,
                "focused Continue waits for activation release");
        sf::Event blur{};
        blur.type = sf::Event::LostFocus;
        require(guard.accepts(blur), "LostFocus reaches the scene for cancellation");
        require(win.handleEvent(blur, window) == SceneCommand::None,
                "focus loss cannot activate Continue");
        const auto nodes = win.accessibilityNodes(0);
        require(
            std::none_of(nodes.begin(), nodes.end(), [](const auto &node) { return node.focused; }),
            "focus loss clears the modal's focused control");
        require(!guard.accepts(key(sf::Keyboard::Enter, true)),
                "unfocused old activation release is rejected");
        require(win.handleEvent(key(sf::Keyboard::Enter, true), window) == SceneCommand::None,
                "scene cancellation independently clears its pending activation");
        require(!guard.accepts(key(sf::Keyboard::Enter)), "unfocused key repeats stay rejected");
        sf::Event text{};
        text.type = sf::Event::TextEntered;
        text.text.unicode = 'n';
        require(!guard.accepts(text), "unfocused text is rejected");
        require(!guard.accepts(pointer({100, 100})), "unfocused pointer input is rejected");
        sf::Event close{};
        close.type = sf::Event::Closed;
        require(guard.accepts(close), "window close remains available while unfocused");
    });
}
void matrix(const sf::Font &font, const std::filesystem::path &output, Json &report) {
    for (unsigned density : {1U, 2U}) {
        sf::RenderWindow window(sf::VideoMode(920 * density, 660 * density), "UI acceptance",
                                sf::Style::Titlebar | sf::Style::Close);
        window.setVisible(false);
        window.setView(createContentView());
        require(window.getSize() == sf::Vector2u(920 * density, 660 * density),
                "physical input density");
        sf::RenderTexture canvas;
        require(canvas.create(920 * density, 660 * density), "render target");
        canvas.setView(createContentView());
        SplashScene splash(font, 856, 596);
        HighScoresScene scores(font, 856, 596);
        PlayingScene playing(font);
        WinScene win(font, 856, 596);
        GameOverScene over(font, 856, 596);
        numtilo_core::Game game(7);
        setName(splash, window, toUnicode("Çağrı"));
        auto menu = [&](const sf::Event &e) { return splash.handleEvent(e, window); };
        auto back = [&](const sf::Event &e) { return scores.handleEvent(e, window); };
        auto play = [&](const sf::Event &e) { return playing.handleEvent(e, window, 856).command; };
        auto victory = [&](const sf::Event &e) { return win.handleEvent(e, window); };
        auto end = [&](const sf::Event &e) { return over.handleEvent(e, window); };
        auto menuReset = [&] { splash.resetInputState(); };
        auto backReset = [&] { scores.resetInputState(); };
        auto playReset = [&] { playing.resetInputState(); };
        auto winReset = [&] { win.resetInputState(); };
        auto endReset = [&] { over.resetInputState(); };
        auto menuDraw = [&](sf::RenderTarget &t) { splash.render(t); };
        auto backDraw = [&](sf::RenderTarget &t) { scores.render(t, {}); };
        auto playDraw = [&](sf::RenderTarget &t) {
            playing.render(t, font, game, 856, 668, "Çağrı");
        };
        auto winDraw = [&](sf::RenderTarget &t) { win.render(t, std::numeric_limits<int>::max()); };
        auto endDraw = [&](sf::RenderTarget &t) {
            over.render(t, std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        };
        auto menuHover = [&](sf::Vector2f p) { splash.updateHover(p); };
        auto backHover = [&](sf::Vector2f p) { scores.updateHover(p); };
        auto playHover = [&](sf::Vector2f p) { playing.updateHover(p, 856); };
        auto winHover = [&](sf::Vector2f p) { win.updateHover(p); };
        auto endHover = [&](sf::Vector2f p) { over.updateHover(p); };
        const std::vector<Control> controls{
            {"splash-start", ButtonVariant::Primary, 2, false, SceneCommand::StartGame, false,
             false, menu, menuReset, menuDraw, menuHover},
            {"splash-scores", ButtonVariant::Secondary, 1, true, SceneCommand::ShowHighScores,
             false, false, menu, menuReset, menuDraw, menuHover},
            {"scores-back", ButtonVariant::Secondary, 1, false, SceneCommand::ShowSplash, false,
             false, back, backReset, backDraw, backHover},
            {"playing-new", ButtonVariant::Primary, 1, false, SceneCommand::RestartGame, true,
             false, play, playReset, playDraw, playHover},
            {"playing-menu", ButtonVariant::Secondary, 1, true, SceneCommand::ShowSplash, true,
             false, play, playReset, playDraw, playHover},
            {"playing-sound", ButtonVariant::Primary, 2, false, SceneCommand::ToggleSound, true,
             true, play, playReset, playDraw, playHover},
            {"win-continue", ButtonVariant::Primary, 1, false, SceneCommand::ContinueGame, false,
             false, victory, winReset, winDraw, winHover},
            {"win-new", ButtonVariant::Secondary, 1, true, SceneCommand::RestartGame, false, false,
             victory, winReset, winDraw, winHover},
            {"over-replay", ButtonVariant::Primary, 1, false, SceneCommand::RestartGame, false,
             false, end, endReset, endDraw, endHover},
            {"over-menu", ButtonVariant::Secondary, 1, true, SceneCommand::ShowSplash, false, false,
             end, endReset, endDraw, endHover}};
        const auto capture = [&](const std::string &label,
                                 const std::function<void(sf::RenderTarget &)> &draw) {
            context = label;
            const auto view = canvas.getView();
            canvas.clear(theme::colors::kBackground);
            draw(canvas);
            require(canvas.getView().getCenter() == view.getCenter() &&
                        canvas.getView().getSize() == view.getSize() &&
                        canvas.getView().getViewport() == view.getViewport(),
                    "drawing preserves content view");
            canvas.display();
            const auto image = canvas.getTexture().copyToImage();
            const auto filename = label + "-" + std::to_string(density) + "x.png";
            require(image.saveToFile((output / filename).string()), "PNG saved");
            report["images"].push_back({{"path", filename},
                                        {"density", density},
                                        {"width", 920 * density},
                                        {"height", 660 * density}});
            return image;
        };
        inputChecks(controls, window, canvas, font, capture, report);
        capture("splash-normal", menuDraw);
        setName(splash, window, toUnicode("ŞİĞÖÇÜğıöçüÇağrı"));
        capture("splash-long-turkish", menuDraw);
        splash.focusAccessible("name");
        const auto fieldNodes = splash.accessibilityNodes();
        const auto field = std::find_if(fieldNodes.begin(), fieldNodes.end(),
                                        [](const auto &n) { return n.id == "name"; });
        require(field != fieldNodes.end(), "name field exposes its frame");
        const auto fieldBorder = window.mapCoordsToPixel(
            {field->frame.x + 1.f, field->frame.y + field->frame.height / 2.f});
        const auto fieldFocused = capture("name-focus", menuDraw);
        require(fieldFocused.getPixel(fieldBorder.x, fieldBorder.y) ==
                    theme::colors::kInputFocusBorder,
                "name focus border is visible");
        setName(splash, window, "");
        require(splash.activateAccessible("start") == SceneCommand::None,
                "empty name remains blocked");
        const auto invalid = capture("name-empty-error", menuDraw);
        const auto invalidNodes = splash.accessibilityNodes();
        const auto invalidField = std::find_if(invalidNodes.begin(), invalidNodes.end(),
                                               [](const auto &node) { return node.id == "name"; });
        require(invalidField != invalidNodes.end() && invalidField->value.empty() &&
                    !invalidField->error.empty(),
                "empty-name validation remains explicit and correctable");
        const auto visibleErrorInk = errorInk(invalid, window, invalidField->frame);
        require(visibleErrorInk >= 4 * density,
                "empty-name error has visible antialiased error-color ink");
        report["validation_ink"].push_back(
            {{"density", density}, {"error_pixels", visibleErrorInk}});
        splash.setAccessiblePlayerName(std::string(18, 'i'));
        require(!splash.setAccessiblePlayerName(std::string(19, 'i')),
                "19th bulk character rejected");
        capture("name-18-limit", menuDraw);
        const auto limited = splash.accessibilityNodes();
        const auto nameNode = std::find_if(limited.begin(), limited.end(),
                                           [](const auto &n) { return n.id == "name"; });
        require(nameNode != limited.end() && nameNode->value == std::string(18, 'i') &&
                    !nameNode->error.empty(),
                "limit notice retains full editable value");
        setName(splash, window, toUnicode("ŞİĞÖÇÜğıöçüÇağrı"));
        playing.setSoundEnabled(false);
        const auto soundOff = capture("playing-sound-off", playDraw);
        require(pixels(soundOff, theme::colors::kSoundOff).width > 0,
                "Sound off palette is visible");
        const auto soundNodes = playing.accessibilityNodes(game, 856, 668, "Çağrı");
        const auto soundNode = std::find_if(soundNodes.begin(), soundNodes.end(),
                                            [](const auto &n) { return n.id == "sound"; });
        require(soundNode != soundNodes.end() && !soundNode->checked,
                "Sound off semantic state matches painting");
        playing.setSoundEnabled(true);
        for (unsigned rows = 0; rows <= 5; ++rows) {
            std::vector<numtilo_core::ScoreEntry> entries;
            for (unsigned n = 0; n < rows; ++n)
                entries.push_back({std::numeric_limits<int>::max() - int(n), "2026-01-01T00:00:00Z",
                                   n ? "ŞİĞÖÇÜğıöçüÇağrı" : "Çağrı", "ui-" + std::to_string(n)});
            capture("scores-" + std::to_string(rows),
                    [&](sf::RenderTarget &t) { scores.render(t, entries); });
        }
        capture("playing-normal", playDraw);
        capture("playing-wide", [&](sf::RenderTarget &t) {
            playing.render(t, font, game, 856, std::numeric_limits<int>::max(), "ŞİĞÖÇÜğıöçüÇağrı");
        });
        capture("win-wide", winDraw);
        capture("gameover-wide", endDraw);
        capture("tiles-and-cards", [&](sf::RenderTarget &t) {
            unsigned index = 0;
            for (int value : {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384,
                              1073741824, std::numeric_limits<int>::max()}) {
                const auto fit = drawTile(t, font, value,
                                          {70.f + 130.f * (index % 4), 70.f + 115.f * (index / 4)});
                require(fit.meetsMinimum, "full tile number meets readability floor");
                ++index;
            }
            for (int value : {0, 2000000000, std::numeric_limits<int>::max()}) {
                const auto fit =
                    drawScoreCard(t, font, {650.f, 40.f + float(index++ - 16) * 120.f, 124.f, 88.f},
                                  "Skor", std::to_string(value));
                require(fit.caption.meetsMinimum && fit.value.meetsMinimum,
                        "wide score retains floor and full value");
            }
        });
        for (bool gameOver : {false, true})
            for (int focused = 0; focused < 3; ++focused)
                for (auto sample : {std::pair{403.f, 0}, std::pair{404.f, 1}}) {
                    context = "modal 403/404";
                    std::function<SceneCommand(const sf::Event &)> event =
                        gameOver ? std::function<SceneCommand(const sf::Event &)>(end)
                                 : std::function<SceneCommand(const sf::Event &)>(victory);
                    if (gameOver)
                        endReset();
                    else
                        winReset();
                    for (int n = 0; n < focused; ++n)
                        event(key(sf::Keyboard::Tab));
                    const auto p = window.mapCoordsToPixel({428, sample.first});
                    event(pointer(p));
                    const auto expected = gameOver ? (sample.second ? SceneCommand::ShowSplash
                                                                    : SceneCommand::RestartGame)
                                                   : (sample.second ? SceneCommand::RestartGame
                                                                    : SceneCommand::ContinueGame);
                    require(event(pointer(p, true)) == expected,
                            "half-open modal owner survives focus changes");
                }
        playing.resetVisualEffects();
        game.loadState({{{2, 2, 4, 8}, {}, {}, {}}});
        const auto move = game.applyMove(numtilo_core::Direction::Left, false);
        playing.onMove(move);
        require(playing.hasActiveAnimations(), "real changed move starts visuals");
        capture("motion-start", playDraw);
        std::this_thread::sleep_until(
            Clock::now() + std::chrono::duration_cast<Clock::duration>(
                               std::chrono::duration<float>(kFloatingScoreDuration + 0.1f)));
        playing.tickVisuals();
        require(!playing.hasActiveAnimations(), "motion endpoint settles");
        const auto settled = capture("motion-end", playDraw);
        playing.resetVisualEffects();
        canvas.clear(theme::colors::kBackground);
        playDraw(canvas);
        canvas.display();
        const auto fresh = canvas.getTexture().copyToImage();
        require(std::equal(settled.getPixelsPtr(),
                           settled.getPixelsPtr() + settled.getSize().x * settled.getSize().y * 4,
                           fresh.getPixelsPtr()),
                "settled motion equals fresh final board");
        splash.onEnter();
        const auto reveal = capture("reveal-start", menuDraw);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        const auto shown = capture("reveal-end", menuDraw);
        require(!std::equal(reveal.getPixelsPtr(),
                            reveal.getPixelsPtr() + reveal.getSize().x * reveal.getSize().y * 4,
                            shown.getPixelsPtr()),
                "panel reveal has distinct endpoints");
        if (density == 1) {
            outcomes(font, window, report);
            semanticChecks(font, window, report);
            keyboardInputEdges(font, window, report);
        }
        window.close();
    }
}
} // namespace

int main(int argc, char **argv) {
    std::filesystem::path fontPath, output;
    Json report = {{"schema_version", 1},
                   {"status", "failed"},
                   {"scenes", {"Splash", "HighScores", "Playing", "Win", "GameOver"}},
                   {"densities", {1, 2}},
                   {"images", Json::array()},
                   {"controls", Json::array()},
                   {"input_edges", Json::array()}};
    try {
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc)
                throw std::runtime_error("Expected --font PATH --output DIRECTORY");
            const std::string option = argv[i];
            if (option == "--font" && fontPath.empty())
                fontPath = argv[i + 1];
            else if (option == "--output" && output.empty())
                output = argv[i + 1];
            else
                throw std::runtime_error("Unknown or duplicate option: " + option);
        }
        if (fontPath.empty() || output.empty())
            throw std::runtime_error("Expected --font PATH --output DIRECTORY");
        fontPath = std::filesystem::absolute(fontPath);
        output = std::filesystem::absolute(output);
        std::filesystem::create_directories(output);
        sf::Font font;
        require(font.loadFromFile(fontPath.string()), "font loads");
        require(supportsText(font, toUnicode("ÇğıŞöüİ…")),
                "production Turkish/ellipsis glyph coverage");
        const auto original = toUnicode("你😀");
        const auto projection = presentPlayerName(font, original);
        require(projection.hasReplacements && original == toUnicode("你😀"),
                "unsupported-name projection preserves source");
        sf::Text text("WWWWWWWWWWWWWWWWWW", font, 26);
        require(!fitTextWidth(text, 1.f, 16.f).meetsMinimum,
                "impossible width reports missed floor");
        text.setString("A");
        require(fitTextWidth(text, 100.f, 16.f).meetsMinimum && text.getScale().x == 1.f,
                "short text restores scale");
        matrix(font, output, report);
        report["status"] = "passed";
        report["checks"] = checks;
        report["font"] = fontPath.string();
        report["scope"] = "Controlled graphical scenes, glyph/readability, input ownership, public "
                          "session/outcome, motion endpoints. Native OS delivery, screen readers "
                          "and App/persistence routing remain separate evidence.";
        std::ofstream stream(output / "ui-acceptance.json");
        stream << report.dump(2) << '\n';
        stream.flush();
        if (!stream)
            throw std::runtime_error("Report write failed");
        std::cout << "PASS UI acceptance: " << checks << " checks; " << report["images"].size()
                  << " PNGs; 5 scenes at 1x/2x\n";
        return 0;
    } catch (const std::exception &error) {
        report["checks"] = checks;
        report["error"] = error.what();
        if (!output.empty()) {
            std::ofstream stream(output / "ui-acceptance.json");
            stream << report.dump(2) << '\n';
        }
        std::cerr << "UI acceptance failed: " << error.what() << '\n';
        return 1;
    }
}
