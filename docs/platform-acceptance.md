# Platform acceptance — step 5.5

The implemented gate verifies native runtime paths and isolated persistence across process
boundaries. Its repeatable portion runs without a graphical display. Native macOS graphical
acceptance is recorded separately. A pass on macOS does not certify Windows or Linux execution.

Commands and contracts below use current Numtilo names. Dated native/package results retain
their original application names, proof paths and outcomes; they are not reruns after the
rename. Paths under `build/` identify retained local, ignored evidence.
The [Numtilo checkpoint](testing.md#numtilo-rename-checkpoint--2026-10-15) records the
narrow build/diagnostic and isolation-guard checks completed after the rename.

## Production diagnostic

```bash
# Linux or a non-bundle macOS build:
/absolute/path/to/numtilo --check-runtime
# Default macOS bundle:
/absolute/path/to/Numtilo.app/Contents/MacOS/Numtilo --check-runtime
```

On Windows the executable is `Numtilo.exe`. The CMake application target is `numtilo`
on every platform, independent of the platform-specific output spelling.

The command emits a version-1 JSON document and exits with `0` on success, `1` on a runtime
path/resource failure, or `2` on invalid CLI arguments. Its report contains native user-data and
asset roots, selection sources, resource paths/byte counts and error domains/codes. The same
`AssetCatalog` names are used by normal loading: seven resources comprising
`fonts/Inter-Variable.ttf`, `icons/numtilo.png` and five WAV sounds.

It opens no window, bootstraps no user-data directory, loads no profile, runs no migration and
writes no scores/settings. Readability and nonempty bytes (`binary_read`) are the diagnostic's
scope: it does not decode the font, PNG or sounds, render, play audio, validate score/settings
contents or certify save permissions. A nonempty invalid PNG can pass this diagnostic but
fail normal startup's image decoding before profile bootstrap. All five sounds are required
for package completeness by this gate; normal gameplay still tolerates unavailable effects.
Existing corrupt/future profile files are not inspected or replaced. There is no production CLI
option to redirect writable profiles.

## Repeatable gate

A full `NUMTILO_BUILD_APP=ON`, `BUILD_TESTING=ON` build produces the actual application,
`core_unit_tests` and a separate `platform_acceptance_worker`. Python 3 is required for native
acceptance. CMake reports explicitly
when the gate cannot be registered because Python is absent or the build is cross-compiled.

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -L platform
```

For Visual Studio multi-configuration builds, use `--config Debug` while building and `-C Debug`
with CTest. A unit-only build should run `ctest -L unit`; it does not produce the application or
worker needed by the native platform label. Sanitizer and coverage CI jobs use this unit scope.

The harness may also target an installed or extracted runtime directly:

```bash
python3 scripts/verify_platform.py \
  --executable /absolute/package/Numtilo.app/Contents/MacOS/Numtilo \
  --worker /absolute/build/platform_acceptance_worker \
  --output /absolute/build/platform-evidence \
  --mode installed
```

For Windows use `python`, the package's `Numtilo.exe`, and the test build's
`Debug/platform_acceptance_worker.exe` or `Release/platform_acceptance_worker.exe`. Plain
macOS/Linux layouts use the sibling `numtilo` executable. The worker is excluded from Runtime
installation and distribution archives. `--mode` annotates evidence; it does not change path
selection or weaken the assertions.

Each run claims a unique output directory and retains a versioned `report.json`, exact subprocess
commands/CWDs and stdout/stderr. All mutations are inside copied runtimes or new temporary
profiles. The process environment is not redirected, and the real OS profile is never loaded
or bootstrapped. Native OS path queries are read-only.

| Case | Required behavior |
| --- | --- |
| Foreign launch directories | Two distinct CWDs yield the same native profile and asset roots; every packaged resource is readable |
| Unicode relocation | The copied runtime finds its own assets, retains the OS profile path, and reads byte-identical resources |
| Missing font | Only the copied package is modified; exit `1` identifies its missing full path despite valid CWD decoy assets |
| Missing window icon | Removing only the copied `icons/numtilo.png` yields exit `1` with its missing selected path; the seven-resource catalog remains intact and valid CWD decoy assets are ignored |
| Separate-process write/reopen | Unicode profile/legacy roots import once; best `60000`, three completed rounds, one fixed round ID and sound-off survive repeated write/reopen without byte changes; later higher legacy scores cannot replace canonical data |
| Protected source/recovery | Invalid legacy scores create no canonical score target, dirty memory survives repeated blocked saves, settings persist independently, and a new process imports a repaired source once |

The persistence worker requires an existing regular sandbox directory with exact marker
`.numtilo-acceptance` containing `numtilo-acceptance-v1\n` (22 bytes, including one final LF).
Profile and legacy roots must be absolute, disjoint descendants without parent traversal,
embedded NULs or inside-sandbox
symlinks. Profile leaves cannot be symlinks. Native wide Windows argument parsing preserves the
Unicode paths supplied by Python. These checks prevent accidental default-profile use; this
test fixture is not intended to defend against an adversary replacing its files concurrently.

Location import, normal loaders and atomic storage are production implementations. Legacy
sources remain byte-identical, format upgrades retain exact `.bak` bytes in the destination,
and clean separate-process reopens do not duplicate or rewrite history. Repairing a blocked
source requires a fresh process, preserving the existing no-mid-round-reload contract.

The default production profile is now the native user-data base plus `numtilo`. Normal
startup retains `sfml-2048` only as a compatibility source: an existing destination file
is authoritative; otherwise each missing score/settings file first checks the old sibling
profile, then the captured legacy working directory only when that native source is missing.
An invalid, unreadable or otherwise failed native source blocks that file's fallback.
The persistence worker instead supplies both explicit sandbox roots, bypassing real native
and ambient working-directory migration sources. Its scenarios do not certify a migration
of the actual user's old profile.

On macOS, optional `--launch-services` runs the same read-only production diagnostic through
`open -n -W` against a `.app`. It asserts native path equivalence and records logs. This is a
LaunchServices probe, not physical Finder interaction or a graphical rendering test.

## CI and release gates

The Linux/macOS/Windows build matrix runs unit and native platform tests, then installs the
`Runtime` component and repeats native acceptance against that staged executable. Release jobs
also extract the generated ZIP and run acceptance against the actual extracted application
before package upload. macOS extraction uses `ditto` to preserve bundle symlinks and executable
modes; Windows uses `Expand-Archive`.

MSVC C++ compilation explicitly uses `/utf-8` even when warnings are disabled, so BOM-free
source literals and Unicode fixtures do not depend on the Windows host's active code page.

The installed and ZIP checks catch packaging regressions that a build-tree test can miss.
They run on the build runner and do not establish compatibility with a machine lacking the
development toolchain. No CI run, tag, GitHub Release or publication is triggered by the local
implementation work.

## Native macOS results — 2026-10-06

On this arm64 host, Debug and UBSan each passed **249 cases and 281,933 assertions**, including
eleven new read-only runtime-diagnostic cases. The full Debug application/worker build and
format check passed. The CTest platform gate passed all five native subprocess cases using
the actual Debug bundle. Existing project `scores.json` and `settings.json` hashes remained
unchanged.

The final harness revision also checks idempotent repeated writes and ignores deliberately
changed, higher-scoring legacy data after the canonical profile exists. It passed five cases
against the Debug bundle and five against the installed Release bundle. The ZIP-extracted
Release passed six, including the read-only native LaunchServices probe.

| Evidence | Result | Location |
| --- | --- | --- |
| Debug build | 5/5 | `build/refactor/platform-acceptance-ci/platform-bdae7y3y/report.json` |
| Installed Release | 5/5 | `build/runtime-acceptance-native/installed-final/platform-4d4hx4ij/report.json` |
| ZIP-extracted Release | 6/6 | `build/runtime-acceptance-native/package-final/platform-mt6eege0/report.json` |
| Bundle packaging inspection | Passed | `build/runtime-acceptance-native/native-packaging-evidence.json` |
| Graphical LaunchServices fixture | Passed | `build/platform-acceptance/gui-oyic7let/report.json` |
| Finder graphical replay | Passed | `build/platform-acceptance/gui-oyic7let/finder_results.json` |

Installed and ZIP-extracted bundles passed `codesign --verify --deep --strict`. Each contains
twelve Mach-O files whose dependency inspection found only bundle-relative or system references,
and seven managed assets matching source bytes. Archives contain no acceptance worker/static
test libraries. These bundles use local ad-hoc signing, not distribution signing.

Graphical acceptance uses ignored App/main source copies with compile-time sandbox profile and
legacy overrides, injected deterministic events, diagnostic observations and automatic close.
Production path/resource selection, SFML decoders, scenes, session, persistence and close
checkpoints remain in use. The copied installed bundle supplies embedded Frameworks and actual
Resources/assets. It is not an unmodified production-binary graphical run.

The LaunchServices fixture rendered 1,695 frames, loaded 876,576 font bytes and a real glyph,
loaded all five sounds, performed a real merge with score `4`, toggled sound off and restarted.
After restart, best `4`, zero completed games and sound-off were verified from its fresh
temporary profile. No user profile or project legacy file was changed.

For the Finder replay, CUA double-clicked the selected `.app` in Finder and observed the actual
`2048 Platform QA` window with score `0`, best `4` and two starting tiles. Its fresh profile was
distinct from the preceding LaunchServices run. A close-button action was followed by a
terminated-process accessibility result; the final fixture report passed after 1,058 rendered
frames and confirmed the same merge/restart/best/history/settings invariants. This checks native
Finder launch, a visible frame and successful finalization; it is not broad button hit-testing,
manual gameplay, UI design or audio-playback acceptance.

## Phase 6 package regression — 2026-10-07

Step 6.7 repeated the desktop runtime contract on native macOS arm64 with exact CMake 3.29.6
and fresh vcpkg Debug/Release builds, each with tests enabled and disabled. Both test-enabled
builds passed the full **249-case, 281,933-assertion** unit suite and the five build-tree platform
cases. All four installed runtimes passed **5/5**, and all four actual ZIP-extracted runtimes
passed **6/6**, including read-only LaunchServices launch/path diagnostics. Test-disabled
packages used an external worker from the matching test-enabled build and did not gain worker
or unit-test installation. Foreign/Unicode runtime checks and guarded persistence scenarios
preserved the actual user profile and project score/settings files.

These are runtime/path/persistence regression results from this Mac. The LaunchServices gate
opens no gameplay window and does not establish production-binary rendering or sound playback.
Independent initial package inspection also passed for all four cases: seven exact managed
assets, strict deep ad-hoc signatures, ZIP integrity/extraction checks and no test/profile/build
payload. Each bundle contains one arm64 Mach-O executable with only system library/framework
loads because these vcpkg desktop dependencies are linked statically. Final refreshed package
metadata needs a separate strict audit against the frozen README/CHANGELOG; the initial layout
report does not establish that match.

A copied, instrumented App/main graphical fixture passed separately with compile-time temporary
profile/legacy roots. It rendered 544 frames, decoded a real font glyph and all five sounds,
merged to score 4, toggled sound off, restarted and verified best 4, empty completed history,
sound-off persistence and an exact legacy backup. Production sources, the source installed
bundle and protected profiles remained unchanged. This is isolated graphical application
integration, not an unmodified production GUI, physical Finder or audible-playback pass.
Its report is `build/step-6.7-independent/gui/gui-xttu4t2g/report.json`; initial independent
package evidence is `build/step-6.7-independent/packaging/initial-layout-evidence/report.json`.

Commands and exact reports are indexed in `build/step-6.7-validation/desktop/summary.json` and
the final `build/step-6.7-validation/summary.json`. The full option matrix and independent
headless audits are recorded in
[testing.md](testing.md#step-67-final-build-and-package-regression).

## Remaining platform evidence

Windows and Linux adapters and workflow gates are implemented, but their new native jobs have
not been executed from this Mac. Before claiming those platforms accepted, obtain successful
native CI build/installed/archive reports. Windows graphical launch and a clean machine still
require their own check. macOS Developer ID signing, notarization, quarantine behavior and
external clean-machine acceptance remain separate distribution work; local ad-hoc signing
establishes execution validity only.

Phase 6 now provides a dependency-minimal headless CMake configuration and separate headless
CI. Application-disabled configuration does not discover SFML/fmt/OpenAL or produce desktop
Runtime/CPack output. Native macOS phase 6 regression is complete; the new hosted matrix and
Linux sanitizer/coverage jobs still need their actual runner results.
