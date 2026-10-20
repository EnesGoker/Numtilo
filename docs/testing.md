# Testing Strategy

Current commands use the Numtilo identity. Dated validation narratives and numbered
checkpoint records retain their original names, options, build/proof paths and results;
they do not establish a rerun after the rename. Evidence under `build/` is retained local,
ignored output, not a report committed to the repository.

The application target is `numtilo`. Its project options are `NUMTILO_BUILD_APP`,
`NUMTILO_BUILD_UI_ACCEPTANCE`, `NUMTILO_ENABLE_WARNINGS`, `NUMTILO_ENABLE_SANITIZERS`,
`NUMTILO_ENABLE_COVERAGE` and `NUMTILO_MACOS_BUNDLE`; `BUILD_TESTING` and the preset names
remain unchanged. See the [build configuration contract](build-contract.md).

## Scope

Primary quality gate is unit testing of `src/core` because gameplay correctness lives there and should not depend on SFML.

The Catch2 `core_unit_tests` target includes:

- `tests/core_unit_tests.cpp`: existing game, tile, board, score-storage, and golden tests.
- `tests/board_move_tests.cpp`: explicit movement rules and complete movement-record contracts.
- `tests/board_move_properties.cpp`: asymmetric full-board fixtures and spatial symmetry properties.
- `tests/board_state_tests.cpp`: empty-cell enumeration, safe placement, and game-over boundaries.
- `tests/game_spawn_tests.cpp`: normal spawn invariants, seeded round lifecycle, and RNG preservation.
- `tests/win_state_tests.cpp`: winning threshold, latched achievement, independent flags, and continued play.
- `tests/score_memory_tests.cpp`: independent best/history, derived top five, round-ID deduplication,
  dirty transitions, v2 roundtrips, and legacy migration guards.
- `tests/score_schema_tests.cpp`: version/field validation, transactional load failures, duplicate
  keys/IDs, stable legacy migration, and source preservation.
- `tests/score_storage_tests.cpp`: native atomic publication, byte-exact migration backup, injected
  I/O failures, retry idempotence, source/backup conflicts and temporary-file ownership.
- `tests/game_session_tests.cpp`: round identity, one-time completion acknowledgement, restart,
  and exact seeded gameplay equivalence without an SFML window.
- `tests/score_coordinator_tests.cpp`: live best snapshots, completion before storage, retry across
  round boundaries, notification deduplication, startup migration, and native mid-game reload.
- `tests/score_acceptance_tests.cpp`: real won-to-continued-play lifecycle, abandoned best versus
  lower completed score, and native full-history/ranking/identity roundtrips across seven rounds.
- `tests/app_paths_tests.cpp`: independent path roots, strict overrides, deterministic source
  precedence, selected-path errors and side-effect-free resolution in temporary directories.
- `tests/asset_paths_tests.cpp`: asset-only/composite parity, authoritative overrides, injected
  native discovery faults, selected-root validation and read-only native discovery smoke.
- `tests/asset_resolver_tests.cpp`: one-root typed resource lookup, unsafe input rejection,
  Unicode/binary reads, exact error paths and side-effect-free symlink/resource behavior.
- `tests/user_data_paths_tests.cpp`: asset-independent profiles, injected/native discovery,
  controlled bootstrap, XDG selection, path obstructions and score/settings integration.
- `tests/settings_store_tests.cpp`: strict settings schemas, protected sources, exact legacy
  backups, dirty retries and native atomic roundtrips independent from the score model.
- `tests/profile_validation_tests.cpp`: native immutable byte snapshots, shared score/settings
  parser parity, literal-NUL rejection, escaped-NUL metadata, and nonregular/symlink protection.
- `tests/profile_migration_tests.cpp`: independent location import, canonical-target priority,
  byte-exact source preservation, destination-only format upgrades, file-specific write barriers,
  fresh-start retry, source rechecks and concurrent target publication.
- `tests/runtime_check_tests.cpp`: read-only diagnostics, independent provider errors, strict
  overrides, native Unicode paths, selected-root resource errors and unchanged profile snapshots.

The separate `platform_acceptance_worker` target belongs only to builds with both
`NUMTILO_BUILD_APP=ON` and `BUILD_TESTING=ON`. Its native
profile writes require explicit disjoint roots beneath an existing sentinel-marked sandbox.
`scripts/verify_platform.py` runs that worker and the actual application's `--check-runtime`
command in separate processes. It never redirects or bootstraps the real user's profile.

The [build configuration contract](build-contract.md), defined in step 6.1 and implemented in
step 6.2, makes application and unit-test selection independent. Unit tests keep their complete
source list and backend coverage with the application disabled; their resource fixtures do not
need the packaged font or sounds. Platform acceptance exercises the actual application and
therefore is unavailable in a headless build.

## Numtilo rename checkpoint — 2026-10-15

The narrow native macOS arm64 Release checkpoint configured and built target `numtilo`,
then passed `--help` and the read-only `--check-runtime` diagnostic. The resulting
`Numtilo.app/Contents/MacOS/Numtilo` and bundle metadata use Numtilo and
`io.github.enesgoker.numtilo`; all seven bundled assets match their source bytes.
The runtime probe blocked network access, file writes and reads of the real current/legacy
profile contents. It opened no score/settings contents and wrote no profile. Its local
summary is `build/numtilo-interim-release-20261015-vb14d30a/reports/summary.json`.

The subsequent marker checkpoint passed 19 isolation-guard controls for
`.numtilo-acceptance` and exact bytes `numtilo-acceptance-v1\n`. Each worker deliberately
stopped with the expected guard error before native profile discovery or persistence;
this is not a pass of the five full platform scenarios. Worker writes and network access
were denied. Fixture preparation verified 24 source anchors and prepared 15 cases for
`NumtiloEdge112Lifecycle` without compiling or launching the fixture. Local records are
`build/numtilo-step-7-9rsik81p/reports/summary.json` and
`build/numtilo-step-7-9rsik81p/reports/marker-tests.json`.

These checkpoints did not run the full test suite, GUI, VoiceOver, installation,
packaging, release, hosted CI or Windows/Linux execution. Earlier results below keep
their original scope; they are not renamed-output regression results.

## UI acceptance scope (step 10)

The [UI quality audit](ui-quality-audit.md) records the current graphical baseline,
findings, evidence levels and the acceptance gates for all UI work in steps 1–9.
Unit/platform checks do not certify painted layout, typography or native interaction.
Step 10.6.2 adds an opt-in maintained graphical target using current sources and
versioned inputs; earlier ignored fixtures remain historical local evidence.
Controlled rendering/scene input, native mouse delivery and actual VoiceOver use
are separate gates. Headless selection remains independent of SFML.

Step 10.1 added no permanent test target and changed no application behavior.
Its isolated current-source proofs confirmed outcome-animation command loss and
Game Over margin cancellation inconsistency; the corresponding fixes and regression
checks belong to 10.4/10.5. Source, controlled-render and native evidence are labeled
separately. Step 10.2 adds local shared-component/pointer regressions at 1×/2×,
current motion/lifecycle reruns and native pointer tours; it adds no permanent test
target. A retained diagnostic corrected independently timed glyph-bound comparison
to one logical pixel of raster tolerance while keeping separation/ink checks.
Source changes and evidence are listed in the audit; native accessible exposure
is assessed but not implemented or certified. Windows/Linux hosted graphical
acceptance remains step 12.

Step 10.3 adds the current 1×/2× menu/table matrix: 1,725 checks, 74 state PNG files,
48 mapped-pointer scenario runs, original-entry/identity preservation, real RGBA
ink containment/column alignment and explicit wide/unsupported/invisible name
validation. The 173 current reveal checks and 22 representative whole-UI renders
also pass. Native evidence covers field pointer/focus, empty validation, ASCII
18-character feedback, keyboard Scores/Back/Start and existing Unicode-profile
projection. Failed coordinate actions/tool Unicode input are retained as diagnostics,
not counted as native IME or successful action proof. A07 still requires maintained
repository regression entry points in 10.6.

Step 10.4 adds the strengthened current-source Playing matrix: 5,469 checks / 178 PNGs
at 1×/2×, every representable power-of-two tile, explicit synthetic numeric stress,
full-surface glyph containment, fixed name/caption reservations, independent whole-
board motion references and 72 mapped pointer scenario runs. Pointer mapping uses one
hidden event window alongside both render densities. App-derived routing passes 64
isolated/reopened profiles and 32,841 checks; an entry-source negative control
reproduces all ten original losses. Native keyboard routes and all three Playing
pointer controls pass; two windows exit zero and 41 checks reopen real stores with
correct best/history/name/settings. Short-motion races use controlled source/clock
fixtures; native captures do not establish millisecond latency. Fixture assumptions
and tool failures are retained separately. Twenty-two baseline renders confirm
changes remain in intended palette/player regions, with four table frames identical.
No maintained graphical target is added; A03/modal work is 10.5 and A07/A08/A09 plus
combined/native-IME/other-platform readiness remain open.

Step 10.5 adds the current outcome matrix: 1,575 checks / 94 PNGs at 1×/2×, fixed
card/shadow and hit geometry, full numeric/heading/icon ink, backdrop coverage and
whole-layer reveal. Four negative hit controls and two card-jitter controls use clean
entry scene copies. App-derived routing passes 34,583 checks over 165 reopened
profiles, including 10.4 preservation; 694 entry-filter checks reproduce 16 margin
failures. Both legal Continue→terminal→Replay and separately labeled controlled
simultaneous outcome priority pass. All 22 unfocused baseline frames stay identical;
nine contrast and four zero-restoration comparisons pass. Native delivery passes all
four modal pointer actions and five held-Space backdrop sequences with no blur in
the recorded input chain. Four windows exit zero; 79 native trace/store checks confirm transitions and
reload actual stores, with default settings clean/missing and no file creation. Test-assumption
diagnostics are preserved. A15's remaining five action buttons and Sound row,
maintained graphical gate, combined coverage/accessibility and native IME/platform
readiness stay open in 10.6. Final closure is deferred by the user.

Step 10.6.1 closes shared A15 geometry with two actual scene variants: each passes
1,730 checks / 682 controlled scenarios at 1×/2×. The entry reproduces 96 wrong
commands; current has zero. All 68 scene/state PNG pairs are RGBA/byte-identical;
each render variant passes 411 checks. Current App/menu pointer and focused edge
checks pass natively; a public-session Playing App copy passes NewGame/Sound/MainMenu
and reentry by keyboard. Two clean exits and actual store reopening preserve best,
history, identity and Sound-off preference. Coordinate-pointer failures remain
explicit pending Playing renewal in 10.6.4, rather than being counted as successful
native tests. Source/clock/event/render/native evidence is separated in the audit.
Release, 255 unit cases / 282,023 assertions, five platform scenarios and formatting
pass. No permanent graphical target or test-source change is introduced; maintained
acceptance is 10.6.2, native accessible operation 10.6.3 and combined acceptance 10.6.4.
Local evidence is under `build/ui-quality-step-10.6.1/`; old build dependencies are
retained until the maintained acceptance work and later controlled cleanup.

## Maintained graphical acceptance (step 10.6.2)

Run from the repository root in a native graphical session with Python 3. System
dependencies or pinned FetchContent fallbacks:

```bash
cmake --preset ui-acceptance-release
cmake --build --preset build-ui-acceptance-release
ctest --preset test-ui-acceptance-release
```

For vcpkg, keep `VCPKG_ROOT` set for all three commands:

```bash
cmake --preset vcpkg-ui-acceptance-release
cmake --build --preset build-vcpkg-ui-acceptance-release
ctest --preset test-vcpkg-ui-acceptance-release
```

Both UI test presets select the exact `ui` label, show failures and fail on an empty
test selection. `NUMTILO_BUILD_UI_ACCEPTANCE` defaults OFF and requires
`NUMTILO_BUILD_APP=ON`, `BUILD_TESTING=ON`, Python and a native build. The worker and
QA output are excluded from Runtime installation/packages. Ordinary desktop and
headless presets explicitly retain the graphical option OFF.

The worker uses the current five SFML scenes and production Inter font. The
standard-library-only runner validates a fresh schema/coverage report, output-path
containment and PNG signatures, dimensions and all chunk CRCs. Evidence is under
`build/<configure-preset>/ui-acceptance/`: `ui-acceptance.json`, `worker.log` and
102 1×/2× PNGs. The recorded step 10 system Release run passed **772 checks** with 20 input
control/density records and 20 independently checked visual control/density records.
It requires no historical ignored harness/baseline input or UI framework. Success
is retained in `build/ui-quality-step-10.6/closure-focus-ctest.log`, which passes all
four then-current tests in 21.39 s; the earlier UI-only success is retained in
`ui-expanded-ctest.log`. Failed attempts remain diagnostics.

Step 11.2 adds five controlled held-key/text/blur cases (59 prepared assertions)
to the maintained worker and requires their exact `input_edges` matrix in the
runner. The current worker and application compile; the added cases have not
executed during preparation. At the explicitly authorized 11.4 checkpoint, the
fresh worker passes **831 checks / 102 images**, including the five rows totaling
59 checks (`build/ui-edge-step-11.4/ui/ui-acceptance.json`). The image matrix is
unchanged; 772 remains historical step 10 evidence, not the expanded worker result.

Run the unchanged core units, portable bridge cases and isolated platform acceptance
in the same desktop build with:

```bash
ctest --test-dir build/ui-acceptance-release -C Release -L '^(unit|platform)$' --output-on-failure --no-tests=error
```

Use the analogous vcpkg build directory for that configuration. Local core evidence
is 255 cases / 282,023 assertions; portable accessibility evidence is eight cases /
456 assertions, using a test-local fake platform without AppKit or a display. Five
native platform scenarios cover isolated resource/profile behavior.

The graphical report covers controlled rendering, scene input and public outcomes.
It does not certify native mouse delivery, screen-reader speech, millisecond input
latency, complete IME/caret editing, font fallback or complex shaping. Separate
actual macOS VoiceOver and native pointer/keyboard acceptance close 10.6.3/10.6.4
on 2026-10-14. The final native oracle passes 233 checks over seven closed runs /
473 trace records: 214 base checks plus 19 final-focus checks. The base tour has six
runs / 430 records, six physical mouse command pairs, nine VoiceOver action targets
plus one directional board action, and six reopened profiles. It checks Name
validation/Turkish readback, board values, Sound, populated ranking/Back and both
outcome actions. The final focus-refresh hook has separate repeated-click proof:
three clicks with VoiceOver off and two with it on retain the Name text field;
VoiceOver reads the required field. The original second-click `AXWindow` failure is
retained as a negative control. Base revision scope and final hook proof are recorded
separately in [the quality audit](ui-quality-audit.md); the base tour is not relabeled
as a full rerun of the hook.

AX-tree inspection is not VoiceOver operation, and indexed activation is not proof
of SFML mouse press/release. Windows/Linux native AT, broader IME/shaping and hosted
verification remain step 12. Step 11.1's inventory, 11.2's input/window preparation
and 11.3's transition/record preparation are in [the step 11 plan](ui-edge-case-plan.md).
Step 11.4's automated gates and fourteen controlled App combinations passed. The
user reported all six E11-03 physical-keyboard manual steps passed; the checkpoint
and step 11 are closed within the recorded local scope. Real profiles/
runtime files are preserved and VoiceOver, clipboard use and QA processes are
restored. Native report: `build/ui-quality-step-10.6/native/final-native-acceptance.json`.

The independent fresh headless acceptance retains the full include/link/symbol/
runtime audit. Exactly two existing pure standard/core UI contract headers are
allowed, without exempting their transitive dependencies. Object evidence follows
the actual compiler output, including nested CMake 4 dependencies. A fresh CMake
4.0.2 macOS Release audit passes all stages with 145 translation units and the full
core suite: `build/ui-quality-step-10.6/headless-final/headless-213sfez0/report.json`.
Its 17 native verifier controls, including positive and forbidden-dependency
controls, pass. The combined Python control suite reports 27 tests: 26 executed
passes and one explicit skip because Ninja is unavailable for the multi-config
control; see `build/ui-quality-step-10.6/closure-python-tests.log`. To run the
independent verifier controls when that checkpoint is authorized:

```bash
python3 -m unittest discover -s scripts/tests -p test_verify_headless.py -v
```

### Step 11.2/11.3 preparation and execution boundary

The opening paragraph records the original step 11.2/11.3 preparation checkpoint;
the current preparation contract is stated separately below it.

The maintained worker covers E11-01 at the controlled scene/guard layer. The
versioned [fixture preparer](../scripts/prepare_ui_edge_fixture.py) and
[helper](../tests/ui_edge_fixture.hpp) prepare E11-02 current-App dispatch and
E11-03 native lifecycle recording. Step 11.3 extends the same fixture with twelve
E11-04/E11-05/E11-06 final-move/close, navigation ordering and failed-record retry
combinations. Product sources, targets and dependencies are unchanged. Preparation
only compiles; it never launches a worker.
Current build metadata is supported for the local Makefiles build.

For current preparation, the preparer reads `CMakeFiles/numtilo.dir` metadata and
names the separate macOS QA bundle `NumtiloEdge112Lifecycle.app`, with identifier
`local.codex.numtilo-edge112.lifecycle`. `--build-dir` and `--output` are required.
Without `--compile` it generates the fixture sources and preparation record only;
`--compile` also compiles/links, and neither mode launches the fixture.

Current source-only preparation example, checked against source but not executed for this
documentation update: run from the repository root on a native Makefiles host with system
dependencies already installed/discoverable. Both example directories must be fresh and
unused; choose other fresh paths if either already exists.

```bash
cmake --preset release -G "Unix Makefiles" -B build/numtilo-edge-release -DBUILD_TESTING=OFF
cmake --build build/numtilo-edge-release --config Release --target numtilo
python3 scripts/prepare_ui_edge_fixture.py --build-dir build/numtilo-edge-release --output build/numtilo-edge-prepared
```

The explicit build path follows the `-B` override; a build preset would retain its
preset-owned directory. The final command omits `--compile`, so it creates no fixture
executable/bundle and launches nothing.

Rebuild `numtilo` before generating/compiling, and use a fresh output below
`build/`. Runtime byte checks cover three named sources, not every linked object;
regenerate/recompile after 11.3 or any relevant change. Eventual execution requires
an explicit case and a fresh isolated profile/output. E11-02 bypasses native
callbacks and `takeActions`; its deliberate fixture stop is not OS-close proof.
E11-03's App semantic selection is not native AX focus evidence, and exit 0 is
only a recording pending native review. See the [checkpoint recipe](ui-edge-case-plan.md#e11-02e11-03-recipe-for-the-explicitly-approved-114-checkpoint)
for inputs, expected state and evidence requirements. Do not run these commands
until the user's explicit 11.4 approval.

That approval boundary and the linked recipe belong to the original 11.4 checkpoint;
its results below retain the original fixture identities and proof paths.

The transition/record modes preserve the actual App event-loop/Closed body while
supplying a controlled event group. They verify actual animation state and separate
original in-memory completion from durable disk publication/reopen. The failed-save
wrapper uses the existing storage interface and delegates recovered saves to the
production writer; it is not an actual filesystem-permission fault. Provider
teardown observations do not by themselves invoke a retained native callback.
Those limits and all twelve mode IDs are in the [11.3 preparation record](ui-edge-case-plan.md#113-transitionrecord-preparation).
The retained-callback fake-platform test remains separate component coverage in
the existing bridge gate. In the explicitly authorized 11.4 run, all fourteen
controlled App modes pass with 375 assertions. Native focus-loss/cancellation,
fresh New, actual pointer Sound and normal close provide partial E11-03 evidence;
Quartz posted-key state differs from the SFML physical-key query, and minimization
was not confirmed by automation. On 2026-10-14 the user reported all six manual
steps worked without problems, closing E11-03 through separate user-reported manual
acceptance. Raw partial/failed automation reports remain unchanged. No source/oracle
fix, new automation or additional test execution followed that confirmation.

Fresh unit/platform, Python and headless results are recorded in the
[11.4 execution and manual closure record](ui-edge-case-plan.md#114-execution-record--automated-and-manual-evidence).
Python has one explicit Ninja environment skip. All executable QA windows are
closed, and production/test sources and actual user data are preserved.

## Headless presets (step 6.4)

All four headless configure presets set `NUMTILO_BUILD_APP=OFF` and `BUILD_TESTING=ON`.
Choose the dependency source and configuration, then run its matching configure, build and
test presets:

| Configure | Build | Test | Build directory |
| --- | --- | --- | --- |
| `headless-debug` | `build-headless-debug` | `test-headless-debug` | `build/headless-debug` |
| `headless-release` | `build-headless-release` | `test-headless-release` | `build/headless-release` |
| `vcpkg-headless-debug` | `build-vcpkg-headless-debug` | `test-vcpkg-headless-debug` | `build/vcpkg-headless-debug` |
| `vcpkg-headless-release` | `build-vcpkg-headless-release` | `test-vcpkg-headless-release` | `build/vcpkg-headless-release` |

```bash
cmake --preset vcpkg-headless-debug
cmake --build --preset build-vcpkg-headless-debug
ctest --preset test-vcpkg-headless-debug
```

System presets discover installed JSON/Catch2 or use the pinned FetchContent fallbacks. vcpkg
presets use the feature selection from step 6.3. Keep `VCPKG_ROOT` set through configure, build
and test: vcpkg presets are unavailable in each phase when it is unset or empty. Each preset
has a separate build directory; no generator, compiler or triplet is imposed. Debug/Release
configuration is explicit for configure, build and test, including multi-config generators.

The shared test policy selects only labels matching `^unit$`, prints failures, and uses
`noTestsAction=error` so missing registrations fail. Preset schema 5 and the declared CMake
3.24 minimum are aligned. Native macOS runtime/minimum-version validation is recorded in
the step 6.4 evidence below; earlier step 6.2/6.3 results describe their own checkpoints.
Step 6.6 adds separate CI jobs using these vcpkg headless presets; its configuration gate and
actual validation scope are described below.

## Headless dependency acceptance (step 6.5)

The repository verifier requires Python 3, CMake 3.24+, a native GNU/Clang compiler and a
Makefiles/Ninja generator. It uses `nm` or `llvm-nm` for archives and macOS `otool` or Linux
`ldd` for the unit executable. Visual Studio, Xcode, MSVC and cross-compilation are outside
its supported contract; the native runs recorded below are macOS arm64.

From the source root, run Debug and Release with the full unit suite, then a backend-only
Release audit:

```bash
python3 scripts/verify_headless.py --source . --output build/headless-acceptance --config Debug
python3 scripts/verify_headless.py --source . --output build/headless-acceptance --config Release
python3 scripts/verify_headless.py --source . --output build/headless-acceptance --config Release --no-tests
```

Use `--dependency-prefix /absolute/package/prefix` for existing JSON/Catch2 packages. Without a
prefix or toolchain, normal discovery and pinned fallbacks remain available. For vcpkg, use
the toolchain explicitly; each verifier invocation owns a fresh installed tree:

```bash
python3 scripts/verify_headless.py --source . --output build/headless-acceptance --config Debug --toolchain "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

Repeat with `--config Release`, and add `--no-tests` for the JSON-only backend configuration.
Optional `--triplet`, `--generator`, `--cmake`, `--jobs` and per-command `--timeout` select host
tools/resources. Extra configure arguments use repeated `--cmake-arg=-DNAME=value`; the verifier
enforces its application/test/configuration/audit options after those arguments. Run
`python3 scripts/verify_headless.py --help` for the complete CLI. The verifier's Python requirement
does not introduce Python package discovery into headless CMake.

Each invocation creates `build/headless-acceptance/headless-<unique>/`, leaving existing build
directories and dependency prefixes intact. Its schema-version-1 `report.json` records inputs,
source/tool hashes, scope and seven stages:

| Stage | Required evidence |
| --- | --- |
| `configure` | Native toolchain, selected configuration/options, loaded guard and complete CMake File API response |
| `dependency-and-target-graph` | Nine static backend targets; raw imported include/link/location properties; no forbidden discovery, targets, Runtime/CPack output or vcpkg ports |
| `build` | Successful full default-target build |
| `actual-headers` | Built objects and real compiler `-M` scans for every compiled target, including system headers and canonical paths |
| `archive-symbols` | Undefined-symbol inspection of all nine backend archives |
| `unit-tests` | Exactly the audited unit executable with the `unit` label and successful execution, or an empty CTest index with tests disabled |
| `runtime-dependencies` | Unit executable and resolved non-system native dependencies, or explicit inapplicability with tests disabled |

Individual arguments/return codes and stdout/stderr remain under `evidence/`, alongside dependency
events, imported-target properties, header dependency files/index and runtime records. Failures
exit nonzero and retain a failed stage/report. Missing header evidence or imported targets outside
the inspectable scope fail acceptance. The guard rejects both package discovery and forbidden
FetchContent population; it does not substitute mock JSON/Catch2 packages.

Tests-disabled runs prove compilation, target/import metadata, headers and archive symbols;
they do not link a separate consumer or provide a runtime dependency test. macOS system framework
transitive dependencies are not traversed. Fresh build/install trees can reuse a dependency prefix,
pinned source checkout or shared downloads/binary cache; this is not an offline or clean-machine
claim. Step 6.6 integrates this audit into Linux/macOS Release CI; step 6.7 records the completed
native macOS desktop/package regression below.

## Headless CI and portable configuration check (step 6.6)

CI defines six `Headless Build and Test (<runner>, <configuration>)` checks: Linux, macOS and
Windows, each in Debug and Release. They select the matching `vcpkg-headless-*` configure,
build and test presets with CMake 3.29.6 and Python 3.12. Configuration loads the existing
dependency provider and then runs `scripts/check_headless_configuration.py` before building.
Linux/macOS install ccache; the jobs do not add graphics/audio development packages. Windows
uses Visual Studio 2022 x64, with the same configuration check and complete unit suite.

The portable checker reads the actual CMake cache and provider evidence. Its five stages
validate application `OFF`, the requested test gate, native source/build identity, loaded
dependency guard, package/fallback requests, all nine static backend targets, complete raw
target/imported-property closure, installed vcpkg ports and absence of desktop/package output.
Missing, corrupt or incomplete evidence fails. With vcpkg active, JSON must be installed and
Catch2 presence must match the test option. SFML, fmt and OpenAL ports, including `openal-soft`,
are rejected even when unlinked. It also rejects managed-feature bypasses. This check requires
no compiler/header/symbol/runtime inspection and does not execute tests; the subsequent build
and CTest preset supply those separate results. It supports the native MSVC configuration
that the full dependency verifier does not support.

For a local vcpkg configuration check, keep `VCPKG_ROOT` set and run:

```bash
cmake --preset vcpkg-headless-debug \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="$PWD/cmake/VerifyHeadlessDependencies.cmake" \
  -DHEADLESS_AUDIT_DIR="$PWD/build/vcpkg-headless-debug/headless-evidence"
python3 scripts/check_headless_configuration.py \
  --build build/vcpkg-headless-debug \
  --evidence build/vcpkg-headless-debug/headless-evidence \
  --output build/vcpkg-headless-debug/headless-evidence/configuration-report.json
cmake --build --preset build-vcpkg-headless-debug
ctest --preset test-vcpkg-headless-debug
```

Release uses the matching `release` presets/directories. A direct application-disabled,
tests-disabled configuration can use the same provider and checker with `--no-tests`.
`--build` and `--evidence` must match the actual configured directories. The optional JSON
output includes scope, successful/error stages and the failure reason, and the process exits
nonzero on failure. Do not treat this configuration report as the full step 6.5 acceptance.

All three Release matrix jobs run the portable checker's regression controls. Linux/macOS
Release also run the full verifier's native integration controls and two fresh vcpkg audits:
Release with tests `ON`, then Release with `--no-tests`. These use Unix Makefiles and retain
the full verifier's existing compiler/header/symbol/runtime boundary. Windows/MSVC is excluded
from these full audits. The test-disabled audit still does not link a separate backend consumer.

Linux sanitizer and coverage jobs use the guarded `vcpkg-headless-debug` preset in
`build/sanitizers` and `build/coverage`, run the portable checker, build `core_unit_tests`, then
select only `^unit$` with `--no-tests=error`. ASan/UBSan instrumentation and the gcovr 80% line
threshold for `src/core` are retained. Their dependency installation no longer includes desktop
features or graphics/audio development packages. Existing desktop and static-analysis jobs keep
their names and desktop scope; built/installed platform acceptance and the release workflow are
preserved.

Headless compiler-cache and vcpkg-archive namespaces are separate from desktop caches. Matrix
jobs upload provider/configuration reports, CMake caches, installed-port state, CTest/JUnit
results and available full-audit evidence with `always()`. Sanitizer and coverage jobs also
retain their configuration and unit-test evidence after failure. A failed early stage can leave
only partial evidence; artifact upload does not turn it into a successful check.

Run the portable checker controls separately from the full native verifier controls:

```bash
python3 -m unittest discover -s scripts/tests -p test_check_headless_configuration.py -v
python3 -m unittest discover -s scripts/tests -p test_verify_headless.py -v
```

Both local branch-protection profiles include the six new matrix check names and the seven
existing contexts. This updates the template only; applying GitHub branch protection is a
separate operation documented in [maintenance.md](maintenance.md#required-ci-checks).

## Manual headless commands

Configure directly into a new directory without a vcpkg toolchain. JSON and Catch2 can come
from installed packages or the existing pinned FetchContent fallbacks. Manual examples use
`build/manual/` to avoid reusing preset-owned build directories:

```bash
cmake -S . -B build/manual/headless-debug -DCMAKE_BUILD_TYPE=Debug -DNUMTILO_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/manual/headless-debug --config Debug
ctest --test-dir build/manual/headless-debug -C Debug --output-on-failure -L unit
```

The explicit configuration arguments also support multi-config generators. For a backend-only
Release build, disable both the application and test executables:

```bash
cmake -S . -B build/manual/headless-backend -DCMAKE_BUILD_TYPE=Release -DNUMTILO_BUILD_APP=OFF -DBUILD_TESTING=OFF
cmake --build build/manual/headless-backend --config Release
```

For a vcpkg-managed headless build on macOS/Linux, select the toolchain explicitly in a separate
directory; application/test options select its manifest features before installation:

```bash
cmake -S . -B build/manual/headless-vcpkg-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DNUMTILO_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/manual/headless-vcpkg-debug --config Debug
ctest --test-dir build/manual/headless-vcpkg-debug -C Debug --output-on-failure -L unit
```

With tests disabled CMake does not discover/fetch Catch2, discover acceptance Python or create
test executables. With the application disabled it does not discover SFML/fmt/OpenAL or create
the executable, asset-sync target, Runtime-install rules, bundle-fixup scripts or CPack targets.
Step 6.3 aligns vcpkg dependency features with those gates. Initial acquisition may need network
access; completed native macOS dependency validation is recorded below. Headless presets are
implemented in step 6.4; the step 6.5 verifier adds reproducible dependency audits. Step 6.6
adds the separate headless CI matrix and moves sanitizer/coverage configuration to headless.

Reconfiguration in the same directory also supports changing the options. Tests `ON` to `OFF`
regenerates an empty CTest index, removing previous registrations even if the generator keeps
an empty test utility target. Application `ON` to `OFF` clears only project-generated CPack
and asset-install/bundle-fixup configuration files; existing executables, assets and ZIPs remain
intact. Use separate directories for clean dependency evidence.

## Step 6.2 validation (macOS, 2026-10-07)

All four application/test combinations configured and completed a full build in separate
project build directories:

| Application | Tests | CTest result |
| --- | --- | --- |
| `ON` | `ON` | `2/2`: unit suite and native platform acceptance |
| `ON` | `OFF` | No registered tests |
| `OFF` | `ON` | `1/1`: unit suite |
| `OFF` | `OFF` | No registered tests |

Both test-enabled configurations passed **249 cases and 281,933 assertions**. An independent
Release headless build with `BUILD_SHARED_LIBS=ON` passed the same suite and retained all nine
backend targets as static libraries. Compile commands, dependency files, link commands and
native binary dependencies contained no SFML/fmt/OpenAL dependencies. Session/coordinator
sources belonged only to `app_logic`. A configure-time dependency guard and trace verified
that headless builds did not discover SFML/fmt/OpenAL/Python, and test-disabled builds did not
discover Catch2 or acceptance Python.

Default options remained `ON`; deliberately disabling SFML discovery with the application
enabled produced the required configure failure. Unavailable Python kept the unit test and
reported that native acceptance was omitted. Headless target/generated-file inspection found
no desktop executable, asset-sync, acceptance worker, Runtime install or CPack/bundle-fixup output.
An explicit `Development` install produced only `libgame_core.a`.

Same-directory test toggles registered `0/1/0` tests across `OFF/ON/OFF`. Application toggles
created and removed desktop/package targets and Runtime rules correctly while preserving
existing binary, asset and archive sentinels. The installed desktop Runtime passed all five
native acceptance cases; the extracted ZIP passed those five plus read-only LaunchServices
launch acceptance. Packaged font/sound hashes and strict deep code-signature verification
passed. The first LaunchServices waiter reported a fast-exit error; its report was retained
and one bounded retry in a fresh directory passed without changing the acceptance harness.

UBSan with coverage passed the full suite; session/coordinator compilation flags and generated
coverage data confirmed that `app_logic` was instrumented. ASan/UBSan/coverage compilation and
linking also succeeded, but **ASan execution is unverified on this host**: the local Apple Clang
runtime stalled before `main`, reproduced by an empty standalone ASan executable. Independent
UBSan-only and coverage-only smoke executables exited successfully. The standalone timeout and
process sample are retained with the evidence.

Project caches/builds were fresh; the matrix reused pinned JSON and a separately built C++20
Catch2 package, while the independent Release check reused pinned source checkouts for
FetchContent. This validates target separation without claiming offline acquisition or a
machine without SFML installed. Logs, commands and results are under ignored
`build/step-6.2-validation/` and `build/step-6.2-independent-headless-release-shared/` directories.
Native Windows/Linux results and separate headless CI remain pending; steps 6.3-6.7 retain their
dependency, preset and broader regression responsibilities at this historical checkpoint.

## Step 6.3 dependency validation

Independent real-vcpkg dry-run dependency plans passed for all four combinations and standalone
manifest defaults on `arm64-osx`, using the pinned baseline. All four configurations then installed
their dependencies and completed full Debug builds in fresh build and install directories:

| Application | Tests | Planned / installed ports, including helpers | Configure / full build | CTest |
| --- | --- | --- | --- | --- |
| `ON` | `ON` | 19 / 19 | Passed | `2/2`: unit and native platform acceptance |
| `ON` | `OFF` | 18 / 18 | Passed | No registered tests |
| `OFF` | `ON` | 4 / 4 | Passed | `1/1`: unit suite |
| `OFF` | `OFF` | 3 / 3 | Passed | No registered tests |

Standalone defaults matched `ON/ON`. The plans resolved SFML `2.6.2`, Catch2 `3.7.1`, JSON
`3.12.0#2` and desktop fmt `12.1.0`; neither headless graph contained graphics/audio dependencies.
Plan commands and parsed package/version lists are retained in ignored
`build/step-6.3-independent-plans/dependency-plans-summary.json`.

Both test-enabled configurations passed **249 cases and 281,933 assertions**; the desktop build
also passed all five build-mode platform acceptance cases. Headless installed trees contained
exactly JSON, optional Catch2 and `vcpkg-cmake`/`vcpkg-cmake-config`. Their compile commands had
no SFML/fmt/OpenAL dependencies. Every configuration used imported packages from its own
`vcpkg_installed` tree, with no FetchContent or `_deps` fallback. JSON/Catch2 were built from
source during the first headless installation; desktop installation built the remaining ports,
and later fresh directories reused the binary cache. The installed trees and project caches
were fresh regardless of binary-cache reuse. Actual commands/results and per-configuration
dependency checks are under `build/step-6.3-validation/`, including
`clean-build-results.json` and `desktop-build-results.json`. The final CMake source also
reconfigured all four combinations successfully; consolidated evidence is in
`build/step-6.3-validation/acceptance-summary.json`.

Independent toolchain-entry capture probes verified all four feature lists before the first
`project()` call, same-cache transitions and correction of conflicting known cache/normal
variables. Unknown features, direct feature/no-default-feature overrides and both feature-bearing
and neutral response files failed before the toolchain ran. Mixed-case feature/no-default-feature
options were also rejected; unrelated direct installer arguments retained their original spelling.
These capture probes did not install packages; their commands, current CMake
source hash and final results are in
`build/step-6.3-independent-plans/early-feature-probes-final-summary.json`.

Reconfiguration recomputes the requested features. Automatic manifest installation synchronizes
the configured installed tree and may remove unused packages; shared download/binary caches
and earlier application outputs have separate lifetimes. Dependency-absence checks use new
build/install directories.

An independent actual-vcpkg headless round trip in one build directory passed with tests
`OFF → ON → OFF`: installed package counts were `3 → 4 → 3`, CTest registrations `0 → 1 → 0`,
and selected features empty → `tests` → empty. Manifest defaults remained suppressed. Enabling
tests installed Catch2 `3.7.1` and passed the full unit suite; disabling them removed Catch2
from that installed tree while retaining the earlier unit executable. A fatal dependency guard
blocked SFML/fmt/OpenAL/Python project discovery throughout the round trip. Package imports
came from the installed tree without FetchContent. Commands, source hash and results are in
`build/step-6.3-independent-toggle/evidence/actual-toggle-summary.json`.

Installed Runtime, ZIP and LaunchServices acceptance were not rerun in step 6.3; their earlier
step 6.2 evidence remains historical, along with the local ASan execution limitation. Renewed
package regression belongs to step 6.7. At the step 6.3 checkpoint, dedicated presets, headless
CI and native Windows/Linux dependency/build evidence remained separate work.

## Step 6.4 preset validation

All four headless configure/build/test chains passed on native macOS arm64 with CMake **4.0.2**
and Unix Makefiles, using separate fresh project caches. Every Debug/Release chain passed its
`1/1` unit entry with **249 cases and 281,933 assertions**. Each cache selected application
`OFF`, tests `ON` and the `tests` manifest feature; no desktop/package output or FetchContent
fallback was created. The system profiles reused pinned JSON/Catch2 through a validation-only
`CMAKE_PREFIX_PATH`. vcpkg profiles had fresh installed trees containing only JSON, Catch2 and
the two vcpkg CMake helpers, restored through the shared verified binary cache. This was not
an offline acquisition test.

An independent verified CMake **3.24.4** runner parsed schema 5 and checked the full catalog.
Unset/empty `VCPKG_ROOT` hid vcpkg configure/build/test presets; nonempty values enabled them.
Disabled vcpkg entry points refused configure/build/test before creating a build directory.
The minimum runner then completed actual Ninja Multi-Config configure/build/test chains for
`headless-debug`, `headless-release` and `vcpkg-headless-release`. Each passed the full unit
suite. Build/test commands selected the correct `Debug/` or `Release/` executable; the minimum
vcpkg Release run created a fresh four-port installed tree and imported its packages without
fallback. A fatal project dependency guard was active against SFML/fmt/OpenAL/Python discovery;
no such calls occurred in that headless run. Inspection found no desktop/asset/worker/package
targets or CPack files.
Minimum-version execution covered these three chains; all four were executed on CMake 4.0.2.

A deliberately failing platform-labelled test injected only into an owned generated CTest index
was excluded by the unit filter. Empty unit selections failed with exit code `8`, including
with an earlier unit executable still present; restored configurations passed again. Existing
desktop preset names/directories and previous preset settings were checked, with application/tests
explicitly `ON`. Application/backend sources, CMake target definitions, the manifest and CI
workflows were unchanged by this preset step.

Root commands/results are under ignored `build/step-6.4-validation/`, including
`preset-matrix.json`, `commands.json` and `acceptance-summary.json`. Independent minimum-version,
condition and multi-config results are in `build/step-6.4-independent/independent-6.4-summary.json`,
with detailed summaries in `schema-condition-evidence/`, `minimum-multiconfig-evidence/` and
`minimum-vcpkg-evidence/`. The independent snapshot's CMake source/preset hashes matched the
production files; it did not modify those files.

Native Windows/Linux execution, renewed sanitizers/coverage, desktop installed/archive
acceptance and separate headless CI were not performed in this preset checkpoint. Earlier
step 6.2 ASan limits remain unchanged. Steps 6.5-6.7 retain their broader dependency, CI and
regression responsibilities.

## Step 6.5 dependency validation

The final repository verifier completed all seven stages for six fresh native macOS arm64
builds with CMake **4.0.2**, Apple Clang and Unix Makefiles:

| Dependencies | Configuration | Tests | Configure / build / audits | CTest / runtime inspection |
| --- | --- | --- | --- | --- |
| Existing package prefix | Debug | `ON` | Passed | `1/1`; unit executable inspected |
| Existing package prefix | Release | `ON` | Passed | `1/1`; unit executable inspected |
| Existing package prefix | Release | `OFF` | Passed | Empty index; runtime inapplicable |
| vcpkg | Debug | `ON` | Passed | `1/1`; unit executable inspected |
| vcpkg | Release | `ON` | Passed | `1/1`; unit executable inspected |
| vcpkg | Release | `OFF` | Passed | Empty index; runtime inapplicable |

Every test-enabled run passed **249 cases and 281,933 assertions**. Header evidence covered
all **38** compiled translation units in each test-enabled run and **18** in each tests-disabled
run, using actual compiler `-M` commands including system headers. All nine backend targets
remained static and passed archive-symbol inspection. Generated metadata and raw imported-target
properties contained no SFML/fmt/OpenAL dependency; no desktop/assets/worker/Runtime/CPack output
was produced. The dependency guard was active against forbidden discovery/fallback requests;
none occurred in the successful project runs. Test-disabled runs made no Catch2 discovery and
had no test executable or registered tests.

vcpkg installed trees contained exactly JSON, optional Catch2 and the two CMake helper ports:
four packages with tests and three without, with no SFML/fmt/OpenAL or FetchContent fallback.
System runs reused an existing pinned JSON/Catch2 prefix; vcpkg runs owned fresh installed trees
and reused the shared binary cache. These are fresh project cache/build/install checks, not
fresh dependency acquisition. Final reports and complete commands/logs are under ignored
`build/step-6.5-validation/final-{system|vcpkg}-{debug|release|release-no-tests}/headless-*/`.
Every final report's verifier/guard hashes matched the current files. Consolidated results,
commands, independent references and scope limits are in
`build/step-6.5-validation/summary.json`.

An independent CMake **3.24.4** harness used source fixtures with desktop main/App/SoundManager/UI,
packaged assets, platform worker and desktop CMake helpers omitted. The tests-enabled fixture
passed Debug and Release, each with the full unit suite; a second fixture omitted the entire
tests directory and completed a Release backend-only build with an empty test index. Their
CMake source matched the production source. A forced pinned FetchContent fallback with
`BUILD_SHARED_LIBS=ON` also passed: the nine backend libraries remained static while shared
Catch2 libraries and their native dependencies were inspected. These independent checks reused
existing package prefixes or exact pinned source checkouts and did not perform vcpkg installation.
Their commands, manifest and results are in
`build/step-6.5-independent/independent-boundary-summary.json` and `fixture-manifest.json`.

The persistent repository verifier was also exercised with minimum CMake and Ninja Multi-Config.
A tests-disabled Release source-omission run scanned its 18 built objects, correctly selecting
Release from a compile database that also listed unbuilt configurations. A tests-enabled shared
fallback run scanned **144** translation units, including **106** Catch2 sources, passed the
full unit suite and inspected the executable plus both local Catch2 dylibs. The runtime audit
resolved inherited executable RPATHs and excluded a dylib's own identity from dependency recursion.
Results are in `build/step-6.5-independent/persistent-tool-crosscheck-summary.json` and
`persistent-tool-shared-fallback-summary.json`; these are native macOS evidence only.

All **12 native integration controls** passed without skips, failures or errors. Rerun them with:

```bash
python3 -m unittest discover -s scripts/tests -v
```

They exercise a clean backend positive case and forbidden quiet discovery/FetchContent,
Catch2 discovery with tests disabled, hidden/scoped imported dependencies, inline-only helper
headers, system includes, header symlinks, missing compiler evidence, direct audio framework
links and explicit cross-toolchains. Guard-specific fixtures also verify allowed requests,
forbidden requests, native/cross metadata and imported-target closure; evidence is under
`build/step-6.5-guard-fixtures/`. CMake target definitions, presets, the manifest, CI workflows
and all 81 application/C++ test files were unchanged by this acceptance-tooling step.

Native Windows/Linux execution, Visual Studio/Xcode/MSVC validation, offline acquisition, removal
of host desktop packages, macOS system framework transitive closure and a final linked consumer
for tests-disabled builds are not established by this checkpoint. No renewed sanitizer or desktop
package acceptance was performed; the earlier step 6.2 ASan execution limitation and package
results retain their historical scope. Headless CI and renewed package regression remain
steps 6.6 and 6.7.

## Step 6.6 CI validation

Native macOS arm64 validation used the exact CI CMake version, **3.29.6**, with Apple Clang.
Debug and Release ran the vcpkg headless configure presets into fresh isolated `-B` trees with
the dependency provider active, passed the portable configuration check, completed the build
and ran the matching CTest presets with an explicit `--test-dir` override. Each passed
**249 cases and 281,933 assertions**. The configured options were application `OFF`, tests
`ON`, and the `tests` manifest feature; installed trees contained only the permitted headless
ports. The pinned baseline checkout and shared verified vcpkg binary archives were reused.
ccache was unavailable locally, so only the compiler-cache launcher was omitted from these
local runs; hosted POSIX jobs still configure it.

Two additional fresh full-verifier runs passed all seven stages using CMake 3.29.6, vcpkg and
Release, with tests enabled and disabled. The test-enabled audit passed the same full unit
suite and inspected its native runtime dependencies. The backend-only audit required an empty
CTest index and retained the existing limit that no separate final consumer was linked.
Both retained header, target/import, archive-symbol and package evidence.

Official actionlint **1.7.12** passed the CI and release workflows. The existing desktop and
static-analysis job blocks, release workflow, CMake target/preset definitions and C++ application/
test sources were unchanged by step 6.6. The new checker/control implementation is recorded
alongside the local matrix and full-audit results under ignored `build/step-6.6-validation/`:
`native-summary.json`, `final-full-audits/summary.json` and the aggregate `summary.json` retain
exact commands and scope. The final portable checker passed **10 controls** and the full verifier
passed **13 controls**, with CMake 3.29.6 and Ninja 1.13.2; neither suite skipped a control.
The final checker also passed six existing native configurations. Sanitizer/coverage configurations
passed their guarded configuration checks and generated the requested instrumentation flags for
all 18 production translation units; those builds were not executed in this configuration check.

CI review identified the actual OpenAL vcpkg port name `openal-soft`. Both checkers now reject
its installed status even without a link dependency, and a real CMake fixture proves rejection
before building. The step 6.5 control counts above describe their historical checkpoint.

No push, pull request, hosted GitHub Actions run or remote branch-protection update was made
for this local implementation. The six hosted matrix jobs, Linux ASan/UBSan and coverage gates,
and native Linux/Windows execution remain unverified until their actual runners pass. The
workflow specifies Python 3.12; local validation does not certify that hosted environment.
No renewed desktop installed/archive acceptance was performed in step 6.6; earlier package
evidence retains its historical scope, and that regression remains step 6.7.

## Step 6.7 final build and package regression

Date: 2026-10-07. Native macOS arm64, Apple Clang and exact CMake **3.29.6**. All four
application/test combinations were rebuilt in both Debug and Release using eight fresh
project caches and vcpkg installed trees. The pinned vcpkg checkout and shared binary archives
were reused; this was not offline acquisition or a machine without desktop dependencies.

| Application | Tests | Debug and Release result | Manifest features / native installed ports |
| --- | --- | --- | --- |
| `ON` | `ON` | Full build; unit suite and all five build-tree platform cases passed | `desktop;tests` / 19 |
| `ON` | `OFF` | Full build; empty CTest index, no Catch2, acceptance Python or worker | `desktop` / 18 |
| `OFF` | `ON` | Full seven-stage dependency audit and unit suite passed | `tests` / 4 |
| `OFF` | `OFF` | Full seven-stage dependency audit; empty CTest index | None / 3 |

Each test-enabled build passed **249 cases and 281,933 assertions**. The desktop test-enabled
builds omitted both build-option flags during configuration and confirmed their `ON/ON`
defaults. All nine backend targets remained static, with session/coordinator sources owned
only by `app_logic`. The independent headless runs recorded forbidden-discovery, target/import,
actual header, archive-symbol and native runtime evidence, with runtime inspection explicitly
inapplicable in the test-disabled runs. Headless output had no desktop executable, assets,
platform worker, Runtime install or CPack output; native adapter dependencies remained valid.
Tests-disabled headless trees contained JSON and the two vcpkg CMake helpers, with no Catch2.
An explicit native Release backend-only installation probe produced no files for the `Runtime`
component; the optional `Development` component produced only `lib/libgame_core.a`. This verifies
component boundaries without linking or installing a separate backend consumer.

All four desktop builds were installed through the `Runtime` component and packaged as ZIPs.
Each installed runtime passed **5/5** native platform cases. Each actual extracted ZIP passed
**6/6**, including the read-only LaunchServices diagnostic, from Unicode/foreign paths.
Test-disabled runtime acceptance used the separate worker from the corresponding test-enabled
build; the tested package itself contained no acceptance worker. The production executable's
runtime check created no profile or window. Persistence scenarios used guarded temporary
profiles, and the actual user profile and project score/settings files remained absent or
unchanged as appropriate. These LaunchServices checks establish read-only native launch/path
behavior, not an unmodified production gameplay, rendering or audio-playback pass.

An independent initial layout audit passed all four desktop cases: every installed/extracted
bundle had seven byte-exact managed assets, valid deep strict ad-hoc signatures and one arm64
Mach-O executable whose loads resolved only to system libraries/frameworks. These vcpkg builds
link desktop dependencies statically, so the earlier system-package bundle's embedded-library
count does not apply. ZIP CRC, extracted bytes, symlinks/modes and exclusion of tests, workers,
profile data and build payload were checked. That initial audit preceded the final documentation
freeze; a refreshed final archive requires a separate strict metadata audit against the frozen
README/CHANGELOG. The initial layout pass does not establish that final documentation match.

A separate graphical smoke fixture also passed using copied current App/main sources with
instrumentation, compile-time sandbox profile roots and a copied installed Release bundle. It
rendered **544 frames**, decoded the real font and a glyph plus all five sounds, performed a
real merge with score **4**, toggled sound off and restarted. Best **4**, zero completed rounds,
sound-off persistence and an exact legacy-format backup were verified. The production source,
source bundle and protected project/user profiles remained unchanged. This exercises production
decoders, scenes and persistence through a modified isolated fixture; it does not certify an
unmodified production GUI, physical Finder interaction or audible playback.

Exact commands, CTest results, installed/extracted reports and hashes are retained under ignored
`build/step-6.7-validation/desktop/summary.json`; the independent headless reports and commands
are indexed by `build/step-6.7-independent/headless-matrix-summary.json`. The aggregate evidence
is collected in `build/step-6.7-validation/summary.json`. The installation probe is in
`build/step-6.7-validation/headless-install-probe/summary.json`; independent initial package
inspection is in `build/step-6.7-independent/packaging/initial-layout-evidence/report.json`, and
the graphical fixture report is `build/step-6.7-independent/gui/gui-xttu4t2g/report.json`.
Record final package refresh/audit evidence separately in the aggregate. Step 6.7 updates the current build,
architecture, platform and README contracts while retaining earlier checkpoints as historical
evidence. Same-directory option-toggle behavior and the minimum-CMake/multi-config/source-
omission controls remain established by their earlier dedicated checks, rather than by this
fresh-build matrix. Test-disabled backend audits still do not link a separate final consumer.

Phase 6 implementation and its native macOS regression are complete. No hosted GitHub Actions
run, push, pull request, release publication or remote branch-protection mutation is part of
this validation. Native Linux/Windows matrix and package results, hosted Linux ASan/UBSan and
the coverage gate still need actual runner evidence. Earlier local ASan execution limits remain
unchanged. Developer ID signing, notarization, quarantine and clean-machine distribution
acceptance remain separate work; local ad-hoc signing does not establish those properties.

## Test Categories

- Merge behavior:
  - `[2,2,2,2] -> [4,4,0,0]`
  - one-merge-per-tile rule (`[2,2,4,0] -> [4,4,0,0]`)
  - independent pairs (`[2,2,4,4] -> [4,8,0,0]`)
  - edge-first pairing of triples, including gaps
  - a merged tile becomes eligible to merge again on the next turn
- Move behavior:
  - no-op move does not mutate grid
  - no spawn on no-op
  - 24 hand-calculated line fixtures in all four directions at all four line positions
    (384 independent starting boards, tested through both `Board` and `Game`)
  - explicit right/down expectations, including directional differences for triples
  - two asymmetric full-board fixtures with eight hand-calculated grid/score outcomes
  - empty, sparse, packed, unequal, and post-2048 tile values
- Standalone board and tile behavior:
  - sliding and merging in all four directions
  - empty-cell placement and available-move detection
  - move records used by animations, including a stationary merge participant
  - empty tiles cannot merge
- Movement-record contracts:
  - six explicit trace fixtures in all directions and positions (96 combinations)
  - each original source is used at most once; each merge has exactly two equal sources
  - coordinates stay in bounds, on the same row/column, and move toward the requested edge
  - a stationary merge participant is recorded; a stationary non-merge tile is not
  - reconstructing the output from the input and records exactly reproduces the board
  - record content is compared without imposing vector order
- Generated-board properties:
  - 200 deterministic boards in all four directions preserve total tile value, have no gaps
    toward the target edge, and satisfy the full movement-record contract (800 moves)
  - a separate 200-board corpus checks horizontal/vertical reflection and transposition,
    including transformed movement/merge coordinates (2,400 comparisons)
  - tile count decreases by the merge count; score delta equals the sum of merged values
- Game-over detection:
  - dead board vs board with legal merge
  - every one of the 24 orthogonal neighbor pairs on a full board can permit a move
  - diagonal, flattened-row, and opposite-edge matches do not permit a move
  - a vacant cell at any of the 16 positions keeps a board playable
  - status is computed after spawning into the last vacant cell; a full board can still be playable
  - terminal no-op moves preserve the grid, score, records, and status
  - loading a playable board clears a previous terminal status
- Normal spawn invariants:
  - constructor/reset place exactly two distinct normal tiles, each valued `2` or `4`
  - a valid turn with spawning enabled adds exactly one normal tile to the post-move grid
  - the recorded spawn coordinates were empty immediately before spawning; all other cells stay equal
  - tile count increases by one and total value increases by the recorded spawn value
  - movement records and merge score are unaffected by spawning
  - six fixture boards, all single-hole positions, and 200 generated boards cover all four directions
  - 256 deterministic seeds check new rounds and resetting a dirty terminal round
  - no-op and spawn-disabled moves preserve the future RNG sequence against a control game
  - `Board::placeTile` rejects occupied cells for every position and leaves the entire grid unchanged;
    general placement also accepts merged values such as `2048`, beyond the normal spawn set
- Score behavior:
  - cumulative score across multiple moves
  - round score/status remain consistent after moves, loading, and reset
- Winning state:
  - `1024 + 1024` reaches `2048` in every direction and line position
  - `won` stays true after `2048 + 2048` becomes `4096`, including subsequent moves and no-ops
  - won rounds accept legal moves and normal spawning without a core Continue action
  - all four `won` / `gameOver` combinations, including both true after the last spawn
  - loaded values at or above `2048` qualify, at every board position
  - loading a different board derives new flags instead of retaining an earlier achievement
  - reset clears achievement and terminal state without changing seeded initial boards or RNG replay
  - `hasWon()` / `isGameOver()` agree with their corresponding state fields
- Golden deterministic snapshot:
  - fixed seed + fixed move sequence -> expected final grid, score, and move flags
- Score memory and round lifecycle:
  - an ongoing best creates no completed game; lower/equal/negative updates cannot lower it
  - all completed rounds are retained; ranking snapshots cannot mutate their insertion order
  - duplicate IDs with identical or conflicting payloads do not change best/history/dirty
  - failed save and repeated completion preserve one entry; successful save clears dirty
  - v2 roundtrips preserve more than five scores, metadata and round IDs
  - unsupported/corrupt loads preserve memory and prevent overwriting the original file
  - independent best survives v2 save/reload without creating a completed game
  - construction/restart create unique IDs; moves, metadata changes, and acknowledgement retain IDs
  - session identity generation does not alter seeded tiles, move results, score, or flags
  - acknowledgement before logical game-over is ignored; restart permits a new completion

- Versioned profile validation and migration preparation:
  - required fields, integer score limits, nonempty unique IDs, and best/history consistency
  - unknown integer versions are distinguished from invalid data and I/O failures
  - unknown v2 fields, duplicate JSON keys, ambiguous versionless roots, and trailing data are rejected
  - failed loads preserve both clean and dirty memory, including a pending legacy migration
  - legacy entries retain original order, names and timestamps; IDs are deterministic per source position
  - identical legacy payloads are separate historical games, and no history is trimmed to five
  - migration preparation and blocked saves leave the original source bytes unchanged
  - invalid UTF-8 serialization fails before the destination file is opened

- Atomic storage and retry:
  - partial write, sync, close and publication failures keep the original source intact
  - failed saves retain dirty, migration flags and one in-memory completed record
  - retry uses the same snapshot and publishes it without another history append
  - CRLF legacy sources are backed up byte for byte; existing matching backups are reused
  - conflicting backups and externally changed sources cannot be overwritten
  - unique exclusive temp creation preserves unrelated stale files and rejects symlink sources
  - successful migration clears dirty/pending and reopens as v2 with stable IDs

- Application score coordination:
  - ongoing best is saved immediately without adding completed history
  - terminal best/history share one snapshot, with acknowledgement proven before the storage call
  - an existing history ID acknowledges the session without changing its original metadata
  - failed terminal saves retry the identical snapshot across restart without duplicate history
  - early departure persists best and preserves empty completed history
  - first-best notifications occur once per round; every later increase still saves
  - repeated failures are reported once, successful recovery rearms diagnostics, and source-change
    causes remain visible when subsequent writes are blocked
  - clean checkpoints do not call storage; unsupported profiles never reach the writer
  - real native storage migrates a legacy profile with an exact backup at startup
  - a best saved mid-game reopens correctly and survives restart with no completed entry

Step 4.3 tests validate the native pipeline and inject deterministic I/O failures. Step 4.4
tests exercise the application coordinator without SFML and include native disk roundtrips.
Step 4.5 adds full round-lifecycle acceptance tests and a native macOS graphical pass using a
temporary deterministic application fixture. It checks close/restart/menu before the final
overlay, visible best during play/reopen, native write-error recovery, and source preservation.
A controlled SFML event queue separately checks that input after Closed is not consumed.
The fixture alters initial state and diagnostics, not the production sources or score rules.
Results, exact fixture scope and platform limits are recorded in
[score-persistence-acceptance.md](score-persistence-acceptance.md). These checks do not claim
power-loss guarantees, cross-process locking, or Windows graphical execution.

## Determinism Rules

Step 5.1 path tests inject all locations through `PathContext` and `PathOptions`; they never
query real OS profile directories or move production data. Fixtures cover missing user
directories, existing data preservation, one-root asset selection, missing/file/blocked roots,
empty/relative/native-NUL inputs and Unicode paths. Symlink cases run where supported.
Repeated resolution must preserve file bytes, directory contents and modification times.
Resolving an absent user-data directory must leave it absent. The tests do not claim that a
resolved directory is writable, certify Windows/macOS native discovery, or validate a bundle.

- Core uses `std::mt19937`.
- Tests pass a seed to `Game(seed)` or `reset(seed)` for reproducible rounds. The application
  CLI supports `--fps`, `--vsync`, `--no-vsync`, `--check-runtime`, and `--help`; it has no seed option.
- Golden tests use fixed seed (`1234`) and assert exact outcomes.
- Generated-board corpora use fixed seeds and direct `std::mt19937` output to select from a
  fixed list of valid tile values. They do not depend on standard-library distribution behavior.
- Movement fixtures disable spawning to isolate movement rules. Each direction starts from
  a fresh board; only the explicit next-turn test intentionally performs consecutive moves.

## How To Run

All tests (system dependency build):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

With vcpkg presets:

```bash
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
ctest --test-dir build/vcpkg-debug -C Debug --output-on-failure
```

For multi-config generators such as Visual Studio, `-C Debug` selects the configuration
that was built. Use `-C Release` after a Release build. The headless and graphical test
presets already select their matching configuration.

Run only deterministic snapshot case:

```bash
./build/vcpkg-debug/core_unit_tests "[golden]"
```

Run the expanded movement suite (adjust the build directory to match your configuration):

```bash
./build/core_unit_tests "[move-matrix],[move-trace],[full-board]"
```

Run spawn, empty-cell, placement, and game-over checks:

```bash
./build/core_unit_tests "[spawn],[empty-cells],[place-tile],[gameover]"
```

Run the winning-state suite:

```bash
./build/core_unit_tests "[win]"
```

Run application path contract tests:

```bash
./build/core_unit_tests "[app-paths]"
```

Run native user-data/bootstrap and settings tests:

```bash
./build/core_unit_tests "[user-data-paths],[settings]"
```

Step 5.2 fixtures use temporary profile roots and injected providers; only a read-only native
discovery smoke queries the actual OS base. Tests never bootstrap or write the real user profile
and never mutate HOME/XDG/CWD. File obstructions exercise bootstrap failures without relying on
administrator-sensitive permission assumptions. Native settings tests exercise schema/version
protection, exact legacy backups, changed-source guards and retry without reloading dirty memory.
Profile integration saves/reopens independent best and sound preference in a Unicode directory
while preserving separate old-location files. POSIX tests check new/existing permissions and
symlink semantics. Native Windows execution remains dependent on its CI host.

Step 5.2 validation on macOS: Debug and UBSan each passed 167 cases and 279,343 assertions,
including 38 new cases. The full application build, `--help`, C++/Objective-C++ formatting and
whitespace checks passed. Existing CWD score/settings bytes and the asset resolver source were
unchanged. The native discovery smoke returned an absolute base without writing to it.

A temporary application fixture also passed five native macOS scenarios from a foreign CWD:
missing profile + toggle/close, reopen + opposite toggle, validated legacy settings format +
exact backup, unknown settings version + unchanged best profile, and an actual read-only
directory write failure followed by recovery and close retry. The fixture copies `App.cpp`,
injects a short event/scene-command sequence, and supplies a temporary `RunConfig` override;
production path, settings, atomic storage, audio preference and close-checkpoint methods remain
the implementation under test. It initializes a real SFML window and closes it automatically.
This is application wiring acceptance, not button hit-testing or visual design verification.
The native failure produced one warning and retained the preference until successful close save.

Fixture sources, logs and `results.json` are retained under the ignored
`build/user-data-acceptance/` directory. Every writable profile is inside that fixture directory;
the foreign CWD score/settings markers were preserved. No legacy location import, actual-user
profile write, native Windows run or installed bundle validation was part of this pass.

CI static analysis enumerates active `src`/`tests` translation units from the CMake compilation
database rather than scanning every platform source. This prevents Ubuntu from parsing the
Windows-only native adapter without a Windows SDK. Objective-C++ is included in formatting
and in analysis when it appears in the active platform's compilation database.

The `app_paths` target and these tests do not link SFML. At the step 5.2 checkpoint CMake
still discovered SFML before defining targets; step 6.2 removes that configure requirement
when `SFML_2048_BUILD_APP=OFF`.

Run location import and shared validation/snapshot checks:

```bash
./build/core_unit_tests "[profile-migration],[snapshot],[validation]"
```

Step 5.3 adds 41 cases. Each source and target is isolated in a temporary directory, including
Unicode/CRLF payloads, all supported score/settings format pairs, missing sources, malformed or
unknown versions, obstructed roots and source leaf symlinks. Existing targets are authoritative
even when corrupt, unsupported, directories or symlinks. Repeated startup leaves successful
imports unchanged and retries only absent targets; histories are neither merged nor duplicated.
Ordinary loaders convert imported legacy formats and create byte-exact backups only at the
destination. Source files and their directory snapshots remain unchanged.

Injected readers and writers make observed source changes, I/O failures, thrown dependency errors,
and target creation during publication deterministic. Failed import blocks writes for that
profile throughout the process while retaining best/history/preference updates and dirty state
in memory. The other profile remains independently usable. Tests explicitly distinguish this
from ordinary post-load save retry and verify that a fresh startup can import after recovery.
The copy commits a validated captured snapshot; changes after the final source observation do
not change those bytes. These tests do not claim cross-process locking or live synchronization.
Native snapshot tests reject empty/NUL paths, nonregular leaves and symlinks; POSIX FIFO coverage
checks rejection without a blocking stream. Missing-path classification verifies usable ancestors
so a file or dangling symlink obstruction cannot be treated as an empty profile.

Step 5.3 validation on macOS: Debug and UBSan each passed 208 cases and 280,913 assertions.
The full application build and `--help` smoke check passed, as did formatting and whitespace
checks. Existing project score/settings hashes and the asset resolver source hash were unchanged.
Native Windows execution and installed macOS bundle acceptance remain separate platform checks.

A temporary native macOS application fixture passed seven scenarios: supported legacy import,
destination-only exact backups and reopen without duplicate history; both future/corrupt canonical
score/settings combinations; malformed legacy scores with independently usable settings; malformed
legacy settings with independently usable scores; actual read-only publication failure followed
by fresh-process recovery; and the default startup capture from a controlled foreign CWD.
The failed-import case restored permissions during the same process and still kept destinations
absent while best/history/preference updates remained dirty in memory. A new process then imported
the original source successfully. Existing canonical targets and every old-location source
retained their original bytes.

The fixture uses ignored copies of `App.cpp` and `main.cpp`, a short event/scene-command sequence,
and a terminal session snapshot for best/completion checks. Production migration, shared parsers,
native storage, score coordination, settings and close checkpoints are unchanged. Observe-mode
reopen only closes the window and checks that canonical bytes/history remain identical. This
verifies application wiring with a real SFML window; it does not evaluate button hit-testing,
visual layout, Finder launch, installed `.app` resources, Windows execution or cross-process locks.
All writable canonical, legacy and foreign-CWD paths are isolated under the ignored
`build/profile-migration-acceptance/` directory. Its `results.json`, `scope.json`, exact compile/link
commands, fixture sources and per-scenario logs retain the acceptance evidence. Assets are copied
next to the fixture executable for this pass; bundle/asset resolution remains step 5.4.

Run native asset-root, resource lookup and binary-read checks:

```bash
./build/core_unit_tests "[asset-paths],[asset-resolver]"
```

Step 5.4 adds 30 active macOS cases (platform-specific path/symlink cases differ on Windows).
Fixtures isolate selected roots and preserve bytes, directory listings and modification times.
They check asset resolution independently of invalid user-data inputs, composite parity,
override/provider isolation, native failure propagation, missing/obstructed roots, authoritative
bundle selection, rooted/NUL/parent-traversal resource rejection, File/Directory typing and no
fallback to sibling or extra `assets/` trees. Binary reads cover empty files and boundaries at
65,535/65,536/65,537 bytes, multiple chunks, CRLF/NUL data and native Unicode paths. Trusted asset
symlinks remain supported without claiming sandboxing or concurrent mutation protection.

Debug and UBSan each passed 238 cases and 281,527 assertions on macOS. The full Debug application
build, `--help`, formatting and whitespace checks passed. A final application rebuild includes
the startup gate that decodes the required font before profile bootstrap or window creation.
Project score/settings hashes remained unchanged; no real user profile was bootstrapped.

A native discovery probe passed five launch forms: `./` and `../` executable arguments, an
executable symlink, Unicode relocation and an actual `.app` resource URL. A temporary application
fixture passed eleven scenarios using the final App implementation: CMake bundle resources,
plain executable assets, Unicode relocation, authoritative overrides, missing root/font and
invalid/empty font failures, plus partial/all missing sound behavior. Successful cases verify
all five sound buffers and an actual glyph/render frame after font-byte loading. Font failures
return before the RenderWindow creation marker; absent profiles stay absent, existing profiles
and valid legacy sources retain their bytes, and no format backups are created.

The fixture regenerates ignored App/main copies for diagnostics, provider-call counting, one
render frame and automatic close. Production asset selection, binary reads, decoders and sound
loading remain unchanged. Writable user-data and legacy overrides are confined to the ignored
`build/asset-acceptance/` directory. `app_results.json`, `probe_results.json`, `scope.json`, exact
build commands and logs record the evidence. These checks do not certify physical UI input,
Finder launch, native Windows/Linux execution or installed dependency fixup.

Independent native Release packaging checks built and installed the final application, produced
a CPack ZIP and extracted it into a Unicode directory. All seven managed asset files matched
their source bytes; twelve Mach-O dependency sets had no build/Homebrew loader references.
The installed bundle uses embedded non-system dependencies and local ad-hoc signatures after
install-name fixup. `codesign --verify --deep --strict` and the relocated executable's `--help`
passed from a foreign CWD, including the ZIP-extracted copy. Fake-source asset sync checks also
verified content refresh, stale-file removal, metadata exclusion and overlapping-tree rejection
without editing project assets. Evidence is under `build/asset-packaging-validation/`.
Ad-hoc signing is local execution validity; Developer ID signing, notarization, quarantine and
external clean-machine acceptance remain later work. Windows release staging now uses the
matching CMake Runtime layout, but this Mac does not certify native Windows execution.
The macOS `SFML_2048_MACOS_BUNDLE=OFF` configuration also built successfully, placed the font
beside its plain executable under `assets`, and passed `--help` from a foreign CWD.

Step 5.1 validation on macOS: the 14 new path cases and the full suite passed in Debug and
UBSan builds (129 cases, 277,220 assertions in each). The complete application built and its
`--help` smoke check passed. `AppPaths.cpp` also compiled independently with C++20, strict
warnings and `-Werror`, without third-party include paths. Native dependency inspection of the
test executable found only libc++ and libSystem. Format and whitespace checks passed; hashes
of the existing score/settings files and runtime path consumers were unchanged. This validates
the injected contract, not native Windows discovery or installed macOS bundle behavior.

Run score memory and session lifecycle checks:

```bash
./build/core_unit_tests "[score],[memory],[session],[score-manager]"
```

Run profile schema and migration preparation checks:

```bash
./build/core_unit_tests "[schema]"
```

Run atomic storage and retry checks:

```bash
./build/core_unit_tests "[storage],[migration]"
```

Run the application coordinator and final native lifecycle acceptance tests:

```bash
./build/core_unit_tests "[coordinator],[acceptance]"
```

Run with AddressSanitizer and UndefinedBehaviorSanitizer on a Clang/GCC build:

```bash
cmake -S . -B build/sanitized -DCMAKE_BUILD_TYPE=Debug -DNUMTILO_BUILD_APP=OFF -DBUILD_TESTING=ON -DNUMTILO_ENABLE_SANITIZERS=ON
cmake --build build/sanitized --target core_unit_tests
ctest --test-dir build/sanitized -C Debug --output-on-failure -L "^unit$" --no-tests=error
```

The movement suite is intended to catch source reuse, incorrect right/down orientation,
missing stationary merge participants or merge events, incorrect merge scores, and false
positive `moved` flags. Temporary mutations can verify these protections without changing
the production source files.

The spawn/state suite also targets illegal normal values, multiple spawns, occupied-cell
selection, ignored spawn-disable flags, RNG consumption on no-ops, stale post-spawn status,
overwrite-capable placement, and missing horizontal/vertical move detection. Reachability of
both normal values is checked with fixed seeds; probabilistic frequency thresholds are avoided.

## CI Enforcement

GitHub Actions (`.github/workflows/ci.yml`) runs:

- format check (`clang-format`)
- headless Debug/Release preset build + full unit suite on Linux/macOS/Windows
- portable guarded configuration/imported-target/installed-port checks before every headless build
- fresh full dependency audits with tests enabled/disabled on Linux/macOS Release, plus controls
- desktop build + unit/platform tests on Linux/macOS/Windows
- built and installed desktop runtime acceptance with native paths and isolated profile subprocesses
- headless sanitizer job on Linux (ASan/UBSan)
- coverage job (`gcovr`) with `--fail-under-line 80` for `src/core`

Headless, sanitizer and coverage jobs retain configuration/test/audit evidence with `always()`.
Existing desktop check names are preserved. The workflow and branch-protection template define
the six new headless checks; successful hosted execution is not claimed by local validation.
Step 6.6's actual native results and remaining runner evidence are recorded above.

Release workflow (`.github/workflows/release.yml`) checks the installed Runtime and the actual
ZIP-extracted executable before uploading packages. Native acceptance needs a full application
and worker build; `ctest -L unit` is the appropriate scope when only `core_unit_tests` was built.
If Python is unavailable or the build is cross-compiled, CMake explicitly reports that native
platform acceptance is not registered. That omission is not a platform acceptance pass.

Step 5.5's contract, commands and native evidence are in
[platform-acceptance.md](platform-acceptance.md). Historical results above describe their
respective steps; later acceptance adds installed application/Finder checks independently.
