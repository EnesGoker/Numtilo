#include "app/SoundManager.hpp"
#include "app/AssetResolver.hpp"
#include "app/AssetCatalog.hpp"
#include "app/SettingsStore.hpp"

#include <utility>

namespace app {

SoundManager::SoundManager(AssetPaths paths, SettingsStore &settings)
    : paths_(std::move(paths)), settings_(settings) {
}

bool SoundManager::loadSoundAssets() {
    missingFiles_.clear();

    for (const SoundEffect effect : {SoundEffect::TileSlide, SoundEffect::Merge, SoundEffect::Spawn,
                                     SoundEffect::GameOver, SoundEffect::HighScore}) {
        auto &audio = audioForEffect(effect);
        audio.loaded = false;

        const auto relative = std::filesystem::path(effectAssetPath(effect));
        const auto data = readAssetFile(paths_, relative);
        if (data.error || !audio.buffer.loadFromMemory(data.bytes.data(), data.bytes.size())) {
            missingFiles_.push_back(paths_.directory() / relative);
            continue;
        }

        audio.sound.setBuffer(audio.buffer);
        audio.sound.setVolume(effectVolume(effect));
        audio.loaded = true;
    }

    return missingFiles_.empty();
}

void SoundManager::play(const SoundEffect effect) {
    if (!settings_.soundEnabled()) {
        return;
    }

    auto &audio = audioForEffect(effect);
    if (!audio.loaded) {
        return;
    }

    audio.sound.stop();
    audio.sound.play();
}

bool SoundManager::isEnabled() const noexcept {
    return settings_.soundEnabled();
}

void SoundManager::setEnabled(const bool enabled) noexcept {
    settings_.setSoundEnabled(enabled);
}

void SoundManager::toggleEnabled() noexcept {
    settings_.setSoundEnabled(!settings_.soundEnabled());
}

const std::vector<std::filesystem::path> &SoundManager::missingFiles() const noexcept {
    return missingFiles_;
}

const char *SoundManager::effectAssetPath(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return kSoundAssets[0];
    case SoundEffect::Merge:
        return kSoundAssets[1];
    case SoundEffect::Spawn:
        return kSoundAssets[2];
    case SoundEffect::GameOver:
        return kSoundAssets[3];
    case SoundEffect::HighScore:
        return kSoundAssets[4];
    }

    return "sounds/unknown.wav";
}

float SoundManager::effectVolume(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return 35.f;
    case SoundEffect::Merge:
        return 50.f;
    case SoundEffect::Spawn:
        return 38.f;
    case SoundEffect::GameOver:
        return 58.f;
    case SoundEffect::HighScore:
        return 62.f;
    }

    return 45.f;
}

SoundManager::EffectAudio &SoundManager::audioForEffect(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return tileSlide_;
    case SoundEffect::Merge:
        return merge_;
    case SoundEffect::Spawn:
        return spawn_;
    case SoundEffect::GameOver:
        return gameOver_;
    case SoundEffect::HighScore:
        return highScore_;
    }

    return tileSlide_;
}

const SoundManager::EffectAudio &SoundManager::audioForEffect(const SoundEffect effect) const {
    switch (effect) {
    case SoundEffect::TileSlide:
        return tileSlide_;
    case SoundEffect::Merge:
        return merge_;
    case SoundEffect::Spawn:
        return spawn_;
    case SoundEffect::GameOver:
        return gameOver_;
    case SoundEffect::HighScore:
        return highScore_;
    }

    return tileSlide_;
}

} // namespace app
