[English](README.md) | [Türkçe](README.tr.md)

<p align="center">
  <img src="artwork/numtilo-logo-original.png" alt="Numtilo logo" width="128" />
</p>

# Numtilo

Numtilo is a desktop 2048 puzzle game with a Turkish interface, built with C++20
and SFML. Slide matching tiles together, build your score and reach 2048.

<p align="center">
  <img src="docs/images/numtilo-game.png" alt="Real Numtilo gameplay showing a 4×4 board, player Demo and score 140" width="720" />
</p>

Current release: **[v0.2.2](https://github.com/EnesGoker/Numtilo/releases/tag/v0.2.2)**.

## Download and play

**You do not need to build from source to play.** Download the ZIP for your platform
from the [v0.2.2 release](https://github.com/EnesGoker/Numtilo/releases/tag/v0.2.2).
These are ZIP archives, not installers. Extract the entire archive before opening
the application; keep the supplied assets and libraries in their extracted locations.

| Platform | Download | Run after extracting |
| --- | --- | --- |
| macOS ARM64 (Apple Silicon) | [macOS ZIP](https://github.com/EnesGoker/Numtilo/releases/download/v0.2.2/Numtilo-0.2.2-darwin-arm64.zip) | Open `Numtilo.app` in Finder. |
| Windows x64 | [Windows ZIP](https://github.com/EnesGoker/Numtilo/releases/download/v0.2.2/Numtilo-windows-latest.zip) | Open `Numtilo.exe`; a console window also opens. |
| Linux x86_64 | [Linux ZIP](https://github.com/EnesGoker/Numtilo/releases/download/v0.2.2/Numtilo-0.2.2-linux-x86_64.zip) | Run `./numtilo` from the extracted folder; if needed, first use `chmod +x numtilo`. |

The macOS package is not Developer ID signed or notarized; macOS security checks
may block a downloaded copy. Manual virtual-machine results and automated checks
do not establish compatibility with every host or distribution.

## Features

- A 4×4 board, merge scoring and continued play after reaching 2048.
- Player names, a live best score and completed-round history, with a top-five leaderboard.
- Tile animations and five sound effects, with a saved sound on/off preference.
- Local score/settings profiles and guarded imports from the old `sfml-2048` profile.

The active board is not saved: reopening the application starts a new game.
See [architecture](docs/architecture.md) for persistence and migration details.

<details>
<summary>Main menu and completed-round scores</summary>

<p align="center">
  <img src="docs/images/numtilo-menu.png" alt="Numtilo main menu with the player name field, Start Game and Scores buttons" width="600" />
</p>

The leaderboard below shows a completed game from an isolated Demo profile.

<p align="center">
  <img src="docs/images/numtilo-scores.png" alt="Numtilo top-five leaderboard showing Demo with 684 points from a completed game in an isolated profile" width="600" />
</p>

</details>

## Controls

| Context | Controls |
| --- | --- |
| Player name | Type or Backspace; Enter validates and starts |
| Playing | Arrow keys move; N starts a new round; Esc returns to the menu |
| Control focus | Tab / Shift+Tab navigate; Enter / Space activates a focused action on release |
| Win panel | Esc continues; Tab selects an action for Enter / Space |
| Game Over | N or unfocused Enter replays; Esc returns to the menu |
| Leaderboard | Esc / Q / Backspace returns to the menu |

During play, Enter starts a new round when no control or accessible board has focus.
Mouse clicks operate controls; the window close control exits. WASD is not bound.

## Build and run from source (optional)

For the vcpkg route below, use **Git**, **CMake 3.24+**, a **C++20 compiler** and a
[vcpkg](https://github.com/microsoft/vcpkg) checkout outside this source tree.
Use Xcode Command Line Tools on macOS or Visual
Studio 2022 with C++ tools and Developer PowerShell on Windows. Linux also needs
graphics/audio development packages; [CI's Ubuntu setup](.github/workflows/ci.yml)
provides an example.

Run from the project root. Bootstrap vcpkg once, set `VCPKG_ROOT` to its absolute
path, and keep it set for configure, build and test. Initial dependency acquisition
may require network access. These commands assume fresh Numtilo build trees;
preserve pre-rename caches and evidence.

### macOS / Linux

```bash
export VCPKG_ROOT="/absolute/path/to/vcpkg"
"$VCPKG_ROOT/bootstrap-vcpkg.sh"
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
```

Run on macOS:

```bash
open build/vcpkg-debug/Numtilo.app
```

Run on Linux:

```bash
./build/vcpkg-debug/numtilo
```

### Windows

```powershell
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat"
cmake --preset vcpkg-debug -G "Visual Studio 17 2022" -A x64
cmake --build --preset build-vcpkg-debug --config Debug
.\build\vcpkg-debug\Debug\Numtilo.exe
```

The [build contract](docs/build-contract.md) covers system dependencies, Release
presets and `NUMTILO_*` options. The vcpkg manifest pins SFML to 2.6.2.

For CLI options, run the executable with `--help`.
`--check-runtime` reads the packaged font, icon PNG and five WAV files without
opening a window or loading scores/settings. It does not test decoding, rendering,
audio playback or save permissions.

## Platform status

- **macOS ARM64:** the user reports manual acceptance of the v0.2.2 ZIP in a virtual
  machine. Developer ID signing and notarization are not provided.
- **Windows x64:** the user reports manual acceptance of the v0.2.2 ZIP in a virtual
  machine. A console window opens alongside the game.
- **Linux x86_64:** the user reports manual acceptance of the v0.2.2 ZIP in a virtual
  machine.

The macOS VoiceOver adapter is implemented; [historical accessibility evidence](docs/ui-quality-audit.md)
does not establish a complete post-rename GUI/VoiceOver pass. Windows/Linux screen
reader adapters are not implemented. GitHub CI and Release workflows completed
successfully for v0.2.1. The manual results above were reported by the user.

## Tests

Headless Catch2/CTest unit tests do not require the desktop application:

```bash
cmake --preset vcpkg-headless-debug
cmake --build --preset build-vcpkg-headless-debug
ctest --preset test-vcpkg-headless-debug
```

For the desktop Debug build, including platform acceptance when Python 3 is available:

```bash
ctest --test-dir build/vcpkg-debug -C Debug --output-on-failure
```

See [testing](docs/testing.md) for system-dependency commands, Release configurations
and opt-in graphical acceptance.

## Documentation

- [Architecture](docs/architecture.md) · [Build contract](docs/build-contract.md)
- [Testing](docs/testing.md) · [Platform acceptance](docs/platform-acceptance.md)
- [Score persistence acceptance](docs/score-persistence-acceptance.md)
- [Maintenance](docs/maintenance.md) · [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)

## License

Project code: [MIT](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md)
and the bundled [Inter font license](assets/fonts/LICENSE-Inter.txt).
The five sound effects are selected from Kenney's Interface Sounds 1.0 package,
under CC0 1.0. Source and license details are in [third-party notices](THIRD_PARTY_NOTICES.md).
