# UI edge cases — roadmap step 11

Status, 2026-10-14: **11.4 checkpoint and step 11 closed within the recorded local
macOS scope.** Automated gates passed with one explicit Ninja environment skip;
the user confirmed all six E11-03 manual steps worked without problems. No product
defect was established and no fix was made. Automated and user-reported manual
evidence remain separate. Preparation records below retain their original timing;
the latest execution and closure results are at the end of this document.

Naming note, 2026-10-15: this is the historical step 11 record from before the
Numtilo rename. Completed commands and checkpoint recipes below retain their
original `sfml_2048` target, `2048Edge112Lifecycle.app` bundle, build paths and
evidence; they are not current launch instructions. For maintained instructions,
see [testing.md](testing.md). Current preparation uses the `numtilo` target,
`NUMTILO_` build options and the generated `NumtiloEdge112Lifecycle.app` QA bundle,
with fresh owned output below `build/`. The rename does not rerun this acceptance
or authorize a new checkpoint; the recorded results and evidence limits stand.

## Approved work packages

| Substep | Scope | Exit condition |
| --- | --- | --- |
| 11.1 — scope and scenario map | Read current sources and prior evidence; select remaining combinations and state their expected behavior. | A bounded acceptance list, explicit coverage boundaries and a successful application build. |
| 11.2 — input/window preparation | Prepare the selected input, request-batch and window-lifecycle scenarios in the existing verification structure. | Reviewed, buildable preparation; no test execution. Stop for approval before 11.3. |
| 11.3 — transition/record preparation | Prepare the selected close, navigation and record-consistency combinations. | Reviewed, buildable preparation and a checkpoint-ready list; no test execution. |
| 11.4 — explicitly approved checkpoint | Run the prepared scenarios and relevant existing checks after the user's explicit approval. | Report actual results; obtain separate approval for fixes, then reverify the approved changes before closure. |

The ordinary loop is scope-only work, code-quality review, build, brief report and
stop. Gameplay, screenshots, recordings, VoiceOver, UI interaction and regression
execution are reserved for an explicitly approved checkpoint. Preparing a scenario
does not authorize running it. A successful build is not a runtime acceptance pass.

## Existing coverage retained

These results were read as prior evidence, not rerun during preparation. Older scene/App
fixtures retain their original source revision and execution limits. A current
source implementation is not proof that an old native tour was repeated with it.

| Behavior already covered | Evidence and layer | Treatment in step 11 |
| --- | --- | --- |
| Five scenes at 1×/2×; ten controls; focus/hover/press geometry; pointer ownership, drag/blur/reset and unmatched releases | [Maintained UI worker](../tests/ui_acceptance.cpp), `controls`; [step 10 audit](ui-quality-audit.md), 772 checks / 102 PNGs | Retain the gate. Do not create another screenshot matrix or redesign controls. |
| Empty/18–19-character/Turkish/unsupported names; 0–5 ranking rows; complete large scores and readability | Maintained UI worker, `semantics` and scene captures; step 10.3–10.5 findings | Retain existing checks rather than repeat name/score boundary work. |
| Held/repeated keys, modifier/text leakage, scene-owned releases, blur/regain and fresh reactivation | [Navigation history](ui-design-system.md#roadmap-step-63--navigation-acceptance-and-step-6-closure); retained `build/ui-navigation-step-6.3/navigation_smoke.cpp` | Existing controlled proof, with only a narrow keyboard subset proposed for permanent coverage below. |
| New/Menu versus active, clock-crossing and settled outcomes | Step 10.4 A01 evidence: `build/ui-quality-step-10.4/routing/README.md`, 36 active routes, 14 clock crossings, eight settled priorities and six stale-release routes | A01 stays closed. The proposed close/batch combinations add an ordering dimension, not another 64-route tour. |
| Win waiting, once-per-round presentation, Continue identity and Win-before-GameOver priority | [Outcome units](../tests/gameplay_outcome_tests.cpp), maintained worker `outcomes`, step 7.4 native history | Keep the established policy, including the controlled won-and-terminal fixture. |
| Best/history separation, completion identity, retry snapshots, no duplicate completion and reopening | [Score coordinator units](../tests/score_coordinator_tests.cpp), [score acceptance units](../tests/score_acceptance_tests.cpp), [persistence acceptance](score-persistence-acceptance.md) | Do not rewrite backend/storage tests or expand recovery guarantees. |
| Close stops a queued SFML input tail | Persistence acceptance's historical `[Closed, N, Left]` App-loop fixture | Retain its proof; renew only the selected current close/native-provider combinations. |
| Native request bounds/FIFO, stale generation/round/capabilities, publication, focus refresh and retained callbacks after detach | [Bridge units](../tests/accessibility_tests.cpp), eight cases / 456 assertions | Component safety is covered; real App changes within one drained request group are a separate candidate. |
| Actual macOS actions, board values/move, normal close, profile reopening and repeated Name-click focus | Step 10.6 oracle: `build/ui-quality-step-10.6/native/final-native-acceptance.json`, 233 checks with explicit revision scopes | Keep that acceptance. No full VoiceOver replay is required merely to start step 11. |

## Selected acceptance list

The following six items are **coverage selections, not diagnosed defects**. Each
has one owner substep. E11-01–E11-03 were prepared in 11.2 and E11-04–E11-06 in
11.3. Their 11.4 results distinguish controlled passes from user-reported physical
keyboard verification. None was implemented or runtime-verified by 11.1. Use bounded
event sequences and existing public contracts; additional native execution belongs
to 11.4 only.

| ID / owner | Selected sequence | Expected result | Existing proof / remaining boundary |
| --- | --- | --- | --- |
| E11-01 / 11.2 | Move a small representative set of held/repeated Enter, N/direction and text-leakage cases across scene changes into maintained coverage; require release and a fresh input afterward. | The old scene's held key cannot trigger a new scene or insert a name character. A release followed by fresh input works under the scene's existing activation policy. | Historical navigation fixtures cover the behavior. The maintained worker directly checks stale primary-pointer ownership and a semantic held-Space case, but not the corresponding keyboard guard matrix. This is consolidation, not new keyboard behavior. |
| E11-02 / 11.2 | Route an immutable group of requests through current App: Playing New followed by old-tag New/Sound/Menu; Win Continue followed by old-tag New. | After the first accepted command changes the round or scene visit, remaining requests with old tags are rejected even where the same control ID still exists. New requests for the current visit remain usable. Continue retains the round, making its changed scene epoch significant. | The current [App](../src/app/App.cpp) routing/dispatch fixture passed both groups in 11.4, while bypassing native callbacks and `takeActions`. Bridge FIFO/drain remains separate component evidence; native request delivery is not inferred. |
| E11-03 / 11.2 | Minimize or lose focus with an activation key held; release while outside the app, restore and make a fresh keyboard/pointer input using the current macOS semantic host. | Pending activation is cancelled; known held-key state is reconciled; no phantom command occurs. Focus follows the existing scene reset policy, and a fresh selection resolves to the correct semantic control and action. Focus change alone does not alter round, best/history or Sound; retaining the previous focused button is not required. | Old navigation and Win tours cover focus/minimize with earlier sources. The final current adapter proof covers repeated Name clicks, not this OS lifecycle combined with its first-responder refresh. Native focus/input evidence is required only at the checkpoint; VoiceOver startup is not inherently required. |
| E11-04 / 11.3 | Close after a real final move while animation is active and at its settled boundary, for an unfinished winning round and a terminal round. | Close does not wait for a modal to draw. It checkpoints scores/settings, detaches the provider and exits. An unfinished win retains best with no fabricated completion; a terminal round has exactly one original-ID/name/score completion. No later SFML input acts after Closed. | Historical persistence acceptance includes immediate terminal close. All six base step 10.6 close records have `animations=false`; they do not prove timing at the final-animation boundary with the current provider. |
| E11-05 / 11.3 | In one SFML event group, accepted New/Menu then Closed, and Closed then New/Menu; include a remaining release/callback tail. | Navigation first checkpoints the old round before reset/leave, then close saves the resulting accepted state. Closed first ends polling; later SFML navigation cannot run. After detach, retained callbacks cannot enqueue another action. | Dispatch/record ordering and detach are separately covered. The current App batch-plus-close combination is selected; neither the old `[Closed, N, Left]` proof nor fake-platform detach alone replaces it. |
| E11-06 / 11.3 | A terminal save fails; accepted navigation starts/leaves a round; close then retries with storage recovered or still unavailable, using an isolated profile. | Recovery saves one completion with the original player/round/score. A persistent failure preserves the existing file and reports the established failure state; it is not claimed as a durable save or promised recovery after process exit. No new-round identity replaces the original completion. | Coordinator units already cover failed-save/restart retry. The selected addition is its App navigation-and-close composition, not a new journal, retry policy or storage implementation. |

## Source-derived ordering and invariants

- [App](../src/app/App.cpp) drains native requests before polling SFML events. Each
  native request is revalidated against the current scene visit, round and enabled
  actions. A valid native action already processed before SFML Closed is not
  retroactively undone; close-first guarantees apply to the remaining SFML queue.
- Closed is handled before outcome/input routing: scores/settings checkpoint,
  accessibility detach, window close, then break. A modal need not be presented
  before saving a logically completed round.
- `dispatchAction` checkpoints an accepted navigation's old state before applying
  reset/scene change. Start/Restart and real transitions advance the accessibility
  generation and reset input ownership; frame refresh alone does not.
- [SceneInputGuard](../src/app/ui/SceneInputGuard.hpp) owns tracked keys and primary
  gestures by scene, blocks old held keys and reconciles known blocked keys on
  GainedFocus. Scene handlers also clear pending activation on LostFocus.
- [GameplayOutcome](../src/app/ui/GameplayOutcome.hpp) preserves its existing
  animation wait and once-per-round Win-before-GameOver policy. The controlled
  won-and-terminal fixture may support a selected combination if needed; no new
  production state-loading API is authorized.
- [GameSession](../src/app/GameSession.cpp) and
  [ScoreCoordinator](../src/app/ScoreCoordinator.cpp) retain round identity,
  completion acknowledgement and save semantics. Focus/animation alone cannot
  create history, and clean/repeated checkpoints cannot fabricate a completion.

## Preparation constraints and checkpoint contract

Use the maintained UI/unit verification structure where it covers the required
layer. Do not add a UI framework, dependency, permanent target or production test
switch just to increase case counts. If a real App sequence needs an isolated
fixture, record its source revision and exact boundary; a scene-only simulation
must not be labeled as full App acceptance. Historical ignored fixtures are
inventory evidence, not required inputs to the maintained gate.

For each selected case, prepare its initial state, ordered inputs, expected scene,
round/score/best/history/Sound effects and required evidence layer. At 11.4, reports
must distinguish prepared, executed/pass, failed, blocked and not applicable. A
case is not passed through code inspection or compilation alone. Use temporary
profiles for any eventual execution; preserve actual user data and prior evidence.

The checkpoint includes relevant existing core, bridge, platform and graphical
checks once, plus the selected new combinations. Additional screenshots or
accessibility tours require an actual scenario need and the explicit checkpoint
authorization. Report failures before requesting approval for fixes; stop before
starting any later roadmap step.

Windows/Linux native acceptance, broader IME/font shaping/fallback and hosted CI
remain step 12. Build/evidence cleanup remains 15.1. UI redesign, backend recovery
expansion, simultaneous writers, forced termination and power-loss guarantees are
outside this package.

## 11.1 completion record

The deliverable is this inventory and acceptance map, with current status links
in the UI audit/design, testing and maintenance documents. Production source,
tests, scripts, build definitions, assets and runtime JSONs are preserved.

The build-only command was:

```bash
cmake --build build/ui-acceptance-release --target sfml_2048 --parallel 4
```

The existing supported build was reused and the application target completed
successfully. The log is `build/ui-edge-step-11.1/build.log`. The command did not
run the game or the acceptance/unit workers. Document links/whitespace and source
preservation were reviewed; no runtime pass is inferred from that review.

No gameplay, screenshots/recordings, native interaction, VoiceOver, test execution
or step 11.2 implementation was performed in this inventory step. Prior test counts
above describe their recorded checkpoint; they are not new step 11 results.

## 11.2 completion record

Only input/window preparation was implemented. Production sources, build
definitions, assets, backend tests and real runtime JSONs are unchanged.

| Case | Prepared entry point | Status and evidence boundary |
| --- | --- | --- |
| E11-01 | [Maintained UI worker](../tests/ui_acceptance.cpp), `keyboardInputEdges`; [runner](../scripts/verify_ui.py), `INPUT_EDGES` | Five controlled guard/scene cases: Enter, N, direction, multi-key Unicode leakage and blur cancellation. They add 59 assertions once at 1×, with five mandatory `input_edges` report rows. Compiled; not executed. No physical keyboard reconciliation is claimed. |
| E11-02 | [External fixture preparer](../scripts/prepare_ui_edge_fixture.py) and [helper](../tests/ui_edge_fixture.hpp), `e11-02-playing-batch` / `e11-02-win-batch` | Exact current-App routing and dispatch, immutable old-tag groups, accepted/rejected counters and fresh current-tag recovery. Win uses the maintained public seed-zero replay and actual GameplayOutcome transition. Compiled; not executed. Native callbacks and `takeActions` are bypassed. |
| E11-03 | Same external fixture, `e11-03-lifecycle`, plus the native recipe below | Current native App loop records bounded SFML event/state and semantic snapshots. Compiled; no minimize/restore or native focus observation has run. App semantic focused IDs are not native AX focus proof. |

The UI report keeps schema 1, its existing 20 input and 20 visual control/density
records and 102 images. The runner additionally requires the exact five E11-01
rows, each passed at its controlled layer with a positive integer check count.
The last measured worker result remains step 10's 772 checks. The prepared worker
would have 831 checks if all assertions pass; this is an expected count, not a
measured result.

The fixture generator changes only a generated App translation unit under
`build/`. Its reviewed anchors must match the current source, and reversing the
patches must restore its exact bytes. It reuses the existing local Makefiles
compile/link metadata, replaces only App/main objects, adds no dependency, CMake
target or product test switch, and compiles with warnings treated as errors.
The quoted include path avoids the case-insensitive macOS `VERSION` / `<version>`
collision found during compilation; the project file was preserved.

Each future run requires an explicit case and a fresh ordinary directory below
project `build/`; profile and legacy data overrides stay inside that directory.
No default launch or run option exists in the preparer. Runtime preflight compares
the embedded exact bytes of App.cpp, ui_acceptance.cpp and ui_edge_fixture.hpp.
Those three matches do not certify the other linked objects: rebuild the product,
then regenerate/recompile the fixture after 11.3 or any relevant source change.
The report records that limit and the input/source hashes. Evidence writes check
flush and close failures; a missing result is not acceptance.

Build-only preparation completed with:

```bash
cmake --build build/ui-acceptance-release --target sfml_2048 ui_acceptance --parallel 4
python3 scripts/prepare_ui_edge_fixture.py --build-dir build/ui-acceptance-release --output build/ui-edge-step-11.2/app-fixture --compile
```

Logs and metadata are `build/ui-edge-step-11.2/build.log`,
`fixture-preparation-final.log` and `app-fixture/preparation.json` (prepared,
compiled, zero launches). Earlier compilation attempts are retained separately.
Python syntax, C++ formatting, source preservation and preparation metadata were
reviewed without executing a worker. No gameplay, screenshots/recordings, native
interaction, VoiceOver or regression/unit tests were performed. Stop before 11.3.

## E11-02/E11-03 recipe for the explicitly approved 11.4 checkpoint

Historical recipe for the 2026-10-14 checkpoint; its original names and paths are
preserved verbatim. Use the maintained instructions above for current preparation.

This recipe is preparation, not permission to run it during 11.2/11.3. At the
checkpoint, first rebuild the product/worker and regenerate/recompile into a fresh
output directory. For example, if this path is absent:

```bash
cmake --build build/ui-acceptance-release --target sfml_2048 ui_acceptance --parallel 4
python3 scripts/prepare_ui_edge_fixture.py --build-dir build/ui-acceptance-release --output build/ui-edge-step-11.4-app --compile
```

For E11-02, run each explicit mode with its own fresh output/profile:

```bash
build/ui-edge-step-11.4-app/ui_edge_fixture --case e11-02-playing-batch --run-output "$PWD/build/ui-edge-step-11.4-app/runs/playing-1"
build/ui-edge-step-11.4-app/ui_edge_fixture --case e11-02-win-batch --run-output "$PWD/build/ui-edge-step-11.4-app/runs/win-1"
```

Review `trace.json`, `result.json`, all recorded checks and the process exit for
each run. Controlled acceptance requires a complete trace and
`passed-controlled-App-dispatch`. Playing requires one accepted New and three
old-tag rejects before fresh Sound/New recovery. Win requires one Continue and
one old-tag reject before fresh Playing New. Compare round/generation, scene,
grid/score, best/history and Sound with the recorded baselines. Fixture stop
detaches/closes deliberately; it is not an OS Closed/checkpoint proof or a native
callback/queue-drain proof.

For E11-03, launch the generated macOS bundle with explicit arguments and a new
profile; observe the actual OS window and native accessibility focus separately:

```bash
open -n -W build/ui-edge-step-11.4-app/2048Edge112Lifecycle.app --args --case e11-03-lifecycle --run-output "$PWD/build/ui-edge-step-11.4-app/runs/lifecycle-1"
```

1. Confirm Playing with player Çağrı, score 0 and seed 112; retain initial round,
   grid, best/history and Sound from the trace. Use a fresh Tab selection for New
   and record both its semantic selection and the actual native AX focused control.
2. Hold Space on that focused button so activation is pending until release.
   Actually minimize the OS window while the key is held; verify minimize and
   LostFocus, release outside the app, then restore/activate and verify GainedFocus.
   Record real held/released keyboard state and the OS window observations. The
   release may never appear in this app's SFML queue. Synthetic focus events or
   app-directed key events alone do not prove this sequence.
3. Require no Restart/Sound/navigation command during blur or restoration. The
   baseline round, grid, score, best/history and Sound must remain unchanged.
   Existing focus reset policy applies; retaining the previous button is not a
   requirement. A fresh Tab selection must identify the matching native control.
4. Fresh Space press/release must create exactly one New round and keep score 0,
   best/history and Sound. Then activate Sound with an actual pointer press/release,
   matched to the SFML trace: it toggles once without replacing that round. Record
   native focus where selection changes; semantic focused IDs alone are insufficient.
5. Close normally to finish the recording. This adds no final-animation/close-timing
   acceptance for 11.3. Review the complete trace and isolated result files.

The lifecycle result intentionally remains `recorded-pending-native-review`, even
with exit 0. Mark E11-03 passed only after matching the native observations to the
trace and expected state; truncation, missing focus/input evidence or an incomplete
sequence cannot pass. Report failed or blocked honestly. VoiceOver startup is not
required for this case; any screenshots must serve an actual checkpoint need.

## 11.3 transition/record preparation

The existing external fixture is extended for the remaining three selected cases,
without a product switch, new dependency or permanent target. Only generated
current-App code and test helpers use a controlled event source or injected storage
failure. The original App event-loop routing and Closed branch remain the path
under examination. Synthetic SFML events do not certify physical input delivery or
an OS close notification.

| Case | Bounded combinations | Required state and evidence |
| --- | --- | --- |
| E11-04 | Unfinished first Win / terminal final move × active / just-settled animation | Replay only the public prefix, then make the final changed move through current App dispatch and PlayingScene.onMove. Confirm the observed animation boundary, send Closed next, and require the original round/grid/score/name, correct best/history, settings checkpoint and detached provider. Close must not depend on presenting a modal. |
| E11-05 | New / Menu × navigation before Closed / Closed before navigation | Use a terminal original round and one immutable controlled event group with a remaining release/direction tail. Navigation first must checkpoint the old identity before reset/leave. Closed first must break polling without processing navigation or its tail. Stored completion occurs once; no phantom command or Sound change is accepted. |
| E11-06 | New / Menu × storage recovered for Close / still unavailable at Close | Arm a test-local IoError before the terminal move's checkpoint and keep it armed during navigation. Completion is acknowledged once in memory under the original ID/name/score. Recovery at Close publishes that snapshot through the production writer; persistent failure retains dirty memory and the exact existing profile bytes. Reopening the isolated saved file must match the permitted disk result. |

A terminal final move already checkpoints and acknowledges completion **before**
PlayingScene.onMove and modal presentation. E11-04 therefore checks preservation
and duplicate prevention at Close; it does not claim that Close first creates the
completion. An unfinished win is not a completion. With Closed queued next, its
branch runs before outcome resolution even when the animation just settled.
Each mode uses a fresh sole-round profile; final best must equal the final score
and remain unchanged through close/navigation. Player remains original, and the
final board is preserved unless New was actually accepted before Closed.

Animation timing uses the current public clock/tick behavior with a bounded wait.
The active predicate must actually be observed; a missed window or settle timeout
cannot be relabeled as a pass. No clock freeze, fixed millisecond responsiveness
guarantee or forced outcome state is part of this preparation.

E11-06 uses the existing ScoreStorage interface and a wrapper around the selected
production writer. An armed wrapper returns IoError without modifying disk; after
recovery it delegates ordinary publication with the original expected-source
snapshot. This proves only the controlled retry composition. It is not an actual
permission fault, concurrent-write test, persistent journal or recovery guarantee
after process exit. Settings keep their ordinary storage path.

After explicit checkpoint approval and fresh preparation at the path used above,
execute the twelve transition/record modes with separate new output directories:

```bash
edge_modes=(
  e11-04-win-active-close e11-04-win-settled-close
  e11-04-terminal-active-close e11-04-terminal-settled-close
  e11-05-new-before-close e11-05-menu-before-close
  e11-05-close-before-new e11-05-close-before-menu
  e11-06-new-recovery-close e11-06-menu-recovery-close
  e11-06-new-persistent-close e11-06-menu-persistent-close
)
for edge_mode in "${edge_modes[@]}"; do
  build/ui-edge-step-11.4-app/ui_edge_fixture --case "$edge_mode" --run-output "$PWD/build/ui-edge-step-11.4-app/runs/$edge_mode-1" || break
done
```

This loop is a future recipe, not a test performed in 11.3. Stop on a failed run
and preserve its evidence. Unexecuted modes remain unexecuted; a successful
process exit does not replace review of the recorded assertions and profile.

For provider closure, use actual App attachment/detachment observations and the
public bridge state/queue checks. The existing retained-callback test in
[accessibility_tests.cpp](../tests/accessibility_tests.cpp) is separate fake-platform
component evidence. An empty queue after detach does not by itself prove that a
retained native callback was invoked. An external stale AXUIElement failing after
process exit is not evidence of the bridge's weak callback lifetime guard.

The twelve modes use `passed-controlled-App-event-loop` only after execution and
all recorded assertions, including actual provider attachment, detach, empty drain
and rejection of a valid pre-close snapshot after detach. Their reports explicitly
set retained native callback injection and initially populated native queue flush
proof to false. A persistent IoError mode can satisfy its expected failure contract
with process exit 0, while its failed checkpoint, dirty memory and unchanged disk
remain recorded; it must not be described as a successful durable save.

Build-only preparation completed with:

```bash
cmake --build build/ui-acceptance-release --target sfml_2048 ui_acceptance --parallel 4
python3 scripts/prepare_ui_edge_fixture.py --build-dir build/ui-acceptance-release --output build/ui-edge-step-11.3/app-fixture --compile
```

Logs are `build/ui-edge-step-11.3/build.log` and `fixture-preparation.log`; metadata
is `app-fixture/preparation.json` (prepared, compiled, zero launches), and scope
preservation is in `summary.json`. An earlier compiled preparation is retained
separately after final review tightened the player/grid/best assertions; the final
fixture matches the reviewed source hashes. The same two versioned preparation
files were extended, and the plan/testing/design/maintenance documents updated.
No new permanent fixture source, framework or build target was added.

Production sources, backend tests, assets, build definitions and real root runtime
JSONs are byte-identical to the 11.3 entry inventory. Python syntax, C++ formatting,
document links, source normalization and preparation metadata were reviewed.
No test, game, worker, screenshot/recording, native UI interaction or VoiceOver run
was performed. The six-case preparation is checkpoint-ready; 11.4 is not started.

## Step 11 checkpoint readiness and result contract

The preparation inventory has six selected case IDs. At 11.4, record their status
and required layers separately:

| Case | Current controlled entry point | Required additional evidence |
| --- | --- | --- |
| E11-01 | Maintained worker, five mandatory input_edges rows | Current worker execution/report; historical 772 checks do not pass the new rows. |
| E11-02 | Two current-App immutable request groups | Complete controlled routing traces and fresh-current-request recovery; retain the native-callback/takeActions boundary. |
| E11-03 | Current native App loop recorder | Actual minimize/restore, held/released keyboard and native focus observations matched to the trace; no automatic pass. |
| E11-04 | Four controlled final-move/Closed combinations | Observed active/settled boundary, original state and persisted profile result, provider teardown and complete tail accounting. |
| E11-05 | Four controlled navigation/Closed orderings | Actual App checkpoint/reset/leave order and unconsumed tail; relevant existing retained-callback component test remains separately labeled. |
| E11-06 | Four controlled storage-failure/navigation/Closed combinations | Recorded failed/retried checkpoint results, original completion identity, disk bytes and production-reader reopen result. |

Rebuild the current product and worker, then regenerate/recompile the external
fixture into a fresh output at the approved checkpoint. Old 11.2 preparation
binaries become historical after helper changes; their byte preflight must not be
bypassed. Use a fresh run directory/profile for every mode. Keep all prior failure
logs and actual user data intact.

Run the relevant existing core, bridge, platform and graphical gates once, plus
the selected external combinations and E11-03 native recipe. Broader headless/CI
or platform tours are not silently added to an ordinary substep. No new screenshot
matrix or full VoiceOver replay is required merely because these cases were added.

For controlled cases, inspect process exit, complete trace, every check and final
state/profile report together. A missing artifact, skipped combination, truncated
trace or unmet timing/layer requirement cannot pass. Keep E11-03 pending until the
native observations are reviewed. Report failures before requesting approval for
fixes; do not change an oracle to match a failure or continue to step 12.

## 11.4 execution record — automated and manual evidence

The user explicitly authorized all checkpoint tests and required approval before
fixing any defect. The application, existing workers and fresh current-App fixture
were rebuilt. No production, test, runner or oracle source was changed during
execution. All new run profiles and artifacts are under `build/ui-edge-step-11.4/`;
old evidence and actual user data are preserved.

| Gate/case | Actual result | Evidence and limit |
| --- | --- | --- |
| Core / bridge units | Passed: 255 / 282,023 and 8 / 456 cases/assertions; unit CTest 2/2 | `unit-ctest.log`, `unit-full.log`, `unit-platform-summary.json`; bridge callbacks use the existing fake platform. |
| Platform acceptance | Passed: five isolated native scenarios | Fresh `platform/platform-dfyq03_j/report.json`; the missing-font negative control succeeds by detecting its expected runtime failure. |
| Maintained UI / E11-01 | Passed: 831 checks, 102 images; five new rows total 59 checks | `ui-gate.log`, `ui/ui-acceptance.json`; controlled rendering/guard/scene evidence at 1×/2×. Four representative stress/ranking/modal captures also visually reviewed. |
| E11-02 | Passed: both immutable request groups, 19 assertions | `app-fixture/runs/e11-02-*`; source-normalized current App dispatch, not native callbacks or takeActions delivery. |
| E11-04 | Passed: all four active/settled Win/terminal close modes, 112 assertions | Real final App move and public clock state; controlled event source, real Closed routing/checkpoint/detach body. |
| E11-05 | Passed: all four navigation/close orders, 104 assertions | Expected accepted state, exact original completion and unconsumed release/direction tails; callback retention remains separate bridge-unit proof. |
| E11-06 | Passed: all four New/Menu recovery/persistent-failure modes, 140 assertions | Original identity/timestamp/payload and expected-source bytes stable. Recovered close delegates the real writer; persistent failure preserves disk and is explicitly not a durable save. |
| Python verifier controls | Passed 26; one environment skip; zero failures | 27 discovered tests, `python-tests.log`; Ninja unavailable for the multi-config control, not waived as passed. |
| Fresh independent headless Release | All seven audit stages passed; fresh core 255 / 282,023 | `headless-cached/headless-2nx5hj97/report.json`; existing dependency sources reused, fresh build/object/header/symbol/runtime evidence. An interrupted download-only attempt is retained separately, not a product/test failure. |
| E11-03 automated native lifecycle | Partial automated evidence, preserved unchanged | `native/evidence-review.json`, native status/emission files and 82-row actual App trace; 27 evidence checks passed. The physical sequence is accepted separately through the user's manual confirmation below. |

The fourteen controlled App modes exit 0 with their exact scoped passed status,
375 assertions in total. An independent read-only audit performs 285 evidence
comparisons against current hashes, trace order, state and actual disk bytes:
`evidence-audit.json`. These totals describe separate layers, not 660 new runtime
test cases. UI and platform gates use the same maintained runner/worker commands
with fresh output paths so historical reports are not overwritten.

For E11-03, initial New selection matches actual native AXButton `new`. Native
Space reaches SFML before actual LostFocus; cancellation and GainedFocus preserve
round/grid/score/best/history/name/Sound with no phantom command. Fresh selection
again matches native New, then fresh Space creates exactly one round. A global
Quartz pointer pair reaches actual SFML press/release and toggles Sound once
without replacing that round. Normal Cmd+Q supplies Closed, checkpoints settings,
detaches the actual provider and exits 0. Only the isolated Sound-off file is saved;
the zero-score unfinished run correctly creates no score file.

The native hold driver cannot establish physical keyboard ownership: during its
posted hold Quartz HID state is true, while the same SFML global Space query is
false. Its minimize stage also fails to confirm AXMinimized=true and returns 5;
the full failed attempt is retained and Space is released by its cleanup guard.
Later focus loss/regain is genuine, but neither that nor a changed Quartz state
replaces the planned physical held-key/minimize-or-blur/release-outside proof.
A bounded deep AX inspection exhausted its six-second read budget; its partial
tree is not used as complete native focus evidence. Focus claims use complete
targeted AX status observations instead. No hardware events, permission change
or full VoiceOver replay is claimed.

This is an automation/evidence boundary, not a diagnosed product defect. The raw
fixture result remains `recorded-pending-native-review`, and the original partial
automation report, failed driver attempt and environment skip are preserved. They
have not been relabeled as a complete automated physical-keyboard pass.

### User-reported manual confirmation and closure

On 2026-10-14, after receiving the six-step manual procedure, the user reported:
“Tamam bilader bütün adımları denedim sorunsuz çalışıyor.” This direct confirmation
closes the remaining E11-03 physical scenario:

| Manual sequence | Result and provenance |
| --- | --- |
| Start a round, wait for animations and note board/score/Sound | User reports completed without problems. |
| Select New with Tab and hold physical Space | User reports completed without problems. |
| Minimize while holding Space, then release while minimized/outside | User reports completed without problems. |
| Restore; no phantom New and board/score/Sound unchanged | User reports completed without problems. |
| Reselect New and press/release fresh Space once; exactly one new round | User reports completed without problems. |
| Click Sound once; one toggle and no round reset | User reports completed without problems. |

Separate evidence: `build/ui-edge-step-11.4/manual-confirmation.json`. This is the
user's manual acceptance, not an agent-observed rerun; the exact manual binary and
profile were not independently captured. No new automation, test, game launch,
VoiceOver or build was performed to record this confirmation. No acceptance scope
was expanded and no source/oracle fix was made.

E11-03 is accepted through that manual confirmation. All six selected case IDs are
closed at their stated layers, and 11.4/step 11 are closed for this local package.
The Python Ninja control stays explicitly skipped, and Windows/Linux, hosted CI
and broader accessibility remain step 12. No later roadmap step is started.

No application fix was made or approved, no later roadmap step was started, and
the isolated QA process is closed. VoiceOver/clipboard/security settings were not
changed. The checkpoint report distinguishes automated passes, user-reported manual
acceptance, the retained automation limitation and the environment skip.
