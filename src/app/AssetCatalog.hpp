#pragma once

#include <array>

namespace app {

inline constexpr char kFontAsset[] = "fonts/Inter-Variable.ttf";
inline constexpr char kWindowIconAsset[] = "icons/numtilo.png";
inline constexpr std::array<const char *, 5> kSoundAssets{
    "sounds/slide.wav", "sounds/merge.wav", "sounds/spawn.wav", "sounds/game_over.wav",
    "sounds/high_score.wav"};

} // namespace app
