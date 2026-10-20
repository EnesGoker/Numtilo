# Score persistence acceptance — step 4

Date: 2026-10-06. Local execution: macOS, arm64.

Steps 4.1–4.5 are complete for the score persistence contract: independent best, completed-round
history, derived top five, versioned validation/migration, atomic publication, controlled retries,
and application lifecycle integration. This is an acceptance of the backend and its application
integration; Windows runtime and installed-package validation remain separate work.

## Automated acceptance

The complete Catch2 target passed in both normal Debug and UBSan builds:

| Check | Result |
| --- | --- |
| Normal Debug suite | 115 cases, 276,922 assertions passed |
| UBSan, halt on error | 115 cases, 276,922 assertions passed |
| Full SFML application build | Passed |
| clang-format | Passed |
| `git diff --check` | Passed |

Commands used with the existing local system-dependency builds:

```bash
cmake --build build/refactor --parallel 4
build/refactor/core_unit_tests --reporter compact
cmake --build build/core-step1-ubsan --target core_unit_tests --parallel 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  build/core-step1-ubsan/core_unit_tests --reporter compact
bash scripts/check_format.sh
git diff --check
```

`build/core-step1-ubsan` uses `-fsanitize=undefined -fno-omit-frame-pointer`, including the linker
sanitizer flag. This report records UBSan results, not an ASan pass.

| Accepted behavior | Permanent test evidence |
| --- | --- |
| New best persists before logical completion; no fabricated history | `score_coordinator_tests.cpp`, `score_acceptance_tests.cpp` |
| Restart/abandon retains a best higher than a later completed game | `score_acceptance_tests.cpp` |
| Winning does not record a completed game or change round identity | Seed-0 winning fixture in `score_acceptance_tests.cpp` |
| Continued play retains won/identity; terminal completion happens once | Same fixture continues through legal moves to game-over |
| Failed save/retry preserves one completion and its original metadata | `score_coordinator_tests.cpp`, `score_storage_tests.cpp` |
| Full history reopens; top five is derived; equal-score distinct IDs remain separate | Seven-round native lifecycle in `score_acceptance_tests.cpp` |
| Invalid/unsupported profiles and changed sources cannot be overwritten | `score_schema_tests.cpp`, `score_storage_tests.cpp` |
| Legacy migration validates first and preserves an exact original backup | `score_schema_tests.cpp`, `score_storage_tests.cpp` |
| Partial write, sync, close, backup and replacement failures preserve source/dirty state | Native storage pipeline with injected failures in `score_storage_tests.cpp` |
| No-op/clean checkpoints do not write; notifications are independent of disk success | `score_coordinator_tests.cpp` |

The real winning fixture uses 1,029 legal moves with normal spawning, reaching `won=true`,
`gameOver=false` and score 21,196. A saved profile at that point has best 21,196 and no completed
games. Continued moves keep the same round ID; history receives that ID only at logical
game-over. The test requires no search at runtime, private access, or production state hooks.

## Native graphical acceptance

The macOS SFML window was exercised through native keyboard and window-close actions. These
timing checks use a temporary application fixture compiled from `App.cpp` against the actual
game, score, session, coordinator, storage and UI objects. Production source files were not
changed to add test controls.

The fixture's differences are explicit: seed 9, a known player name, direct Playing startup,
seeded reset, checkpoint/overlay diagnostic traces, and an optional 196-move legal prefix.
The prefix leaves a playable board at score 2,104; the next Left move ends it at 2,108. Animation
durations and the production move/checkpoint/scene-command flow are unchanged. Diagnostic traces
show completion before restart/menu/close and no overlay transition before those actions.

Each isolated scenario used temporary score/settings files; sounds were disabled. Results were
checked against rendered state, process exit, serialized profiles, exact source bytes, and traces:

| Scenario | Observed result |
| --- | --- |
| Ongoing merge from a fresh round | Screen and disk best become 4 while completed history remains empty |
| No-op/idle, then early restart | Round score resets to 0, best stays 4, history stays empty; file bytes and modification time stay unchanged |
| Close and reopen after the ongoing best | Best 4 reloads visibly with an empty history |
| Terminal move then immediate restart | Terminal score is saved once before the overlay; a fresh round opens with the retained best |
| Terminal move then immediate Escape/menu | Terminal score is saved once before returning to Splash |
| Terminal move then native close | Process exits successfully; disk contains one completed round at 2,108 |
| Valid legacy profile at startup | Version 2 is published with stable legacy ID and a byte-exact CRLF `.bak` |
| Unknown version or corrupt JSON | Gameplay/restart/quit work; original bytes remain exact; repeated failed checkpoints emit one save diagnostic |
| Unwritable directory during an ongoing record | Original profile remains intact; visible in-memory best survives; restoring write permission and restarting publishes best without history |
| Unwritable directory during terminal move and restart | Completion stays once in memory; after permission recovery the previous round's ID/name/score are published once from the fresh round's checkpoint |

A separate in-process SFML event fixture supplies `[Closed, N, Left]` to the same application
loop. Only Closed is consumed; neither reset nor movement runs afterward. This is controlled
event delivery, not a claim that an OS input burst was captured after a physical close.

Local raw evidence, fixture source/diff and the result manifest are retained in the ignored
`build/score-acceptance/` directory. These diagnostic binaries are not shipped with the app.

## Data preservation and remaining boundaries

The repository's existing runtime files are unchanged after the acceptance pass:

```text
scores.json   af512314ed0354ce2ec5e51e80e81d22068ef645ad898da03b50e551d9f6e765
settings.json 635935c90ce697aace5af4e4f1eb4a48d6fdec1c43e5c9a9db941df23baa9147
```

- Native storage/graphical execution was verified on macOS; Windows runtime and reparse-point
  execution are not certified by this pass.
- At step 4.5, `scores.json` was still resolved from the working directory. Steps 5.2/5.3
  subsequently added native per-user paths and safe old-location import.
- The graphical fixture places assets beside its executable. Conventional macOS bundle
  resource lookup needed the platform pass: the old extra candidate resolved to
  `<app>/Resources`, whereas standard bundle resources live in `<app>/Contents/Resources`.
  Step 5.4 replaces that search with native bundle resource discovery and matching CMake layouts;
  current results are in [testing.md](testing.md).
- File sync and atomic visibility do not guarantee every power-loss outcome. Source snapshot
  checks are not a cross-process lock or compare-and-swap.
- Forced process termination, a disk error that persists through close, and simultaneous writers
  are outside the recovery contract. Notification flags were tested; muted graphical runs do
  not certify audible sound quality.

No score persistence correctness defect remained in the tested scope. Step 4.5 added permanent
acceptance tests, build registration and documentation; it did not require a production change.
