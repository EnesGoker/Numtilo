#pragma once

#include "app/AppPaths.hpp"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include <filesystem>
#include <vector>

namespace app {

class SettingsStore;

enum class SoundEffect { TileSlide, Merge, Spawn, GameOver, HighScore };

class SoundManager {
  public:
    // SettingsStore outlives this manager; preferences and disk I/O belong to the store.
    SoundManager(AssetPaths paths, SettingsStore &settings);

    bool loadSoundAssets();
    void play(SoundEffect effect);

    bool isEnabled() const noexcept;
    void setEnabled(bool enabled) noexcept;
    void toggleEnabled() noexcept;

    const std::vector<std::filesystem::path> &missingFiles() const noexcept;

  private:
    struct EffectAudio {
        sf::SoundBuffer buffer;
        sf::Sound sound;
        bool loaded{false};
    };

    static const char *effectAssetPath(SoundEffect effect);
    static float effectVolume(SoundEffect effect);

    EffectAudio &audioForEffect(SoundEffect effect);
    const EffectAudio &audioForEffect(SoundEffect effect) const;

    AssetPaths paths_;
    SettingsStore &settings_;
    std::vector<std::filesystem::path> missingFiles_;

    EffectAudio tileSlide_;
    EffectAudio merge_;
    EffectAudio spawn_;
    EffectAudio gameOver_;
    EffectAudio highScore_;
};

} // namespace app
