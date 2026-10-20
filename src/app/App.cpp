#include "app/App.hpp"
#include "app/AssetResolver.hpp"
#include "app/AssetLocations.hpp"
#include "app/AssetCatalog.hpp"
#include "app/GameSession.hpp"
#include "app/ScoreCoordinator.hpp"
#include "app/SoundManager.hpp"
#include "app/SettingsStore.hpp"
#include "app/UserDataPaths.hpp"
#include "app/ProfileMigration.hpp"
#include "app/ui/Accessibility.hpp"
#include "app/ui/GameOverScene.hpp"
#include "app/ui/GameplayOutcome.hpp"
#include "app/ui/HighScoresScene.hpp"
#include "app/ui/PlayingScene.hpp"
#include "app/ui/SceneInputGuard.hpp"
#include "app/ui/SplashScene.hpp"
#include "app/ui/UiTheme.hpp"
#include "app/ui/WinScene.hpp"
#include "core/ScoreManager.hpp"

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <iostream>
#include <system_error>

namespace {

using app::ui::SceneCommand;
using app::ui::SceneId;

constexpr unsigned int kWindowAntialiasingLevel = 16;

const char *pathFailureReason(app::PathErrorCode code) {
    switch (code) {
    case app::PathErrorCode::MissingLocation:
        return "gerekli konum bulunamadı";
    case app::PathErrorCode::EmptyPath:
        return "boş dosya yolu";
    case app::PathErrorCode::RelativePath:
        return "mutlak dosya yolu gerekli";
    case app::PathErrorCode::EmbeddedNull:
        return "dosya yolunda geçersiz NUL karakteri";
    case app::PathErrorCode::NotDirectory:
        return "seçilen konum bir klasör değil";
    case app::PathErrorCode::IoError:
        return "seçilen klasöre erişim işlemi tamamlanamadı";
    }
    return "dosya yolu çözümlenemedi";
}

void reportPathFailure(const app::PathError &error) {
    std::cerr << (error.kind == app::PathKind::UserData ? "Kullanıcı veri yolu hatası: "
                                                        : "Uygulama kaynak yolu hatası: ")
              << error.path << " (" << pathFailureReason(error.code);
    if (error.systemError) {
        std::cerr << ": " << error.systemError.message();
    }
    std::cerr << ")\n";
}

const char *assetFailureReason(app::AssetErrorCode code) {
    switch (code) {
    case app::AssetErrorCode::EmptyPath:
        return "boş kaynak adı";
    case app::AssetErrorCode::AbsolutePath:
        return "kaynak adı seçilen klasöre göreli olmalı";
    case app::AssetErrorCode::EmbeddedNull:
        return "kaynak adında geçersiz NUL karakteri";
    case app::AssetErrorCode::ParentTraversal:
        return "kaynak adı üst klasör içeremez";
    case app::AssetErrorCode::Missing:
        return "kaynak bulunamadı";
    case app::AssetErrorCode::WrongType:
        return "kaynak bir normal dosya değil";
    case app::AssetErrorCode::IoError:
        return "kaynak güvenle okunamadı";
    }
    return "kaynak yüklenemedi";
}

void reportAssetFailure(const app::AssetError &error) {
    std::cerr << "Uygulama kaynağı yüklenemedi: " << error.path << " ("
              << assetFailureReason(error.code);
    if (error.systemError) {
        std::cerr << ": " << error.systemError.message();
    }
    std::cerr << ")\n";
}

const char *importFailureReason(app::ProfileImportStatus status) {
    switch (status) {
    case app::ProfileImportStatus::InvalidLegacyLocation:
        return "eski kayıt konumu kullanılamıyor";
    case app::ProfileImportStatus::DestinationIoError:
        return "yeni dosyanın yokluğu güvenle doğrulanamadı";
    case app::ProfileImportStatus::SourceIoError:
        return "eski kayıt güvenle okunamadı";
    case app::ProfileImportStatus::InvalidSource:
        return "eski kayıt biçimi geçersiz";
    case app::ProfileImportStatus::UnsupportedVersion:
        return "eski kaydın sürümü desteklenmiyor";
    case app::ProfileImportStatus::SourceChanged:
        return "eski kayıt aktarım öncesinde değişti";
    case app::ProfileImportStatus::PublicationFailed:
        return "doğrulanmış kayıt yeni konuma güvenle yazılamadı";
    case app::ProfileImportStatus::DestinationPresent:
    case app::ProfileImportStatus::SourceMissing:
    case app::ProfileImportStatus::Imported:
        return "aktarım hatası";
    }
    return "aktarım hatası";
}

void reportImportFailure(const app::ProfileImportResult &result) {
    if (result.allowsProfileWrites()) {
        return;
    }
    std::cerr << "Uyarı: eski kayıt aktarılamadı: " << result.source << " -> " << result.destination
              << " (" << importFailureReason(result.status);
    if (result.systemError) {
        std::cerr << ": " << result.systemError.message();
    }
    std::cerr
        << "). Bu oturumda ilgili dosyaya yazma engellendi; yeniden açılışta tekrar denenecek.\n";
}

void playMoveSounds(app::SoundManager &soundManager, const numtilo_core::MoveResult &result) {
    soundManager.play(app::SoundEffect::TileSlide);
    if (result.scoreDelta > 0) {
        soundManager.play(app::SoundEffect::Merge);
    }
    if (result.spawnedTile.has_value()) {
        soundManager.play(app::SoundEffect::Spawn);
    }
}

void playScoreSounds(app::SoundManager &soundManager, const app::ScoreCheckpointResult &result) {
    if (result.firstBestOfRound) {
        soundManager.play(app::SoundEffect::HighScore);
    }
    if (result.roundCompleted) {
        soundManager.play(app::SoundEffect::GameOver);
    }
}

const char *scoreSaveFailureReason(const numtilo_core::ScoreSaveStatus status) {
    switch (status) {
    case numtilo_core::ScoreSaveStatus::WriteBlocked:
        return "dosya güvenle yüklenemediği veya değiştiği için yazma engellendi";
    case numtilo_core::ScoreSaveStatus::SerializationError:
        return "veri JSON biçimine dönüştürülemedi";
    case numtilo_core::ScoreSaveStatus::SourceChanged:
        return "dosya dışarıdan değişti; mevcut dosya korundu";
    case numtilo_core::ScoreSaveStatus::BackupConflict:
        return "eski formatın yedeğiyle çakışma var; mevcut dosyalar korundu";
    case numtilo_core::ScoreSaveStatus::IoError:
        return "dosya yazma işlemi tamamlanamadı";
    case numtilo_core::ScoreSaveStatus::NotSaved:
    case numtilo_core::ScoreSaveStatus::Saved:
        return "kayıt tamamlanamadı";
    }
    return "kayıt tamamlanamadı";
}

bool needsScoreCheckpoint(const SceneCommand command) {
    switch (command) {
    case SceneCommand::StartGame:
    case SceneCommand::RestartGame:
    case SceneCommand::ShowHighScores:
    case SceneCommand::ShowSplash:
        return true;
    case SceneCommand::None:
    case SceneCommand::ToggleSound:
    case SceneCommand::ContinueGame:
        return false;
    }
    return false;
}

void applySceneCommand(const SceneCommand command, SceneId &scene, app::GameSession &session) {
    switch (command) {
    case SceneCommand::None:
        return;
    case SceneCommand::StartGame:
    case SceneCommand::RestartGame:
        session.resetGame();
        scene = SceneId::Playing;
        return;
    case SceneCommand::ShowHighScores:
        scene = SceneId::HighScores;
        return;
    case SceneCommand::ShowSplash:
        scene = SceneId::Splash;
        return;
    case SceneCommand::ToggleSound:
        return;
    case SceneCommand::ContinueGame:
        if (scene == SceneId::Win) {
            scene = SceneId::Playing;
        }
        return;
    }
}

} // namespace

namespace app {

int run(const RunConfig &config) {
    // A test legacy source alone must not select or mutate a real native profile.
    if (config.legacyDataOverride && !config.userDataOverride) {
        std::cerr << "Eski veri dizini belirtildiğinde kullanıcı veri dizini de açıkça "
                     "belirtilmelidir.\n";
        return 1;
    }
    const auto legacyLocation =
        captureLegacyDataLocation(config.userDataOverride, config.legacyDataOverride);
    PathOptions pathOptions;
    pathOptions.userDataOverride = config.userDataOverride;
    pathOptions.assetOverride = config.assetOverride;
    const auto assetResolution = resolvePlatformAssetPaths(pathOptions);
    if (assetResolution.error) {
        reportPathFailure(*assetResolution.error);
        return 1;
    }
    const auto &assetPaths = *assetResolution.paths;
    // Font bytes outlive the Font and every scene: SFML's lazy glyph loading
    // keeps using the buffer supplied to loadFromMemory throughout its lifetime.
    const auto fontData = readAssetFile(assetPaths, kFontAsset);
    if (fontData.error) {
        reportAssetFailure(*fontData.error);
        return 1;
    }
    sf::Font font;
    if (!font.loadFromMemory(fontData.bytes.data(), fontData.bytes.size())) {
        std::cerr << "Font biçimi yüklenemedi: " << (assetPaths.directory() / kFontAsset) << "\n";
        return 1;
    }
    // Validate the required icon before any profile bootstrap or migration. The
    // macOS bundle supplies its native icon; Windows/Linux use these pixels below.
    const auto windowIconData = readAssetFile(assetPaths, kWindowIconAsset);
    if (windowIconData.error) {
        reportAssetFailure(*windowIconData.error);
        return 1;
    }
    sf::Image windowIcon;
    if (!windowIcon.loadFromMemory(windowIconData.bytes.data(), windowIconData.bytes.size())) {
        std::cerr << "Pencere ikonu biçimi yüklenemedi: "
                  << (assetPaths.directory() / kWindowIconAsset) << "\n";
        return 1;
    }
    const auto dataResolution = resolvePlatformUserDataPaths(pathOptions);
    if (dataResolution.error) {
        reportPathFailure(*dataResolution.error);
        return 1;
    }
    const auto &dataPaths = *dataResolution.paths;
    const auto directory = ensureUserDataDirectory(dataPaths);
    if (directory.error) {
        // Keep the selected paths. Ordinary save failures retain their snapshots;
        // unreadable existing profiles stay protected by the individual loaders.
        reportPathFailure(*directory.error);
    }

    const auto migration = migrateLegacyProfile(dataPaths, legacyLocation);
    reportImportFailure(migration.scores);
    reportImportFailure(migration.settings);

    SettingsStore settings(dataPaths.settingsFile(), storageAfterProfileImport(migration.settings));
    if (!settings.load()) {
        std::cerr << "Uyarı: ayar dosyası güvenle yüklenemedi; mevcut dosya korunuyor: "
                  << settings.settingsFilePath() << "\n";
    }
    std::optional<numtilo_core::FileSaveStatus> reportedSettingsFailure;
    const auto checkpointSettings = [&] {
        if (!settings.isDirty() && !settings.needsMigration()) {
            return;
        }
        if (settings.save()) {
            reportedSettingsFailure.reset();
            return;
        }
        const auto status = settings.lastSaveStatus();
        // Preserve the original source-change diagnostic once later writes are blocked.
        if (reportedSettingsFailure == numtilo_core::FileSaveStatus::SourceChanged &&
            status == numtilo_core::FileSaveStatus::WriteBlocked) {
            return;
        }
        if (reportedSettingsFailure != status) {
            std::cerr << "Uyarı: ayar dosyası kaydedilemedi: " << settings.settingsFilePath()
                      << " (" << scoreSaveFailureReason(status) << ")\n";
            reportedSettingsFailure = status;
        }
    };
    checkpointSettings();

    constexpr auto width = ui::theme::sizes::kWindowWidth;
    constexpr auto height = ui::theme::sizes::kWindowHeight;

    sf::ContextSettings windowSettings;
    windowSettings.antialiasingLevel = kWindowAntialiasingLevel;

    sf::RenderWindow window(sf::VideoMode(width, height), "Numtilo",
                            sf::Style::Titlebar | sf::Style::Close, windowSettings);
#if defined(_WIN32) || defined(__linux__)
    const auto iconSize = windowIcon.getSize();
    window.setIcon(iconSize.x, iconSize.y, windowIcon.getPixelsPtr());
#endif
    window.setVerticalSyncEnabled(config.vSyncEnabled);
    if (config.frameLimit.has_value()) {
        window.setFramerateLimit(*config.frameLimit);
    }
    window.requestFocus();

    const auto contentView = ui::createContentView();
    const auto contentSize = contentView.getSize();
    window.setView(contentView);

    app::SoundManager soundManager(assetPaths, settings);
    if (!soundManager.loadSoundAssets()) {
        std::cerr << "Uyarı: bazı ses dosyaları yüklenemedi:\n";
        for (const auto &missing : soundManager.missingFiles()) {
            std::cerr << "  - " << missing.string() << "\n";
        }
    }

    numtilo_core::ScoreManager scoreManager(dataPaths.scoresFile(),
                                            storageAfterProfileImport(migration.scores));
    if (!scoreManager.load()) {
        std::cerr << "Uyarı: skor dosyası yüklenemedi: " << scoreManager.scoreFilePath() << "\n";
    }
    GameSession session;
    ScoreCoordinator scoreCoordinator(scoreManager);
    const auto checkpointScores = [&] {
        const auto result = scoreCoordinator.checkpoint(session);
        if (result.failureToReport.has_value()) {
            std::cerr << "Uyarı: skor dosyası kaydedilemedi: " << scoreManager.scoreFilePath()
                      << " (" << scoreSaveFailureReason(*result.failureToReport) << ")\n";
        }
        return result;
    };
    checkpointScores();

    ui::SplashScene splashScene(font, contentSize.x, contentSize.y);
    ui::HighScoresScene highScoresScene(font, contentSize.x, contentSize.y);
    ui::PlayingScene playingScene(font);
    playingScene.setSoundEnabled(soundManager.isEnabled());
    ui::GameOverScene gameOverScene(font, contentSize.x, contentSize.y);
    ui::WinScene winScene(font, contentSize.x, contentSize.y);
    ui::GameplayOutcome gameplayOutcome;
    ui::SceneInputGuard inputGuard;
    const auto resetSceneInput = [&] {
        splashScene.resetInputState();
        highScoresScene.resetInputState();
        playingScene.resetInputState();
        gameOverScene.resetInputState();
        winScene.resetInputState();
    };

    const auto beginScenePresentation = [&](const SceneId entered, const SceneId previous) {
        switch (entered) {
        case SceneId::Splash:
            splashScene.onEnter();
            break;
        case SceneId::HighScores:
            highScoresScene.onEnter();
            break;
        case SceneId::Playing:
            // The existing panel stays visible when dismissing an outcome modal.
            if (previous == SceneId::Splash) {
                playingScene.onEnter();
            }
            break;
        case SceneId::Win:
            winScene.onEnter();
            break;
        case SceneId::GameOver:
            gameOverScene.onEnter();
            break;
        }
    };

    SceneId scene = SceneId::Splash;
    splashScene.onEnter();
    std::uint64_t accessibilityGeneration = 1;
    const auto updateGameplayScene = [&] {
        if (scene == SceneId::Playing) {
            playingScene.tickVisuals();
        }
        const auto nextScene = gameplayOutcome.nextScene(scene, session.gameId(), session.game(),
                                                         playingScene.hasActiveAnimations());
        if (nextScene != scene) {
            const auto previousScene = scene;
            scene = nextScene;
            ++accessibilityGeneration;
            inputGuard.onSceneChanged();
            resetSceneInput();
            beginScenePresentation(scene, previousScene);
        }
    };

    const auto dispatchAction = [&](const ui::SceneAction &action) {
        if (action.direction.has_value()) {
            const auto result = session.applyMove(*action.direction);
            if (result.moved) {
                const auto scoreResult = checkpointScores();
                playMoveSounds(soundManager, result);
                playScoreSounds(soundManager, scoreResult);
                playingScene.onMove(result);
            }
        }

        const auto command = action.command;
        if (needsScoreCheckpoint(command)) {
            checkpointScores();
            checkpointSettings();
        }
        if (command == SceneCommand::StartGame) {
            // Capture the previous round before replacing its player's metadata.
            session.setPlayerName(splashScene.playerName());
        }
        const auto previousScene = scene;
        applySceneCommand(command, scene, session);
        if (scene != previousScene || command == SceneCommand::StartGame ||
            command == SceneCommand::RestartGame) {
            ++accessibilityGeneration;
            inputGuard.onSceneChanged();
            resetSceneInput();
        }

        if (scene != previousScene) {
            beginScenePresentation(scene, previousScene);
        }

        if (command == SceneCommand::StartGame || command == SceneCommand::RestartGame) {
            playingScene.resetVisualEffects();
        }
        if (command == SceneCommand::ToggleSound) {
            soundManager.toggleEnabled();
            checkpointSettings();
            playingScene.setSoundEnabled(soundManager.isEnabled());
        }
        if (command == SceneCommand::ShowSplash) {
            playingScene.resetVisualEffects();
        }
    };

    ui::AccessibilityBridge accessibility;
#if defined(__APPLE__)
    // The native accessibility adapter is AppKit-only; other platforms use the stub.
    if (accessibility.supported() && !accessibility.attach(window.getSystemHandle())) {
        std::cerr << "Uyarı: erişilebilirlik kontrolleri bağlanamadı.\n";
    }
#endif
    const auto accessibilitySnapshot = [&] {
        ui::AccessibilitySnapshot snapshot;
        snapshot.generation = accessibilityGeneration;
        snapshot.roundId = session.gameId();
        snapshot.clientPixelWidth = window.getSize().x;
        snapshot.clientPixelHeight = window.getSize().y;
        switch (scene) {
        case SceneId::Splash:
            snapshot.sceneName = "Oyuncu Girişi";
            snapshot.nodes = splashScene.accessibilityNodes();
            for (const auto &node : snapshot.nodes)
                if (node.id == "name" && !node.error.empty())
                    snapshot.announcement = node.error;
            break;
        case SceneId::HighScores:
            snapshot.sceneName = "En İyi 5 Skor";
            snapshot.nodes = highScoresScene.accessibilityNodes(scoreManager.topScores());
            break;
        case SceneId::Playing:
            snapshot.sceneName = "Oyun";
            snapshot.nodes = playingScene.accessibilityNodes(
                session.game(), contentSize.x, scoreManager.bestScore(), session.playerName());
            break;
        case SceneId::Win:
            snapshot.sceneName = "Kazandın";
            snapshot.modal = true;
            snapshot.announcement = "2048! Kazandın. Devam Et veya Yeni Oyun.";
            snapshot.nodes = winScene.accessibilityNodes(session.game().getScore());
            break;
        case SceneId::GameOver:
            snapshot.sceneName = "Oyun Bitti";
            snapshot.modal = true;
            snapshot.announcement = "Oyun Bitti. Tekrar Oyna veya Ana Menü.";
            snapshot.nodes = gameOverScene.accessibilityNodes(session.game().getScore(),
                                                              scoreManager.bestScore());
            break;
        }
        const auto viewport = window.getViewport(contentView);
        const auto origin = contentView.getCenter() - contentView.getSize() / 2.f;
        const float scaleX = viewport.width / contentView.getSize().x;
        const float scaleY = viewport.height / contentView.getSize().y;
        for (auto &node : snapshot.nodes) {
            node.frame.x = viewport.left + (node.frame.x - origin.x) * scaleX;
            node.frame.y = viewport.top + (node.frame.y - origin.y) * scaleY;
            node.frame.width *= scaleX;
            node.frame.height *= scaleY;
        }
        return snapshot;
    };
    const auto focusAccessible = [&](const std::string_view id) {
        switch (scene) {
        case SceneId::Splash:
            return splashScene.focusAccessible(id);
        case SceneId::HighScores:
            return highScoresScene.focusAccessible(id);
        case SceneId::Playing:
            return playingScene.focusAccessible(id);
        case SceneId::Win:
            return winScene.focusAccessible(id);
        case SceneId::GameOver:
            return gameOverScene.focusAccessible(id);
        }
        return false;
    };
    const auto dispatchAccessibleAction = [&](ui::SceneAction action) {
        if (scene == SceneId::Playing && action.command != SceneCommand::RestartGame &&
            action.command != SceneCommand::ShowSplash) {
            updateGameplayScene();
            if (scene != SceneId::Playing)
                action = {};
        }
        dispatchAction(action);
    };
    const auto routeAccessible = [&](const ui::AccessibilityAction &request) {
        updateGameplayScene();
        const auto current = accessibilitySnapshot();
        if (!ui::acceptsAccessibilityAction(current, request))
            return;
        if (request.kind == ui::AccessibilityActionKind::Focus) {
            focusAccessible(request.targetId);
            return;
        }
        if (request.kind == ui::AccessibilityActionKind::SetValue) {
            if (scene == SceneId::Splash && request.targetId == "name")
                splashScene.setAccessiblePlayerName(request.value);
            else if (scene == SceneId::Playing && request.targetId == "sound" &&
                     (request.value == "true" || request.value == "false")) {
                const bool desired = request.value == "true";
                if (desired != soundManager.isEnabled())
                    dispatchAccessibleAction(playingScene.activateAccessible("sound"));
            }
            return;
        }
        ui::SceneAction action;
        if (request.kind == ui::AccessibilityActionKind::Press) {
            switch (scene) {
            case SceneId::Splash:
                action.command = splashScene.activateAccessible(request.targetId);
                break;
            case SceneId::HighScores:
                action.command = highScoresScene.activateAccessible(request.targetId);
                break;
            case SceneId::Playing:
                action = playingScene.activateAccessible(request.targetId);
                break;
            case SceneId::Win:
                action.command = winScene.activateAccessible(request.targetId);
                break;
            case SceneId::GameOver:
                action.command = gameOverScene.activateAccessible(request.targetId);
                break;
            }
        } else if (scene == SceneId::Playing && request.targetId == "board") {
            switch (request.kind) {
            case ui::AccessibilityActionKind::MoveLeft:
                action = playingScene.activateAccessible("left");
                break;
            case ui::AccessibilityActionKind::MoveRight:
                action = playingScene.activateAccessible("right");
                break;
            case ui::AccessibilityActionKind::MoveUp:
                action = playingScene.activateAccessible("up");
                break;
            case ui::AccessibilityActionKind::MoveDown:
                action = playingScene.activateAccessible("down");
                break;
            default:
                break;
            }
        }
        dispatchAccessibleAction(action);
    };

    while (window.isOpen()) {
        for (const auto &request : accessibility.takeActions())
            routeAccessible(request);
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                checkpointScores();
                checkpointSettings();
                accessibility.detach();
                window.close();
                break;
            }

            // Resolve settled outcomes before routing the next queued input. A key
            // arriving at the animation boundary must not move the board past its win.
            updateGameplayScene();
            if (!inputGuard.accepts(event)) {
                continue;
            }
            if (event.type == sf::Event::LostFocus) {
                resetSceneInput();
            }

            if ((event.type == sf::Event::MouseButtonPressed ||
                 event.type == sf::Event::MouseButtonReleased) &&
                !ui::containsViewPixel(window, {event.mouseButton.x, event.mouseButton.y}) &&
                (scene != SceneId::GameOver || event.mouseButton.button != sf::Mouse::Left)) {
                // Game Over dims the full client surface. Its primary background
                // gestures must reach the modal to cancel pending activation.
                continue;
            }

            ui::SceneAction action;
            switch (scene) {
            case SceneId::Splash:
                action.command = splashScene.handleEvent(event, window);
                break;
            case SceneId::HighScores:
                action.command = highScoresScene.handleEvent(event, window);
                break;
            case SceneId::Playing:
                action = playingScene.handleEvent(event, window, contentSize.x);
                break;
            case SceneId::Win:
                action.command = winScene.handleEvent(event, window);
                break;
            case SceneId::GameOver:
                action.command = gameOverScene.handleEvent(event, window);
                break;
            }

            if (scene == SceneId::Playing && action.command != SceneCommand::RestartGame &&
                action.command != SceneCommand::ShowSplash) {
                // The handler also advances animation clocks. Recheck if it crossed
                // the boundary before applying a gameplay action. Navigation already
                // accepted while Playing owns the transition and is applied below.
                updateGameplayScene();
                if (scene != SceneId::Playing) {
                    action = {};
                }
            }

            dispatchAction(action);
        }

        if (!window.isOpen()) {
            break;
        }

        const auto mousePixel = sf::Mouse::getPosition(window);
        const sf::Vector2f mousePos = ui::containsViewPixel(window, mousePixel)
                                          ? window.mapPixelToCoords(mousePixel)
                                          : sf::Vector2f{-contentSize.x, -contentSize.y};
        if (scene == SceneId::Splash) {
            splashScene.updateHover(mousePos);
        } else if (scene == SceneId::HighScores) {
            highScoresScene.updateHover(mousePos);
        } else if (scene == SceneId::Playing) {
            playingScene.updateHover(mousePos, contentSize.x);
            playingScene.tickVisuals();
        } else if (scene == SceneId::GameOver) {
            gameOverScene.updateHover(mousePos);
        } else if (scene == SceneId::Win) {
            winScene.updateHover(mousePos);
        }

        updateGameplayScene();

        window.clear(ui::theme::colors::kBackground);

        if (scene == SceneId::Splash) {
            splashScene.render(window);
            accessibility.publish(accessibilitySnapshot());
            window.display();
            continue;
        }

        if (scene == SceneId::HighScores) {
            highScoresScene.render(window, scoreManager.topScores());
            accessibility.publish(accessibilitySnapshot());
            window.display();
            continue;
        }

        playingScene.render(window, font, session.game(), contentSize.x, scoreManager.bestScore(),
                            session.playerName());
        if (scene == SceneId::GameOver) {
            gameOverScene.render(window, session.game().getScore(), scoreManager.bestScore());
        } else if (scene == SceneId::Win) {
            winScene.render(window, session.game().getScore());
        }

        accessibility.publish(accessibilitySnapshot());
        window.display();
    }

    return 0;
}

} // namespace app
