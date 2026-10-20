# Build configuration contract

Date: 2026-10-07. Step 6.1 defined the option, target and dependency contract below before
changing CMake. At that checkpoint SFML discovery was unconditional and
`SFML_2048_BUILD_APP` was a planned option.

Step 6.2 implements the CMake target boundaries and application switch. Direct CMake builds
can disable the desktop application independently of unit tests. At that checkpoint the vcpkg
manifest still installed desktop dependencies regardless of the options. Step 6.3 now aligns
manifest feature selection with those same options before the toolchain runs; all four native
vcpkg configurations passed installation/build validation. Step 6.4 adds matching Debug/Release
configure/build/test presets with native macOS and minimum-version validation. Verification is
recorded in [testing.md](testing.md). Step 6.5 adds a reusable fresh-build dependency verifier.
Step 6.6 separates headless and desktop CI, with native macOS validation; the new hosted jobs
still need execution evidence. Step 6.7 completes the native macOS four-combination Debug/Release
regression and installed/ZIP runtime acceptance. Phase 6 implementation is complete locally;
cross-platform hosted execution remains unverified.

Current contract tables use Numtilo's `NUMTILO_` options and `numtilo` target names. The dated
checkpoint narrative above and historical validation sections retain their original scope and
identifiers; these naming updates do not establish new hosted, package or graphical results.
Use fresh Numtilo build trees. Pre-rename `SFML_2048_*` cache entries are not aliases for
the new options and are not migrated by these presets; preserve old trees as evidence.

## Independent options

| Option | Default | Responsibility |
| --- | --- | --- |
| `NUMTILO_BUILD_APP` | `ON` | Build the desktop executable, synchronize assets and enable Runtime installation/packaging |
| `BUILD_TESTING` | `ON` | Build and register backend unit tests; allow desktop platform acceptance when the application is also enabled |
| `NUMTILO_BUILD_UI_ACCEPTANCE` | `OFF` | Enable graphical UI acceptance; requires the application, tests, Python and a native build, with a display at execution |
| `NUMTILO_ENABLE_WARNINGS` | `ON` | Enable strict project compiler warnings |
| `NUMTILO_ENABLE_SANITIZERS` | `OFF` | Enable AddressSanitizer/UndefinedBehaviorSanitizer on supported GNU/Clang builds |
| `NUMTILO_ENABLE_COVERAGE` | `OFF` | Enable coverage instrumentation on supported GNU/Clang builds |
| `NUMTILO_MACOS_BUNDLE` | `ON` on macOS, `OFF` elsewhere | Select the macOS application-bundle layout when building the application |

The application option must explicitly default to `ON`, preserving normal desktop builds.
`BUILD_TESTING` uses the standard option from [CMake's CTest module](https://cmake.org/cmake/help/v3.24/module/CTest.html),
declared before `project()` so dependency installation sees the requested value. CTest later
reuses that value.
Neither option forces the other on or off. All four combinations are supported by the final
implementation. On macOS, `NUMTILO_MACOS_BUNDLE` selects the desktop layout only; it has no
effect when the application is disabled.

## Configuration matrix

| Application | Tests | Build outputs | CTest entries | Runtime install / CPack |
| --- | --- | --- | --- | --- |
| `ON` | `ON` | Backend libraries, `app_accessibility`, `numtilo`, assets, `core_unit_tests`, `accessibility_unit_tests`, `platform_acceptance_worker` | Both unit tests; `platform_acceptance` when its prerequisites are available | Enabled |
| `ON` | `OFF` | Backend libraries, `app_accessibility`, `numtilo`, assets | None | Enabled |
| `OFF` | `ON` | Backend libraries, `core_unit_tests` | `core_unit_tests` | Disabled |
| `OFF` | `OFF` | Backend libraries | None | Disabled |

The existing unit suite remains available in both test-enabled configurations, including game
rules, persistence, session/score coordination, settings, migration, paths and runtime diagnostics.
Headless tests require no display, audio device, packaged font or sound files. Resource tests
use their own temporary fixtures; runtime diagnostics remain testable as a library.

`platform_acceptance` exercises the real application and its packaged resources. Its worker
is built only when both options are `ON`; the CTest entry additionally requires a Python 3
interpreter and a native build (`NOT CMAKE_CROSSCOMPILING`). Missing Python or cross-compilation
skips this entry with a configure status message, while unit tests remain available. The worker
is never installed or packaged. CTest labels remain `unit` and `platform` respectively.
The separate `NUMTILO_BUILD_UI_ACCEPTANCE` option adds `ui_acceptance` and its `ui`-labelled
CTest entry when its prerequisites are met; the matrix above describes the default `OFF` value.

## Target boundaries

| Target group | Targets / responsibilities | Condition |
| --- | --- | --- |
| Backend | `game_core`, `app_logic`, `app_paths`, `asset_resolver`, `asset_locations`, `user_data_paths`, `app_settings`, `profile_migration`, `runtime_check` | Always |
| Desktop | `numtilo`, `numtilo_assets`, `app_accessibility`, UI/input/sound sources, bundle properties/fixup, Runtime installation and CPack configuration | Application `ON` |
| Unit tests | `core_unit_tests`, Catch2 integration and its CTest registration; `accessibility_unit_tests` when the application is also enabled | Tests `ON` |
| Platform acceptance | `platform_acceptance_worker`, Python discovery and conditional `platform_acceptance` registration | Application `ON` and tests `ON` |
| UI acceptance | `ui_acceptance` and its `ui`-labelled CTest registration | UI acceptance `ON`, application/tests `ON`, Python available and a native build |

The logical project and application target are both `numtilo`. Output naming follows the
platform and layout, while the backend and test target names above remain unchanged:

| Platform / layout | Application output |
| --- | --- |
| Linux | `numtilo` |
| Windows | `Numtilo.exe` |
| macOS bundle, the default | `Numtilo.app/Contents/MacOS/Numtilo` |
| macOS with `NUMTILO_MACOS_BUNDLE=OFF` | `numtilo` with sibling `assets/` |

The bundle name/display name are `Numtilo`; the identifier is `io.github.enesgoker.numtilo`.
Its executable-name plist field follows CMake's output name. The vcpkg manifest name is
`numtilo`; CPack's package name is `Numtilo`. Its binary ZIP basename is
`Numtilo-<version>-<lowercase CMAKE_SYSTEM_NAME>-<CMAKE_SYSTEM_PROCESSOR>`: platform names remain
`linux`, `darwin` and `windows`, and processor spelling comes directly from CMake. For example,
an arm64 macOS configuration names its ZIP `Numtilo-0.2.1-darwin-arm64.zip`.
The Windows release workflow retains its separate Runtime-install/ZIP method and existing
`matrix.os`-based format, `Numtilo-windows-latest.zip`, without version or architecture fields.
The source ZIP script defaults to `Numtilo-0.2.1-source.zip` with a `Numtilo-0.2.1/` root;
both derive their version from `VERSION`. These configured names do not imply a published release.

Source packaging uses the shared staging-only selector `scripts/stage_source.py`. It reads
NUL-delimited Git index paths/modes and copies each selected file's current working-tree bytes,
including uncommitted tracked changes. It never recursively copies the checkout or falls back
to a directory scan. Untracked/ignored files are not selected; tracked `.github/` and `.vscode/`
maintenance files remain included. Conflicts, unsupported Git modes, invalid/colliding paths,
known private/generated paths (including score/settings temporary and backup families and
AppleDouble `._*` files),
symlinks, hardlinks, special files and missing/changed sources stop staging.

Required-source checks include `scripts/stage_source.py`, `scripts/verify_source_package.py`,
`cmake/StageSourcePackage.cmake.in` and `cmake/VerifySourcePackage.cmake.in`. Until these helpers
are Git-tracked, real source packaging refuses with their names rather than silently omitting
them or adding untracked files. No Git add is performed automatically.
CPack Source now defaults to and supports only `ZIP`; its actual generator is checked during
staging and final verification, so an explicit `-G TGZ` or another non-ZIP override fails.
The version remains `0.2.1`. Shell keeps `Numtilo-0.2.1-source.zip` with `Numtilo-0.2.1/`;
CPack keeps `Numtilo-0.2.1-Source.zip` with `Numtilo-0.2.1-Source/`. VERSION is read through
the guarded selector; CPack refuses a working-tree version that differs from its configured
version. Binary generators and Runtime installation retain their separate behavior.

The generated CPack source configuration clears raw `CPACK_INSTALLED_DIRECTORIES`,
`CPACK_INSTALL_CMAKE_PROJECTS` and `CPACK_INSTALL_COMMANDS`, then uses `CPACK_INSTALL_SCRIPTS`
with the source-only staging hook. The CMake 3.24 mechanism has been checked statically;
the actual hook, guarded staging and error propagation were exercised with synthetic
fixtures on macOS arm64 using CMake/CPack 4.0.2. CMake 3.24 has not been exercised dynamically.
Configure does not stage files or create archives. Binary CPack configuration, Runtime
installation, bundles and dependency selection are unchanged; an isolated Runtime-only
binary ZIP control also passed.
Existing generated source configurations do not acquire this hook automatically; use a fresh,
approved configuration before source packaging. Historical build trees are not updated here.

The selector can also write an exclusive, owned, mode-0600 manifest alongside (never inside)
the staged tree after all source/index/root checks succeed. Its SHA-256 values, sizes and Git
modes describe the bytes actually copied; verification does not refresh this snapshot from a
later working tree. CPack uses the source-only `CPACK_POST_BUILD_SCRIPTS` hook to invoke
`scripts/verify_source_package.py` on exactly its expected temporary ZIP. Shell invokes the
same verifier before atomic publication. Missing helpers or verification errors have no
directory-scan or unchecked-publication fallback.

The verifier reads archives without extraction and requires the exact expected file set,
hashes, sizes and executable modes under the correct safe root. It rejects private/AppleDouble
paths, `__MACOSX`, duplicate/colliding names, unexpected directories/types, encrypted members,
unknown extra fields, contradictory headers and unused/hidden compressed data. Standard
timestamp, Unix UID/GID, consistent Unicode-path and necessary ZIP64 fields are structurally
checked; this is not a claim that every standard ZIP metadata field is stripped. Reads are
bounded and use the existing POSIX no-follow/identity checks.

Real CPack 4.0.2 tests disproved the earlier assumption that post-build `FATAL_ERROR` alone
prevents final copying. The source post-hook therefore blocks `CPACK_OUTPUT_FILE_PREFIX`
under its own regular script file as its first operation, and restores the requested output
directory only after successful verification and the exact version response. If CPack
continues after a hook error, a regular file cannot serve as the copy's parent directory.
Malformed ZIPs, verifier failure and missing Python were exercised with this guard: no new
final output appeared and existing final sentinels were unchanged. Invalid archives can remain
in CPack's temporary work area for diagnosis; a failed run does not validate any stale output.
The runtime prefix lookup is also present in CMake 3.24.4's source, but only 4.0.2 was executed.

Git and Python 3 are required only when source packaging runs. The selector requires physical
absolute paths and POSIX descriptor-relative, no-follow APIs; unsupported platforms, including
native Windows source packaging, fail closed. Normal configure/build gains no Git/Python
requirement from this mechanism, and an extracted source tree without `.git` can still build
with its usual dependencies. Binary Windows packaging is unaffected.

The Shell ZIP path validates the X.Y.Z version and a safe `.zip` basename, rejects path/option
arguments and existing outputs, and stages in a new private temporary directory. Publication
uses an atomic no-overwrite hard link; cleanup is restricted to that invocation's owned
temporary directory. `.numtilo-source-*/` is also ignored by Git in case safe cleanup refuses;
this ignore rule is not the source selector's security boundary. Python's standard ZIP writer
serializes only the private staged tree, preserves Unicode names and executable modes, and
does not transfer OS extended attributes or accept `ZIPOPT`. Source metadata, index and
root checks detect observable replacements
or changes, but do not provide an atomic snapshot of the whole tree or complete protection
against a concurrent process with the same user identity.

The earlier isolated 2026-10-15 security checkpoint passed 45 selector controls and 45 Shell controls.
All 150 existing tracked path names plus the two required helpers were tested with synthetic
contents. The Shell ZIP and nondefault CPack Source ZIP had identical normalized file names,
SHA-256 contents and executable modes for the 35-file packaging fixture.

That earlier checkpoint found 47 unselected AppleDouble members in each default tar source
generator (`TBZ2`, `TGZ`, `TXZ`, `TZ`) on macOS/CPack 4.0.2. `COPYFILE_DISABLE=1` and attempted
extended-attribute removal in owned staging did not resolve it. The approved resolution is
ZIP-only source distribution and final verification, not a change to CMake's archive writer.
Those tar formats are now rejected before source archiving. Mixed `-G 'ZIP;TGZ'` runs fail
at TGZ and produce no tar, but a verified ZIP from the first generator may already remain;
multi-generator invocations are not an all-or-nothing transaction.
No real Numtilo source archive or user-profile content was used in these checkpoints.
Earlier package evidence retains its historical scope.

Step 6.2 makes `game_core` explicitly `STATIC`, matching the other backend libraries; toggling
`BUILD_SHARED_LIBS` does not change these project targets into shared libraries. The `app_logic`
static library owns `GameSession.cpp` and `ScoreCoordinator.cpp` and links `game_core` publicly.
The application and unit tests link this target instead of compiling those sources separately.
Project warnings, sanitizer and coverage instrumentation apply to it just as to the other
backend libraries.

Backend targets may depend on each other, JSON and native operating-system libraries, but must
not acquire SFML, fmt or OpenAL include paths or link dependencies. macOS Foundation/Objective-C++
and Windows `shell32`, `ole32` and `uuid` remain valid dependencies of their existing native
adapters. Headless means independent of desktop graphics/audio dependencies, not independent
of the host platform.

The existing opt-in `game_core` Development installation remains available. Headless builds
must not generate application asset synchronization, bundle fixup, Runtime install rules or
CPack package targets, including generator expressions that reference `numtilo`.

Reconfiguring an existing build directory honors the same options. Turning tests off regenerates
an empty CTest index so earlier test registrations cannot remain active; a generator may retain
an empty test utility target. Turning the application off removes the project-generated CPack
configuration and asset-install/bundle-fixup scripts, preventing stale package targets and
desktop install configuration. Previously built executables, assets and archives are retained.
Separate build directories remain the recommended basis for clean dependency validation and
the desktop/headless presets.

## Dependencies and failure behavior

| Dependency | When needed | Contract |
| --- | --- | --- |
| `nlohmann_json` | Always | Required by backend persistence; retain package discovery and the existing pinned FetchContent fallback |
| SFML graphics/window/system/audio | Application `ON` | Required; missing SFML fails configure clearly rather than silently disabling the application |
| fmt | Application `ON` | Preserve existing desktop policy: required with vcpkg, optional with system dependencies |
| OpenAL | Application `ON` | Preserve existing desktop discovery/link behavior; no independent backend requirement |
| Catch2 | Tests `ON` | Required for unit tests; retain package discovery and the existing pinned FetchContent fallback |
| Python 3 interpreter | Application `ON` and tests `ON` | Optional; controls native platform acceptance registration |

With application `OFF`, CMake must not call SFML, fmt or OpenAL package discovery. With tests
`OFF`, it must not discover or fetch Catch2, discover Python for acceptance, build test executables
or register tests. Missing required JSON or Catch2 must fail if discovery and fallback acquisition
cannot satisfy them. A first-time fallback download may require network access; a headless build
is not an offline-build guarantee.

## Manifest dependency selection (step 6.3)

The manifest keeps `nlohmann-json` as a common dependency. The `desktop` feature owns SFML and
fmt; the `tests` feature owns Catch2. CMake declares both build options and computes the feature
list before the first `project()` call, when the vcpkg toolchain resolves and installs packages.

| Application | Tests | Selected manifest features | Direct dependencies |
| --- | --- | --- | --- |
| `ON` | `ON` | `desktop;tests` | JSON, SFML, fmt, Catch2 |
| `ON` | `OFF` | `desktop` | JSON, SFML, fmt |
| `OFF` | `ON` | `tests` | JSON, Catch2 |
| `OFF` | `OFF` | None | JSON |

The manifest defaults to `desktop` and `tests` for standalone vcpkg installation. CMake sets
`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON` and manages `VCPKG_MANIFEST_FEATURES` as derived cache
and effective variable values, so manifest defaults, earlier configurations and normal-variable
shadows cannot override application/test choices.
Manually supplied known features are recomputed from the two build options; unknown features
fail configure. Raw feature/no-default-feature overrides and `@response-file` entries in
`VCPKG_INSTALL_OPTIONS` also fail configure, regardless of option-name casing. Unrelated direct
installer arguments retain their original spelling. [vcpkg expands response-file contents into arguments](https://learn.microsoft.com/en-us/vcpkg/commands/common-options#response-files-file),
which could conceal feature overrides from the project input policy. Use `NUMTILO_BUILD_APP` and
`BUILD_TESTING` to select dependencies instead of setting these managed feature inputs.
Recomputing features changes the requested dependency graph. When automatic manifest installation
is enabled, vcpkg synchronizes the configured installed tree and may remove packages no longer
required, as implemented in its [pinned install-plan synchronization](https://github.com/microsoft/vcpkg-tool/blob/44bb3ce006467fc13ba37ca099f64077b8bbf84d/src/vcpkg/commands.set-installed.cpp).
Shared download/binary caches and earlier application outputs have separate lifetimes. Use new
build/install directories to prove dependency absence.

The existing baseline remains `66c0373dc7fca549e5803087b9487edfe3aca0a1`. SFML remains pinned to
`2.6.2`; Catch2 is explicitly pinned to `3.7.1`, matching the existing non-vcpkg FetchContent
fallback. JSON and other transitive versions follow the pinned baseline. Package discovery and
the existing JSON/Catch2 fallback paths are retained for direct builds without vcpkg.

Independent real-vcpkg dependency plans passed for all four combinations and standalone defaults.
The native `arm64-osx` plans resolve JSON to `3.12.0#2` and desktop fmt to `12.1.0`; headless plans
contain only JSON, optional Catch2 and the two vcpkg CMake helper ports. Fresh installation and
full builds passed for all four configurations without FetchContent. An actual headless
test-option round trip synchronized the installed tree from three packages to four and back
to three, adding and removing Catch2. Policy and test results are recorded in [testing.md](testing.md).
Step 6.4 presets use separate desktop/headless directories so cached dependencies do not stand
in for clean-build evidence.

## Preset workflows (step 6.4)

`CMakePresets.json` uses schema 5, supported by [CMake 3.24](https://cmake.org/cmake/help/v3.24/manual/cmake-presets.7.html),
matching both the project's minimum and `cmakeMinimumRequired`. The existing desktop configure
presets (`debug`, `release`, `vcpkg-debug`, `vcpkg-release`) and their build preset names/directories
are preserved. Their common base explicitly chooses application `ON` and tests `ON`, restoring
normal desktop behavior even after those options were turned off in the same cache.

Headless configure presets are `headless-debug`, `headless-release`, `vcpkg-headless-debug` and
`vcpkg-headless-release`, each explicitly choosing application `OFF` and tests `ON`. Matching
build/test names use `build-`/`test-` prefixes. Every configure preset owns
`${sourceDir}/build/${presetName}`; manual examples use separate `build/manual/` directories.
No generator, compiler or vcpkg triplet is hardcoded. Debug/Release configuration is explicit
in configure, build and test phases, including multi-config generators.
Optional machine-specific overrides belong in ignored `CMakeUserPresets.json`.

vcpkg configure/build/test presets require a nonempty `VCPKG_ROOT` in each phase. Test presets
filter labels with `^unit$`, enable `outputOnFailure` and use `noTestsAction=error`. They cannot
report a successful test run when the unit selection is empty. Actual preset execution and
minimum-version validation passed on native macOS; commands/results are in [testing.md](testing.md).
Step 6.6 uses the vcpkg headless presets in a separate CI matrix described below.

## Headless dependency acceptance (step 6.5)

[`scripts/verify_headless.py`](../scripts/verify_headless.py) creates a new owned run directory,
loads [`cmake/VerifyHeadlessDependencies.cmake`](../cmake/VerifyHeadlessDependencies.cmake)
through `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`, configures application `OFF`, then builds and audits
the selected Debug/Release configuration. Tests default to `ON`; `--no-tests` selects `OFF`.
The opt-in guard leaves allowed JSON/Catch2 discovery and fallback acquisition unchanged.
It records package requests and fails on SFML/fmt/OpenAL/Python discovery or FetchContent
population, plus Catch2 when tests are disabled. Passing project runs contain no such requests;
negative fixtures verify that attempted forbidden requests fail.

Acceptance requires all nine backend targets to be static, complete generated target/source
metadata, raw imported include/link/location properties and no desktop/asset/worker/Runtime/CPack
output. Imported targets whose properties cannot be inspected from the audit scope fail the
audit instead of being silently skipped. Every compiled translation unit in the selected graph,
including any built fallback dependencies, must have an object and real compiler `-M` header
evidence including system headers. Both recorded and canonical paths are checked so symlinks
cannot hide forbidden headers. Missing/empty header evidence fails. Undefined symbols in all
nine backend archives are checked for SFML/fmt/OpenAL references.

With tests enabled, CTest must register exactly the audited `core_unit_tests` executable with
the `unit` label, and the full suite must pass. Native runtime inspection checks that executable
and resolved non-system dependencies. With tests disabled, CTest must be empty; compilation,
target/import metadata, headers and archive symbols are audited, while final-executable linkage
and runtime dependency checks are explicitly inapplicable. The verifier does not link a separate
backend-only consumer.

The tool requires Python 3, CMake 3.24+, a native GNU/Clang toolchain, Makefiles or Ninja, and
`nm`/`llvm-nm`; runtime inspection uses macOS `otool` or Linux `ldd`. Visual Studio, Xcode,
MSVC and cross-compilation are outside this verifier's supported contract. macOS system
frameworks such as Foundation remain valid native adapter dependencies; their own transitive
dependency graph is not traversed. Actual native validation is recorded separately from the
supported-tool list in [testing.md](testing.md#step-65-dependency-validation).

Each run retains schema-version-1 `report.json`, stage success/error data, input/source hashes,
individual command arguments and stdout/stderr. Success requires all seven stages, including
complete dependency evidence; a failure exits nonzero and retains its report/logs. Fresh project
cache/build and vcpkg installed trees are separate from reused dependency prefixes, source,
download and binary caches. This proves the selected project's dependency boundary without
claiming an offline acquisition or a host machine with desktop packages uninstalled. Python
is required by this external acceptance tool, not by headless CMake package discovery.

## CI boundaries (step 6.6)

The CI `headless` matrix selects `vcpkg-headless-debug` and `vcpkg-headless-release` on
`ubuntu-latest`, `macos-latest` and `windows-latest`. Every job configures with
`VerifyHeadlessDependencies.cmake` as a top-level dependency provider, checks the actual cache,
native provider evidence, complete target-property closure, installed vcpkg state and absence
of desktop/package output through `scripts/check_headless_configuration.py`, then builds and
runs the matching unit-test presets. Test selection retains `^unit$` and empty-selection failure.
Windows uses Visual Studio 2022 x64; the portable checker does not require a compile database,
compiler header scans, archive tools or runtime inspection.

Linux/macOS Release additionally run the full verifier's integration controls and fresh
`verify_headless.py` Release audits with tests enabled and disabled. Those audits retain the
step 6.5 GNU/Clang, Makefiles/Ninja and native binary-tool boundary; Windows/MSVC is excluded
from the full audit. Portable-checker controls run in all three Release jobs.

Linux sanitizer and coverage jobs configure the guarded `vcpkg-headless-debug` preset in
their existing dedicated directories, perform the portable configuration check, build
`core_unit_tests` and run only the exact unit label with `--no-tests=error`. Coverage remains
an 80% line gate for `src/core`. These headless jobs add no SFML, graphics or audio development
packages. Compiler caches and vcpkg archives have separate headless namespaces; reused binary
archives do not replace the installed-port check. JSON/Catch2 acquisition can still need network
access. Matrix, sanitizer and coverage jobs retain configuration/test/audit evidence after failures.

Existing desktop `Build and Test (...)` checks retain full application, unit/platform tests and
installed Runtime acceptance. Static analysis keeps its desktop configuration. Those job names
and the release workflow are preserved. Local branch-protection profiles add the six
`Headless Build and Test (<runner>, <Debug|Release>)` contexts without removing any of the
seven existing checks. The implementation does not apply remote branch protection.

Native local macOS results and the limits of the unexecuted hosted matrix are recorded in
[testing.md](testing.md#step-66-ci-validation). Configuration evidence is a narrower check than
the full compiler/header/symbol/runtime audit; a portable pass does not imply that full audit.

## Final regression boundary (step 6.7)

All four application/test combinations passed fresh native macOS Debug and Release builds
with exact CMake 3.29.6 and real vcpkg installed trees. Desktop default configurations omitted
both option flags and confirmed `ON/ON`; explicit test-disabled desktop builds retained runtime
installation/packaging with no unit executable, worker, Catch2 or acceptance Python discovery.
Every test-enabled build retained the full unit suite. All nine backend targets remained static,
and session/coordinator compilation remained owned by `app_logic`.

Independent application-disabled builds passed the seven-stage dependency verifier with tests
enabled and disabled in both configurations, retaining no desktop/package output. All four
desktop installed runtimes and actual extracted ZIPs passed native acceptance; test-disabled
packages were exercised with a separate corresponding test-enabled worker. The real user
profile and project score/settings sources were preserved. Existing per-user data/asset layout
and production diagnostics were unchanged.
An explicit headless Release installation produced no `Runtime` files and only
`lib/libgame_core.a` for the optional `Development` component. Independent initial package
layout/signature/asset inspection and an isolated graphical fixture passed; final refreshed
archive metadata requires its own strict audit against the frozen packaged documentation.

This closes the phase 6 implementation and local macOS regression. Shared pinned vcpkg source
and binary caches were reused. The tests-disabled headless audit still has no separate final
consumer linkage, native system framework transitive closure is unchanged, and this matrix
does not replace the earlier same-directory toggle or minimum-version controls. Detailed
results and outstanding hosted/distribution evidence are in
[testing.md](testing.md#step-67-final-build-and-package-regression).

## Checkpoints and remaining work

Step 6.1 completed the design contract: defaults, all four combinations, target ownership,
dependencies and failure behavior. It did not itself provide headless configure/build/test
evidence. Step 6.2 applies those CMake boundaries. Step 6.3 implements the matching dependency
selection, step 6.4 adds reusable headless workflows, and step 6.5 provides persistent dependency
acceptance tooling. Step 6.6 adds the separate CI jobs and portable configuration gate.
Step 6.7 completes the four-combination native regression, desktop package checks and current
documentation. Broader cross-platform execution evidence remains separate work.

| Step | Deliverable | Current status |
| --- | --- | --- |
| 6.1 | Build option and target/dependency contract | Complete; preserved in this document |
| 6.2 | Implement CMake option/target boundaries and shared `app_logic` | Complete with native macOS validation; ASan execution and other native platforms remain unverified |
| 6.3 | Align vcpkg feature selection with application/test options | Complete with native macOS dependency-plan/install/build/test, policy and same-directory synchronization evidence |
| 6.4 | Add compatible headless Debug/Release presets and commands | Complete with native macOS preset workflows, CMake 3.24.4/multi-config, conditions and empty-unit failure evidence |
| 6.5 | Prove clean SFML-independent configure/build/link/test behavior | Complete with native macOS Debug/Release/testless audits, minimum-version source-omission/fallback checks and negative controls; backend-only final linkage is not tested |
| 6.6 | Separate headless and desktop CI verification | Implemented with native macOS CMake 3.29.6 preset/checker/unit and fresh Release audit evidence; hosted Linux/macOS/Windows jobs await execution |
| 6.7 | Recheck the four combinations, desktop packages and documentation | Complete with native macOS CMake 3.29.6 Debug/Release four-combination builds, independent headless audits, installed/ZIP runtime acceptance and documentation review |

Phase 6 implementation and native macOS verification are complete; desktop behavior and the
full unit suite were preserved.
Native Windows/Linux acceptance from [step 5.5](platform-acceptance.md) still requires results
from those platforms; this contract does not close that outstanding verification.
