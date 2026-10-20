# UI quality audit — roadmap step 10

Status, 2026-10-14: **Step 10 complete within the defined local macOS scope, including
10.6.1–10.6.4.** The maintained graphical gate, actual VoiceOver operation, renewed
Playing pointer input and final focus regression pass. The separate step 11
checkpoint is also closed within its recorded local scope after automated gates
and the user's manual E11-03 confirmation; its evidence and limits are tracked
in [the step 11 plan](ui-edge-case-plan.md).
This is the inventory, quality contract and findings register for every UI change
from steps 1–9.
No application source, asset, dependency, build definition or permanent test was
changed in 10.1. Finding status is recorded below; this audit does not declare the
application free of defects or ready for an unrestricted production-quality claim.

Naming note, 2026-10-15: this audit preserves the pre-Numtilo checkpoint record.
References to current sources, commands, option names, binary identities, hashes
and proof paths below describe their recorded revision, not a new acceptance run.
The historical `SFML_2048_BUILD_UI_ACCEPTANCE` option is now
`NUMTILO_BUILD_UI_ACCEPTANCE`, requiring `NUMTILO_BUILD_APP=ON` and `BUILD_TESTING=ON`.
Use [the maintained testing instructions](testing.md#maintained-graphical-acceptance-step-1062)
for current builds. Historical counts, failures, manual confirmation and platform
limits remain unchanged; this documentation update provides no fresh UI evidence.

## Scope and evidence rules

The user's expanded scope covers typography, visual consistency, geometry,
interaction, focus/navigation, modal behavior, motion, readability and verification
quality across Splash, HighScores, Playing, Win and Game Over. Shared theme,
primitives, the fixed window/content view, App's UI routing, SceneInputGuard and
GameplayOutcome are included. The core, session/persistence model, profile paths,
migration and headless dependency boundary remain protected.

Keep Inter as the established primary font, the 920×660 client, 32 logical pixel
margins and 856×596 content space. Retain intentional hierarchy and approved normal
motion. Resolve measured defects within the relevant substep; App may change only
where a UI routing fix requires it, with round/checkpoint invariants verified.
Do not resize every text role merely because different roles use different sizes.

Use four explicit evidence levels:

- **S — source/inventory:** a code path or declared value was inspected.
- **R — controlled rendering/events:** current production drawing/scene/controller
  functions were exercised in an isolated fixture; this is not native input timing.
- **N — native:** Computer Use or explicitly authorized local macOS tools observed/
  operated a real window. Distinguish an unchanged application entry wrapper from
  instrumented App copies and correlate actual mouse/key/VoiceOver delivery with
  their recorded results.
- **P — pending:** the condition has no current successful verification. Older
  successful evidence remains historical rather than becoming a fresh run.

A successful build, backend unit suite or path/persistence platform test does not
establish graphical correctness. Assertion counts that include loops/polling are
not counts of distinct acceptance scenarios. Keep failed/inconclusive diagnostics.
A defect closes only with a recorded fix and a regression check. A policy gap closes
with a documented choice and verification; it cannot silently become a pass.

## Measurable quality contract

These are project acceptance benchmarks. W3C describes text contrast minima of
4.5:1 for ordinary text and 3:1 for large text, with exceptions for pure decoration.
Use specified opaque sRGB colors and the effective fitted size when selecting the
benchmark. This does not require a fade's invisible endpoint to retain contrast.
[W3C contrast guidance](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html).

Keyboard focus must have a persistent, visible indicator while focus remains on a
control. [W3C focus guidance](https://www.w3.org/WAI/WCAG22/Understanding/focus-visible.html).
These references guide this desktop audit; WCAG2ICT provides informative guidance
for non-web software and does not itself set conformance requirements.
[W3C WCAG2ICT](https://www.w3.org/TR/wcag2ict/).

| Gate | Required outcome |
| --- | --- |
| G01 — hierarchy/ownership | Each text role has an intentional size, weight, color, alignment and fitting rule. Equal sizes may serve distinct roles; unused tokens are not a reason to resize a screen. |
| G02 — containment/readability | Text and icon ink stay within their reserved area with deliberate padding at 1×/2×. Complete scores remain complete; name truncation is explicit. Adopt and verify a minimum effective readable size per role before approving stress-value fitting (A10). |
| G03 — settled contrast | Functional text meets the selected size-dependent contrast benchmark in normal/focus/hover states. Decorative differences are reviewed for consistency without calling them normative failures. |
| G04 — geometry | Stable card/board bounds, button hit regions, column alignment and view mapping. Pressed drawing may shrink while the logical clickable region stays stable. |
| G05 — pointer/keyboard contract | Activation, drag cancellation, unmatched release, repeated/held/modifier keys, Tab/Shift-Tab, background clicks, MouseLeft and scene crossing have explicit rules and matched visual feedback. No duplicate or unintended command. |
| G06 — focus/window lifecycle | Blur cancels pending activation; regain reconciles held keys. Focus/reset preserves names and live presentation where intended. Each full-client background region follows its agreed cancellation policy. |
| G07 — outcome routing | Navigation during ordinary, winning and terminal movement is handled intentionally. No swallowed command; settled Win priority, one Win per round, Continue identity and one completion remain correct. |
| G08 — motion | Existing 90 ms slide / 120 ms merge / 140 ms spawn, 100/60/90 ms button feedback, 180 ms entry and 850 ms +delta behavior retain endpoint, reset and interruption correctness. Floating text never blocks input/outcomes. |
| G09 — data/resources | UI changes preserve score/best/history, round identity, settings and authoritative asset/profile roots. Test tours use isolated profiles; the user's profile and Desktop bundle are untouched. |
| G10 — accessible operation | Sighted keyboard operation and native assistive-technology exposure are verified separately. Painted controls must not be presumed accessible merely because Tab works (A09). |
| G11 — repeatability | A lean graphical acceptance entry point must work from supported current builds and versioned inputs, without old ignored screenshots or hard-coded historical build directories (A07). Keep headless selection independent and add no UI framework. |
| G12 — honest closure | Record current-source 1×/2× coverage, native tours, open findings and platform scope. Hosted Windows/Linux UI remains step 12. Full production/accessibility claims remain open while their gates are unresolved. |

## Current typography inventory

Values are logical SFML character sizes; Regular/Bold are SFML styles. Centering
uses glyph bounds rather than the string's nominal rectangle. Uniform fitting can
reduce the effective size below the nominal value.

| Role | Current size/style | Alignment and fitting | Owner |
| --- | --- | --- | --- |
| Splash display title | 96 Bold | Centered | Splash; display role |
| Playing display title | 64 Bold | Top-left in sidebar | Playing; playing-title role |
| Leaderboard / Win / Game Over heading | 46 Bold | Centered; leaderboard centers trophy/title group | Three scenes; heading role |
| Splash / leaderboard / Win supporting text | 22 Regular | Centered | Scene subtitle role |
| Playing instructions | 18 Regular | Two lines, top-left; 48-high reservation | Playing description role |
| Splash field label | 20 Regular | Left with input | Label role |
| Input / placeholder | 26 Regular | Centered; fit within 296 | Input role / drawTextInput |
| Validation | 16 Regular | Centered; reserved slot; error color | Caption role / Splash |
| Empty leaderboard | 22 Regular | Centered in table | Subtitle role |
| Table headers | 19 Bold | Rank centered, name left, score right | Table-header role |
| Table names | 22 Regular | Left; ellipsis in 348-wide name column | Dedicated leaderboard-name role |
| Table scores | 22 Bold | Right; uniform fit to 160 | Dedicated leaderboard-score role |
| Table ranks | Medal: 18 Bold; 4–5: 20 Regular | Centered | Medal-number / leaderboard-rank roles |
| Player caption / name | 16 Regular / 22 Bold | Top-left; name ellipsis in 264 | Caption / player-name roles |
| Score-card captions / values | 20 Regular / 26 Bold | Centered at 30%/70% card height; width fit | Dedicated score-caption / score-value roles |
| Win inline score | 24 Bold | Centered; fit to 326, minimum 20 | Dedicated Win-score role |
| Action labels | 22 Regular | Centered label or centered icon/label group | Button role; icon group width-fits |
| Sound label | 20 Regular | Left, vertically centered in 24-high row | Label role |
| Score increase | 20 Bold | Centered below current card; fit to 120, minimum 16; timed rise/fade | Dedicated score-feedback role |
| Board digits | 46 through 64; 38 through 512; 32 for 1024; 30 for 2048+; all Bold | Centered, fit to 106, minimum 16; reports actual tile motion scale | Tile roles |
| Decorative digits | 40 below 100; otherwise 32; Bold | Centered and rotated with tile | Decoration roles |

The 96/64 display titles, 18-size Playing description, 16-size player caption,
medal rank treatment and inline-vs-paired score presentation are deliberate.
Tile tiers are existing approved behavior; 1024 and 2048 having different tiers
is not itself a defect. `kLargeButtonSize`, `kModalHeadingSize` and
`kCompactButtonSize` have no current consumers; no automatic cleanup is required.
The 10.1 role coupling in A05 was maintenance debt; separating its ownership in
10.2 does not require making 26/22/24 equal.

10.1 baseline SFML fitting measurements (before the 10.2 numeric reservation change):

| Input | Available width | Nominal size | Effective size |
| --- | --- | --- | --- |
| Playing card 2147483647 | 100 | 26 Bold | 16.05 |
| Game Over card 2147483647 | 131 | 26 Bold | 21.02 |
| +2147483647 feedback | 100 | 20 Bold | 14.29 |
| 18 W characters in input | 296 | 26 Regular | 16.48 |
| INT_MAX tile render stress | 86 | 30 Bold | 13.87 |

The last tile row is a drawing-helper stress value, not a legal non-power-of-two
board tile. Fresh board views also contain legal ten-digit tile 1073741824.
Containment alone does not close the readability decision in A10. Use actual
fitted sizes for contrast: the held green +delta is 4.23:1 against the sidebar,
which clears the large-text benchmark at nominal size but falls below 4.5:1 when
long values fit to 14.29. The normal primary hover label is 4.531:1; compare exact
values rather than rounded ratios. The brown focus outline against menu/sidebar/
modal surfaces measures 4.87/4.20/5.31:1; no invisible-focus defect was confirmed.

## 10.1 coverage baseline

| Area / steps | Existing retained evidence | Fresh 10.1 evidence / remaining gate |
| --- | --- | --- |
| Theme/components/shell — 1–2 | Component gallery, view/margin mapping; current shared styles and step-9 endpoint checks | Source role/color inventory; fixed-view preservation in 22 new frames. Full shared-state acceptance belongs 10.2. |
| Splash — 3 | Normal/error/corrected/focus/blur/tab/long names/Unicode spaces; native Start/Scores | 1×/2× empty, error, 18 W and unsupported-glyph views; native menu/AX snapshot. G05/G10 and glyph policy remain open. |
| HighScores — 4 | 0–5/capped rows, medals, alignment, Turkish/long/control names, INT_MAX, Back/reset; native Back | 1×/2× empty and five-row stress views. Renew all row counts and interaction/text limits in 10.3. |
| Playing/board/input — 5–6 | Empty/full/gallery, four-direction motion, edge merge, name/score/sound/focus/direct controls; native New Game/Main Menu | 1×/2× board/name/max-score view, font fitting, native Playing/AX; real-move routing proof exposes A01. Full state matrix belongs 10.4. |
| Win — 7 | Continue/new round, first-Win/higher tiles/terminal priority; normal 1×/2× reveal/buttons; native both actions | 1×/2× zero/INT_MAX current renders. Renew ordinary/eight-digit/fitted value transitions and routing in 10.5. |
| Game Over — 8 | Full-client dim, paired cards, native Replay/Menu, one completion/reopen; earlier 1×/2× score stress | 1×/2× zero/INT_MAX current renders; four margin-cancellation proofs expose A03. Renew view/interaction/transition combinations in 10.5. |
| Motion — 9 | 88 views, nine buttons/18 release routes, 176 +delta checks, 173 reveals, 690 lifecycle polling checks | Evidence reviewed, not counted as a fresh rerun. Static inventory does not revalidate timed endpoints; combined gate remains 10.6. |
| Native accessible controls | Ordinary pointer/keyboard tours passed previously | The fresh 10.1 macOS AX snapshots exposed window/menu chrome only, without named custom controls (A09); no VoiceOver tour was performed in 10.1. The subsequent provider and actual local VoiceOver closure are recorded in 10.6.3/10.6.4 below. |
| Build/headless/resources | 255 unit cases / 282,023 assertions; five path/resource/persistence scenarios in 9.4 | Source hashes retained; suites not rerun for this documentation-only inventory. They are not graphical tests. |
| Windows/Linux graphical behavior | No hosted UI acceptance yet | Pending step 12; macOS density checks are not cross-platform proof. |

The latest 9.4 main layout harness is 1×; its 2× checks cover normal reveals,
button feedback and +delta rather than every edge view. Historical Win/Game Over
stress fixtures remain useful, but need current-source compatibility review and
rerun before typography closure. New 10.1 frames are baseline observations and
view-preservation checks, not the complete future acceptance matrix.

## Findings register

Priorities describe impact, not a claim of security severity. A01/A02 and A13 are closed by 10.4; A04 display policy is applied across all three owners. A03/A10 and A14 are closed by 10.5. A15 is closed by 10.6.1's shared-control application. A07 is closed by the fresh maintained 10.6.2 gate. A08/A09 are closed within the defined local macOS acceptance scope by 10.6.3/10.6.4; other-platform and IME/shaping gates remain separate.

| ID / class | Verified baseline observation | Impact / evidence | Current status / closing requirement |
| --- | --- | --- | --- |
| A01 — high, behavior defect | Playing resets visuals before returning RestartGame/ShowSplash; App's post-handler outcome check then opens Win/Game Over and discards that command during the final move. | Ten command-loss cases across a legal seed-0 win at 21196 and seed-9 terminal move at 2108. N, Escape, unfocused Enter and focused New Game/Main Menu Space are affected. Ordinary controls and Sound are comparison passes. | **Closed, 10.4:** App owns checkpoint/reset after accepted Restart/Menu navigation; pre-handler settled outcomes retain priority. Current 64-route/profile matrix passes, and the entry-baseline negative control reproduces all ten original losses. |
| A02 — medium, readability defect | Opaque white tile labels: 8=1.85:1, 16=2.24:1, 32=2.64:1. Small fitted green +delta has only 4.23:1. | Specified RGB calculation plus current rendered palette/fitting. Tile 64 is 3.19:1; it is not grouped with the failing large-text pairs. | **Closed, 10.4:** tiles 8/16/32 use primary dark text (7.164/5.914/5.033:1); feedback green clears 4.923:1. All representable power-of-two tile values and sampled long deltas meet the 16 floor at both render densities. Tile 64 retains its valid large-text treatment. |
| A03 — medium, cancellation inconsistency | Game Over dims full client but App filters mouse events in the 32-pixel margins. Inside-view empty clicks cancel held Replay; outer-margin clicks do not. | Tab → hold Space → click background → release Space: all four margins return RestartGame, inside point returns None. This is pending-command cancellation, not background dismissal. | **Closed, 10.5:** primary GameOver margin presses/releases reach its existing cancellation handler. All 16 controlled margin combinations and four native held-Space margin sequences cancel without dismissal; input remains exclusively modal. Full dim and existing Win inset policy are preserved. |
| A04 — medium, input/render gap | Names accept Unicode scalars absent from Inter. Chinese 你/中 and emoji 😀/🧩 are stored in the scene's name and rendered as missing-glyph boxes. | Fresh `font.hasGlyph`, 1×/2× input/Playing/table renders. Turkish İ/ş/ğ/ç/Ö/Ü and ellipsis are present. This is controlled rendering; native IME behavior is not verified. | **Display policy complete, 10.3–10.4:** new input validates glyph visibility/readability; existing table and Playing names use explained display-only projection. Playing stays nominal 22 with ellipsis and restores its ordinary caption. Stored UTF-8 identity is retained. Native IME remains a separate unverified readiness gate. |
| A05 — low, semantic maintenance debt | Card values inherit input size; table names/scores inherit button size; Win has a dedicated score role. | UiPrimitives score-card/table code and role inventory. No evidence requires making all sizes equal. | **Closed, 10.2:** dedicated card/table/Win/feedback roles retain nominal sizes. Shared-component, all-scene 1×/2× render comparisons and control/motion reruns passed. |
| A06 — medium, pointer policy gap | Commands activate from a release hit alone; visual drag-off cancellation is independent. Back fires without a matching press and after drag-off/return. | Existing retained behavior reproduced; step9 preserved this contract, so this is not a claimed regression. Pointer ownership across scene transitions is not tracked. | **Closed, 10.2:** shared gesture ownership drives feedback and activation; scene guard rejects stale releases. Controlled mapped routes, cancellation/reset checks and current native tours passed (details below). |
| A07 — high, regression-gate gap | Graphical harnesses and baselines exist only under ignored build paths; runner depends on historical build internals/older ignored PNGs. | CMake registers unit/platform checks only. Existing native/modal evidence is accurately scoped but not a maintained graphical gate. | **Closed, 10.6.2:** versioned worker/standard-library runner, production font and supported system/vcpkg presets. A fresh system Release build passes 772 checks and 102 1×/2× PNGs without historical harness inputs. The option defaults OFF; independent headless acceptance passes. |
| A08 — medium, coverage gap | Existing latest 2× stress coverage is partial; short-motion proof is controlled rendering, not native screenshots. | Coverage matrix above and old/new fixture sources. Fresh 22 frames expand baseline but do not close every state/combination. | **Closed locally, 10.6.4:** the maintained five-scene 1×/2× matrix passes, with 20 input and 20 independently checked visual control/density records. Fresh physical Playing New/Sound/Menu and menu/table navigation delivery, ordinary keyboard regression, actual VoiceOver/modal tours and isolated-store reopening pass. Earlier coordinate-tool failures remain diagnostics. The separate step 11 edge tour has not started. |
| A09 — high, accessibility-readiness gap | Observed native Splash/Playing accessibility trees expose only window/menu chrome; input/actions/toggle/board values have no observed names/roles/state. | Fresh saved macOS AX trees and screenshots using unchanged app::run through isolated wrapper. No full screen-reader run or Windows/Linux assistive-tech claim. | **Closed locally, 10.6.3:** the public AppKit provider exposes scene-owned controls/full values, ranking/board cells, Name/error information and board moves through bounded tagged requests. Actual VoiceOver validation/readback, nine action activations and a board move pass, with separate final repeated-click/focus proof. Eight portable cases / 456 assertions pass. Windows/Linux native AT and broader IME/shaping remain step 12. |
| A10 — medium, readability-policy gap | Uniform fitting gives functional values around 13.87–16.48 logical character-size equivalents; no minimum readable-size policy is recorded. | Fresh font metrics above. Current inspected stress views fit; small size is a policy/quality gap rather than a clipping claim. | **Application complete, 10.2–10.5:** shared/menu/table/Playing/modal owners meet their recorded floors. Win explicitly applies minimum 20 and sampled full scores stay 24; GameOver sampled long card values reach 22.351, above 16. Complete strings, restoration, contrast and both densities pass; accessibility/readiness remains a separate gate. |
| A11 — medium, invalid-field focus ambiguity | Invalid state overrode focus color, leaving the focused/unfocused invalid name field visually identical. | Current 10.3 source review and normal/Retina error→focus/blur renders; native brown-focused/red-unfocused observation. | **Closed, 10.3:** focus outline has priority; red validation caption stays visible, shape/hit padding unchanged. |
| A12 — low, silent name-length boundary | The 19th scalar was ignored without feedback, including 18 whitespace followed by a visible character. | Current 10.3 event proof; first policy draft still missed whitespace-only cap feedback and was repaired before closure. | **Closed, 10.3:** neutral 18-character message on rejected extra edit; accepted buffer preserved, valid 18-character Start remains allowed, error messages retain priority. |
| A13 — medium, adjacent merge overlap | Two adjacent 110-wide merged tiles at peak 1.17 become 128.7 wide on 126-spaced centers, overlapping by 2.7 logical pixels. | Entry source-bound peak calculation confirms overlap; final four-direction real merges and 1×/2× peak references verify the fix. Isolated pop retains its established clearance. | **Closed, 10.4:** adjacent merge amplitudes ease smoothly to 122/110, preserving a 4-pixel gap. Independent whole-board peak comparisons pass; isolated 1.17 and all stage durations/endpoints remain. |
| A14 — low, focus-dependent card layout | GameOver derives card placement from Replay's outline-inclusive bounds; focus grows border 2→3 and shifts both cards upward 0.5 logical pixels. | Clean entry scene renders reproduce the shift at 1×/2×; final fixed-position card and 8 px shadow-region comparisons cover focus/hover/press. | **Closed, 10.5:** anchor above the fixed button face with the normal border reservation, preserving default placement and removing the shift. |
| A15 — medium, focus-dependent hit geometry | Outline-inclusive bounds plus padding make adjacent modal hit regions overlap 1 px when focused; logical 404 changes from lower action to upper. The shared helper also affects five other action buttons and Playing Sound row. | Four clean entry-modal negative controls, current 403/404/band-edge checks, source review of remaining consumers. | **Closed, 10.6.1:** shared bounds use the exact normal SFML outline independently of focus. All nine action buttons retain their existing extra padding (4 for menu/table/modals, 0 for Playing); Sound retains its normal panel outline with no extra padding. 682 controlled scenarios pass at both densities; entry reproduces 96 wrong commands and current has zero. Modal 403/404 ownership and all 68 paired render states remain unchanged. Broader native Playing pointer renewal passed in A08/10.6.4; its separate proof is recorded below. |

Additional review item D01, **closed in 10.3**: decorative 2/4/2048 now select
text from the same tile palette as the board; geometry/fills/rotations are retained.
The former white/yellow 2048 and legacy gray 2/4 were consistency choices, not normative
decoration failures. Tile 8 retained the existing text color in 10.3; A02 changes it for both
the board and shared menu decoration in 10.4. The menu tagline is now Turkish, “Sayıları birleştir”. Win's
inset backdrop remains an intentional choice for 10.5 review.

### Source anchors and reproduction limits

10.1 baseline A01: `PlayingScene::activateControl` (PlayingScene.cpp:206) and App's post-handler
`updateGameplayScene` (App.cpp:424). The isolated proof calls current App/scene/
controller functions and reproduces that ordering; it does not run bootstrap or
native pointer timing. It checks 18 actual-move routes, ten lost-command cases and
sound/ordinary controls. A03: App.cpp:399 viewport filtering, GameOverScene.cpp:73
pending activation reset and :143 full-client rendering. Coordinates: margins
(2,330), (918,330), (460,2), (460,658); empty content (40,40).

A02/D01: UiTheme.hpp palette and SplashScene.cpp:17; A04: SplashScene.cpp:184 and
UiPrimitives.cpp:188 fixed-string glyph checks; A05: UiPrimitives.cpp:670/:753;
A06: ButtonFeedback cancellation plus all scene release handlers; A07:
CMakeLists.txt:424–489 and the historical runner/layout fixtures; A09: native AX
files, not an inference from screenshots alone.

## Substep work packages

| Substep | Expanded assignment | Exit evidence |
| --- | --- | --- |
| **10.1 — inventory and audit contract** | Whole-UI source/evidence review, typography/color/state inventory, controlled defect proofs, fresh representative views, prioritized register and invariants. | This document, source-preservation report, 22 1×/2× frames, font/contrast reports, two native AX states and isolated routing proof. Complete. |
| **10.2 — shared components and rules** | A05 roles; A02/A10 readable text policy; A06 pointer/feedback contract; accessible-control feasibility for A09; shared focus/geometry/DPI review. | Explicit rules and targeted common-component checks at 1×/2×; stable logical hit regions and backend boundary. |
| **10.3 — Splash and HighScores** | A04 glyph/input/display handling, D01/copy review, validation, truncation, empty/1–5/capped rows, medals/columns, focus/navigation. | Current-source full menu/table stress and interaction matrix at both densities. |
| **10.4 — Playing, board and motion** | A01 command-loss fix; A02 palette/+delta; A04 player display; A10 score/tile limits; all earlier panel/board/toggle/motion invariants. | Real ordinary/win/terminal move routes, every tile tier/long value, sound/settings and score feedback checks at both densities. |
| **10.5 — outcomes and backdrop** | A03 cancellation, both score/heading/icon hierarchies, Win/new-round/Continue and Game Over/Replay/Menu; full/inset coverage policy. | Zero/normal/eight-digit/INT_MAX modal matrix, fitted-value restoration, margins, preserved round/best/history and terminal priority. |
| **10.6 — combined acceptance and closure** | A07 repeatable graphical gate; remaining A15 shared hit geometry; close A08 matrix and A09 accessible-operation gate; native keyboard/pointer/blur/scene crossing; required existing checks and evidence report. | Findings explicitly resolved and regressions passed. Any remaining readiness/platform gap stays visible; no blanket production/accessibility claim while a required gate is open. |

The user subsequently split the remaining 10.6 work into four bounded substeps.
All four 10.6 packages are complete within their recorded local scope as of 2026-10-14.
The user authorized all remaining step 10 work and excluded step 11, which has not
started. Other-platform acceptance and controlled build cleanup remain separate.

| Substep | Assignment | Exit evidence |
| --- | --- | --- |
| **10.6.1 — stable shared control geometry** | Close A15 for Splash Start/Scores, HighScores Back and Playing NewGame/MainMenu/Sound; consolidate the existing modal rule. | Stable normal-border hit bounds, matching pointer feedback, preserved focus styling/padding and targeted before/current interaction/render comparisons. |
| **10.6.2 — maintained graphical acceptance** | Close A07 with a small current-build acceptance entry point and versioned inputs. | Fresh supported build run without dependence on historical ignored harnesses; headless boundary retained. |
| **10.6.3 — native accessible operation** | Implement and verify native control exposure and screen-reader workflows on the available macOS platform for A09. | Actual roles/names/state/actions and assistive-technology operation; Windows/Linux native proof remains step 12. |
| **10.6.4 — combined acceptance and closure** | Close A08 and run the current-source combined view/input/window lifecycle matrix. | Required checks, native tour, findings register and honest readiness/platform scope. |

The current authorization covers all remaining step 10 work and excludes step 11.
The original stage 10.1 established work and proof; it did not implement the fixes
assigned above or authorize silently waiving a finding.
Hosted Windows/Linux UI remains step 12. If a later fix exposes a backend defect,
record it separately and preserve this UI audit's implementation boundary.

## Evidence retained for 10.1

Under `build/ui-audit-step-10.1/` (ignored local artifacts):

- `before.json`, `source-report.json`, `summary.json`: entry/source inventory and
  scope preservation, with documentary changes separated from runtime sources.
- `interaction/interaction_proof.cpp`, build/run logs, `interaction-proof.json`:
  6,312 successful assertions demonstrating current behavior, not fixed behavior;
  includes 18 routes, ten lost commands and all four margin comparisons.
- `font_layout_probe.cpp`, build/run logs, `font-layout-report.json`, `baseline/`:
  actual Inter glyph/fitting metrics and 22 new static current-source renders.
- `contrast-report.json`: specified-color ratios using the sRGB luminance formula.
- `audit_app.cpp`, temporary bundle/profile, `native-menu-ax.txt`,
  `native-playing-ax.txt`, two screenshots and exit-zero record: a small isolated
  wrapper around production app::run, with no production seed/trace hooks.

The step9.4 evidence remains under `build/ui-motion-step-9.4/` and is labeled prior
acceptance. Ten-step closure requires maintainable fresh verification, not simply
adding together historical assertion counts. No commit, push, branch change,
Desktop replacement or real-profile write occurred in this inventory.

## 10.2 — shared rules and components

Completed on 2026-10-13. The existing hierarchy, Inter resource, outer card/board/
button bounds, content view, palette and motion timings are retained. This substep
changes shared input ownership and text fitting; it does not fix A01/A03 in App,
change core/session/storage, or implement an assistive-technology provider.

### Pointer contract

A primary press inside a control owns one gesture. Only its matching inside release
activates, once. Leaving the logical hit region cancels permanently for that gesture,
even after returning inside. MouseLeft, blur, input reset and scene crossing cancel;
a fresh valid press/release recovers. Right-button activity is neutral. Logical hit
bounds and existing padding stay stable while feedback shrinks the drawn button.
`PointerActivation` owns this rule; `ButtonFeedback` samples the same ownership for
presentation and returns pointer activation to scenes. Name input and sound row use
the same helper. `SceneInputGuard` rejects unmatched/stale primary releases across
scene/window lifecycle changes. A primary press cancels any pending keyboard
activation; held/repeated keyboard behavior and modifier rules are retained.

### Text ownership and readability

Dedicated card caption/value, leaderboard name/score/rank, Win-score and score-
feedback tokens replace unrelated input/button/label references, retaining the
approved nominal sizes and styles. These project floors are logical SFML character-
size equivalents; they are not a physical point-size or accessibility certification.

| Role | Minimum effective size | Overflow rule / application status |
| --- | --- | --- |
| Input | 16 | Full accepted input, bounded width; shared helper verified. Field/glyph policy remains 10.3. |
| Label / caption | 18 / 16 | Intentional fixed labels; fitted caption reports a miss. |
| Action | 18 | Bounded copy centered with its icon; report includes pressed drawing scale. |
| Card value | 16 | Keep complete number; bounded fit and explicit failure for infeasible reservations. |
| Leaderboard score | 18 | Full score, right aligned; renewed all-row-count acceptance belongs 10.3. |
| Leaderboard / player name | 22 / 22 | Single line and explicit ellipsis, retaining original stored identity; screen policy 10.3/10.4. |
| Win score | 20 | Complete inline value; application review 10.5. |
| Score increase / tile | 16 / 16 | Complete numeric content; fitting/color application remains 10.4. |

`fitTextWidth` restores scale on reuse, never enlarges above nominal, and returns
both effective size and `meetsMinimum`. The floor does not force overflow in a slot
that cannot satisfy it. Empty/missing-font and nonpositive-width cases are explicit.
Its guarantee is local horizontal ink for ordinary unrotated text; height, rotation,
scaled shapes and external rendering transforms require separate caller checks.
Input/button drawing centers copies from glyph bounds without mutating source
labels. Display-only ellipsis clears inherited scale before measuring.

A legal ten-digit score, 2000000000, measured 15.57 in the former 100-wide reservation.
Within the same 124-wide card, numeric inset is now 4 per side (116 reserved width);
caption inset stays 12. Sampled wide valid values, including INT_MAX and digit-heavy
values, measure at least 17.64 at both densities. Short scores retain nominal 26.
This is sampled-domain evidence, not enumeration of every score. Long numbers remain
complete, and an intentionally too-narrow card explicitly fails readability rather
than clipping to enforce a floor. The Game Over value reservation likewise grows
from 131 to 147 within its existing card.

Settled functional text follows the existing size-dependent contrast benchmark,
using the effective fitted size. A smaller number cannot inherit the nominal large-
text allowance. Color changes for A02 remain 10.4: 8/16/32 and fitted +delta must clear
the selected benchmark, including their minimum size. Focus must remain visible;
pure decoration retains its documented exception and D01 review in 10.3.

### Native accessibility assessment — A09 remains open

Native exposure is feasible without a new UI framework. SFML's public system handle
provides the attachment point; a desktop-only semantic snapshot/action boundary can
keep core/headless code independent. A09 is not closed by this feasibility result.

| Host | Viable attachment | Required implementation / validation |
| --- | --- | --- |
| macOS | AppKit accessibility elements retained under the NSWindow content view, Objective-C++ desktop adapter | Public attachment spike, roles/labels/values/focus/actions/editing and notifications; actual VoiceOver workflow. |
| Windows | HWND UI Automation fragment provider, preserving window-message handling | COM/thread/lifetime handling, Invoke/Toggle/Value/Text patterns, events; native SDK build and Narrator/NVDA workflow. |
| Linux | AT-SPI D-Bus application/object tree | Select/package a desktop-only transport, registry lifetime, Accessible/Component/Action/Text interfaces and events; native host and Orca workflow. |

Official implementation references: [Apple custom controls](https://developer.apple.com/library/archive/documentation/Accessibility/Conceptual/AccessibilityMacOSX/ImplementingAccessibilityforCustomControls.html),
[Microsoft provider exposure](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-howto-expose-serverside-uiautomation-provider),
[GNOME AT-SPI](https://github.com/GNOME/at-spi2-core/blob/main/README.md).
These are implementation options assessed against local SFML 2.6.2 and the current
application, not APIs already implemented by this project.

Shared semantics must expose full names/values independently of painted ellipsis,
validation/required state, toggle state, heading/table relationships and 16 stable
board cells including empty cells. Publishing a grid alone does not prove playable
screen-reader controls: directional operation and name editing need actual host-
assistive-technology verification. Actions must pass through scene validation and
App dispatch on the application thread, reject stale scene/round requests and modal-
background actions, and detach before window destruction. Notifications must be
bounded rather than emitted every animation frame, with correct client/screen/DPI
coordinates. No private SFML view replacement or synthetic-input workaround is
approved by this assessment. Provider implementation plus real screen-reader tours
remain a readiness gate; current sighted keyboard tours are separate evidence.

### Current-source verification and closure

Release build, formatting/whitespace and the registered unit/platform suite passed:
255 unit cases / 282,023 assertions and five native path/resource/persistence
scenarios. They protect their own boundaries; they are not graphical acceptance.
The following local controlled fixtures ran sequentially against current UI code:

| Evidence | Current result / scope |
| --- | --- |
| Pure pointer/scene guard | 13 scenarios, 35 checks; matching release, cancellation and scene ownership. |
| Shared text/components | 204 checks at 1×/2×; ten sampled scores per density, complete values/readability, reuse/empty/missing-font/narrow slots, input and icon/label copies. |
| All action controls | Nine buttons × ten pointer contract checks, 18 focused Enter/Space release routes; hover/depression/reversal/reset and 2× drawing. |
| Name input / sound row | Two controls × 12 scenarios × two mapped densities: 48 scenario runs, 317 checks; actual text focus/entry and sound commands. |
| Entry presentation | 173 checks: five scene groups, view preservation, opacity, reset/reentry and 1×/2×. |
| Score feedback | 176 checks: real merge deltas, hold/rise/fade/expiry, rapid moves, input/reset and 1×/2×. |
| Combined lifecycle | 689 time-dependent checks: first Win/Continue/higher tiles, new round, terminal priority, one Game Over completion, Replay/Menu and profile reopen. |
| All-scene static baseline | 22 fresh 1×/2× PNGs: 12 menu/table frames identical to 10.1; the other ten differ only in widened long-score glyph reservations, including cards behind modals. |

The strengthened component comparison subtracts an identical blank card/input over
the entire render surface, including antialiased edges and an expanded-area guard.
Horizontal inset and both-axis centering pass with at most one logical pixel of
allowed raster fringe; measured maximum fringe is 0.5 and centering error 0.40.
This is sampled coverage, not an exhaustive score-domain or independent vertical-
clearance proof. The font probe now caps fitted size at nominal rather than labeling
an unconstrained width ratio as effective size.

One initial rapid-feedback diagnostic required exact equality between independently
timed glyph bounds. It was retained and corrected to one logical pixel per density,
keeping the existing separation and 3% ink-coverage checks. Score-feedback timing
and rendering behavior did not change; only semantic role-token references changed. Existing ignored fixture locations/build-internal
linking are still a repeatability gap (A07); no permanent graphical target is added
in 10.2, and the complete broader matrix remains 10.3–10.6.

Native Computer Use activated all nine current action buttons, plus the name field
and sound row. Splash/Playing drag-off cancellation preserved the menu/live score,
and fresh matching clicks recovered. The ordinary tour calls production `app::run`
through an isolated paths-only entry wrapper. Modal tours use ignored current-App
copies with public legal deterministic move prefixes and trace/profile recording;
production has no seed or trace hook. A real final Left reaches Win at 21,196.
Pointer Continue preserves grid/score/round; Win New Game resets to zero and retains
best/name with no completion. A real final Left reaches Game Over at 2,108. Replay
resets to zero with exactly one completed round; Main Menu → Scores displays one
Çağrı/2,108 row. Saved traces/profiles pass 52 invariants, and all seven test windows,
including two diagnostic Continue attempts, exit zero. A separate native Tab/Space
Continue also passed; it is sighted keyboard evidence, not a screen-reader tour.

The first Continue window had ScreenCaptureKit -3811 and `noWindowsAvailable` tool
errors; rebinding/raising did not restore pointer input. A fresh identity also had a
failed coordinate attempt, although keyboard Continue worked. Both are retained as
diagnostics. A further fresh executable/bundle identity successfully dispatched the
pointer Continue; its screenshot capture then failed -3812 and succeeded on retry,
showing the preserved board/score. No production fix or claim about the underlying
tool's reliability is inferred. The successful current pointer trace and observed
final screenshot close that route's acceptance independently of the failed attempts.

Entry/current hashes prove changes are limited to 11 existing UI source/header
files, one new UI gesture helper and four documents. App dispatch, core/session/
storage/paths/migration, assets/dependencies/build/CI and permanent tests are
unchanged from entry. The user's profile and Desktop bundle were not used. Evidence
is under `build/ui-quality-step-10.2/`: verified before copies/hashes, scoped diff,
source/component/input/native reports, logs, PNGs, isolated profiles and retained
diagnostics. A05/A06 are closed; A02/A10 policy and A09 feasibility are complete,
while their remaining application/readiness gates stay open. A01 is still 10.4 and
A03 is still 10.5. The next substep is **10.3 — Splash and HighScores**.

## 10.3 — Splash and HighScores

Completed on 2026-10-13. This substep applies the shared contract to the menu and
leaderboard, including additional measured input/focus edge cases. Inter, the fixed
client/content view, input/action/card sizes, hit padding, entry timing and stored
identity remain established. The Turkish tagline uses the existing glyph-centered
header/form flow; its changed glyph height can slightly reposition that centered
group. Decoration positions, rotations, sizes and fills are retained.

### Name policy and field behavior

Accepted new names retain the existing 18-Unicode-scalar buffer limit and Unicode
edge-whitespace trimming. Their complete trimmed display must contain visible,
supported Inter glyphs and fit the 296-wide input at effective size 16 or higher.
Font coverage alone is insufficient: Inter contains zero-ink U+200B/U+FEFF, and
wide supported symbols can violate the floor even with only 18 scalars. The matrix
includes single/mixed invisible characters, CJK/emoji, Turkish letters, 18 W,
18 per-mille/per-myriad symbols and wide arrows.

`presentPlayerName` is a display-only projection. Recognized whitespace becomes
single-line spaces; unavailable or zero-ink non-whitespace characters become `?`.
An otherwise blank legacy name gets a visible `?` fallback. Projection is lossy and
does not provide font fallback or full Unicode shaping. Original input and stored
UTF-8 identity remain separate and unchanged; no silent transliteration or profile
migration is introduced. New names with replacements cannot issue StartGame.

Invalid edits remain in the original buffer so Backspace can correct them. The field
shows an explicit unsupported-character or too-wide message. A too-wide edit uses a
nominal-size ellipsis preview instead of shrinking below the readable floor; it is
still rejected by Start. Accepted input remains complete and width-fitted. Each edit
restores the full projection before measuring, so long/invalid → short/valid recovery
clears errors and restores rendering without losing the authoritative text. Start
validates through every direct/focused/pointer route; entering the menu again also
refreshes validation after a Scores visit.

Attempting a 19th scalar shows a neutral length-limit message while preserving the
accepted buffer. A valid 18-character name can still start. Empty, unsupported and
width errors have priority over this notice. An intermediate draft missed the
18-whitespace → visible-19th case; independent review caught it, the message was
moved outside the nonempty branch, and the final matrix verifies both ASCII and
Unicode whitespace with deletion/re-entry recovery.

An invalid focused field uses the brown focus outline; its error caption remains
red. On blur/background focus loss the invalid outline is red. The shape, outline
thickness and logical hit reservation remain unchanged. This separates focus from
validation without adding a caret/editing framework. Existing append/Backspace
editing remains; native IME and assistive-technology editing are separate gates.

### Leaderboard, decoration and copy

Table rows use the same projection before nominal-22 ellipsis. Complete scores stay
right aligned in their existing 160-wide column with the 18 minimum. Empty/1–5/capped
rows retain medals, rank treatment, fixed geometry and header/name/score alignment.
If a visible row needs replacement, the supporting line explains that `?` represents
characters the font cannot display. It restores the normal description when no
visible row needs replacement. An unsupported hidden sixth row cannot change the
five-row rendering or explanation. Rendering leaves original names, scores, dates,
IDs and vector order unchanged.

The decorative 2/4/2048 now use the board's text palette through one shared lookup;
board color behavior is identical. Decorative 8 retains the current palette pending
A02's functional tile change in 10.4. The supporting menu copy is Turkish,
“Sayıları birleştir”. These resolve D01/copy consistency without altering the
established heading/decoration hierarchy. Fifteen settled functional menu/table
color pairs clear the selected text/focus benchmark; the lowest text ratio is
4.5315:1 for the existing primary hover label. Decorative exceptions and timed fades
retain the audit's prior scope; this does not close Playing contrast A02.

### Verification and limits

The final controlled current-source matrix passes **1,725 checks and 74 static PNGs
at 1×/2×**, including 48 mapped pointer scenario runs over Name/Start/Scores/Back.
It covers validation/recovery, explicit limits, forward/reverse focus, modifiers,
held/repeated releases, blur/reset/scene crossing, drag cancellation, empty through
capped table rows and immutable original entries. Whole-surface RGBA differences
include antialiased edges and verify horizontal/vertical field/caption/column
reservations, text centering, full numeric strings and right/header alignment.
The initial 1,477-check run is retained separately from the final repaired run.
Re-entry snapshots first draw then settle for 320 ms; they are static state evidence,
not a timing measurement. The independent current reveal fixture passes 173 checks.

Twenty-two fresh whole-UI baseline PNGs compare with 10.2: the ten Playing/Win/Game
Over renders and two empty-table renders are byte/pixel-identical. The other ten
contain the intentional menu copy/decorations/focus/name-projection and legacy-table
projection/description changes. Release build, formatting/whitespace, 255 unit cases /
282,023 assertions and five native platform scenarios pass. No new permanent test
or dependency is introduced; A07 still belongs to 10.6.

Native macOS Computer Use observes empty validation, field/background pointer
focus, brown-focused/red-unfocused error treatment, complete ASCII 18-W input with
19th-character notice, keyboard Scores → Back → Start and the resulting Playing
view. The five-row isolated profile contains Turkish, long, unavailable and invisible
names; its visible projection/explanation, complete long scores and ordering are
observed. The first two windows exit zero and the profile bytes, five original identities,
completed history and INT_MAX best remain exactly unchanged. This uses production
`app::run` with a paths-only wrapper, without seed/trace hooks or the user's profile.

Tool limits are explicit: one split keyboard sequence reached Scores rather than
the intended edit state, and later coordinate actions failed `noWindowsAvailable`.
Those attempts are not counted as successful action evidence. A fresh executable/
bundle restored field/background pointer input; later Scores pointer input failed
again while keyboard routing worked. Current Name/Start/Scores/Back pointer contracts
pass the controlled 48-scenario matrix; fresh native pointer Start/Scores/Back renewal
remains for combined acceptance. Tool `typeText` of CJK did not produce visible SFML
input, so native unsupported editing/IME is unverified; controlled Unicode
TextEntered validation passes. AX still exposes OS chrome, not custom semantic
controls. No full IME, screen-reader or Windows/Linux graphical claim is made.
Diagnostics and successful observations are retained separately. A final bounded
pointer-tour attempt used an ignored current-App copy with trace-only recording;
the tool reported `windowNotFoundAtPosition`, and its trace contains ready state
without any button command. That diagnostic window also exits zero with unchanged
profile bytes. It is not a successful pointer tour; production has no trace hook.

Changes are limited to five existing UI source/header files and four documents.
App dispatch, Playing/Win/GameOver sources, core/session/storage/paths/migration,
assets/dependencies/build/CI and permanent tests are unchanged from entry. Evidence
is under `build/ui-quality-step-10.3/`: before hashes/copies, source/scoped diff,
menu/table/contrast/baseline/native reports, logs, state PNGs and the isolated profile.
A04 menu/table and A10 menu/table application are complete; A04 player display,
A01/A02 and Playing limits remain 10.4. A11/A12 and D01 are closed. A03 remains 10.5;
A07/A08/A09 and combined native/IME readiness remain open. The next substep is
**10.4 — Playing, board and motion**.

## 10.4 — Playing, board and motion

Completed on 2026-10-13. A01/A02, Playing's A04/A10 application and the newly measured
A13 adjacent-merge defect are resolved within this substep. Inter, 920×660 client,
32-pixel margins, board/sidebar/card/button geometry, round/storage model and the
existing motion durations remain established. Earlier findings and native-tool
limits remain recorded; this is not a full production/accessibility certification.

### Navigation ownership and outcomes

Playing returns RestartGame/ShowSplash intent without clearing animation effects.
App checkpoints the old round, applies the accepted navigation, resets input
ownership and clears the old effects. Navigation accepted while Playing owns an
event remains accepted if the handler's clock update crosses the animation boundary.
A settled outcome is still resolved before the next queued input; stale Playing
releases cannot activate the newly entered modal.

Directions, idle input and Sound retain the post-handler outcome check. A Sound
event crossing the boundary yields to Win/Game Over and does not toggle settings.
This existing priority is explicit; it is distinct from accepted Restart/Menu
navigation. The ordinary sound row keeps its colors, keyboard/pointer contract,
whole-row hit area and setting persistence.

The source-derived controlled routing matrix passes **32,841 checks over 64 isolated
profiles**: 36 active routes, 14 deliberate handler clock crossings, eight already
settled priorities and six stale releases followed by fresh modal activation. It
uses legal seed-zero first-win and seed-nine terminal moves through the real session,
score coordinator, outcome and scene/input guard functions. Copied App dispatch/reset/
presentation/outcome fragments and source hashes are recorded; fixture-only timing
hooks are absent from production. Reopening all profiles verifies best, player and
round identity, exactly 26 original terminal completions recorded once, 38 empty
histories, nine active Sound changes and both unchanged boundary-Sound cases.

The independent negative control compiles the verified entry App/Playing/primitives
copies and headers. Its **6,284 assertions across 18 routes** reproduce exactly ten
original losses: five Win and five terminal navigation cases. These assertion counts
include repeated/lifecycle checks and are not counts of distinct user scenarios.

### Readability and player display

Tile fills and numeric tiers remain. Labels for 8/16/32 now use dark primary text;
opaque ratios are 7.164/5.914/5.033:1. White 64 remains 3.190:1 at nominal 46 Bold and
meets its large-text benchmark. Green +delta changes to RGB (45, 107, 55), giving 4.923:1
against the sidebar at held opacity. Translucent motion/fade endpoints remain
intentional and are separate from the settled contrast gate.

Two logical pixels of tile text inset reserve 106 within each 110-wide cell; two
pixels of feedback inset reserve 120 within the existing 124-wide score card.
Full numbers and the leading plus are retained. All 30 representable positive
power-of-two tiles from 2 through 1,073,741,824 meet the 16 floor; the lowest is
17.667. Explicit non-power render stresses INT_MAX, 2,000,000,000 and 1,444,444,444
are labeled synthetic; the lowest sampled size is 16.224. Sampled +delta reaches
16.327 at +1,444,444,444; score/best cards retain their 116 reservation and minimum
sampled size 17.637. These synthetic states do not establish core reachability.

`drawTile` returns the settled fit combined with actual supplied motion scale;
`renderFloatingScores` returns its bounded minimum-16 fit. An infeasible reservation
or huge synthetic tile at scale 0.82 reports failure without forcing overflow.
Actual spawned 2/4 tiles remain readable at 0.82. These reports are logical SFML
character-size equivalents, not a physical-pixel/font-height guarantee.

Playing applies the existing display-only `presentPlayerName` projection before
nominal-22 ellipsis. A replacement changes the caption to “Oyuncu (gösterilemeyen: ?)”;
ordinary names restore “Oyuncu”. The complete caption remains nominal 16 within 264.
Unavailable/zero-ink characters remain visible as `?`; original stored UTF-8 is not
changed. Single-line whitespace, long/Turkish names, blank/invisible legacy names
and restoration are verified. This does not add font fallback, shaping or IME.

### Motion, geometry and controlled verification

Adjacent simultaneous merges now use a smaller sinusoidal amplitude, reaching
122/110 = 1.10909 at peak and retaining a four-logical-pixel gap. The progress easing,
90 ms slide, 120 ms merge and 140 ms spawn remain. Isolated merges keep their 1.17 peak.
Latest-only +delta, 850 ms hold/rise/fade, 22-pixel travel, input resets and expiry remain.
Fixed hit areas, Tab/Shift-Tab focus and both sound states are verified.

The final current-source visual matrix passes **5,469 checks and 178 PNGs at 1×/2×**.
It exercises actual Playing drawing/ticking/onMove with an ignored deterministic
clock, real four-direction double/isolated merges and seeded spawns. Independent
quarter/half/three-quarter positions, alpha and exact endpoints pass. Every adjacent
peak matches an independent 122/110 reference across the whole board and its 12-pixel
surrounding shadow/gap region, without reusing the production amplitude helper.
Seventy-two mapped pointer scenario runs and control-edge cases use the same hidden
SFML event window alongside the two render densities; they are not two physical
monitor/input configurations.

Full-surface RGBA glyph differences include antialias fringe with a bounded one-
logical-pixel tolerance. Caption/name reservations have independently fixed tops
198/222 and the existing score-card clearance. Early fixture assumptions of 22/24
name height failed for accented nominal 22 ink 25 (26 including Retina fringe); all
three diagnostics are retained. The actual player section accommodates it without
production changes. Review also strengthened the peak/vertical-position oracles;
the earlier 5,405-check pass is archived separately from the final 5,469 run.

Twenty-two fresh whole-UI renders compare with 10.3. Four table frames are identical;
the other 18 change only within the expected board/player regions and the menu's
shared decorative 8 text. Old unsupported-name ink required a corrected antialias
mask extent; that fixture diagnostic is retained. Modal panels/actions outside
those underlying regions remain identical. This is a preservation check, not the
full modal acceptance assigned to 10.5. Release build, formatting/whitespace,
255 unit cases / 282,023 assertions and five native platform scenarios pass.

### Native evidence, preservation and remaining scope

Two ignored current-App copies seed legal rounds through public session moves,
record event/outcome snapshots and use isolated profiles. Production has no seed,
clock or trace hooks. Native macOS Computer Use observes explained mixed legacy
name projection, a winning move, keyboard Continue with the same board/score/round,
N restart, keyboard Sound/Menu, terminal Game Over and keyboard Replay. In the
fresh second window, Playing Sound / NewGame / MainMenu all pass native pointer input
with full 1840×1384 captures. Both windows exit zero. Forty-one read-only checks
reopen the real score/settings stores: best 21,196/2,108, sound off, no false winning
completion and one original terminal completion with unchanged “Çağrı你\u200b” identity.

Tool diagnostics remain separate: later first-window captures are 180×200 Stage
Manager thumbnails, so they are not client-geometry proof; its first Menu pointer
attempt failed `windowNotFoundAtPosition` and issued no command. An initial terminal
capture failed `stageManagerProcessNotRunning`; refreshed capture recovered the full
frame and the subsequent three Playing pointer routes succeeded. Short-motion and
navigation-race evidence is controlled, not measured by native screenshot latency.
AX still exposes OS chrome; native IME, custom semantic controls and hosted Windows/
Linux GUI remain separate open gates. Menu/table pointer renewal stays with 10.6.

Changes are limited to six existing application UI source/header files and four
documents. App's UI ordering is the only dispatcher change; Win/GameOver sources,
core/session/storage/paths/migration, assets/dependencies/build/CI and permanent
tests are unchanged from entry. The user's profile and Desktop bundle were not used.
Evidence is under `build/ui-quality-step-10.4/`: verified entry copies/hashes, scoped
diff, current/baseline routing and profile reports, visual/native/baseline reports,
PNGs, logs, isolated profiles and retained fixture/tool diagnostics. A03/modal work
remains 10.5; A07 maintained graphical acceptance, A08 combined coverage and A09 native
accessible operation remain 10.6. The next substep is **10.5 — outcomes and backdrop**.

## 10.5 — Outcomes and backdrop

Completed on 2026-10-13 within the requested substep. A03, modal A10 application and
A14 card jitter are resolved; four modal buttons meet A15's stable hit geometry.
The user's instruction leaves the final 10.6 closure for later. No maintained UI
framework/gate, accessible-provider implementation or broad production-readiness
claim is introduced in this substep.

### Backdrop and input ownership

GameOver retains its full-client dim. App now forwards primary press/release events
in the four outer margins while GameOver owns input, so a background press cancels
held keyboard/pointer activation. It neither dismisses the modal nor activates an
underlying Playing control. Secondary margin events, other scenes and Win's existing
content-inset backdrop/viewport policy remain established. Win outer strips remain
outside its routed content region; inside-background presses cancel pending actions.
This coverage policy is explicit, rather than silently making both backdrops equal.

The App-derived current routing fixture passes **34,583 checks over 165 reopened
profiles**: the full 32,841-check 10.4 preservation matrix and 1,742 modal checks. Forty
background cases cover both modals, both focused actions, Enter/Space and four
margins plus inside background. All 16 GameOver margin combinations and four inside
combinations cancel; all 16 Win margin-policy combinations retain their behavior.
Fresh pointer/keyboard routes, modifiers, blur, exclusive modal directions/shortcuts,
margin/control crossover, duplicate/stale releases and input reset also pass.

The clean entry-App filter negative control passes 694 checks over 40 isolated
profiles and reproduces exactly 16 original GameOver margin failures. Current
profiles have 77 unique original completions recorded once and 88 empty histories;
nine sound changes from the preserved Playing matrix persist, while 156 default
settings remain enabled and unwritten. These counts include repeated backend/event
checks and are not counts of distinct native acceptance scenarios.

A real seed-zero Win→Continue sequence reaches a terminal score 22,388 after 86 changed
moves, records the original round once, and reopens correct best/name/history after
Replay. Simultaneous first-Win/terminal priority and new-round Win eligibility use an
explicitly controlled public board, real final animation, outcome/scene methods and
App dispatch. They are separate from legal-sequence/profile proof. Accepted Playing
navigation ordering and checkpoint-before-reset remain unchanged from 10.4.

### Stable modal geometry and readable content

Default 390×400 panels, 326×56 actions, fonts, colors, trophy/restart/home icons and
normal placement remain. GameOver card placement now uses Replay's fixed filled top
minus the normal border reservation. Focus's 2→3 outline no longer shifts both cards
up 0.5 pixels. Two clean entry scene controls reproduce the old shift; current card
pixels, including an 8 px surrounding shadow region, remain identical during both
focus states, hover and press. Default geometry stays byte-identical to 10.4.

Both modal handlers compensate pointer padding for the thicker focus outline.
Normal 2 px border plus 4 px padding stays a total 6 px expansion through focus; drawing
and keyboard semantics remain. Upper 336≤y<404 and lower 404≤y<472 bands abut at 404.
The padded visible-gap center intentionally belongs to the lower action; it is not
claimed neutral. Four entry controls reproduce the old 404 wrong upper command and
lower-focused 403 release expansion. Seventy-two current mapped band-boundary checks and separate crossover checks pass.
A 404-owned press followed by 403 release cancels after focus changes.

This exposed a wider shared-helper issue: Splash Start/Scores, HighScores Back,
Playing NewGame/MainMenu and the Sound row still derive hits from focus-inclusive
bounds. Their correction and combined verification are now explicit A15 work in 10.6.
Name input keeps its 2 px border and is outside this particular finding. The modal
correction does not silently close the whole shared-control issue.

Win explicitly supplies its 20 minimum to bounded score fitting; every sampled full
inline score remains nominal 24 within 326. GameOver's existing 155×88 cards reserve 147
for values and 131 for captions; the lowest sampled long value is 22.3509 at 1,444,444,444,
above the shared 16 floor. Zero, ordinary, eight-digit, INT_MAX and digit-heavy score/
best combinations retain complete strings and correct column assignment. Four
whole-PNG byte comparisons prove zero→wide/max→zero restores identical images at both
densities. These injected scores establish render behavior, not core reachability.

Nine fresh settled functional text/focus ratios clear the selected benchmark; the
lowest is the primary-hover label at 4.53149:1. Trophy gold remains decorative; its
outward handle strokes have a fixed 44×40 envelope at 406/128, inside the modal and
clear of the heading. An initial nominal 40-box test failed because it omitted the
existing 2 px outward strokes. The corrected independent envelope keeps 1 px raster
fringe; the diagnostic is retained and the production icon is unchanged.

### Rendering, motion and native verification

The final current-source modal matrix passes **1,575 checks and 94 PNGs at 1×/2×**.
It exercises actual current Win/GameOver render/event objects and current primitives
with an ignored deterministic clock. Private observation and renamed clean snapshot
classes are fixture-only. Independent fixed geometry, full-surface RGBA glyph ink,
heading/trophy/icon/label clearances, complete numbers and source/caller-view
preservation pass. The 112 held/pointer/reset runs use one mapped SFML event window
alongside two render densities; they are not two physical input configurations.

Sixteen controlled 45/90/135/180 ms reveal phases match independent smoothstep whole-
surface composition, permitting three eight-bit channel levels for intermediate
premultiplied rounding. Delayed first draws, immediate controls, re-entry, reset
neutrality and exact settled endpoints pass. Forty complete uncovered-surface checks
verify GameOver includes all client margins/corners and Win preserves its outer
strip. Caller center/size/viewport and diagnostic rotation are restored. The final
card-shadow oracle was expanded 4→8 px; the earlier pass is archived. All 22 fresh
unfocused whole-UI baseline images remain RGBA- and PNG-byte-identical to 10.4.

Native macOS Computer Use runs four isolated current-App copies seeded only through
public legal moves, with trace-only observations and path overrides. All four modal
pointer actions pass: Win Continue/NewGame and GameOver Replay/MainMenu. Five held-
Space background sequences pass: Win inside background and each of GameOver's four
margins. Recorded ordering is Tab→Space down→primary press/release→Space up, without
intervening blur; the original modal/round/score/history remain until explicit action.
All 13 captures are full 1840×1384 windows; all four processes exit zero.

Seventy-nine read-only native trace/store checks verify transitions and reopen actual
score/settings stores. Traces confirm Continue preserves winning grid/score/round,
NewGame/Replay create fresh zero-score rounds and Menu preserves terminal identity.
Stores confirm retained best and completions. Win best 21,196 has no false completion;
each terminal profile has best 2,108 and one original “Çağrı” completion. Default sound
remains enabled, clean and absent on disk. An initial ignored checker wrongly
expected a saved settings file; its diagnostic is retained, the expectation was
corrected to the established Missing/clean contract, and production was unchanged.

Release build, formatting/whitespace, 255 unit cases / 282,023 assertions and five
native path/resource/persistence platform scenarios pass. Source changes are limited
to App.cpp, WinScene.cpp and GameOverScene.cpp plus four documents. Shared primitives,
Playing/menu/table sources, core/session/storage/paths/migration, assets/dependencies/
build/CI and permanent tests remain unchanged from entry. The user's profile and
Desktop bundle were not used. Evidence and verified entry copies/hashes are under
`build/ui-quality-step-10.5/`; 132 of 139 entry files remain unchanged.

A03/A10/A14 are closed and A15 modal application is complete. A15's remaining five
buttons/Sound row, A07 maintained graphical acceptance, A08 combined coverage and A09
native accessible operation remain open for 10.6. Native IME/full shaping/fallback
and hosted Windows/Linux GUI remain separate readiness/platform gates. Short-motion
and pixel-boundary evidence is controlled; native snapshots do not measure latency.
The final **10.6 — combined acceptance and closure** is deferred at the user's request.


## 10.6.1 — Stable shared control geometry

Completed on 2026-10-13 within the requested substep. A15 is now resolved across
Splash Start/Scores, HighScores Back, Playing NewGame/MainMenu/Sound and the four
previously corrected modal buttons. Maintained acceptance, native accessibility and
the combined current-source tour remain 10.6.2–10.6.4.

`controlBounds` keeps the exact normal SFML rounded-outline bounds. Normal controls
use a direct fast path; a focused control is copied and its outline normalized before
querying bounds. The source shape, filled geometry, origin, scale, rotation and drawn
focus outline remain unchanged. This preserves fractional extrema, including Sound's
normal left edge near 566.00067; replacing the shape with a nominal rectangle would
change accepted boundary pixels. Shared pointer feedback uses these bounds with its
existing caller padding. Hover queries use the same normal-outline geometry while
retaining keyboard-focus highlighting. Sound uses the normal panel border.

The existing padding policy is preserved: menu/table/modals add 4 to their normal
2-pixel outline; Playing actions and Sound add no extra padding. The modal-specific
focus compensation is removed because the shared helper now owns normalization.
Name-input geometry, draw routines, timing, App routing and backend are untouched.

Current and clean-entry scene variants each pass **1,730 checks over 682 distinct
controlled scenarios at 1×/2×**: 406 edge, 160 pointer lifecycle, 80 keyboard and 36
modal boundary scenarios. Actual scene objects receive mapped SFML input through
physical windows at both sizes. Independent normal geometry is checked against
rendered fill pixels before boundary probes. The entry produces **96 observable
wrong commands** across the six affected controls; current produces zero. Forty-six
are direct ring clicks, 48 are releases after a focus change and two expose ownership
masked by a neighboring action's priority. Normal-owner priority and the upper-403 /
lower-404 modal bands remain correct; drag-off/return, unmatched/secondary/stale
releases, MouseLeft, blur/reset, Enter/Space and fresh recovery pass.

Before/current visual variants each pass **411 checks**. All **68 1×/2× scene/state
pairs are RGBA- and PNG-byte-identical**, comparing 103,224,000 pixels with zero
differences. All five normal scenes, nine button focus/hover/press states and
Name/Sound focus are represented; 54 button-state metadata records match. Primitive
feedback uses an ignored deterministic clock and one mapped event window alongside
the two render densities. This is targeted preservation, not the broader A08 matrix
or native latency evidence.

Native macOS evidence passes three menu pointer actions (Scores, Back, Start), three
focused outside-edge cancellations, and Playing NewGame/Sound/MainMenu plus Start
reentry by keyboard. The first App copy adds trace-only observation and isolated path
overrides; the second additionally creates a public seed-1061 named session and
starts Playing. New rounds, named identity and Sound-off reentry pass. Two windows
exit zero; nine full 1840×1384 JPEG captures retain original screenshot bytes. The
score file remains byte-identical to its two-entry/best-668 fixture; actual stores
reopen cleanly with those entries/best and the persisted Sound-off setting (nine
store checks).

**Native coverage limit:** coordinate pointer actions in Playing repeatedly returned
Computer Use `noWindowsAvailable`, including the unique retry bundle. Indexed window
background clicks and keyboard input succeeded, but those failed control-coordinate
attempts are not passes. Fresh native Playing NewGame/Sound/MainMenu pointer and
outside-edge renewal remain explicit A08/10.6.4 items. All ten controls are covered by
the successful controlled pointer matrix. Earlier native Playing results remain
historical; they are not relabeled as a fresh run.

Diagnostics are retained: the first interaction oracles omitted a neighboring
normal owner and priority-masked ownership; the corrected probes keep both normal
priority and strict current bounds. The visual runner uses the bundled Python/Pillow
runtime after the default interpreter lacked Pillow. Native trace validation retains
preceding asynchronous Tab events and reads JPEG metadata rather than assuming PNG.
These are fixture/tool corrections; no production workaround was added.

Release build, formatting/whitespace, **255 unit cases / 282,023 assertions** and five
native path/resource/persistence scenarios pass. Seven existing UI files and four
documents change; 128 of 139 entry project files remain unchanged, with no new or
removed project files outside ignored evidence. App/core/session/storage/profile
paths/migration, assets/dependencies/build/CI and permanent tests remain unchanged.
Root runtime JSON and the real profile's recorded file presence/hashes are preserved;
the Desktop bundle was not used. Evidence is under `build/ui-quality-step-10.6.1/`.
No generated build cleanup or full production/accessibility closure is performed.
Historical handoff after 10.6.1: **10.6.2 — maintained graphical acceptance**.
Its current result and remaining native requirements are recorded below.


## 10.6.2–10.6.4 — Maintained acceptance and native operation

Completed locally on 2026-10-14. Maintained controlled acceptance, actual VoiceOver,
renewed pointer/keyboard delivery and final native focus proof pass. Earlier sections record
the scope at the time each substep completed. Their deferred/open statements are
historical rather than the current authorization.

### Maintained graphical gate — 10.6.2 complete

The repository entry point is `tests/ui_acceptance.cpp`, run through the
standard-library-only `scripts/verify_ui.py`. Supported system/vcpkg Release presets
build current sources and the versioned production Inter font without old ignored
harnesses, baseline PNGs, a UI framework or production test hooks.
`SFML_2048_BUILD_UI_ACCEPTANCE` defaults OFF; enabling it requires the application,
tests, Python and a native graphical session. The worker and QA output are excluded
from Runtime installation and packaging. Headless builds remain independent of
SFML and the desktop accessibility provider.

The fresh system `ui-acceptance-release` build passes **772 checks / 102 PNGs** at
1×/2×. All five scenes, ten controls at both densities, focus/hover/press drawing,
Name error/limit feedback, Sound state, zero-to-five leaderboard rows, legal tile
values, separately labeled integer stress, complete scores, pointer/key ownership,
modal 403/404 bands, motion/reveal endpoints and public round/outcome semantics are
covered. Twenty visual control/density records independently check focus outlines,
hover palettes and button press scale; Sound retains its established unchanged-row
behavior. Twenty input control/density records are a separate count.

The runner checks report freshness/schema, required coverage, output containment,
PNG signatures/dimensions and all chunk CRCs. Successful evidence is
`build/ui-acceptance-release/ui-acceptance/ui-acceptance.json`, its `worker.log` and
102 PNGs; CTest success is retained in
`build/ui-quality-step-10.6/closure-focus-ctest.log` (all four current CTest entries
pass). The earlier expanded UI-only success remains in `ui-expanded-ctest.log`.
Earlier failed worker attempts remain diagnostics, including `final-ctest.log`; they are not the successful gate.
Run instructions are in [testing.md](testing.md#maintained-graphical-acceptance-step-1062).

### Native semantics — 10.6.3 complete locally

Each scene owns accessible names, roles, complete UTF-8 identity/score values,
frames and current capabilities. The macOS adapter attaches public AppKit
accessibility elements to the SFML window through a transparent view whose hit
testing passes pointer input through. It exposes the required editable Name field
and validation/limit help, action focus, Sound checkbox state, ranking cells, all
sixteen board cells and four directional board actions. Stable read-only static-text
children make live cell values available to VoiceOver while preserving row/column
semantics. Win/Game Over expose only the active modal tree. Non-macOS desktop builds currently use a capability stub;
other-platform native providers/assistive-technology proof remain step 12.

Native callbacks enqueue immutable requests rather than mutate App. The queue is
bounded at 64 requests, with UTF-8 values limited to 4096 bytes. App revalidates
scene epoch, round identity and current enabled capabilities before using the same
scene commands and checkpoint ordering as ordinary input. Every actual scene
transition and accepted Start/Restart advances the epoch; animation frames do not.
Accepted activations and board moves cancel pending pointer/key activation. Settled
outcomes retain priority. Changed Name errors/outcome messages request native
announcements; changed board-action availability notifies clients. Closing detaches
the provider before destroying the window.

The transparent semantic host owns the native first-responder route so AppKit can
resolve semantic keyboard focus; inherited keys continue through the SFML responder
chain. Repeated real clicks revealed one final defect: identical-snapshot
deduplication skipped reconnecting focus after SFML became first responder. The
bridge now refreshes native focus independently of snapshot publication, after
releasing its queue mutex. The adapter reconnects only from its known renderer/prior
responder and restores the prior responder on detach. Three repeated clicks with
VoiceOver off and two with it on retain the named `AXTextField`; the entry hook
reproduces `AXWindow` focus on the second click. Actual VoiceOver reads the required
Name field after the fix. This narrow latest-source proof is separate from the
earlier full six-run native tour.

Eight permanent bridge cases / **456 assertions** cover Unicode/tree validation,
enabled declared actions, queue bounds/FIFO, stale identity/capability pruning,
idempotent semantic publication, focus refresh independent of deduplication and
retained callbacks through detach/destruction. They link the portable bridge with a test-local fake platform and need neither a native
adapter nor a graphical session. These checks prove request/lifecycle safety.
Separate actual VoiceOver evidence
passes Name editing/validation/Start, live board reading/custom moves, Sound,
populated HighScores/Back and both actions of each outcome modal.
Adapter value/selection replacement does not establish a conventional native caret
editor, complete IME, font fallback or complex shaping.

### Existing checks and combined closure — 10.6.4 complete locally

The unchanged core suite passes **255 cases / 282,023 assertions**. The five isolated
platform resource/profile scenarios pass. A fresh CMake 4.0.2 macOS Release headless
dependency audit passes all stages, including 145 translation units, full actual
header scans, backend symbols, runtime dependencies and the same core suite. Its
report is `build/ui-quality-step-10.6/headless-final/headless-213sfez0/report.json`.

The headless verifier resolves built objects from the compiler's actual `-o`,
relative to its recorded working directory, including nested CMake 4 dependencies.
Exactly `Scene.hpp` and `GameplayOutcome.hpp` are allowed as existing pure
standard/core contracts under the UI directory. Their real transitive dependencies
still receive the full audit. Seventeen verifier controls pass, including native
nested-object and real pure-header positives and negative controls for transitive
SFML or a neighboring runtime UI header. Runtime UI dependencies are not exempted.

The final current Release build and formatting check pass. All four CTest entries
pass in **21.39 s**: core 255 cases / 282,023 assertions, portable bridge eight cases /
456 assertions, five platform scenarios and graphical 772 checks / 102 PNGs. Logs
are `build/ui-quality-step-10.6/closure-focus-build.log`, `closure-focus-format.log`
and `closure-focus-ctest.log`. The Python control suite reports 27 tests: 26 executed
passes and one explicit skip because Ninja is unavailable for the multi-config
control (`closure-python-tests.log`). No installation or waived pass is implied.

The final native oracle passes **233 checks**: 214 from the six-run base tour plus
19 from the separate latest focus regression. Together they cover **seven closed
acceptance runs / 473 trace records**. The base tour has six runs / 430 records;
the final focus run adds one / 43 and five real mouse pairs. Six physical mouse command pairs renew Playing New/Sound/Menu,
Splash Scores/Start and HighScores Back; traced SFML press/release establishes
actual delivery separately from AX activation. Physical Enter/Space/arrow paths
also pass. Actual HID VoiceOver shortcuts and copied spoken phrases correlate with
nine accepted action targets and a directional board request. The four legal
outcome runs prove Continue retains the round/grid/score, New/Replay creates a new
round, and Game Over records one completion. Six profiles reopen through production
read APIs with no save calls, preserving best/history/identity and Sound state.
The report is `build/ui-quality-step-10.6/native/final-native-acceptance.json`.

Revision scope is explicit: the Splash base run used the earlier field-focus
adapter, while Playing and all four modal runs used the cell-text refinement. App
and scene commands are current and unchanged by the final focus hook. The latest
focus-refresh mini-regression separately verifies repeated physical clicks, native
Name focus and actual VoiceOver readback; it does not relabel the earlier six runs
as a complete rerun of that hook. Earlier interrupted runs, failed caption/copy
attempts and `noWindowsAvailable` actions are retained diagnostics and not passes.

The protected-source comparison confirms **47 backend/application infrastructure
entry files are byte-identical**. Real profile and root runtime-JSON presence/hashes
are preserved. VoiceOver is restored to its initial off state, all QA processes
are closed and temporary clipboard use is restored. A08/A09 and step 10 are closed
for this defined local package. The report distinguishes native operation from
controlled rendering and retains the exact accepted revision scope.

Step 11 is the separate UI edge-case package. Its inventory, preparation and
authorized 11.4 results are tracked in [the step 11 plan](ui-edge-case-plan.md).
Automated gates and fourteen controlled App combinations pass. Partial native
automation remains recorded separately from the user's successful six-step
physical-keyboard manual confirmation. Step 11 is closed within that local scope;
one Ninja environment skip remains and no product/oracle fix was made. Step 12 retains
hosted CI and other native platform/assistive-technology verification. Controlled
build/evidence cleanup remains 15.1; no old build archive, real profile or Desktop
bundle was removed by this package. No unrestricted production, distribution or
cross-platform accessibility claim follows from these local checks.
