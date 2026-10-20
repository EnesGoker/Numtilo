# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

No unreleased changes are recorded here yet.

## [0.2.2] - 2026-10-21

Maintenance release for runtime packaging and documentation; no gameplay or UI changes.

### Fixed
- Exclude Windows system and API-set DLLs from runtime packages while preserving
  SFML, OpenAL, MSVC redistributables and other required third-party libraries.
- Exclude Linux glibc and ELF loader components without broadly removing required
  third-party libraries from system library directories.
- Resolve library symlinks and parent junctions before checking package containment.
- Accept conflicting Linux package/system library copies only when every copy has
  the same SHA-256 content and the packaged companion remains inside the package.

### Changed
- Add focused packaging regression controls to CTest and the existing CI/Release
  checks for installed runtimes and extracted packages. Failed dependency checks
  now expose diagnostics, with CI reports and command logs retained as artifacts.
- Refresh contributing links, security reporting instructions and the bug-report
  version example. Update bilingual downloads and runtime notes for 0.2.2.

### Validation
- The user reports manual gameplay acceptance of the macOS ARM64, Windows x64 and
  Linux x86_64 ZIPs in virtual machines. This is user-reported acceptance, separate
  from automated package checks.
- Existing limitations remain: Windows also opens a console window; macOS packages
  are not Developer ID signed or notarized.

## [0.2.1] - 2026-10-20

### Release
- Published [Numtilo v0.2.1](https://github.com/EnesGoker/Numtilo/releases/tag/v0.2.1)
  on GitHub with macOS ARM64, Windows x64 and Linux x86_64 ZIP packages, not installers.
- This release includes the Numtilo name and technical identity, approved logo and
  platform icons, Turkish interface and UI improvements, and Kenney CC0 sound effects.
- GitHub CI and Release workflows completed successfully. macOS gameplay was manually
  accepted; the Windows Release ZIP's screens, controls and sounds were verified by
  the user in a Windows virtual machine. Linux automated tests passed; real-desktop
  manual acceptance remains pending.
- Known limitations: Windows opens a console window alongside the game; the macOS
  package is not Developer ID signed or notarized.

The detailed development record below is included in this release. Historical names,
intermediate states and checkpoint verification limits are retained as recorded;
the publication date does not imply a new full regression run.

### Added
- Opt-in maintained graphical acceptance with system/vcpkg Release presets, current
  SFML scenes, the production font and validated 1×/2× JSON/PNG evidence.
- macOS native scene/control semantics for editable names, full values, focus, Sound
  state, ranking/board cells, exclusive modal actions and board moves, using bounded
  tagged requests routed through existing App commands. Actual local VoiceOver Name,
  board, Sound, ranking and outcome operation passes; other-platform providers and
  broader IME/shaping remain separate work.
- Portable bridge queue/lifecycle regression cases independent of AppKit and a display.
- Game Over redesign with full-window dimming, paired final/best score cards and wide,
  stacked Replay/Main Menu buttons with shared vector icons and keyboard focus styling.
- Separate headless Debug/Release CI jobs for Linux, macOS and Windows, with guarded CMake
  configuration checks, vcpkg installed-port checks and retained failure evidence. Linux/macOS
  Release jobs also run fresh test-enabled and backend-only dependency audits.
- Portable headless configuration checker with cache, dependency-provider, imported-target
  closure, installed-port and generated-desktop-output validation, plus native regression controls.
- Six headless check contexts in both local branch-protection profiles; existing check names
  are preserved. Applying the profiles to GitHub remains a separate operation.
- Final native macOS phase 6 regression evidence for all four application/test combinations
  in Debug and Release with CMake 3.29.6, independent headless dependency audits, preserved
  option defaults and the full unit suite, installed/ZIP-extracted runtime acceptance, headless
  component-install boundaries and an isolated graphical application-integration smoke fixture.
  Hosted Linux/Windows, sanitizer/coverage execution and distribution signing/clean-machine
  acceptance remain separate verification.
- Independent `SFML_2048_BUILD_APP` desktop build option, defaulting to `ON`, with headless
  backend/unit-test configurations using the standard `BUILD_TESTING` option independently.
- Shared SFML-independent `app_logic` static target for session and score-coordination services.
- vcpkg `desktop` and `tests` manifest features, with common JSON and Catch2 pinned to `3.7.1`.
- System/vcpkg headless Debug/Release configure, build and unit-test presets with separate build
  directories, explicit configurations, failure output and failure on an empty unit selection.
- Ignore rules for optional machine-specific `CMakeUserPresets.json` overrides and Python
  `__pycache__/` acceptance-test artifacts.
- Repeatable fresh-build headless acceptance with an opt-in CMake dependency guard, generated
  target/imported-property audits, real compiler header scans, backend archive symbol checks,
  unit execution and native runtime inspection, retaining staged JSON and command evidence.
- Native integration controls for forbidden discovery/fallback, hidden imported dependencies,
  inline/system/symlink headers, missing evidence and cross-toolchain rejection.
- Repository baseline metadata for enterprise-style project hygiene.
- CMake-based build configuration (`CMakeLists.txt`) with C++20 and warning flags.
- CMake preset workflow (`CMakePresets.json`) for Debug/Release and vcpkg builds.
- `vcpkg.json` manifest for automatic SFML dependency resolution.
- `src/core` + `src/app` architecture split with `game_core` library target.
- New SFML-independent core API: `reset(seed)`, `applyMove(Direction)`, `getGrid()`, `getScore()`, `isGameOver()`.
- Catch2-based `core_unit_tests` suite with merge, no-op, game-over, score, and golden snapshot coverage.
- Four-direction movement regression matrix with explicit grids/scores, exact animation records,
  next-turn merge eligibility, and deterministic full-board invariants and symmetry checks.
- Normal spawn/non-overwrite invariants, exhaustive neighbor and empty-cell boundaries,
  post-spawn game-over checks, and deterministic reset/no-op RNG regression tests.
- Independent, latched `won` and derived `gameOver` round flags, a `hasWon()` query,
  and win/continued-play/load/reset regression tests.
- Independent best score, full completed-round history, derived top-five snapshots, in-memory
  round-ID deduplication and dirty tracking, with score/session lifecycle regression tests.
- Version-2 score profiles with independent best and persisted round IDs, transactional validation,
  explicit load outcomes, duplicate-key/ID protection, and in-memory legacy migration preparation.
- Native atomic score storage, byte-exact legacy backups, source/backup conflict detection,
  explicit save outcomes, and failure-injection tests for retry without duplicate history.
- SFML-independent application score coordinator with combined profile checkpoints, live best
  persistence, one-time round acknowledgement/notifications, and controlled failure retries.
- Native lifecycle acceptance tests for real win/continued-play/completion, independently durable
  abandoned best, and full completed-history/ranking/identity preservation across reopen.
- Score persistence acceptance report with native macOS animation-boundary, migration,
  protected-source, failed-write/recovery and queued-close event checks.
- SFML-independent `AppPaths` contract separating user data and assets, injected context/overrides,
  deterministic root selection, typed path failures and temporary-directory regression tests.
- Native per-user profile paths for macOS, Windows and Linux, asset-independent resolution,
  explicit directory bootstrap and isolated discovery/Unicode/profile integration tests.
- SFML-independent settings store with strict v1 validation, protected unknown/corrupt sources,
  exact legacy-format backup, atomic publication and dirty snapshot retry tests.
- Independent score/settings location migration from one startup working-directory snapshot,
  strict shared schema validation, exact native byte snapshots, observed source-change guards,
  atomic no-overwrite publication and per-file memory-only protection after unresolved imports.
- Location-migration tests covering target precedence, unchanged legacy sources, destination
  format backups, independent profile failures, retry on a later startup and publication races.
- Asset-only path snapshots, native executable/bundle resource discovery and one-root typed
  asset resolution, with isolated precedence, validation and binary-read regression tests.
- Read-only `--check-runtime` JSON diagnostics using the same asset catalog as font/sound loading,
  with isolated resource-error, provider-failure and profile-preservation regression tests.
- Repeatable native platform acceptance for foreign launch directories, Unicode runtime/profile
  paths, missing-font fallback protection and separate-process persistence/recovery. CI checks
  built and installed runtimes; release jobs also verify the extracted archive before upload.
- GitHub Actions CI pipeline with multi-OS matrix, format check, and sanitizer job.
- `scripts/check_format.sh` for local/CI clang-format enforcement.
- CMake install rules for runtime binary, assets, metadata files, and runtime dependencies.
- CPack ZIP packaging configuration for distributable release artifacts.
- Tag-driven release workflow (`.github/workflows/release.yml`) to build/package on Ubuntu, macOS, and Windows and publish GitHub Releases.
- vcpkg manifest version pinning (`builtin-baseline` + `overrides`) to keep SFML on `2.6.2` across platforms.
- Executable-based asset path resolver with fallback candidates for robust font loading across run locations.
- Game over overlay now provides clear `NEW GAME` and `QUIT` actions (mouse + keyboard shortcuts).
- Engineering documentation set: `docs/architecture.md` and `docs/testing.md`.
- Contribution and governance docs: `CONTRIBUTING.md` and `SECURITY.md`.
- Source-only archive script (`scripts/package_source.sh`) using metadata-clean ZIP output.
- CI/release caching improvements for vcpkg archives and compiler cache (`ccache` on Linux/macOS).
- Branch protection bootstrap script (`scripts/apply_branch_protection.sh`) for required checks and review gate.
- PR template and structured issue forms for bug/feature intake.
- Dependabot configuration for weekly GitHub Actions dependency updates.
- Release artifacts now include `SHA256SUMS.txt` and SPDX SBOM files.
- Property-style core tests for invariant preservation and left/right mirror symmetry.
- Repository ownership and compliance inventory files: `.github/CODEOWNERS` and `THIRD_PARTY_NOTICES.md`.
- Sound effects module (`SoundManager`) with persisted on/off preference (`settings.json`).

### Changed
- Replaced recursive source-package copying with a shared, fail-closed Git-tracked
  working-tree selector for Shell and CPack Source, guarded staging, Shell no-overwrite
  ZIP publication and fail-closed CPack verification. Source distribution supports
  ZIP only, rejecting non-ZIP CPack source overrides. Shell and CPack verify final ZIP members, hashes and executable
  modes against a trusted staging snapshot before publication. A source-only copy gate
  also blocks CPack 4.0.2's final copy when a post-build error alone would not stop it.
  Binary CPack and Runtime installation remain separate; validation uses synthetic fixtures.
- Transitioned the product to Numtilo: `numtilo` CMake project/targets and vcpkg manifest,
  `NUMTILO_*` options, `numtilo_core`/`numtilo_edge` namespaces and `NumtiloAX*` symbols.
  Platform outputs are `Numtilo.app/Contents/MacOS/Numtilo`, `Numtilo.exe` and Linux
  `numtilo`; the macOS bundle identifier is `io.github.enesgoker.numtilo`.
- Selected the `numtilo` user profile with independent, no-overwrite imports from the old
  native `sfml-2048` profile, falling back to launch-folder JSON only for missing native
  sources. Updated test identities and the exact `.numtilo-acceptance` marker protocol.
- Aligned current documentation with these names while retaining historical evidence.
  Updated CPack, release ZIP and source archive names to `Numtilo`. The approved logo
  and application icons are integrated in the published Numtilo v0.2.1 release.
- macOS semantic focus refreshes independently of unchanged-snapshot deduplication,
  outside the request-queue mutex and only from known native responders. A repeated
  real-click/VoiceOver regression verifies the Name field keeps its accessible focus;
  live table cells expose stable read-only text values for VoiceOver.
- Headless acceptance resolves nested CMake object outputs from actual compiler
  commands and permits exactly two existing standard/core UI contracts while
  retaining full forbidden-dependency scans and native regression controls.
- Playing/control documentation now matches current focus/release activation,
  Win Continue and Game Over Replay/Main Menu behavior.
- Shared action buttons and the Sound row retain their exact normal click/hover bounds
  when keyboard focus thickens their outline; existing padding and focus drawing remain.
  The four modal buttons now use the same shared geometry rule.
- GameOver background presses/releases now cancel pending activation throughout its
  full client backdrop, including outer margins, with exclusive modal routing.
- Outcome buttons keep stable padded hit bounds through focus changes; GameOver score
  cards retain their normal position when Replay gains focus. Complete modal scores
  apply the declared readability floors without altering default geometry or motion.
- Current outcome rendering, entry negative controls, isolated round/history/settings
  persistence and all four native modal pointer actions passed scoped acceptance.
- Playing navigation accepted during a move now preserves Restart/Main Menu intent
  across outcome boundaries, with App-owned checkpoints and effect resets. Settled
  outcomes retain priority before queued input.
- Tile 8/16/32 and held score-increase text meet the selected contrast/readability
  rules; wider text reservations retain full long numbers within existing geometry.
  Playing explains unavailable legacy-name glyphs without changing stored identity.
- Adjacent simultaneous merge pulses retain a visible gap; isolated pulse, easing,
  durations and round state remain established. Current 1×/2× rendering, legal outcome
  routes, isolated persistence and native Playing controls passed scoped acceptance.
- Menu names now validate supported visible glyphs and full-name readability, with
  readable invalid-edit previews, distinct error/focus styling and 18-character feedback.
  Existing leaderboard names use an explained display projection while stored identities
  remain intact. Menu copy is Turkish and decorative digits share the board text palette.
- Shared UI controls now activate only after a matching primary-pointer press/release;
  dragging off, leaving the window, losing focus or crossing scenes cancels ownership.
- Dedicated score-card, leaderboard, Win-score and score-feedback text roles with
  bounded fitting/readability reports. Wider numeric reservations retain full long
  scores at a readable size within existing card geometry.
- Readable green score-increase feedback with held opacity, eased upward travel and
  fade, centered long values and one latest-move label during rapid merges; combined
  local macOS animation/interaction acceptance completed, including all nine native
  pointer button routes and preserved outcome/history behavior.
- Smooth 180 ms panel/modal entry across menu, leaderboard, playing sidebar, Win and
  Game Over, with grouped opacity, preserved full-window Game Over dimming and immediate
  controls; existing panels stay visible when continuing or restarting a round.
- Shared button hover easing and subtle held-state scaling across all five scenes;
  focused menu/leaderboard Enter and Space actions now activate once on release.
- Tile slide, merge and spawn transitions ease their progress at both endpoints while
  preserving existing durations, scale limits and gameplay/input timing.
- Headless installed-port audits reject the actual OpenAL vcpkg port name `openal-soft`, with
  a real CMake negative control proving failure before building.
- Linux sanitizer and coverage CI jobs now configure the headless Debug preset in dedicated
  build directories, validate dependency-provider evidence, install only their analysis/build
  tools and fail on an empty unit selection. The core coverage threshold remains 80%.
- CMake gates desktop dependencies, executable/assets, Runtime installation and CPack on the
  application option. Unit tests remain available without SFML; platform acceptance and Python
  discovery require both application and tests.
- Preset schema 5 matches the declared CMake 3.24 minimum. Existing desktop preset names/paths
  are preserved, desktop options are explicit, and vcpkg configure/build/test phases require
  a nonempty `VCPKG_ROOT`; generator/compiler/triplet selection remains host-configurable.
- Application/test options are declared before `project()` and derive managed vcpkg manifest
  features before toolchain installation. CMake suppresses manifest defaults, recomputes known
  feature overrides and rejects unknown features, raw feature overrides of any option-name
  casing and installer response files.
  Unrelated direct installer arguments, the existing baseline and SFML `2.6.2` pin are retained.
- `game_core` is explicitly static; the application and unit suite link `app_logic` instead of
  compiling its sources twice, with warnings/sanitizer/coverage instrumentation retained.
- CTest separates unit and native platform labels. Sanitizer/coverage jobs build and run the unit
  target explicitly; full native jobs include executable and isolated persistence acceptance.
- MSVC compiles C++ sources with `/utf-8` independently of the warnings option, preserving
  Unicode literals and test fixture paths across Windows code pages.
- Fonts and sounds use native-path binary reads and SFML memory decoders from one selected
  assets root; runtime resource loading no longer searches the CWD, source tree or exe parents.
- macOS builds create an application bundle with `Contents/Resources/assets` by default.
  Runtime assets refresh on every application build, and installation/archive layouts match
  the resolver on macOS, Windows and Linux.
- Required font decoding finishes before profile bootstrap and window creation. Installed
  macOS bundles embed non-system dependencies, fix loader paths and restore local ad-hoc
  signature validity after fixup; distribution signing/notarization remain separate.
- Application score/settings paths now use one per-user profile instead of the working directory.
  Only missing profile files import supported old-location data; existing targets remain
  authoritative even when corrupt or from a future version. Original source files are preserved.
- Score and settings loaders share their strict byte validators with location migration. Literal
  NUL bytes are rejected without excluding escaped JSON string metadata; native snapshot reads
  reject symlinks and non-regular files and safely classify obstructed missing paths.
- SoundManager consumes preferences from SettingsStore; settings checkpoints handle toggle,
  lifecycle and close retries. Score and settings share model-independent FileStorage while
  retaining the ScoreStorage API and separate profile models.
- Static analysis selects active translation units from CMake's compile database, avoiding
  non-host platform adapters; format checks now include Objective-C++ sources.
- `GameSession` now owns an opaque round identity independent of gameplay RNG; the app submits
  that identity immediately at logical game-over, independently of animation completion.
- The app saves new best scores during play and retries pending profile writes before restart,
  menu transitions and close/quit. Displays read current best directly; idle/no-op input performs
  no score I/O. Repeated persistence errors and score sounds are deduplicated, and close/quit
  stops queued event processing after its final checkpoint.
- Score saving now serializes v2 before opening the destination, preserving best/history/IDs.
  Unique exclusive temporary files are synced and closed before publication. Legacy migration
  preserves the original backup before replacing the source; failed saves retain dirty/migration
  state. Unsupported/corrupt loads still block writes without discarding memory.
- Round status no longer uses a mutually exclusive enum; won games accept further moves,
  and loaded boards derive fresh achievement/terminal flags with a target of at least `2048`.
- Normal spawn records are returned only after successful board placement; randomized core test
  fixtures now use valid 2048 values starting at `2` instead of admitting `1`.
- Standardized runtime font asset path to `assets/fonts/Geneva.ttf`.
- README build instructions migrated to CMake + vcpkg flow.
- Documented `pkg-config` prerequisite for macOS vcpkg bootstrap flow.
- App layer now consumes `MoveResult` from core instead of mutating game state directly.
- Test harness migrated from ad-hoc binaries to a single Catch2 unit test executable.
- CMake now supports opt-in ASan/UBSan via `SFML_2048_ENABLE_SANITIZERS`.
- CMake project version now reads from the repository `VERSION` file (SemVer validated).
- Text centering now uses `bounds.left/top`-aware origins to prevent per-font alignment drift.
- Spawn animations are explicitly cleared on reset/menu transitions and when entering game-over state.
- Removed `std::char_traits<unsigned int>` workaround header from app build path.
- CI and release workflows now pin `vcpkg` checkout to `vcpkg.json` `builtin-baseline`.
- Added CI coverage gate (`gcovr`) with minimum line coverage threshold for `src/core`.
- CI now runs a dedicated clang-tidy static analysis job (`bugprone`, `performance`, `portability`).
- CI/release provision CMake `3.29.6` explicitly on all runners.
- Linux CI/release jobs now install SFML system prerequisites required by vcpkg builds.
- Windows CI/release now run multi-config aware build/test/package commands (`--config` / `ctest -C`).
- Core tile spawn RNG now uses an implementation-independent bounded sampler for cross-platform deterministic seed behavior.
- CI coverage filter paths now use workspace-relative regexes to avoid platform/path encoding mismatches.
- CI static-analysis now reports clang-tidy warnings without hard-failing on warning-level findings.
- GitHub Actions workflow actions were upgraded to latest major versions (`checkout@v6`, `cache@v5`, `download-artifact@v7`, `upload-artifact@v6`).
- vcpkg baseline pin was updated to `66c0373dc7fca549e5803087b9487edfe3aca0a1`.
- Branch protection setup script now supports `single-maintainer` (safe default) and `team` profiles.
- Release workflow now generates SPDX SBOM files from extracted package contents per platform artifact.
- README now includes CI/release/license badges and clearer screenshot/branch-protection guidance.
- README screenshot now points to a concrete gameplay image (`docs/screenshots/gameplay.png`) instead of placeholder art.
- README was redesigned as a bilingual (TR/EN), showcase-focused portfolio landing page with simplified quick-start guidance.
- Windows release packaging now uses explicit PowerShell ZIP staging (exe + assets + DLLs) to avoid CPack failures on `windows-latest`.
- App CLI supports runtime performance flags (`--fps`, `--vsync`, `--no-vsync`) and `--help`.
- App asset path discovery was extracted from `App.cpp` into a dedicated `AssetResolver` module to reduce UI-layer coupling.
- App UI now shows `Best: <score>` in the top panel and game-over overlay, backed by persisted high-score data.
- Playing scene now includes a top-panel `NEW GAME` button and score-delta floating text feedback.
- Tile visuals were modernized with rounded custom shapes and move-timeline effects (slide lerp, merge pop, spawn fade).
- Added interactive sound toggle button plus action-based sound cues (slide, merge, spawn, game over, high score).
- Playing top panel layout now stacks `Best` under `Score` on the left and moves actions into a right-side hamburger menu.

### Removed
- Committed build artifacts (`*.o`, local binaries, `.DS_Store`).
- Legacy Makefile with machine-specific Homebrew include/library paths.

## Historical pre-Numtilo record — 0.2.1 — 2026-02-23

This earlier development record retains its original date; the GitHub v0.2.1
publication is recorded above on 2026-10-20.

### Changed
- CMake post-build asset copy command now uses explicit quoting and `VERBATIM` for safer cross-platform path handling.
- Manifest and project version metadata are aligned at `0.2.1` (`VERSION` and `vcpkg.json`).
- README rendering quality was improved (`# sfml-2048` header fix, normalized LF policy via `.gitattributes`, and explicit coverage gate note).

### Legal
- Replaced `assets/fonts/Geneva.ttf` with OFL-licensed `Inter-Variable.ttf`.
- Added `assets/fonts/LICENSE-Inter.txt` and updated `THIRD_PARTY_NOTICES.md` accordingly.

## [0.1.0] - 2026-02-20

### Added
- Initial playable SFML-based 2048 implementation.
