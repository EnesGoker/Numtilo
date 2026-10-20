# UI design system — roadmap step 1

Naming note, 2026-10-15: the roadmap records below retain their historical names
and revision scope. The current window, Splash and Playing product titles are
`Numtilo`; the game's `2048` goal, Win achievement wording and decorative `2048`
tiles remain unchanged. Earlier headings, measurements and verification records
are preserved for their original revisions, with no claim that those checks were
rerun for the new name. Use [the maintained testing instructions](testing.md)
for current preparation and acceptance procedures.

Current status (2026-10-14): roadmap steps 1–9 are complete, including gameplay
substeps 5.9/5.10, navigation substeps 6.1–6.3, win substeps 7.1–7.4 and
Game Over substeps 8.1–8.4.
The sections below record each substep at the time it was completed; earlier
statements about the next unstarted substep are historical.
Roadmap step 7 (win overlay and Continue) passed its combined native macOS acceptance.
The win modal supports Continue/New Game, pointer and focused keyboard activation,
and Escape to Continue. Step 8 (Game Over visual redesign) passed combined native
macOS acceptance: full-client dimming, paired score cards, stacked icon actions,
pointer/keyboard navigation and one-time completed-score persistence.
Step 9 (animation and interaction polish) is complete: tile-motion easing, shared
button feedback, panel/modal entry and score feedback passed combined local macOS
acceptance. The earlier physical pointer QA pending item is closed: all nine buttons
were activated with native Computer Use, including both outcome modals.
Step 10 now covers a whole-UI quality audit in addition to typography, as requested
by the user. Substeps 10.1–10.5 are complete: inventory/criteria, shared pointer ownership,
readability rules and menu/table/Playing/outcome quality passes. Substep 10.6 is now
split into four bounded packages, all complete within the defined local macOS
scope: shared geometry, maintained graphical acceptance, actual native accessible
operation and combined closure. A final repeated-click/VoiceOver-focus regression
also passes. Step 11.1 has completed the inventory of remaining combined edge cases;
11.2/11.3 have prepared and compiled the selected input/window and transition/record
combinations without runtime tests. The
current scope and approval boundaries are in [the step 11 plan](ui-edge-case-plan.md).
Substep 11.4 has run with explicit approval: automated gates and fourteen controlled
App combinations pass. The native lifecycle run supplies partial automated evidence;
the user subsequently confirmed all six physical-keyboard manual steps passed.
Step 11 is closed within its local scope, with the Ninja environment skip and
automation limitation retained separately. Step 12 has not started. See
[UI quality audit](ui-quality-audit.md) for the current finding register, evidence
limits and expanded work packages. The local findings register is resolved within
its recorded scope; earlier phase acceptance remains historical. This closure does
not declare unrestricted production readiness or cross-platform accessibility.
Broader Windows/Linux UI acceptance remains roadmap step 12.

Step 1 is complete: the existing four SFML scenes use a shared theme and drawing helpers.
Step 1 established the styling foundation with the then-existing 450×530 window and board geometry.
The reference mockup's screen layouts, navigation, win screen and animation polish belong to
later roadmap steps. No UI framework, dependency or CMake target was added.

## Theme ownership

`src/app/ui/UiTheme.hpp` owns visual choices. Scenes own positions, input and commands.

| Namespace | Responsibility |
| --- | --- |
| `theme::colors` | Cream surface, primary/secondary text, button variants and hover colors, input/focus/error, panels, board, tile palette, score feedback, dim overlay and shadow |
| `theme::typography` | Display, subtitle, heading, modal heading, label, input, button, caption, score and tile sizes; display/heading/body/button/emphasis styles |
| `theme::spacing` | 4, 8, 12, 16, 24 and 32 logical pixels |
| `theme::radius` | Button/input, panel, tile and leaderboard row corners |
| `theme::shadows` | Shared shadow offset; shadow color/alpha lives in `colors` |
| `theme::sizes` | Borders, button padding, input/toggle dimensions, row metrics and current board/menu/modal dimensions |

Sizes are SFML view coordinates, not physical Retina pixels. Button padding constants are the
total extra width/height around the label. The shared shadow is a subtle offset silhouette.
`kTilePalette` retains the existing 2–4096 colors; larger values retain the fallback color.
Tile character size decreases with the number of digits. Animation constants remain unchanged.

Inter remains the application's existing font asset. The application owns its lifetime and
passes it by reference; the theme neither reloads fonts nor resolves paths. SFML 2.6's regular
and bold text styles supply body/button and heading/emphasis weights. `localizedText` keeps
its existing glyph-based ASCII fallback. No font loading or missing-asset behavior was changed.

## Shared components

`src/app/ui/UiPrimitives.hpp/.cpp` provides seven small drawing components:

| Component | API and behavior |
| --- | --- |
| Button | `createButtonForText`, `styleButton`, `updateButtonHover`, `updateButtonFocus`, `drawButton`; shared variants, optional vector play/trophy icons and a keyboard focus outline |
| Panel | `createPanel`, `stylePanel`, `drawPanel`; standard, side, score and modal variants |
| TextInput | `createTextInput`, `updateTextInputStyle`, `drawTextInput`; normal/focus/error borders, error takes priority, text fits available width |
| ScoreCard | `drawScoreCard`; shared panel, secondary caption, emphasized value, text fits width |
| Toggle | `drawToggle`; compact rounded track/thumb, on/off and hover states |
| LeaderboardRow | `drawLeaderboardRow`; alternating rows, shared rank/name/score alignment and width fitting |
| Modal | `createModalOverlay`, `drawModal`; common dim layer, panel and shadow |

The menu, leaderboard, playing HUD/menu and game-over scene now use these helpers where their
current layouts need them. ScoreCard and Toggle are ready for the later right-panel/modal
layouts and were rendered in the component gallery; the current sound action still uses its
existing menu button. The empty leaderboard draws its message without overlapping placeholder
rows. The complete leaderboard redesign is still a later step.

Rendering accepts `sf::RenderTarget`, so the same code works with a real window or an offscreen
`sf::RenderTexture`. Event handlers still receive `sf::RenderWindow`. Drawing helpers do not own input,
focus, game state, sound, navigation or persistence. ButtonFeedback and PanelReveal
own their short presentation clocks; scenes and App supply input and lifecycle states.

Text-sized buttons use a centered origin; `createPanel` and `createTextInput` use a top-left
origin. Styling an existing shape preserves its size, origin and position. Apply text style
before centering its origin. Drawing shadows does not enlarge clickable bounds. Reuse the
existing `containsWithPadding` for hit testing. Components expect positive bounds and enough
height for their labels; screen-specific layout constraints belong to the scene.

```cpp
sf::Text label(ui::toUnicode("Oyuna Başla"), font,
               ui::theme::typography::kButtonSize);
auto button = ui::createButtonForText(label, ui::ButtonVariant::Primary);
ui::styleButton(button, label, ui::ButtonVariant::Primary);
ui::centerTextOrigin(label);
button.setPosition(center);
label.setPosition(center);
ui::updateButtonHover(button, hovered, ui::ButtonVariant::Primary);
ui::drawButton(target, button, label);
```

To change all button corners, border widths or padding, edit `theme::radius::kButton` or the
corresponding `theme::sizes` token. To change a variant's palette, edit `theme::colors`.
No per-scene button palette or font-size literal remains.

## Validation

Native macOS arm64 Release build passed. Existing CTest checks passed: 249 unit test cases
with 281,933 assertions and all five platform acceptance scenarios, including missing-font
handling, runtime relocation and isolated persistence recovery.

An isolated SFML smoke harness rendered the production helpers/scenes to eight PNGs and checked
rounded-corner bounds, unchanged cell positions, hover geometry, focus/error precedence,
Turkish glyphs, empty-name Enter rejection and existing restart commands. It constructed sample
data in memory without running `App` or accessing the user's saved profile. Visual inspection
covered menu normal/validation, leaderboard 0/1/5 entries, gameplay, game over and all seven
components, including long Turkish names, an eight-digit score, and 2048/4096/8192 tiles.

Local evidence is under the ignored `build/ui-design-system/` directory: `build.log`,
`tests.log`, `ui-smoke.log`, `ui_smoke.cpp`, `screenshots/` and `summary.json`.
The before/after source hash comparison preserves every preexisting file outside `src/app/ui/`.
Formatting and whitespace checks passed. This is native macOS evidence; Windows/Linux hosted
CI has not been run for this step.

## Roadmap step 2.1 — fixed native window

The current window is 920×660 logical pixels. `theme::sizes::kWindowWidth` and
`kWindowHeight` are the single source for that size; `App` no longer derives window
dimensions from board cells. The existing `sf::Style::Titlebar | sf::Style::Close`
keeps the native OS frame and disables user resizing. Scene positions, outer margins
and board geometry have not been redesigned in this substep.

Native macOS Release build, all 249 unit cases (281,933 assertions), and the five
platform acceptance scenarios passed. The real `app::run` was opened in an isolated
QA bundle with separate profile/legacy directories. The menu opened, the OS zoom
control was disabled, and closing with the native X returned success. The Retina
window capture was 1840×1384 physical pixels, including the native title bar.
Local evidence is under `build/ui-window-step-2.1/`. The shared content area and
32 px outer margins belong to substep 2.2.

## Roadmap step 2.2 — shared content area

`theme::sizes::kWindowMargin` reserves 32 logical pixels on all four sides of the
920×660 window. `ui::createContentView()` exposes a local 856×596 coordinate space
with a normalized inset viewport. `App` installs that view once and passes its
size to every scene and the playing scene's render/input/hover methods. Local
(0, 0) maps to window (32, 32). The whole window is cleared with the shared cream
background; scene drawing is clipped to the content viewport. Scene-specific
padding remains inside this area. No scene layout or animation algorithm changed.

Drawing and input use the same active view. `mouseButtonPosition` maps the stored
mouse event coordinates through it, rather than polling the cursor again after
an event has been queued. `containsViewPixel` uses SFML's actual target viewport:
mouse button events outside the content area are filtered before scene dispatch,
and hovering outside clears button hover. Keyboard events and native window close
keep their existing behavior. No extra per-scene margin translation is needed.

Native macOS Release build, all 249 unit cases (281,933 assertions), and the five
platform acceptance scenarios passed. Offscreen SFML checks exercised all four
viewport boundaries, drawing clipping, and tile coordinate round trips at 1× and
2× pixel density; five production scene PNGs were visually inspected. A separate
event smoke used a real SFML window and confirmed input focus, Turkish name entry,
start, scores/back, game menu, sound, new game, and existing modal commands with
mapped mouse events. It measured a 920×660 target and viewport (32, 32, 856, 596).

The actual `app::run` also opened with an isolated profile and closed successfully
using the native X. The desktop automation tool could inspect it but returned
`noWindowsAvailable` when sending coordinate clicks, so physical pointer clicks
were not verified through that tool. The scene event tests above directly
exercised the production handlers; they did not inject OS input or read the user's
saved profile. Evidence is under `build/ui-shell-step-2.2/`: build/test logs,
`shell-smoke.log`, `event-smoke.log`, five `screenshots/`, and `summary.json`.
Core, persistence, paths, migration, headless configuration and animation timing
remain unchanged. Substep 2.3 covers the remaining scene-specific placement.

## Roadmap step 2.3 — existing scene placement

All four existing scenes now fit the shared content area with deliberate spacing:

- Splash centers the measured title/form/button group. The player label aligns
  with the input, both buttons share its width, and validation has reserved space
  beneath the field, so an error does not overlap or move a button.
- Leaderboard centers a card capped at 640 logical pixels. Its height comes from
  the five row slots, header and padding. Column headings align with row text;
  the title/card/back group is vertically centered and the back button is separate.
- Playing centers the unchanged board beneath the existing HUD. Static cells,
  slide endpoints, merge and spawn rendering share one translation. Tile sizes,
  gaps, animation durations and interpolation remain unchanged. The HUD is inset
  so its border/shadow stay visible, score feedback anchors after the score text,
  and the compact menu fits beside the board, including maximum merge expansion.
- GameOver centers the existing modal on the content surface. Text positions are
  derived from the panel/preceding text bounds, with equally sized, aligned actions
  and consistent padding. Existing restart/quit commands remain unchanged.

This is placement of the existing screens. The reference menu card/decorations,
leaderboard medals, enlarged board/right control panel and new modal/navigation
designs remain in their original later roadmap steps.

Native macOS Release build, 249 unit cases (281,933 assertions), and all five
platform acceptance scenarios passed. A small isolated SFML harness rendered the
production scenes, found buttons from their actual painted bounds and sent mapped
events to the real window-backed handlers. Checks covered form/error spacing,
table/back placement, board/modal centering, menu clearance, and existing commands.
Thirteen PNGs were inspected: menu normal/error, leaderboard 0/1/5, gameplay/menu,
large scores, game-over normal/large scores, and slide/merge/spawn/settled frames.
The harness did not run `App`, inject OS pointer input or access the user's saved
profile. App bootstrap, content view/input mapping, core, persistence, paths,
migration and headless configuration were unchanged in this substep.
Local evidence is under `build/ui-layout-step-2.3/`, including `layout_smoke.cpp`,
build/test/smoke logs, `screenshots/` and `summary.json`. Substep 2.4 remains the
full application acceptance pass for the window-shell work.

## Roadmap step 2.4 — window-shell acceptance and closure

Step 2's implementation and automated acceptance are complete: a fixed 920×660
native window, a shared 32 px margin / 856×596 content view, and consistent placement
of the four existing scenes. This pass required no production code changes.

| Check | Result and evidence |
| --- | --- |
| macOS arm64 Release build | Passed against the current sources. |
| Unit regression | 249 cases, 281,933 assertions passed. |
| Platform acceptance | All five scenarios passed, including relocation, missing-font handling and persistence reopen/recovery. |
| Scene rendering and mapped input | The step 2.3 harness was rerun against its unchanged source/object baseline. All four scene handlers, form validation, Turkish input, scores/back, new game, sound commands, modal actions and animation samples passed; 13 PNGs regenerated. |
| Current application launch | The real `app::run`, linked from the current production objects, opened in an isolated QA bundle/profile. The native title bar and disabled zoom control were verified. |
| Native keyboard validation | Empty-name Enter displayed the validation message in its reserved space. |
| Native window close | The OS X button closed the real application; exit code was 0 and its process ended. |
| Native pointer acceptance | Pending: the desktop automation tool returned `noWindowsAvailable` for a coordinate click. Full application mouse navigation, movement, restart and sound interaction were not verified through OS input. Direct scene event checks are separate evidence. |
| Frozen source scope | All core, persistence, path, migration, headless, build and test files matched the step 2.1 baseline. During step 2.4 only this documentation changed. |
| Formatting / whitespace | Both checks passed. |

The application command dispatcher was also reviewed: start/restart reset the round
and enter Playing; scores/back select their existing scenes; sound toggles through
the existing settings path; checkpoints still precede scene changes and native
close. The GameOver quit action remains existing behavior for roadmap step 6.
This review does not substitute for the pending native pointer acceptance above.

Local evidence is under `build/ui-acceptance-step-2.4/`: build/test/format logs,
`layout-smoke.log`, isolated `acceptance_qa.cpp` and bundle, native menu/validation
captures, `window-exit.txt`, source hash comparison and `summary.json`. The actual
user profile and the Desktop application were not used or replaced. No branch,
commit or push was performed; Windows/Linux hosted CI was not run.

The next implementation step is roadmap 3, the main menu/player-entry redesign.
It has not started. The manual native pointer tour remains explicitly outstanding;
the later full UI acceptance step must include it before release.

## Roadmap step 3.1 — main menu header and layout

Substep 3.1 is complete. Splash now has a 96 px bold `2048` title without an
underline and a 22 px secondary-color `Join the numbers` subtitle. Both sizes and
the display style live in the shared theme; only Splash consumes these tokens.

The scene measures the title, subtitle and existing form separately. It centers
their combined group within the 856×596 content area, with an 8 px title/subtitle
gap and a 24 px gap before the future card boundary. Another 24 px is reserved
inside that boundary above and below the form for substep 3.2's panel padding.
The player label remains left-aligned with the input; the input and two buttons
retain their equal widths and reserved validation space. No panel or decorative
tile is drawn in this substep. Button labels, focus/validation logic, keyboard
handling and navigation retain their existing behavior.

Native macOS arm64 Release build, all 249 unit cases (281,933 assertions) and all
five platform acceptance scenarios passed. The existing SFML layout smoke was
recompiled against the current production UI objects and passed: content-margin
clipping, form/error spacing, Turkish name input, start/scores/back commands and
the other scenes' layout/command checks. Normal and validation menu PNGs were
visually inspected. This is render/window-backed scene-event evidence; it does
not claim an OS pointer tour or a new full `app::run` acceptance pass.

Evidence is under `build/ui-menu-step-3.1/`, including before snapshots,
build/test/format/smoke logs, `screenshots/`, source-preservation checks and
`summary.json`. Only Splash's header/implementation, theme typography and this
document changed. Core, persistence, paths, migration, headless, App integration
and the other production scenes remain unchanged. No branch/commit/push or
Desktop bundle replacement was performed; hosted Windows/Linux CI was not run.
The next substep is 3.2, the player-entry card; it has not started.

## Roadmap step 3.2 — player-entry card

Substep 3.2 is complete. Splash draws a standard shared panel behind the player
label, input, validation area and both buttons. It uses `createPanel`/`drawPanel`
with the existing theme's cream fill, panel border, 12 px radius and subtle shadow.
No new component, theme variant or dependency was introduced.

The panel width is the 320 px input width plus 24 px padding on each side. Its
height comes from the measured form plus the same top/bottom padding. The header
and panel remain centered as one group, using the space reserved in substep 3.1.
Input and buttons retain their equal widths and existing gaps. Validation space
is included in the panel height from construction, so showing an error does not
resize the card or move controls. The panel is drawn before its contents and has
no event handling of its own. Existing input, button labels and navigation logic
remain unchanged; focus/error polish and button/keyboard changes belong to 3.3/3.4.

Native macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios, formatting and whitespace checks passed. The
existing SFML layout smoke was rebuilt against current production objects and
passed. Its menu checks now also cover card centering, horizontal/bottom padding,
control containment and unchanged card bounds when validation appears. Mapped
window-backed input, start/scores/back commands and the other scene checks still
pass. Normal/error menu PNGs were visually inspected; no clipping or overlap was
found. This evidence does not claim a native OS pointer tour.

Evidence is under `build/ui-menu-step-3.2/`, including snapshots, build/test/smoke
logs, `screenshots/`, source-preservation checks and `summary.json`. Only Splash's
header/implementation and this document changed. Theme, shared drawing helpers,
App integration, other scenes and all frozen backend/build/test sources remain
unchanged. No branch/commit/push or Desktop bundle replacement was performed;
hosted Windows/Linux CI was not run. Substep 3.3 has not started.

## Roadmap step 3.3 — player name focus and validation

Substep 3.3 is complete. Splash tracks name validation separately from focus and
uses the shared input styling helper for normal, focused and invalid borders.
Invalid styling takes priority over focus and survives hover updates. Clicking
the field updates its border immediately; losing window focus removes input
focus and stops text entry until the field is selected again.

Empty-name Start/Enter focuses the field, displays `Lütfen adınızı girin.` in the
reserved space and shows the error border. Entering a valid name clears both
message and error immediately; opening scores also clears validation. Names
containing only ASCII or Unicode whitespace are rejected; surrounding whitespace
is trimmed before the player name is exposed. The existing 18-code-point limit
is now a named private constant, so Turkish multibyte characters count once.
Backspace releases a slot and malformed surrogate/out-of-range input is ignored.
The placeholder and shared text-width fitting remain in use. Layout, labels and
scene commands are unchanged; Tab and Q-key handling remain for substep 3.4.

Native macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios, formatting and whitespace checks passed. The
existing window-backed SFML smoke was rebuilt with focused input checks: immediate
focus border, error auto-focus, error clearing on correction, hover/error
priority, window blur, Turkish names, the 18/19-character boundary, Backspace,
invalid scalar rejection and Unicode whitespace trimming/rejection. Card bounds,
validation placement, viewport clipping and other scene checks still passed.
Six input-state PNGs were visually inspected, including an 18-character wide
name that fits within the field. The harness produces 18 PNGs in total.
These are direct production scene-handler checks, not an OS keyboard/pointer tour.

Evidence is under `build/ui-menu-step-3.3/`: snapshots, build/test/smoke logs,
`layout_smoke.cpp`, `screenshots/`, source-preservation checks and `summary.json`.
Only Splash's header/implementation and this document changed; theme, shared
helpers, App integration, other scenes and all frozen backend/build/test sources
remain unchanged. No branch/commit/push or Desktop bundle replacement was
performed, and hosted Windows/Linux CI was not run. Substep 3.4 has not started.

## Roadmap step 3.4 — menu actions and keyboard access

Substep 3.4 is complete. The menu labels are now `Oyuna Başla` and `Skorlar`,
both using the shared 22 px button typography and equal 320×56 buttons. The
shared button helper draws vector play/trophy icons beside the centered labels,
with text fitting and theme-owned icon size/spacing. No emoji font, asset or
dependency is needed. Other scenes keep the existing draw overload.

Splash has one focus target: none, name, start or scores. Tab selects name first,
then start, then scores, wrapping back to name; Shift+Tab traverses backwards
and selects scores from no focus. Focused buttons brighten and have a 3 px
outline via the shared focus helper, which persists during hover updates.
Enter activates scores when selected, otherwise validates and starts the game.
Scores remains available with an empty name. Invalid start restores name focus;
successful navigation clears focus so returning to the menu has no stale action.
Mouse selection and window blur use the same focus state. Text edits only reach
the selected name field, where Q/q key presses no longer return Quit.
Existing Escape and Q outside the name field remain unchanged for the later
navigation step. No application command, backend or animation behavior changed.

Native macOS arm64 Release build, all 249 unit cases (281,933 assertions), all
five platform acceptance scenarios, formatting and whitespace checks passed.
The SFML smoke was rebuilt and passed the preceding input/card checks plus
forward/reverse Tab wrapping, focused Enter actions, empty-name scores access,
Q/q entry, button-focus edit suppression, blur, focus outlines and mouse hover.
It generated 22 PNGs. Validation text detection accounts for glyph antialiasing;
panel/button bounds and outline color probes remain exact. Menu normal, focus,
error and long-name states were visually inspected.

The current real `app::run` was also opened in an isolated QA bundle/profile.
Native OS keyboard input verified Shift+Tab→scores→Enter→leaderboard,
Escape→menu, Tab→name, `ÇağrıqQ` entry without closing, Tab→start→Enter→Playing,
arrow-key movement/merge, Enter→new round, Escape→menu, empty-name validation
and correction. Restart showed score 0 and retained best 4; the QA-only profile
checkpoint contained best 4 with no completed rounds. Native X closed with exit
0 and the process ended. Ten native captures were saved. Coordinate mouse clicks
were not exercised; their existing desktop automation limitation remains.

Evidence is under `build/ui-menu-step-3.4/`: build/test/smoke logs, window-backed
fixture, isolated app wrapper/bundle/profile, `screenshots/`, `native-*.jpg`,
`window-exit.txt`, source-preservation checks and `summary.json`. Changes are
limited to Splash, additive shared button helpers, theme size tokens and this
document. Other scenes and all frozen App/backend/build/test sources are
unchanged. No branch/commit/push or Desktop bundle replacement was performed;
hosted Windows/Linux CI was not run. Substep 3.5 has not started.

## Roadmap step 3.5 — static decorative menu tiles

Substep 3.5 is complete. Splash surrounds the central header/form with four
decorative tiles: 2 at upper left, 8 at upper right, 2048 at middle left and 4
at lower right. Their sizes (96/108/144 px) and number typography (40/32 px)
live in the shared theme. Fill/text colors come from the existing tile palette,
with the existing tile radius and shared shadow renderer. Shapes and number
labels rotate together by modest angles and are cached during scene construction.
The scene owns their relative positions within the fixed content view.

Decorations draw before the header and card. They remain outside the protected
center column with space around the form, and fit inside the content viewport.
They have no action, focus target, animation, game state or event handler. A click
on a decoration follows the existing empty-background behavior. The central
header/form geometry, input validation, mouse commands and keyboard behavior
remain unchanged. No image asset, framework, dependency or shared helper change
was needed.

Native macOS arm64 Release build, all 249 unit cases (281,933 assertions), all
five platform acceptance scenarios, formatting and whitespace checks passed.
The current window-backed SFML smoke passed all existing checks plus decoration
palette/placement, clearance from the card, viewport containment, no-action
clicks and lack of input focus after clicking tiles. The protected header/form
pixels matched the preceding menu image exactly. Twenty-two PNGs were produced;
normal, error and focused-button menu views were visually inspected. No clipping,
text overlap or control obstruction was found. This pass did not perform a new
native OS keyboard/pointer tour; the final menu acceptance is substep 3.6.

Evidence is under `build/ui-menu-step-3.5/`: snapshots, build/test/smoke logs,
`layout_smoke.cpp`, `screenshots/`, source-preservation checks and `summary.json`.
Only Splash's header/implementation, additive theme size/typography tokens and
this document changed. Shared helpers, App integration, other scenes and all
frozen backend/build/test sources remain unchanged. No branch/commit/push or
Desktop bundle replacement was performed; hosted Windows/Linux CI was not run.
Substep 3.6 has not started.

## Roadmap step 3.6 — final menu acceptance and development closure

Roadmap step 3's implementation is complete. The final acceptance pass required
no production code corrections. Automated checks and the native macOS keyboard
tour passed; native coordinate-pointer acceptance remains pending because the
desktop automation tool returned `noWindowsAvailable` (error -10005). Scene-level
mapped mouse events passed, but do not replace that outstanding OS pointer tour.

| Check | Result |
| --- | --- |
| macOS arm64 Release build | Passed |
| Unit suite | 249 cases, 281,933 assertions passed |
| Platform acceptance | All five scenarios passed |
| Existing window-backed SFML smoke | Passed; 22 PNGs regenerated |
| Empty / whitespace-only name | Native Enter rejected both; field focused and error shown |
| Turkish input | Native `Çağrı`, `İnci`, `Gökçe` rendered correctly; valid editing cleared error |
| Name length / Backspace | 18 Unicode characters retained; 19th ignored; Backspace freed a slot |
| Q/q while editing | Entered as text; application stayed open |
| Tab / Shift+Tab / Enter | Name, start and scores focus/actions passed |
| Scores and return | Keyboard opened leaderboard and returned to menu |
| Start, menu return and re-entry | Keyboard transitions passed; name retained; new round score 0 |
| Native window X | Closed successfully with exit 0; process ended |
| Native coordinate mouse tour | Pending: automation returned `noWindowsAvailable` |
| Format / whitespace | Passed |

The current production `app::run` was linked into an ignored QA wrapper using
isolated profile and legacy directories and the repository assets. Fourteen
native captures record the final menu, validation, Turkish input, character
limit, focus, leaderboard, playing and return/re-entry states. Visual inspection
found no clipping or overlap in the menu. This pass did not exercise gameplay
moves, sound, win or game-over flows; those are outside this menu acceptance.

Evidence is under `build/ui-menu-step-3.6/`: build/test/format/smoke logs, the QA
wrapper/bundle, `native-*.jpg`, `window-exit.txt`, acceptance summaries and source
hash comparisons. The unchanged smoke fixture is
`build/ui-menu-step-3.5/layout_smoke.cpp`; its existing executable regenerated
the 22 PNGs in `build/ui-menu-step-3.5/screenshots/` during this pass.

Only this document changed during 3.6. Across step 3, changes are confined to
Splash, the shared button helpers, additive theme tokens and this document.
App integration, other scenes, core, persistence, paths, migration, headless,
build and test infrastructure retain their step-3 entry hashes. No branch,
commit, push or Desktop bundle replacement was performed. Hosted Windows/Linux
CI was not run. Step 4 (leaderboard redesign) has not started. The development
work for step 3 is closed; full native pointer acceptance is still outstanding.

## Roadmap step 4.1 — leaderboard layout foundation

Substep 4.1 is complete. HighScores keeps the fixed 920×660 application shell
and its 856×596 content view. The title/header, table card and back action form
one horizontally and vertically centered group. The header reserves an 8 px
gap and a 22 px description line for substep 4.2; that line is not drawn yet.
The header-to-card and card-to-button gaps are both 24 px.

The card retains the theme's 640 px width and uses 24 px internal padding.
Its derived height is 332 px: two paddings, the existing 32 px column header,
five 44 px rows and four 8 px row gaps. Existing column labels, row appearance,
placeholder rows, name truncation and empty-state wording remain unchanged.
The existing back button now has stable 320×56 bounds supplied by an additive
theme size token, independent of its current label. Its label, primary style,
hover and navigation handlers are unchanged. Table content uses the same
24 px padding as the card's height calculation.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting/whitespace checks passed. The
existing ignored SFML smoke was rebuilt against the current production scene
objects, with additional rendered-bound checks for the reserved header space,
group centering, stable button bounds, card separation and row/bottom padding.
Mapped mouse events at the rendered back button and the existing Escape action
both returned ShowSplash; table-center clicks returned no action. Twenty-two
PNGs were generated. Empty, one-record and five-record leaderboard images were
visually inspected without clipping or overlap, including Turkish names and
an eight-digit score. Unchanged static images for the menu, gameplay and
game-over screens match the preceding smoke images byte for byte.

Evidence is under `build/ui-leaderboard-step-4.1/`: source snapshots, build/test/
format/smoke logs, `layout_smoke.cpp`, `screenshots/`, source-preservation checks
and `summary.json`. This was a window-backed scene smoke, not a new native OS
keyboard/pointer tour. The previously recorded native coordinate-pointer
automation limitation remains outstanding; final leaderboard acceptance is 4.7.

Only HighScoresScene.cpp, an additive UiTheme size token and this document
changed. HighScores event/hover/truncation methods, all other scene sources,
shared helpers, App integration, backend and build/test infrastructure retain
their 4.1 entry hashes. No branch, commit, push or Desktop bundle replacement
was performed; hosted Windows/Linux CI was not run. Substep 4.2 has not started.

## Roadmap step 4.2 — leaderboard trophy, title and description

Substep 4.2 is complete. The header now draws a trophy beside `En İyi 5 Skor`
and the centered description `En yüksek skorlara ulaşan oyuncular`. The existing
46 px bold heading and 22 px secondary text use the shared typography/colors.
The title and trophy are centered as one group with a 16 px gap. The icon uses
the new theme-owned 40 px heading-icon size, capped by the measured title height
to fit the existing header line. The description occupies the 22 px line
reserved in 4.1, separated from the title by the existing 8 px gap. Card and back
button coordinates and sizes remain unchanged.

The existing Skorlar button's vector trophy drawing was extracted into the
shared `drawTrophyIcon` helper and reused by both callers. Its proportions and
the button rendering are preserved; the header uses an additive gold trophy
color token. No emoji font, image asset, dependency or UI framework is needed.
Description text uses the existing Turkish-glyph/ASCII-fallback helper. The
header introduces no action or focus target; navigation and score data are
unchanged. Table labels, row design, medals and empty-state refinement remain
the later substeps.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five platform
acceptance scenarios and formatting/whitespace checks passed. The current
window-backed SFML smoke passed trophy/title centering, their separation and
vertical alignment, description centering and spacing, non-action header clicks,
existing mapped navigation events and all preceding checks. It generated 22
PNGs. Empty, one-record and five-record leaderboard views and the menu were
visually inspected with no text clipping or overlap. In all three leaderboard
images, the card and everything below it match the 4.1 images pixel for pixel.
All static menu, gameplay and game-over PNGs match 4.1 byte for byte, including
the menu button that now calls the shared trophy helper.

Evidence is under `build/ui-leaderboard-step-4.2/`: source snapshots, build/test/
format/smoke logs, the ignored fixture, `screenshots/`, source-preservation checks,
the scoped diff and `summary.json`. This was a window-backed scene smoke, not a
new native OS keyboard/pointer tour. The previously recorded native pointer
automation limitation remains; final leaderboard acceptance is 4.7.

Changes are limited to HighScoresScene, the extracted shared trophy helper,
additive theme tokens and this document. Other scene sources, App integration,
backend and build/test infrastructure retain their 4.2 entry hashes. Existing
HighScores event/hover/truncation methods are unchanged. No branch, commit, push
or Desktop bundle replacement was performed; hosted Windows/Linux CI was not
run. Substep 4.3 has not started.

## Roadmap step 4.3 — leaderboard columns and record rows

Substep 4.3 is complete. The card now has `#`, `Oyuncu Adı` and `Skor` column
headers, drawn by the shared `drawLeaderboardHeader` helper. Header and row
renderers use one internal column calculation: a centered 48 px rank area,
left-aligned names and a right-aligned 160 px score area. The score width is
theme-owned, and a 12 px gap separates the name and score areas. Name fitting
no longer depends on the particular score's digit count. Headers use the shared
19 px emphasis style; names retain 22 px regular text and scores use 22 px
emphasis. Row ranks are plain numbers under the single `#` header.

Only existing entries are drawn, in their supplied order, capped at five. A
one-record list draws one row instead of four placeholder rows. The 640×332
card stays fixed with 24 px padding, 44 px rows and 8 px row gaps. The existing
alternating row fills/radius remain; medals belong to 4.4. The title, description,
back action, empty-state wording and existing 15-character name truncation are
unchanged. No sorting, score loading, persistence or navigation behavior changed.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting/whitespace checks passed. The
current window-backed SFML smoke verified 0–5 actual rows, omission of
placeholder rows, a six-entry input capped to the identical visible top five,
header/value column alignment, vertical text alignment and separation of long
names from eight-digit scores. Rendered rank-glyph comparisons allow 2 px for
different glyph shapes/weights; the name and score edge comparisons allow 1 px.
The rank probe is restricted to its own column to exclude neighboring text.
All prior smoke checks passed. Twenty-six PNGs were generated; empty, one-,
three- and five-record views were visually inspected without clipping/overlap.
Pixels outside the card match the preceding leaderboard images, while all
static menu, gameplay and game-over PNGs match 4.2 byte for byte.

Evidence is under `build/ui-leaderboard-step-4.3/`: snapshots, build/test/format/
smoke logs, the ignored fixture, `screenshots/`, the scoped diff, source checks
and `summary.json`. This was a window-backed scene smoke, not a new native OS
keyboard/pointer tour. The recorded native pointer automation limitation remains;
final leaderboard acceptance is 4.7.

Changes are confined to HighScoresScene, shared leaderboard drawing helpers,
the additive score-width theme token and this document. Other scene sources,
other shared drawing helpers, App integration, backend and build/test sources
retain their 4.3 entry content. HighScores event/hover/truncation methods are
unchanged. No branch, commit, push or Desktop bundle replacement was performed;
hosted Windows/Linux CI was not run. Substep 4.4 has not started.

## Roadmap step 4.4 — first-three medals and winner emphasis

Substep 4.4 is complete. The shared leaderboard row helper draws flat circular
medals for the first three places: gold, silver and bronze. Their fill/border
palette lives in the shared theme. Medals have a theme-owned 32 px diameter and
2 px outline, centered in the existing 48 px rank column and 44 px row. Dark
18 px emphasized numbers keep the ranks readable. Fourth and fifth place retain
their existing neutral 20 px numbers and alternating row fills. The first row
has a subtle cream/gold background highlight; names and scores keep their existing
typography and alignment. No gradient, image asset or dependency was added.

No scene or navigation code changed. Header, card, row spacing, column widths,
back action, supplied record order/count and name truncation are unchanged.
An empty leaderboard draws no medals or highlighted row. The new appearance is
implemented entirely in the shared row helper and additive theme tokens.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting/whitespace checks passed. The
current window-backed SFML smoke verified distinct medal colors, visible fills,
centered outlines, clearance within the row and from the name column, and the
first-row highlight. Fourth/fifth row pixels match 4.3 exactly. All existing
0–5 record, capped-input, alignment, mapped navigation and other scene checks
passed. Twenty-six PNGs were generated; one-, three- and five-record views were
visually inspected without clipping or overlap. Empty leaderboard, all static
menu, gameplay and game-over PNGs match the preceding images byte for byte;
pixels outside the leaderboard card are unchanged as well.

Evidence is under `build/ui-leaderboard-step-4.4/`: source snapshots, build/test/
format/smoke logs, the ignored fixture, `screenshots/`, scoped diff, source checks
and `summary.json`. This was a window-backed scene smoke, not a new native OS
keyboard/pointer tour. The recorded native pointer automation limitation remains;
final leaderboard acceptance is 4.7.

Only UiPrimitives.cpp, additive UiTheme tokens and this document changed.
All scene sources, other shared drawing helpers, App integration, backend and
build/test infrastructure retain their 4.4 entry content. No branch, commit,
push or Desktop bundle replacement was performed; hosted Windows/Linux CI was
not run. Substep 4.5 has not started.

## Roadmap step 4.5 — empty list and text limits

Substep 4.5 is complete. An empty card now displays only the centered 22 px
secondary message `Henüz skor yok` (with the existing ASCII fallback). Column
headers are drawn when records exist. Card, header/title, medals and back-button
geometry remain unchanged, including when the same scene alternates between
populated and empty data.

The scene's fixed 15-character name truncation was removed. The shared row
renderer receives the full Unicode name, keeps normal 22 px typography and
shortens only names exceeding the 348 px name area. An internal binary search
selects a fitting Unicode-code-point prefix with `…` (or `...` if unsupported by
the font). Fitting names are displayed completely instead of being cut by their
character count or shrunk. Single-line display replaces control characters and
Unicode line separators with spaces in a copy of the name; stored data is not
modified. Score rendering retains its existing 160 px fitting/right alignment
and was verified with eight-digit scores and `2147483647`.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting/whitespace checks passed. The
current window-backed SFML smoke verified the centered empty message, absence
of empty-state column headers, complete fitting 18-character Turkish names at
normal font size, long wide/Unicode names, control-character display, name/score
clearance, row containment and restoration of the exact empty image after a
populated render. The empty-message probe includes antialiased glyph edges;
full-name ink dimensions were compared with independently rendered normal-size
text, allowing 2 px raster variation. All prior scene/layout/navigation checks
passed. Twenty-eight PNGs were generated; empty, normal five-record and text-limit
views were visually inspected without clipping, tiny name text or overlap.
Static menu, gameplay and game-over PNGs match 4.4 byte for byte; leaderboard
pixels outside the card are unchanged.

Evidence is under `build/ui-leaderboard-step-4.5/`: snapshots, build/test/format/
smoke logs, the ignored fixture, `screenshots/`, scoped diff, source checks and
`summary.json`. This was a window-backed scene smoke, not a new native OS
keyboard/pointer tour. The recorded native pointer automation limitation remains;
final leaderboard acceptance is 4.7.

Only HighScoresScene, private name-display helpers in UiPrimitives.cpp and this
document changed. Event/hover methods, other scene sources, other shared drawing
helpers, theme tokens, App integration, backend and build/test infrastructure
retain their 4.5 entry content. Record count/order, scores and persisted names
are unchanged. No branch, commit, push or Desktop bundle replacement was
performed; hosted Windows/Linux CI was not run. Substep 4.6 has not started.

## Roadmap step 4.6 — back action and keyboard access

Substep 4.6 is complete. The centered 320 × 56 px back button uses the shared
secondary style, the shorter label `Geri` and a shared vector back-arrow icon.
The icon/label group remains centered. Existing Play/Trophy icon rendering,
leaderboard title, card, rows, text fitting and geometry are unchanged.

Tab or Shift+Tab focuses the screen's sole button with the shared 3 px focus
outline and hover fill. Enter returns to Splash only while that button has
keyboard focus; an unfocused Enter does nothing. Escape, Q and Backspace retain
their existing Splash destination. A primary mouse release clears keyboard
focus and returns only when it hits the back button. Losing window focus or
returning to Splash also clears focus, preventing stale Enter activation when
the persistent scene is reopened. Pointer hover cannot erase keyboard focus.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The existing
window-backed SFML smoke verified Tab/Shift+Tab, focused/unfocused Enter,
Escape/Q/Backspace, mapped mouse navigation, background-click/blur/return resets,
hover and outline behavior, plus all preceding scene/layout checks. Thirty-two
PNGs were generated. Leaderboard title/card pixels match 4.5; static menu,
gameplay and game-over images are unchanged. Normal five-record and focused
empty views were visually inspected.

A separate native macOS QA bundle with an isolated profile verified menu-to-
leaderboard navigation, unfocused Enter, Tab focus, Enter return and an unfocused
reentry. Five native screenshots record these checks. Further native input was
stopped after the QA window changed to gameplay; the active window was left
open. Native Shift+Tab on the leaderboard, Escape and close were not verified
in this tour. The previously recorded native pointer automation limitation
remains; scene-mapped mouse checks are separate evidence. Final leaderboard
acceptance belongs to 4.7.

Evidence is under `build/ui-leaderboard-step-4.6/`: source snapshots, build/test/
format/smoke logs, the ignored fixtures, scene/native screenshots, scoped diff,
source checks and `summary.json`. Only HighScoresScene, the shared button-icon
enum/drawing overload and this document changed. Theme, other scenes, App,
backend and build/test infrastructure retain their 4.6 entry content. No branch,
commit, push or Desktop bundle replacement was performed; hosted Windows/Linux
CI was not run. Substep 4.7 has not started.

## Roadmap step 4.7 — leaderboard acceptance and closure

Roadmap step 4 development is closed: layout/header, rows, medals, empty state,
text limits and back/keyboard behavior are implemented. The final acceptance
tour found no production defect requiring another change. One native pointer
check remains unverified because the computer-use tool cannot route coordinate
clicks to this window; this is an explicit acceptance limitation, not a passing
manual check.

| Check | Result and evidence |
| --- | --- |
| macOS arm64 Release build | Passed; `build.log` |
| Unit suite | 249 cases, 281,933 assertions passed; `tests.log` |
| Platform acceptance | All five scenarios passed; `tests.log` |
| Formatting and whitespace | Passed; `format.log` and scoped source checks |
| 0–5 rows and input cap | Passed in the existing SFML scene smoke |
| Empty/list restoration, columns and medals | Passed; empty, one and five-record views inspected |
| Turkish/long/control names and large scores | Passed; text-limit view inspected |
| Mouse hit testing, hover and focus reset | Passed with scene-mapped SFML events |
| Native empty and five-record rendering | Passed in an isolated profile; native screenshots |
| Native Tab/Shift+Tab, Enter and reentry | Passed; focus and return screenshots |
| Native Escape/Q/Backspace to Splash | Passed; application remained open |
| Native window close | Both QA runs exited with status 0 |
| Native coordinate click on Geri | Unverified: tool error `-10005: noWindowsAvailable` |
| Hosted Windows/Linux CI | Not run locally; retained for the later cross-platform acceptance phase |

The existing ignored scene fixture was rebuilt against current production
objects and generated 32 PNGs. All 29 static images match 4.6 byte for byte;
animation samples were exercised separately by the fixture. The native QA
bundle used an isolated empty profile first, then five synthetic records with
Turkish names and an eight-digit best score. The seeded scores file is unchanged
after navigation and close. Both QA windows were closed; the earlier 4.6 window
and the user's real profile/Desktop bundle were not operated on in this tour.

Evidence is under `build/ui-leaderboard-step-4.7/`: entry source hashes, logs,
the reused ignored fixtures, screenshots, native exit results and pointer-error
record, `acceptance.md`, scoped documentation diff, preservation checks and
`summary.json`. This acceptance step changes only this document; all production
sources, assets, backend and build/test infrastructure retain their entry
content. No branch, commit or push was performed. Step 5 has not started.

## Roadmap step 5.1 — playing screen columns

Substep 5.1 is complete. The fixed 856 × 596 logical content surface now has
a 520 × 520 left board area, a 24 px column gap and a 312 × 520 right panel.
Both areas are vertically centered at logical y=38. Additive theme tokens
define these dimensions; a small private layout calculation supplies both the
board translation and the relocated control positions. Panel outlines are
inset within the reserved area, preserving the existing 32 px window margins.

The existing 450 px board geometry (100 px cells and 10 px gaps) is centered
inside the left area. Tile sizes, palette, typography and drawing helpers are
unchanged; board background and larger cells belong to 5.2. Static and animated
tiles use the same translated centers. Slide/merge/spawn timing, scales, hidden
cells and movement state are unchanged.

Existing score labels and the hamburger menu moved into the right panel.
The popup and its New Game/Sound actions remain available there until the later
control-panel steps. Rendering, hover and hit testing share the relocated menu
layout. Floating score feedback is anchored below the best label within the
right column, retaining its existing fade, duration and 22 px upward travel.
No new navigation command, panel title, player field, score card or toggle was
added in this substep.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The existing
ignored window-backed SFML fixture was adapted to the new placement and
generated 32 PNGs. It verified left-board/right-panel alignment and containment,
exact baseline tile pixels after translation, eight-digit score clearance,
relocated mapped menu/New Game/Sound clicks, old-HUD/background clicks,
direction/restart/Escape keys, popup clearance, unchanged modal alignment and
animation settlement. Previous menu and leaderboard checks also passed.
Gameplay, open popup and merge/spawn images were visually inspected.

A separate native QA bundle with an isolated profile verified player entry,
the two-column game screen, a Left move/merge (score 4), N restart (score 0,
best 4 retained), Escape return with the entered name and OS close (exit 0).
Four native screenshots record this tour. Native coordinate mouse input was
not retried; scene-mapped mouse evidence is distinct from the previously
recorded native pointer limitation.

Evidence is under `build/ui-gameplay-step-5.1/`: entry source hashes/snapshots,
build/test/format/smoke logs, the reused ignored fixtures, screenshots, scoped
diff, preservation checks and `summary.json`. Only PlayingScene.cpp, additive
UiTheme layout tokens and this document changed. App, other scenes, shared
drawing helpers, core/persistence, assets and build/test infrastructure retain
their entry content. No branch, commit, push or Desktop bundle replacement was
performed; hosted Windows/Linux CI was not run. Substep 5.2 has not started.

## Roadmap step 5.2 — board surface and cell spacing

Substep 5.2 is complete. The left board now fills its reserved 520 × 520 px
area with the shared board color, a 16 px corner radius, the existing panel
outline and subtle shared shadow. The outline is inset inside the reserved
bounds, keeping the application margins and 24 px column gap clear.

Cells are 110 × 110 px; three internal gaps and both outer insets are 16 px:
`4 × 110 + 5 × 16 = 520`. The shared cell-size/gap tokens supply existing cell
centers and drawing helpers. Board bounds and animated/static tile centers
derive from this same extent. Empty cells retain the central empty-tile color,
which is lighter than the board surface. Palette, tile font sizes/styles,
tile radius, animation durations and movement state are unchanged; tile
typography is the next substep. Right-panel layout and control behavior retain
their 5.1 content.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The reused ignored
SFML scene fixture generated 34 PNGs, adding entirely empty and full boards.
Pixel checks verified the exact outer frame, all 16 cell widths/heights, every
horizontal/vertical gap, symmetric insets, rounded corners, the clear column
gap and clearance from the border during the existing maximum merge pop.
Centered tile ink/palette patches match 5.1; right-panel pixels and all 24
menu/leaderboard PNGs are unchanged. Existing mapped control, modal, large-score
and slide/merge/spawn settlement checks passed. Normal/empty/open-menu board
views and merge/spawn rendering were visually inspected.

An isolated native macOS QA bundle verified the new board, player entry,
Left/Down moves with a merge (score 4), Escape return, reentry and N restart.
The first restart capture did not show a reset score, so it was inconclusive;
a fresh reentry/restart check showed score 0 and two starting tiles at new
positions, with best 8 retained. Seven native screenshots preserve both the
initial capture and the recheck. OS close returned 0. Native coordinate mouse
input was not retried; the existing tool limitation remains separate from
passing scene-mapped mouse checks.

Evidence is under `build/ui-gameplay-step-5.2/`: entry source hashes/snapshots,
build/test/format/smoke logs, reused ignored fixtures, scene/native screenshots,
scoped diff, source-preservation checks and `summary.json`. Only PlayingScene.cpp,
UiTheme board geometry/radius tokens and this document changed. Other scenes,
shared drawing helpers, App, core/persistence, assets and build/test
infrastructure retain their entry content. No branch, commit, push or Desktop
bundle replacement was performed; hosted Windows/Linux CI was not run.
Substep 5.3 has not started.

## Roadmap step 5.3 — tile palette and typography

Substep 5.3 is complete. Tile labels now use a dedicated bold theme style and
four nominal sizes: 46 px through 64, 38 px through 512, 32 px for 1024 and
30 px for 2048 and larger. Label origins use actual glyph bounds for visual
centering. The existing shared width fitter limits labels to an 86 px inner
area (110 px cell minus two 12 px insets). Long values remain complete; their
uniform fit scale combines with the existing merge/spawn animation scale.

The central tile palette now supplies both fill and foreground colors. All
twelve existing fills are preserved. Tiles 2/4 and 128–4096 use primary dark
text; 8–64 retain white text. Unlisted positive values retain the dark fill
and white-text fallback. The legacy decoration text color is unchanged, keeping
menu tiles at their previous appearance. Geometry, empty-cell rendering,
other typography and animation timing/state remain unchanged.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The reused ignored
SFML smoke generated 36 PNGs, adding a tile-value gallery and a standalone
`2147483647` drawing. It compared common labels with independently drawn bold
font tiers and verified visible palette foregrounds, centered ink bounds,
complete long values through 1073741824 and INT_MAX, the 86 px inset, and
combined fit/merge (1.17)/spawn (0.82) scaling with partial/zero alpha.
All prior layout/navigation/animation checks passed. Board pixels outside
label areas, right-panel pixels and all 24 menu/leaderboard images retain
their 5.2 appearance. Normal/gallery/max-value and merge/spawn views were
visually inspected without clipping or overlap.

Final native macOS QA used an isolated profile and verified player entry,
the updated 2/4 labels, a Left move, Escape return with the entered name, the
preserved menu decoration appearance and OS close (exit 0). Three screenshots
record this final tour. Native coordinate mouse input was not retried; the
existing tool limitation remains separate from passing scene-mapped checks.

Evidence is under `build/ui-gameplay-step-5.3/`: entry source hashes/snapshots,
build/test/format/smoke logs, reused ignored fixtures, scene/native screenshots,
scoped diff, source-preservation checks and `summary.json`. Only drawTile in
UiPrimitives.cpp, the UiTheme tile palette/typography/inset tokens and this
document changed. All scene sources, other shared drawing helpers, App,
core/persistence, assets and build/test infrastructure retain their entry
content. No branch, commit, push or Desktop bundle replacement was performed;
hosted Windows/Linux CI was not run. Substep 5.4 has not started.

## Roadmap step 5.4 — existing board animation integration

Substep 5.4 is complete. Static cells, sliding tiles and spawn tiles already
use the shared board offset and cell centers established in 5.1/5.2; merge
and spawn scales also retain the label fitting introduced in 5.3. Acceptance
found no integration defect, so no production source change was necessary.
Existing durations remain 90 ms for slide, 120 ms for merge pop and 140 ms
for spawn fade/scale, with the latter stages starting after the slide.
Animation state, input blocking, score feedback and App transitions are unchanged.

The reused ignored SFML scene fixture now samples actual PlayingScene rendering
with its public onMove API and real steady clock. Four directional moves are
captured during slide, spawn and settlement. Measured render intervals stay
inside the relevant stages. Checks verify interpolation between the new cell
centers, a single moving tile, hidden final/spawn values during slide, partial
spawn alpha/scale in the actual spawned cell and exact final centers. Each
settled frame matches a fresh static scene pixel for pixel. Direction input is
blocked during movement and accepted after settlement.

Simultaneous top-left and bottom-left merges (4 and 8192) verify centered pop
near its existing peak and clearance from the board outline and application
margins. The settled board matches static rendering; resetVisualEffects also
clears the floating score. Restart/menu commands during animation clear all
effects immediately. Optional no-spawn moves settle after the slide, and a
no-op move leaves the frame unchanged. These checks generated 15 animation
PNGs alongside 33 prior static PNGs, all of which are byte-identical to 5.3.
Slide, spawn, edge-pop and settled views were visually inspected.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. An isolated native
macOS QA bundle verified player entry, Left/Down moves, a merge with score 4,
N restart to score 0 while retaining best 4, Escape return with the entered
name and OS close (exit 0). Five screenshots record this tour. Native
coordinate mouse input was not retried; its existing tool limitation remains
separate from passing scene-mapped event checks.

Evidence is under `build/ui-gameplay-step-5.4/`: entry source hashes/document
snapshot, build/test/format/smoke logs, ignored scene/native QA fixtures,
48 scene PNGs, native screenshots, static-regression comparison, scoped diff,
source-preservation checks and summary.json. Only this document changed among
the 132 entry source/configuration/document files. Production UI, App, core,
persistence, assets and build/test infrastructure retain their entry content.
No branch, commit, push or Desktop bundle replacement was performed; hosted
Windows/Linux CI was not run. Board work 5.1–5.4 is complete. Substep 5.5
(right-panel heading and player identity) has not started.

## Roadmap steps 5.5–5.6 — panel identity and score cards

Substeps 5.5 and 5.6 are complete together. The right panel now presents a
64 px bold 2048 heading, an 18 px two-line Turkish description, an Oyuncu
caption and a 22 px emphasized player name. The header and player block share
24 px panel padding and fixed vertical reservations, keeping score cards
stationary when names change. New typography and component-height tokens
live in UiTheme.

PlayingScene receives the current GameSession player name as a render argument.
The sole App.cpp change is this argument at the existing drawing call, also
supplying the correct identity behind Game Over. There is no duplicate player
state or change to session/round/checkpoint behavior. Name rendering uses the
existing Unicode conversion, single-line normalization and ellipsis logic;
long names retain their readable font size and remain within the 264 px inner
width. An empty display name falls back to Oyuncu. These transformations affect
the displayed copy only; stored player metadata remains intact.

Skor and En İyi now use the shared drawScoreCard helper. Equal 124 × 88 px
cards occupy the same row with a 16 px gap, centered captions and emphasized
values. Their existing uniform width fitter preserves all digits through
INT_MAX. Current score and best still come directly from their existing game
and score-manager values. The existing floating score is centered below the
Skor card and fitted to its inner width; its lifetime, fade and upward motion
are unchanged. The temporary hamburger/popup is positioned below the cards,
with the popup fully inside the right panel. Its commands and mapped hit
targets remain unchanged. Direct controls and the sound toggle remain later
substeps.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The reused ignored
SFML fixture generated 57 scene PNGs. Added checks cover header/player spacing,
equal card bounds, full antialiased glyph centering, card insets and caption/value
separation at 0, 416/668, 99999999 and 2147483647. Turkish, long, empty and
control-separated names fit; changing a long name back to Çağrı restores the
original frame exactly. The lower popup's drawing and mapped New Game action
agree. All existing four-direction slide/spawn/settlement, edge-merge and
animation-reset checks passed. The 24 menu/leaderboard PNGs and standalone
INT_MAX tile PNG are byte-identical to 5.4. Normal, large-score, popup and
floating-score/edge-pop views were visually inspected.

An isolated native macOS QA tour verified Çağrı entry, Left/Up moves with a
merge updating both cards to 4, N restart to score 0 while retaining best 4
and player identity, Escape return, and starting with a different 20-character
name whose panel display uses an ellipsis. Five native screenshots preserve
these states; OS close returned 0. Native coordinate mouse input was not
retried; the existing tool limitation remains separate from passing scene-mapped
event checks. Hosted Windows/Linux CI was not run.

Evidence is under `build/ui-gameplay-step-5.5-5.6/`: entry hashes/snapshots,
build/test/format/smoke logs, ignored fixtures, scene/native screenshots,
static/behavior-preservation checks, scoped diff and summary.json. Production
changes are limited to PlayingScene, additive UiTheme tokens, exposing the
unchanged text-width fitter and the small single-line fitting wrapper, plus
the App drawing argument. Board rendering, ten PlayingScene event/animation
functions, other scenes, core/persistence, assets and build/test infrastructure
retain their entry content. No branch, commit, push or Desktop bundle
replacement was performed. Substep 5.7 (direct New Game/Main Menu controls)
has not started.

## Roadmap step 5.7 — direct New Game and Main Menu controls

Substep 5.7 is complete, including the user's manual Main Menu confirmation. The
hamburger button, popup drawing/state and unused menu-size tokens are removed.
Yeni Oyun and Ana Menü are now always visible in the right panel, each using
a 264 × 44 px button aligned with the header/card padding. Shared vector
restart/home icons avoid font-dependent symbol rendering. Hover colors and
button styles continue to come from the central theme and shared primitives.

Yeni Oyun returns the existing RestartGame command; Ana Menü returns
ShowSplash, never Quit. Both clear movement/floating effects immediately,
including when pressed during animation. App.cpp is unchanged: its existing
checkpoint dispatch still runs before either transition, RestartGame resets
the round with the current player, and ShowSplash returns to player entry
without closing the application. N/Enter and Escape retain their respective
existing commands. No session, persistence, path, migration or headless code
changed; Game Over remains a later roadmap task.

To retain sound access when removing the popup, the existing sound button is
temporarily exposed between the two actions at 264 × 24 px. It uses the same
ToggleSound command and existing on/off styles. It is not a toggle redesign;
that remains step 5.8. Controls have 8 px layout gaps. Mouse release uses each
drawn control's global bounds without additional hit padding, so adjacent hit
regions do not overlap. Secondary clicks, presses, gaps and the retired
hamburger location do not activate an action. Floating score feedback's
vertical anchor is moved up 12 px to clear the new actions; its scale fitting,
fade, upward-motion amount and lifetime remain unchanged.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The reused ignored
SFML fixture generated 59 scene PNGs. It verifies exact action/sound bounds,
spacing, direct mapped commands, non-activating gaps and secondary input,
hover/on-off colors, and direct navigation/reset during movement. Existing
header/name/large-score checks and four-direction slide/spawn/settlement,
edge-merge and animation-reset checks passed. The 24 menu/leaderboard PNGs
and standalone INT_MAX tile remain byte-identical to 5.5–5.6. Direct controls
and edge-pop/floating feedback were visually inspected without overlap.

Native macOS acceptance is complete through the agent's checks and the user's
manual confirmation. After the user's explicit
Computer Use invocation, the plugin's current node_repl/@oai/sky interface
provided native access. Çağrı entry and a Left merge produced score/best 4.
A successful real New Game click reset score to 0 and left two starting tiles,
while preserving the player and best 4. The initial path-based click returned
noWindowsAvailable; rebinding by bundle ID and raising the window enabled this
successful click.

The first Main Menu attempt was interrupted by user activity. Re-observation
showed gameplay at score/best 368. Subsequent Main Menu coordinate clicks
returned tool error -10005/noWindowsAvailable despite window activation and
rebinding attempts. A Retina-coordinate probe did not activate the button.
The agent's Main Menu pointer attempt remained inconclusive, separate from
the passing scene-mapped command check. On 2026-10-12, the user explicitly
confirmed that they tested the Main Menu button and it returns to the main
menu. This human verification closes the remaining pointer acceptance item;
it is recorded as user-confirmed, not as an agent-performed successful click.
Escape successfully returned to player
entry with Çağrı retained, without closing the app. A relaunch retained best
368. Both native OS-close attempts completed with fresh exit code 0; the
final process was confirmed gone. Seven native PNGs, close observations and
`native-acceptance.json` preserve successful checks and inconclusive attempts.
No production fix was made for this tool limitation. Hosted Windows/Linux
CI was not run.

Evidence is under `build/ui-gameplay-step-5.7/`: entry hashes/snapshots,
build/test/format/smoke logs, ignored fixtures and isolated QA bundle,
scene screenshots, static/behavior-preservation checks, scoped diff and
summary.json. Changes are limited to PlayingScene, two added icon branches
in the shared button renderer, central action/sound heights, removal of menu
tokens, and this document. Board drawing and animation logic, panel identity
and score cards, App, core/persistence, assets, other scenes and build/test
infrastructure retain their entry content. No branch, commit, push or Desktop
bundle replacement was performed. Substep 5.8 (sound toggle) has not started.

## Roadmap step 5.8 — sound toggle

Substep 5.8 is complete. The temporary full-width green/gray sound button is
replaced by a neutral outlined sound row, a left-aligned Ses Açık/Ses Kapalı
label and the existing shared drawToggle helper on the right. The row retains
its 264 × 24 px reservation, so New Game/Main Menu bounds, control gaps,
score feedback and header/board layout remain unchanged. The 44 × 24 px
track has a 12 px right inset; its thumb sits on the right when enabled and
on the left when disabled, with 20 px travel. Central on/off/hover colors and
thumb geometry come from the unchanged shared primitive.

The whole outlined row is the hit region, including the label and whitespace.
Hovering anywhere in that region brightens the track. The localized 20 px
status label is fitted separately and remains clear of the switch. A release
still emits only ToggleSound; the scene does not flip its own preference.
App's existing SoundManager/settings checkpoint then calls setSoundEnabled
to synchronize both text and thumb. Sound changes remain available during
tile animation without clearing effects, board or score. Reset/new round
retains the externally owned sound preference. Keyboard/focus redesign
remains substep 5.9.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five
platform acceptance scenarios and formatting checks passed. The reused ignored
SFML fixture generated 65 PNGs. Added checks verify track/row bounds, thumb
centers, all row hit regions, complete label clearance, on/off hover colors,
state synchronization under hover, no independent scene toggle, no board/score
mutation and unchanged preference after visual reset. Toggle interaction and
state synchronization during movement preserve active animation. Existing
board, navigation, names/large-score and four-direction animation checks pass.
The 24 menu/leaderboard PNGs and standalone INT_MAX tile are byte-identical
to 5.7. On/off/hover views were visually inspected.

Native macOS QA used the Computer Use plugin's node_repl/@oai/sky interface
and an isolated profile. With İnci, a real label-area click changed the label
to Ses Kapalı, moved the thumb left and saved sound_enabled=false. Two initial
restart attempts were interrupted by user activity; after re-observation and
the user's brief handoff, N restarted to score 0 while retaining the off state,
player and best 4. Native OS close returned 0. Relaunch/player entry restored
the off state from disk. A real track click restored Ses Açık/right thumb and
saved sound_enabled=true. Final OS close returned 0 and the process was gone.
Six native screenshots and fresh close/settings records preserve this tour.

Evidence is under `build/ui-gameplay-step-5.8/`: entry hashes/snapshots,
build/test/format/smoke logs, reused ignored fixtures and isolated QA bundle,
scene/native screenshots, static/behavior-preservation checks, scoped diff
and summary.json. Changes are limited to PlayingScene, renaming the central
sound-row height token, and this document. Shared primitives, App, SoundManager,
SettingsStore, core/persistence, assets, other scenes and build/test
infrastructure retain their entry content. Movement/floating-score functions
and tile animation rendering are unchanged. No branch, commit, push or
Desktop bundle replacement was performed; hosted Windows/Linux CI was not
run. Substep 5.9 (keyboard/focus) has not started.

## Roadmap step 5.9 — gameplay keyboard and focus

Substep 5.9 is complete. Tab traverses New Game → Sound → Main Menu and wraps;
Shift+Tab reverses that order, starting at Main Menu when no control is focused.
The shared focus outline and hover colors make the current target visible,
including when the pointer is outside it. The sound row restores its normal
panel border when focus leaves, and preference synchronization retains focus.

Enter and Space activate the focused control on key release. Repeated key-down
events only arm the action, so a held activation key does not repeatedly switch
sound. Changing focus, losing window focus, clicking the board or issuing an
accepted movement cancels the pending action. New Game/Main Menu clear focus
and visual effects before returning their existing commands. No control focus
survives a round restart or a later visit to Playing. Modified Tab and focused
activation events are ignored. Unfocused Enter, N, Escape and direction
keys retain their existing behavior; unfocused Space has no action.

Keyboard controls remain available during movement. Sound preserves active
animation; New Game/Main Menu clear it through the existing visual reset.
Direction input retains its existing animation gate. Focus loss cancels control
activation without discarding movement effects or changing the board. All
commands still go through App; the scene does not change player, round ID, RNG,
settings or core score.

## Roadmap step 5.10 — gameplay acceptance and step 5 closure

Substep 5.10 and roadmap step 5 are complete. Acceptance covers the finished
board and right panel, their input, and regression of the earlier scenes.
It does not claim completion of the later navigation/win/game-over redesigns
or the roadmap step 12 cross-platform UI acceptance.

| Check | Result / evidence |
| --- | --- |
| macOS arm64 Release build | Passed |
| Unit tests | 249 cases, 281,933 assertions passed |
| Platform acceptance | All five scenarios passed, including missing font and persistence reopen |
| Formatting | Passed |
| Board/layout/type | 520 px board, 110 px cells, 16 px gaps, centered fitting tile values and separate controls passed |
| Player/score edge cases | Turkish, long/control-containing names; 0, 99,999,999 and INT_MAX scores passed |
| Input/focus | Both traversal directions/wrap, visible rings, repeated press/single release, blur/background/arrow cancellation and reset passed |
| Animation preservation | Four real timed movement/spawn/settled sequences, edge merge, no-op/no-spawn and control interaction passed |
| Earlier-scene regression | Menu/leaderboard rendering and existing modal layout/commands passed |
| Native macOS tour | Player entry, empty leaderboard/back, gameplay, focus, sound, movement, New Game, Main Menu and OS close passed |

The reused ignored SFML fixture produced 71 PNGs, including six new focus
views. Of the 65 prior images, 58 are byte-identical to 5.8, including every
menu/leaderboard image, static gameplay, panel, sound, standalone INT_MAX tile
and legacy modal views. The seven differing images sample live slide/pop/spawn
times; their measured animation intervals and geometry checks passed. They are
not treated as deterministic screenshot baselines. Focus, large scores and
tile-gallery views were visually inspected without clipping or overlap.

Native QA used the Computer Use plugin's node_repl/@oai/sky interface with an
isolated QA510 profile. Keyboard navigation opened the empty leaderboard and
returned to Splash; Çağrı started Playing. Successive Tab presses visibly
focused New Game, Sound and Main Menu. Return switched sound off and Space
restored it, with matching settings.json values and unchanged board/score.
Focused Main Menu returned to Splash without closing the application. Reentry
had no stale ring. Left from a focused control moved the board and cleared the
ring; Up merged to score/best 4. Focused New Game/Space reset to score 0 and two
tiles while retaining Çağrı, best 4 and sound. Shift+Tab from reset selected
Main Menu, and Return again reached Splash. The native OS close button produced
a fresh exit code 0; the process was confirmed gone. Fourteen native PNGs and
the action/close records preserve this tour. Earlier pointer acceptance remains
recorded in 5.7/5.8, including the user's Main Menu confirmation.

Evidence is under `build/ui-gameplay-step-5.9-5.10/`: entry hashes/snapshots,
build/test/format/smoke logs, reused ignored fixtures, isolated QA bundle,
scene/native screenshots, image regression, preservation checks, scoped diff
and summary.json. This increment changes only PlayingScene.hpp/.cpp and this
document. Rendering/layout, animation functions, shared theme/primitives, App,
other scenes, core, persistence, paths, migration, headless code, assets and
build/test infrastructure retain their entry content. No branch, commit, push
or Desktop bundle replacement was performed. Hosted Windows/Linux CI was not
run. The next roadmap item is step 6, navigation; it has not started.

## Roadmap step 6.1 — Playing and Game Over navigation

Substep 6.1 is complete. Game Over now offers Tekrar Oyna and Ana Menü.
The former ÇIKIŞ mouse action returns ShowSplash instead of Quit, using the
shared secondary button style and localized Ana Menü/ASCII fallback. Replay
continues to return RestartGame. Internal members describe Main Menu rather
than quit. The existing modal, score/best text and two-button layout remain;
this is not the roadmap step 8 visual redesign. Button height still follows
the existing label-metric helper, and both complete labels fit the equal-width
buttons with padding.

The existing App dispatcher already implements the required behavior, so App
and PlayingScene were not changed. RestartGame creates a new round with score
0 while preserving the current player and profile best. ShowSplash changes the
scene without resetting the round or closing the window. App's existing score
and settings checkpoints run before either transition; completed-round identity
and score are captured before restart. Returning to Splash then starting again
uses the existing player-entry flow. An unfinished round remains outside the
completed history.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five platform
acceptance scenarios and formatting passed. The reused ignored SFML fixture
produced 72 PNGs and checks both rendered Game Over hit regions, replay shortcuts,
complete label fitting, secondary hover, background/secondary-click neutrality,
Playing controls and earlier scene/board/focus/animation regression. The 58
byte-identical prior images include all menu/leaderboard, static Playing and
focus views. Two modal images change as expected; the other differences sample
live animation timing and pass their measured geometry/stage checks.

A separate ignored integration fixture includes the actual App.cpp dispatcher
without modifying it. Real GameOverScene/PlayingScene button commands are routed
through needsScoreCheckpoint/applySceneCommand using public GameSession and
ScoreCoordinator APIs. Both completed and unfinished sessions are covered:
player preservation, distinct new-round identity/score 0, ShowSplash retaining
round/board/score, open window, checkpoint-before-reset, original completed
metadata, no duplicate completion, no unfinished completion and fresh disk
reopen all passed. No production test hook or new CMake target was added.

Native Computer Use QA used an isolated seeded fixture containing the actual
scenes and actual App dispatcher, rather than the full app bootstrap. A real
Tekrar Oyna pointer click changed the completed score 2108 to 0, preserved Çağrı
and best 2108, created a distinct round and left the saved completion count at
1. On a separate seeded round after relaunch, a real Ana Menü pointer click
opened Splash and left the window open, retaining session identity/score and
completion count 2. Each launch represents a distinct completed round; neither
button duplicates a completion. Both native OS close actions returned fresh
exit code 0, and the final process was confirmed gone. Four native PNGs and
dispatcher/close records preserve the checks.

Evidence is under `build/ui-navigation-step-6.1/`: entry hashes/snapshots,
build/unit/platform/format/scene/integration logs, ignored fixtures and isolated
QA bundle/profiles, screenshots, source/image preservation checks, scoped diff
and summary.json. Only GameOverScene.cpp/.hpp and this document change in this
increment. App, PlayingScene, core, session/persistence, path/migration/headless,
assets, shared primitives/theme and build/test infrastructure retain their
entry content. No branch, commit, push or Desktop bundle replacement occurred;
hosted Windows/Linux CI was not run.

Scope boundary: legacy Game Over and Splash Q/Escape exit shortcuts are still
present. Their keyboard/OS-close/focus behavior belongs to 6.2, which has not
started. Full eight-transition acceptance remains 6.3. Step 6 as a whole is
therefore still in progress.

## Roadmap step 6.2 — keyboard, focus boundaries and OS close

Substep 6.2 is complete. SceneCommand no longer contains Quit, and the scene
dispatcher no longer receives a window or has a close branch. The normal loop
closes only through the existing SFML Closed event, with the existing score and
settings checkpoints before window.close(). No persistence/session/path logic
was changed. Native macOS Cmd+Q and the window X button both exercise this path
and return cleanly.

| Scene | Escape | Q | Keyboard controls |
| --- | --- | --- | --- |
| Splash | Clear control focus, stay on player entry | Ordinary character in focused name input; neutral elsewhere | Tab/Shift+Tab; Enter retains validation/start behavior; Space activates focused Start/Scores and remains text in the name field |
| Leaderboard | Return to Splash | Retain the existing Back shortcut | Tab/Shift+Tab focuses Back; focused Enter/Space and Backspace return to Splash |
| Playing | Return to Splash | Neutral | Existing direction/N/unfocused Enter preserved; focused Enter/Space activate controls on release |
| Game Over | Return to Splash | Neutral | Tab/Shift+Tab wraps Replay/Main Menu; focused Enter/Space activates on release; N/unfocused Enter replay |

Alt/Control/System-modified key presses do not invoke scene shortcuts or move
the board. Shift remains available for reverse focus traversal and normal name
typing. Game Over uses the existing shared focus border/hover colors; layout,
modal rendering, labels and palette stay as in 6.1. Changing its target, clicking
the background, losing focus or resetting input cancels pending activation.

Each scene exposes a small resetInputState operation. App clears all scene
focus/pending input on navigation, new round, automatic Playing → Game Over
and window focus loss, preserving name input/validation, sound preference and
game state. Playing input reset does not discard animation; its existing visual
reset remains responsible for restart/menu effects.

The small UI-only SceneInputGuard tracks ordinary pressed keys. At scene/new
round boundaries, their repeats and releases are consumed until release; text
from held transition keys is suppressed too. Fresh different keys remain
usable, and modifiers do not block name typing. Background keyboard/text/mouse
button events are ignored. On focus return, physical state reconciles releases
of already blocked keys that happened outside the app. An initial approach
also scanned every physically down key on focus gain; native QA exposed that
this could swallow the first new press during activation. That scan was removed,
the final guard only reconciles known blocked keys, and the native tour was
repeated successfully. Global key-repeat policy and text editing are unchanged.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five platform
acceptance scenarios and formatting passed. The reused scene fixture produced
79 PNGs. Added checks cover safe Escape/Q, modifiers, Space in forms/Back,
Game Over focus order/wrap/rings, repeated press/single release, cancellation
and normal appearance after reset. Sixty-four prior images are byte-identical
to 6.1, including every static scene/focus view; eight live animation samples
differ and pass their interval/
geometry checks. The ignored integration fixture covers held/repeated keys,
text leakage, mouse/automatic transitions, background input, focus regain,
OS-close availability and actual Game Over replay through App's dispatcher:
repeated Enter cannot restart the new round, but a fresh press after release
can. Existing checkpoint/reopen/dedup/unfinished-round checks also pass.

Native Computer Use QA used two isolated fixtures. The seeded Game Over fixture
contains the actual scenes, dispatcher and input guard. Q kept Game Over open;
the first Shift+Tab visibly selected Main Menu; Space returned to Splash, where
Escape/Q kept the app open. In a separate seeded round, a held Enter produced
exactly one RestartGame in the dispatcher trace, preserved Çağrı/best 2108 and
reset score to 0. Playing Escape returned to Splash. Both OS-close actions
returned fresh exit code 0.

The second wrapper calls actual app::run with an isolated profile and no test
hooks. Escape/Q did not close Splash; Shift+Tab/Space opened Scores, Tab/Space
returned via Back, and Gökçe plus focused Start/Space entered Playing. Up merged
to score/best 4; focused sound/Space saved false. Cmd+Q returned fresh exit code
0. Relaunch/player entry restored best 4 and Ses Kapalı. One action was rejected
for user activity before re-observation; it was retried successfully. Final X
close returned fresh exit code 0. Both QA processes were confirmed gone.
Eighteen native PNGs include fourteen final-tour views and four earlier focus-
reconciliation observations; action/close/profile records preserve the results.

Evidence is under `build/ui-navigation-step-6.2/`: entry hashes/snapshots,
build/test/format/scene/integration logs, ignored fixtures and isolated QA
bundles/profiles, screenshots, source/behavior/image preservation checks,
scoped diff and summary.json. Changes are limited to the UI scene/input files,
App's UI dispatch coordination and this document. Core, persistence/session,
paths/migration/headless, assets, theme/primitives and build/test infrastructure
retain their entry content. No branch, commit, push or Desktop bundle replacement
occurred. Hosted Windows/Linux CI and Windows Alt+F4 were not run. Full eight-
transition acceptance and roadmap step 6 closure remain substep 6.3.

## Roadmap step 6.3 — navigation acceptance and step 6 closure

Substep 6.3 and roadmap step 6 are complete. This increment required no
application-code fix: the 6.1/6.2 implementation passed the acceptance tour.
Only this document changes outside ignored evidence. All production source,
assets and build/test infrastructure retain their entry hashes.

| Transition | Integration verification | Native macOS verification | Result |
| --- | --- | --- | --- |
| Splash → Playing | Validated Turkish name, checkpoint before player metadata replacement, distinct round ID, score 0, open window | Çağrı entry/Return in actual app::run | Passed |
| Splash → Leaderboard | Actual Scores command and App dispatcher; session retained | Scores pointer click in actual app::run | Passed |
| Leaderboard → Splash | Actual Back command and App dispatcher; session retained | Tab/Return Back control in actual app::run | Passed |
| Playing → New Game | Actual button command, same player/new ID/score 0; unfinished round stays out of history | Focused Tab/Return New Game; score 4 → 0, Çağrı/best 4/sound off retained | Passed |
| Playing → Splash | Actual button command; checkpoint, retained round/board/score, open window | Shift+Tab/Return Main Menu; Çağrı input retained | Passed |
| Game Over → New Game | Completed checkpoint before reset, same player/new ID/score 0, no duplicate completion | Normal final move, then real Replay pointer click; score 2108 → 0, Çağrı/best 2108 retained | Passed |
| Game Over → Splash | Original completed identity/score saved once, scene change without reset or close | Normal final move, then real Main Menu pointer click; round/score retained, window open | Passed |
| Window Close → App Close | Existing Closed/checkpoint path unchanged; input guard preserves close availability | Actual app::run X close and Cmd+Q; fresh exit 0 and process gone | Passed |

The reused ignored integration fixture records seven scene-command transitions
in command-matrix.json. It invokes the actual scene handlers and actual App
dispatcher, with public GameSession/ScoreCoordinator APIs, and checks disk
reopen, deduplication, unfinished-round handling, held-key/text boundaries,
focus loss/regain and no phantom held key. OS close is accepted through the
native app::run wrapper rather than counting the fixture's cleanup close as
an OS interaction.

Native Computer Use QA used isolated full and seeded profiles. The full wrapper
calls actual app::run without test hooks. After the full navigation tour, X
closed cleanly. Relaunch with İnci restored best 4 and Ses Kapalı, and Cmd+Q
closed cleanly; completed history remained empty for those unfinished rounds.
Two New Game coordinate-click attempts returned tool error -10005/
noWindowsAvailable, including an alternate point after raising/re-observing
the window. Those attempts remain explicitly inconclusive. Current native
keyboard activation and scene-mapped button checks passed; this closure does
not claim those two pointer attempts succeeded or require a production fix
for the tool's routing limitation. Earlier pointer/manual acceptance remains
recorded in the prior substeps.

For Game Over, an ignored fixture replays a public seeded session to one move
before completion, using the actual scenes, dispatcher and input guard. It
does not add production hooks or claim to replace the full bootstrap. A normal
Left input changed score 2104 → 2108, checkpointed the completed round and
reached Game Over after animation settled. Replay then created a new round.
A separate launch repeated the natural completion and Main Menu returned to
Splash. The isolated profile contains exactly two distinct completed IDs,
both with Çağrı/2108. Navigation and later close checkpoints did not duplicate
them. Both seeded OS closes returned fresh exit 0. All four native close tours
passed; both QA processes were confirmed gone. Eighteen native PNGs and the
dispatcher/close/profile records preserve successful and inconclusive checks.

macOS arm64 Release build, 249 unit cases (281,933 assertions), all five platform
acceptance scenarios, formatting, integration and scene smoke passed. The
reused scene fixture produced 79 PNGs; 68 are byte-identical to 6.2, including
every static scene/focus view. Eleven real-time animation samples differ and
pass their measured interval/geometry checks. No new UI framework, dependency,
CMake target or production test hook was added.

Evidence is under `build/ui-navigation-step-6.3/`: entry hashes/document snapshot,
build/test/format/scene/integration logs, ignored fixtures and isolated QA
bundles/profiles, command matrix, screenshots, image/source preservation,
native acceptance, scoped document diff and summary.json. No branch, commit,
push or Desktop bundle replacement occurred. Hosted Windows/Linux CI and
Windows Alt+F4 were not run; roadmap step 12 retains the broader cross-platform
UI acceptance. Win/Continue and the Game Over visual redesign remain roadmap
steps 7 and 8. Step 7 has not started.

## Roadmap step 7.1 — first-win presentation at the animation boundary

Substep 7.1 is complete. The UI-only GameplayOutcome controller reads the existing
Game win/terminal flags and the GameSession round ID. It presents SceneId::Win
only from Playing, after the complete slide/merge/spawn animation has settled.
The presentation latch belongs to the round ID; later frames, focus changes,
future continued moves and 4096 do not present that win again. A different round
ID clears the latch, including rounds created by the existing Start/Restart
dispatcher. No win flag, board, gameplay RNG, score or session identity is changed
by presentation.

Win is checked before Game Over. If the same winning move also ends the round,
Win stays visible; a later return to Playing will then select Game Over. Existing
score checkpoints still record logical completion immediately, independently of
the visual transition. Ordinary unwon Game Over continues to wait for animation.

App resolves settled outcomes before dispatching each queued event and again
after the Playing handler advances its animation clock, before applying that
event's action. This protects the exact boundary from an extra board move or
scene command. Automatic transitions use the existing scene input guard and
input reset. The Win scene receives no board or side-panel actions. OS Closed
continues through the existing score/settings checkpoint and close path.

WinScene supplies only a shared dim overlay, existing modal panel and the basic
"2048! / Kazandın" heading with font fallback. This is the visible state needed
for 7.1 acceptance, not the final modal layout. Trophy/score/button layout remains
7.2, Continue/New Game/Escape/keyboard behavior remains 7.3, and full acceptance
remains 7.4. At this intermediate stage, Win has no dismissal control; OS close
is available. No new UI framework, dependency or CMake target was introduced.

Six permanent SFML-independent unit cases cover animation deferral, once-per-round
presentation, new-round reset, simultaneous win/Game Over priority, inactive scene
isolation and preservation of board/score/flags/RNG. The existing unit target
includes these cases without acquiring a graphics dependency. Native macOS arm64
Release build, all 255 unit cases (282,023 assertions), all five platform acceptance
scenarios, formatting and whitespace checks passed.

The ignored integration fixture uses production scenes, the actual App command
dispatcher, public GameSession/ScoreCoordinator APIs and the existing seed-0
legal winning sequence. It verifies a real animated win at 21196, no completed
history entry merely for winning, held-key/focus isolation, unchanged identity on
a simulated future Continue route, real Restart semantics, a second round's win,
and Win before Game Over after a real terminal move animation. The Continue route
is simulated for the presentation contract; no Continue control exists yet.

Native Computer Use QA runs a fixture copied from the current App loop/bootstrap
with isolated profile/assets/legacy paths, a seed-0 legal replay stopping one move
before 2048, and trace recording in ignored source only. Production has no test
hooks. Holding Left created 2048 and score 19144 -> 21196; the single Win transition
occurred 239 ms after onMove, with hasActiveAnimations false. Further direction,
N/Escape/Tab/Enter inputs, a real pointer click on the dimmed sound control and
minimize/focus return left the same round/player/score/sound/history intact.
Cmd+Q returned fresh exit 0. Relaunch retained best 21196, created a distinct
fixture round, reached Win once again and native X returned fresh exit 0. Both
close traces and exit files confirm completion; the QA process was confirmed gone.
The isolated score profile retains best 21196 and empty completed history.

The reused layout fixture passed and produced 79 PNGs: 70 are byte-identical to
6.3; nine real-time animation samples differ and pass their existing measured
interval/geometry checks. Every static scene view is preserved. Two new offscreen
Win views and six native screenshots were visually
inspected without clipping or drawing outside the shared content view.

Evidence is under `build/ui-win-step-7.1/`: entry hashes/snapshots, build/test/format
logs, ignored seeded/integration/layout fixtures, isolated QA bundle/profiles,
native traces/exit files/screenshots, image comparison, scoped diff and summary.
Changes are limited to App's presentation routing, SceneId, the new UI controller
and basic WinScene, six unit cases, their existing CMake source lists and this
document. Core, session, persistence, settings, paths, assets and all preexisting
scenes/input/theme/primitives retain their entry hashes. No branch, commit, push
or Desktop bundle replacement occurred. Hosted Windows/Linux CI was not run.

## Roadmap step 7.2 — win modal appearance

Substep 7.2 is complete. WinScene now uses a dedicated 390×400 logical-pixel modal
size token, preserving the existing Game Over size. The card is centered over the
dimmed board and side panel, within the existing 32 px content margins. Shared
panel/shadow/button/text primitives and the existing gold vector trophy provide
the appearance; no bitmap, font, dependency, framework or build target was added.

The vertical layout contains a centered trophy, separate "2048!" and localized
"Kazandın" headings, the actual round's "Skor: <value>", then full-width stacked
"Devam Et" and "Yeni Oyun" buttons. Buttons are 326×56 with 32 px horizontal and
bottom padding and a 12 px gap. The primary/secondary theme variants distinguish
the two choices. Each label is centered using its actual glyph bounds. Score text
uses the shared width-fitting helper, including recalculation when a later render
contains a smaller score. The subtitle keeps the existing ASCII glyph fallback.

App passes the current game score to the modal and updates only the WinScene hover
while Win is active. Pointer hover changes the corresponding modal button's theme
color without changing geometry. Board and side-panel input stays blocked by the
7.1 scene routing. This increment supplies appearance and hover feedback only:
clicks do not dismiss/restart, and Tab/Enter/Space/Escape actions remain 7.3. The
presentation controller, scene commands, round/score/settings/persistence and
existing input guard retain their 7.1 implementation. Full step 7 closure is 7.4.

Native macOS arm64 Release build, all 255 unit cases (282,023 assertions), all five
platform acceptance scenarios, formatting and whitespace checks passed. The
ignored raster fixture produced nine modal PNGs and checks actual rendered ink
centers, trophy/title/subtitle/score order, button geometry/label clearance,
nonoverlap, score 0/21196/99999999/2147483647, subsequent small-score rendering,
both hover palettes, hover reset and absence of drawing in the outer margins.
Normal and maximum-score images were visually inspected. The reused 7.1 integration
fixture passed real animation, once-per-round/reset and terminal win priority.
The prior 79-view layout fixture also passed; its image comparison records exact
static-view preservation and any differences in measured real-time animation
samples. No permanent test or test target was added for this appearance-only step.

Native Computer Use QA used the current App loop/bootstrap copied to an ignored
seeded fixture with separate profile/assets/legacy paths and trace recording only
in that copy. A normal Left move created the first 2048 and opened the redesigned
modal at score 21196 after animation. Actual pointer clicks on each modal button
showed its hover appearance while retaining the same round. A click on the dimmed
sound row and Right/N/Escape/Tab/Enter left the board/player/round/score/sound and
unfinished history intact. Four native PNGs were inspected; native X close returned
fresh exit 0, and the isolated profile retained best 21196 and empty completed
history. Production contains no seeded entry, tracing or test hook.

Evidence is under `build/ui-win-step-7.2/`: entry hashes/snapshots, build/test/format
logs, ignored modal/integration/layout/seeded fixtures, isolated QA bundle/profile,
modal/native screenshots, native trace/exit, image comparison, scoped diff and
summary. Only WinScene.cpp/.hpp, the additive theme size token, App's Win score/hover
render wiring/comment and this document change. All other preexisting files retain
their entry hashes. No branch, commit, push or Desktop bundle replacement occurred.
Hosted Windows/Linux UI acceptance was not run. Substeps 7.3/7.4 have not started.

## Roadmap step 7.3 — Continue, New Game and modal keyboard input

Substep 7.3 is complete. WinScene now emits a ContinueGame or existing RestartGame
command. The new ContinueGame dispatcher branch applies only from Win and changes
the scene to Playing; it never resets or mutates GameSession. Continue has no
score/settings checkpoint or sound action. The board, score, round ID, player,
win flag and gameplay RNG therefore remain intact. The existing presentation
latch prevents another Win in the same round. If that round is already terminal,
the next settled outcome check opens Game Over.

New Game uses the existing checkpoint-before-reset lifecycle and RestartGame
dispatcher. It creates a new round ID and fresh board, clears the round score and
visual effects, retains the player/best/sound preference, and lets the presentation
controller reset its latch from the new round ID. Winning alone does not create a
completed-history entry; a continued round later reaching logical Game Over is
still recorded once under its original ID.

| Modal input | Behavior |
| --- | --- |
| Primary pointer release on Devam Et / Yeni Oyun | ContinueGame / RestartGame |
| First Tab / Shift+Tab | Focus Continue / New Game |
| Further Tab / Shift+Tab | Wrap between the two controls |
| Focused Enter / Space | Activate once on release; repeats before release do not activate |
| Unfocused Enter / Space, directions, N or Q | Neutral |
| Escape | Continue regardless of control focus |
| Alt/Control/System-modified key press | Neutral |
| Background primary click, focus change, reset or focus loss | Cancel pending activation; background click/blur/reset clear focus |

Keyboard focus uses the existing shared outline and hover palette. The Win scene's
resetInputState participates in App's existing all-scene reset at focus/navigation
boundaries. The unchanged SceneInputGuard consumes held transition keys until
release, so Escape cannot continue and then navigate Playing to Splash; a held
direction cannot move the board when Continue dismisses the modal. New distinct
keys and fresh presses after release still work. OS Closed continues through the
existing final checkpoints and close path. No modal geometry, theme token, gameplay,
session/storage/settings code, dependency or build/test target changed.

Native macOS arm64 Release build, all 255 unit cases (282,023 assertions), all five
platform acceptance scenarios, formatting and whitespace checks passed. The ignored
control fixture performs 113 checks with the actual Win handler/App dispatcher,
public GameSession/ScoreCoordinator/SettingsStore APIs and isolated profiles. It
covers mapped pointer commands, focus order/wrap/rings, both activation keys and
repeats/releases, neutral/modifier input, background/right clicks, pending/blur
cancellation, Escape, and held-key boundaries. A legal seed-0 winning replay verifies
exact round/grid/score/player/preference preservation and byte-identical profiles
across Continue. A copied public Game confirms identical subsequent spawned boards
and scores, proving RNG preservation. Actual Restart retains player/best/sound and
creates a distinct unfinished round. A continued natural winning session reaches
Game Over, stores exactly one completion for its original ID and remains deduplicated
after a later restart. A public terminal winning fixture verifies Win -> actual
Continue command -> Game Over without a second Win.

Four focus PNGs were inspected. All nine existing modal renders remain byte-identical
to 7.2. The 79-view prior layout fixture passed: 68 PNGs are identical; eleven real-time
animation samples differ and pass their measured interval/geometry checks. Every
static scene view is preserved. The unchanged six presentation unit cases retain
the broader 4096/round reset and state-preservation checks.

Native Computer Use QA used the current App loop/bootstrap copied to an ignored
seeded fixture, with only legal replay setup and trace recording added in that
copy. Profiles/assets/legacy paths are isolated. The first tour turned sound off,
won at 21196, focused Continue with Tab and held Enter for 650 ms. Exactly one
Continue preserved the recorded board/ID/score/player/sound/history; a fresh Right
then moved that same round to score/best 21208 without another Win or restart.
Cmd+Q returned fresh exit 0. A second tour reopened with sound off/best 21208,
won, then a real New Game pointer click created exactly one new round at score 0
with Çağrı/best/sound preserved; X returned fresh exit 0. A third tour verified
the real Continue pointer click and exact board/round preservation, then Cmd+Q.
A fourth held Escape for 850 ms: exactly one Continue left Playing open, with no
Splash/restart/extra move; X returned fresh exit 0. Eleven native PNGs and four
separate trace/exit pairs preserve these tours. The QA process was confirmed gone;
the isolated profiles contain best 21208, sound off and empty unfinished history.

Two initial native Continue pointer attempts returned tool error -10005/
noWindowsAvailable. They remain inconclusive. Selecting the same app through its
bundle ID on later launches allowed both real modal pointer actions to succeed;
no application fix was needed for that automation-routing issue. Production
contains no seeded entry, tracing or acceptance hook. Full cross-platform and
combined step-7 acceptance remain 7.4; this does not close roadmap step 7.

Evidence is under `build/ui-win-step-7.3/`: entry hashes/snapshots, build/test/format
logs, ignored control/modal/layout/seeded fixtures, isolated profiles/QA bundle,
control and native screenshots, behavior/native/image reports, scoped diff and
summary. Only WinScene.cpp/.hpp, SceneCommand, App's UI command/input/reset routing
and this document change; all other preexisting files retain their entry hashes.
No branch, commit, push or Desktop bundle replacement occurred. Hosted Windows/Linux
UI acceptance was not run. Substep 7.4 has not started.

## Roadmap step 7.4 — combined acceptance and step 7 closure

Substep 7.4 and roadmap step 7 are complete. The existing 7.1–7.3 implementation
passed the combined acceptance without an application-code fix. Only this document
changes outside ignored evidence; every production source, asset and build/test
file retains its entry hash.

| Acceptance flow | Verification | Result |
| --- | --- | --- |
| First 2048 -> Win | Real slide/merge/spawn settling in the combined fixture and native seeded App traces; no early modal | Passed |
| Continue -> same round and later move | Actual Win handler/dispatcher, exact board/score/player/ID/preferences and byte-identical profiles, identical future RNG/spawn sequence; native continued session | Passed |
| 4096 and 8192 -> no second Win | Legal successive merges on one controlled board/round, real animation and unchanged presentation latch | Passed |
| Win -> New Game | Actual pointer/focused commands and dispatcher, fresh ID/score 0 with player/best/sound retained; native Shift+Tab/held Space after focus return | Passed |
| Win -> Continue -> Game Over | Natural continued-session storage acceptance and native continued round at 22404; one original ID and exactly one completed entry | Passed |
| Keyboard, focus loss/return and held keys | Current control fixture, fresh native minimize/return/focus/held Space and retained 7.3 held Enter/Escape/pointer evidence | Passed |
| X / Cmd+Q directly from Win | Two separate fresh native tours close while scene is Win, return exit 0 and preserve unfinished best without false completion | Passed |
| Large score layout | Current modal raster acceptance for 0/21196/99999999/2147483647, subsequent smaller score and no overlap/margin leakage; maximum-score visual inspection | Passed |

Native macOS arm64 Release build, all 255 unit cases (282,023 assertions), all five
native platform acceptance scenarios, formatting and whitespace checks passed.
The reused control fixture passed 113 input/dispatcher/state/profile/RNG checks.
The combined fixture additionally passed 188 checks, including frame polling
assertions; that number is not a count of distinct scenarios. No new permanent
test, graphics dependency or build target was introduced.

The combined fixture uses production Playing/Win/Game Over scenes, the actual
App command dispatcher and public Game APIs. For the higher-tile acceptance,
two rows of 1024 tiles are loaded once into a controlled public Game. Successive
legal Left/Left/Up moves produce 2048/4096/8192 on that same board without replacing
state; spawning is explicitly disabled for this repeatable ladder. Actual animation
and Continue command routing establish one Win followed by playable 4096/8192.
This is a controlled board fixture, not a claim that the native seed-0 game naturally
reached 4096. A separate public final-spawn fixture verifies animated simultaneous
Win/Game Over, then the actual Continue command leading to Game Over. A distinct
round ID permits a new Win. Seven combined PNGs cover the initial win, continued
2048/4096/8192, next-round win and terminal Win/Game Over.

All nine modal images are byte-identical to 7.3. The reused 79-view layout fixture
passed: 69 images are identical; ten real-time animation samples differ and pass
their measured interval/geometry checks. Every static scene view is preserved.
Visual inspection covered higher-tile play, terminal Game Over and maximum-score
modal layout, plus native focus/round/close views.

Four native tours use an ignored copy of the current App loop/bootstrap with legal
seed-0 replay setup, isolated per-tour profile/assets/legacy paths and trace recording.
Only the continued-Game-Over mode adds legal replay staging after Continue: it
advances that same public session to one move before a predicted terminal position,
with ordinary checkpoints but without intermediate animation. Subsequent observed
native moves complete the round. Neither staging nor tracing exists in production.

The continued-round tour records first Win at 21196 after animation, Continue with
the exact same board/ID/player/score, and normal native moves ending at 22404. Game
Over waits for the last animation and does not reopen Win. The isolated profile
contains one Çağrı/22404 entry with the original round ID; later X close retains
exactly that one entry and returns exit 0. The staging predicted Right, while the
observed native moves followed a different legal route; acceptance uses the actual
recorded 22404 completion and does not claim the predicted final input was used.

The focus/New Game tour turns sound off before winning. Tab visibly focuses Continue;
minimize produces LostFocus and clears input, and Tab after return focuses Continue
again. Shift+Tab selects New Game. Holding Space for 800 ms creates exactly one fresh
round at score 0, retaining Çağrı/best 21196/sound off and empty completed history.
X close returns exit 0. Two further tours close directly from the visible Win modal,
one with X and one with Cmd+Q. Each returns fresh exit 0, records scene Win at close,
preserves best 21196 and leaves completed history empty. Nine native PNGs and four
separate trace/exit pairs preserve the observed results. The QA process was confirmed
gone. Current native acceptance produced no automation error or production fix.

Evidence is under `build/ui-win-step-7.4/`: entry hashes/document snapshot, build/unit/
platform/format logs, ignored combined/control/modal/layout/seeded fixtures, separate
native profiles and QA bundle, screenshots, native/acceptance/image reports, scoped
document diff and summary. No branch, commit, push or Desktop bundle replacement
occurred. Hosted Windows/Linux UI runs and Windows Alt+F4 were not performed; roadmap
step 12 retains that broader platform tour. Step 7's native macOS closure is recorded
here. Step 8 remains unstarted.

## Roadmap step 8.1 — Game Over card and heading layout

Substep 8.1 is complete. Game Over has its own additive kGameOverModal size token,
390×320 logical pixels, replacing that scene's previous 390×270 card. The shared
legacy kModal and the 390×400 Win modal retain their sizes. The card remains centered
within the 856×596 content view, over the existing dimmed board/side panel and within
the 32 px outer window margins. Existing Modal primitives supply the same cream
surface, semantic border, rounded corners and shadow used by Win; no new asset,
font, primitive, framework, dependency or build target was introduced.

"Oyun Bitti" now uses the same 46 px bold heading tier as Win's main heading,
with a 32 px top line-box inset and glyph-based horizontal centering. The existing
Son Skor/En İyi text block is vertically centered in the space between the heading
and action row, with 24 px clearance reserved at both boundaries. Its existing
12 px line gap, labels, font sizes and emphasis remain intact. The horizontal
action row retains its size, variants and gap and follows the taller card's bottom
anchor. This increment adjusts placement only: score-field visual design remains
8.2, action/icon design remains 8.3, and combined step 8 closure remains 8.4.

GameOverScene event/focus/reset/hover methods are byte-identical to their entry
content. GameOverScene.hpp, App's dispatcher/animation/checkpoint routing, every
other scene, Win presentation/input, gameplay/session/storage/settings, assets
and all build/test source retain their entry hashes. The only production changes
are the dedicated additive size token and GameOverScene's card/header/content
layout. The existing Replay/Menu commands and pointer/keyboard semantics remain
unchanged; repositioned pointer hit areas follow the rendered shapes.

Native macOS arm64 Release build, all 255 unit cases (282,023 assertions), all five
platform acceptance scenarios, formatting and whitespace checks passed. The ignored
Game Over raster fixture produced nine PNGs. Checks cover actual centered panel/
heading/text ink, header inset, ordered nonoverlapping text, balanced clearances,
unchanged equal horizontal button sizes, mapped pointer commands, hover/focus/reset
and outer-margin preservation. Existing labels render without clipping at 0, 2108,
99999999 and 2147483647. Normal and maximum-score views were visually inspected.

The first raster heading assertion compared fully opaque ink with SFML's full
glyph bounds too strictly: measured ink is 212×44 and reported bounds are 215×46.
The ignored fixture now allows up to four horizontal and three vertical pixels
for edge coverage, retaining a meaningful 46 px heading-size check. This was a
fixture-metric correction, not an application fix; the diagnostic log is retained.
No new permanent test or test target was added for this layout increment.

The reused 79-view layout fixture passed: 58 PNGs are byte-identical to 7.4, ten
Game Over views change as intended, and eleven real-time animation samples differ
while passing measured interval/geometry checks. All static views outside Game
Over are preserved. All nine Win modal renders are byte-identical to 7.4.

Native Computer Use QA uses an ignored copy of the current App loop/bootstrap,
with public seed-9 replay stopping one move before Game Over and isolated
profile/assets/legacy paths. A normal Left merges the final score 2104 -> 2108,
checkpoints one completed round and shows the new Game Over after animation.
The first tour's real Replay pointer click creates one new round at score 0,
retaining Çağrı/best 2108 and exactly one completed entry; X close returns fresh
exit 0. A separate tour reaches the same completed score, then the real Main Menu
pointer click opens Splash without replacing the completed round ID/score or
duplicating history; Cmd+Q returns fresh exit 0. Four native PNGs and two trace/
exit pairs preserve these checks. Each isolated profile contains one completion
and best 2108. The QA process was confirmed gone. Production has no seeded entry,
tracing or test hook.

Evidence is under `build/ui-gameover-step-8.1/`: entry hashes/snapshots, build/unit/
platform/format logs, ignored raster/layout/Win/native fixtures, profiles/QA bundle,
screenshots, the ink diagnostic, native/image/source reports, scoped diff and summary.
Only GameOverScene.cpp, the additive UiTheme size token and this document change.
No branch, commit, push or Desktop bundle replacement occurred. Hosted Windows/Linux
UI acceptance was not run. Substeps 8.2–8.4 have not started; step 8 remains open.

## Roadmap step 8.2 — Score cards and full-window Game Over dimming

Substep 8.2 is complete. Son Skor and En İyi now use two equal 155×88 logical-pixel
score cards, with 16 px between them and 32 px horizontal modal insets. The shared
Score panel helper supplies the cream surface, border, 20 px secondary-color label
and 26 px bold primary-color value. Values fit within 12 px inner insets, including
2147483647, without clipping. The pair is centered vertically between the heading
and existing actions. The 390×320 modal, 46 px heading and horizontal buttons retain
their 8.1 layout. A dedicated additive height token controls the score cards; the
old private score text fields were removed. No shared drawing helper changed.

The user's additional request is included: Game Over's translucent black backdrop
now covers the entire client surface, including the previously undimmed 32 px outer
margins. Rendering temporarily uses the target's full default view for one dim pass
and restores the exact caller view before drawing the modal. This preserves card
placement and input coordinate mapping. The existing black alpha of 150 is retained.
The operating system title bar is outside the client surface. Other scenes, including
Win, retain their backdrop behavior.

The native macOS arm64 Release build, 255 unit cases / 282,023 assertions, five local
platform acceptance scenarios, formatting and whitespace checks passed. Nine ignored
score-layout PNGs cover zero, 2108, eight-digit and maximum values, independently
different score/best values, restoration after maximum values, and 1840×1320 Retina
rendering alongside 920×660 rendering. Raster checks verify every outer-margin pixel
is dimmed, a single dim pass inside the content area, exact view restoration, aligned
cards, text clearance and existing mapped Replay/Menu commands. Normal and maximum
score renders were visually inspected.

The initial caption-centering fixture scanned only exactly opaque text-color pixels;
at 20 px some antialiased letters contributed no such pixels, producing a false
15 px offset. The ignored fixture now includes pixels within 30 RGB levels of the
caption color inside the card's text area. Measured caption centers are within 1 px
of their card centers at normal and Retina scale. The diagnostic log is retained;
this correction changed no production code. No permanent test target was added.

The reused 79-view fixture passed: 58 images are byte-identical to 8.1; ten Game Over
images change intentionally and eleven timed animation samples differ while passing
their interval and geometry checks. All other static views are preserved. All nine
Win modal renders are byte-identical to 8.1.

Native Computer Use QA ran two isolated seed-9 tours using an ignored copy of the
current App loop with tracing and profile-path overrides. A normal Left completes
the round at 2108 and shows the new Game Over. Actual Replay and Main Menu pointer
clicks respectively start one new round at zero or return to Splash with the completed
round unchanged. Both profiles retain exactly one completion, best 2108 and player
Çağrı. Four native images and trace/exit pairs preserve the checks; X close and Cmd+Q
each exit with zero. Production App has no new seed, trace or test hook.

Evidence is under `build/ui-gameover-step-8.2/`, including entry snapshots, build/test/
format logs, raster and native fixtures, screenshots, diagnostic and acceptance
reports, source preservation, scoped diff and summary. Only GameOverScene.cpp/.hpp,
the additive UiTheme height token and this document change relative to entry.
GameOverScene input/hover/focus/layout code, App, shared primitives, Win, gameplay,
storage, settings, assets and build/test sources retain their entry content.
No branch, commit, push or Desktop bundle replacement occurred. Windows/Linux UI
acceptance was not run. Substeps 8.3/8.4 have not started; step 8 remains open.

## Roadmap step 8.3 — Stacked Game Over actions

Substep 8.3 is complete. Tekrar Oyna and Ana Menü now form a centered vertical pair:
equal 326×56 logical-pixel buttons, 12 px apart, with 32 px side and bottom insets.
Replay uses the shared primary palette and Restart vector icon; Main Menu uses the
secondary palette and Home vector icon. The existing icon-button helper centers
each full icon/label group with 20 px icons and 8 px text gaps. The complete 22 px
labels fit without scaling. Shared hover colors, focus borders, rounded corners and
shadows remain in use; there is no new asset, font, dependency or component.

The dedicated Game Over modal grows from 390×320 to 390×400 to accommodate the
stacked actions, matching Win's modal height. Its 46 px heading retains the 32 px
top inset. The two 155×88 score cards retain their width, height, 16 px gap and
typography, and remain balanced between heading and actions. The full-client dim
pass and exact caller-view restoration introduced in 8.2 remain byte-identical.

Only GameOverScene's drawing/layout and its existing modal size token change in
production. Input, hover, focus/reset and activation methods are byte-identical to
8.2. Existing shape-based hit tests follow the new positions. Tab/Shift+Tab,
focused Enter/Space on release, N/unfocused Enter replay and Escape to Main Menu
retain their behavior. Q and playing directions remain neutral while Game Over is
open. GameOverScene.hpp, App, shared primitives, Win, animation, gameplay, storage,
settings, assets and build/test sources retain their entry hashes.

## Roadmap step 8.4 — Combined Game Over acceptance and closure

Substep 8.4 and roadmap step 8 are complete. Native macOS arm64 Release build,
255 unit cases / 282,023 assertions, all five local platform acceptance scenarios,
formatting and whitespace checks passed. The final visual/input fixtures and four
isolated native application tours passed the following acceptance matrix.

| Area | Evidence and result |
| --- | --- |
| Final card/header/actions | Centered 390×400 modal, complete 46 px heading, aligned score cards, equal 326×56 stacked buttons, centered Restart/Home label groups, clear insets and gaps; passed |
| Scores | Independent zero/max values, 2108, eight-digit and 2147483647 scores, reset after large values, no clipping or label/value overlap; passed |
| Full-window backdrop | Every outer-margin pixel dimmed exactly once at 920×660 and 1840×1320, caller view restored exactly, normal margins after dismissal; passed |
| Pointer input | Actual wide Replay/Menu clicks start a fresh round or return to Splash; score cards, modal background and right clicks remain neutral; passed |
| Keyboard and state | Tab/Shift+Tab order/wrap, visible focus rings, focused Enter/Space single release, shortcuts, modified-key neutrality, blur/reset/target-change cancellation and hover geometry; passed |
| Logical completion | Seed-9 final move 2104 → 2108, Game Over waits for actual last animation, completion recorded once before presentation/navigation; passed |
| Replay | Actual scene command/dispatcher creates a new ID and playable zero-score board with the same player; clears prior effects, keeps best/history; passed |
| Main Menu | Actual scene command/dispatcher retains completed ID/board/score and returns to Splash without duplicating history; passed |
| Persistence/reopen | Both combined-fixture profiles and all four native profiles contain one original completion for Çağrı and best 2108; reload/JSON checks passed |
| Win compatibility | First win → Continue → 4096/8192, new-round win and animated terminal Win → Continue → Game Over retain outcome priority; passed |
| Close | X and Cmd+Q exit zero; Cmd+Q while Game Over has Replay focus preserves one completion; QA process stopped |

The score/action fixture produced twelve PNGs, including both hovers, reset and
Retina maximum-score rendering. The reused 79-view fixture passed: sixty PNGs are
byte-identical to 8.2; ten Game Over views change intentionally and nine timed
animation samples differ while passing measured interval/geometry checks. All
static views outside Game Over are preserved. All nine Win modal renders remain
byte-identical to 8.2. The combined public-model/production-dispatcher fixture
performed 690 checks and produced nine images; no permanent test target was added.

Native Computer Use QA uses an ignored copy of the current App loop/bootstrap with
seed-9 preparation, tracing and isolated profile/assets/legacy paths. Four tours
cover pointer Replay + X close, pointer Main Menu + Cmd+Q, neutral Up/Q then reverse
Tab/Space Main Menu + X, and Tab Replay focus + Cmd+Q directly from Game Over.
Seven native images and four trace/exit pairs preserve these checks. Up/Q leave
the completed board and score unchanged; each navigation/completion occurs once.
Production App has no new seed, trace or test hook. Normal, maximum-score and
native focus renders were visually inspected.

Evidence is under `build/ui-gameover-step-8.4/`: entry hashes/snapshots, build/unit/
platform/format logs, ignored score/layout/Win/combined/native fixtures, images,
isolated profiles, acceptance matrix, source/image/native reports, scoped diff and
summary. Production changes are limited to GameOverScene.cpp and its modal size
token; this document and the Unreleased changelog record the closure. Existing
work elsewhere is preserved. No branch, commit, push or Desktop bundle replacement
occurred. Hosted Windows/Linux UI acceptance remains roadmap step 12 and was not
run here. Steps 1–8 are complete; the next roadmap step has not started.

## Roadmap step 9.1 — Tile motion easing

Substep 9.1 is complete. Slide, merge-pop phase and spawn fade/scale now use a small
clamped smoothstep curve in PlayingScene's anonymous namespace. The progress curve
keeps exact 0/1 endpoints and zero endpoint velocity: sliding tiles start and arrive
gently, merge growth/shrink starts and ends gently, and spawn alpha/scale settles
gently. The curve is monotonic and adds no overshoot. This is a presentation polish
change, not a correction to gameplay or an animation-position defect.

Existing durations stay 90 ms for slide, 120 ms for merge pop and 140 ms for spawn;
merge/spawn still start after slide and overlap. Merge scale retains its 1.17 peak
and final 1.0; spawn retains 0.82 → 1.0 and alpha 0 → 255. The animation state machine,
deadline, input gate, board coordinates, hidden-cell bookkeeping and reset behavior
are unchanged. Maximum move presentation remains 230 ms with spawn, 210 ms with a
merge but no spawn, and 90 ms for a plain slide without spawn. Floating-score effects,
controls and modal rendering are unchanged and belong to later 9.x substeps.

The only production change is the local easing helper and its three uses in
PlayingScene.cpp. App, all other scenes, public headers, shared theme/primitives,
gameplay rules, score/session/storage, paths/migration, settings, assets and build/
test sources retain their entry content. No framework, dependency, permanent test
target or public animation API was added.

Native macOS arm64 Release build, 255 unit cases / 282,023 assertions, all five local
platform acceptance scenarios, formatting and whitespace checks passed. The reused
scene fixture passed 88 renders: four-direction early/middle/late/arrival samples,
hidden final/spawn tiles during slide, partial alpha/scale, centered edge merge pop
and border clearance, no-spawn/merge-only/no-op settlement, blocked movement during
animation, subsequent movement, blur/sound preservation and immediate restart/menu
reset. Early slide samples progress less than former constant-velocity travel; late
samples progress further while remaining inside the destination. Measured render
intervals stay inside their intended stages. Every settled board matches a fresh
static rendering. Long-number fitting still combines correctly with animation scale.

Of the 79 previous views, 69 remain byte-identical to step 8.4; ten timed animation
views differ intentionally or with sampling time. Nine additional frames cover early/
late movement and merge-only settlement. Every previous static scene, including Game
Over's full-client backdrop and final icon actions, is preserved. The combined
production-dispatcher fixture also passed 690 checks covering Win/Continue/higher
tiles, terminal Win → Continue → Game Over, ordinary completed rounds and Replay/
Menu checkpoint deduplication and reopen. These fixtures remain ignored local QA.

Native Computer Use QA ran the actual unmodified app::run through an ignored small
entry wrapper with isolated asset/profile/legacy paths. Player Motion QA entered
Playing at zero, a real Left merged the initial two 2 tiles into 4, subsequent
direction-key input remained stable, N restarted at zero with best 4 and the same
player, and Escape returned to the populated name form. Native X exit returned zero;
the isolated profile retains best 4 and no falsely completed round. Five screenshots
preserve the tour. The production app has no seed, trace or test hook.

An ignored preview fixture records actual production rendering of one legal move,
two edge merges and a spawn into 26 frames at 16 ms intervals; it saves PNGs after
sampling to avoid slowing the animation clock with image encoding. A short GIF shows
these frames at their approximate capture rate with a final hold. This is preview
evidence, not an application asset or dependency. Early slide, edge pop, settlement
and native application views were visually inspected.

Evidence is under `build/ui-motion-step-9.1/`: entry snapshots/hashes, build/unit/
platform/format logs, reused scene/combined fixtures, native wrapper/profile and
screenshots, preview frames/timings/GIF, source/image reports, scoped diff and summary.
Only PlayingScene.cpp, this document and the Unreleased changelog change relative
to entry. No branch, commit, push or Desktop bundle replacement occurred. Hosted
Windows/Linux UI acceptance remains step 12. Step 9 is open; 9.2–9.4 have not started.

## Roadmap step 9.2 — Shared button hover and press feedback

Substep 9.2 is complete. All nine buttons across Splash, Leaderboard, Playing, Win
and Game Over use shared ButtonFeedback presentation state. Hover smoothly reaches
the existing variant's brighter color over 100 ms. Holding left mouse or focused
Enter/Space depresses the drawing to 98% over 60 ms; release eases back over 90 ms.
Interrupted hover transitions resume from their current value. Theme motion tokens
own the durations/scale, with no new palette, dependency or framework.

The shared drawing helpers transform the button, shadow, label and vector icon
together around the logical button center. Shapes, layout and hit bounds remain
unchanged; no RenderTarget view is changed. This includes the trophy icon helper's
optional RenderStates argument. Normal/settled-hover rendering remains identical
to 9.1. Each scene owns its button feedback and routes events to it, retaining scene
ownership of focus, validation and commands. Dragging off or leaving the window
cancels a mouse press; blur and scene reset discard all feedback. Sound remains the
existing toggle row, with its existing preference behavior.

Focused Splash Start/Scores and Leaderboard Back now activate Enter/Space on release,
matching Playing/Win/Game Over and allowing visible keyboard depression. Repeated
key-down events cannot activate a button repeatedly. Focus changes, blur, explicit
reset and background clicks cancel pending activation. Name-entry/unfocused Enter
remains the existing immediate start shortcut; Escape/Q/Backspace and OS shortcuts
keep their existing scene-specific behavior. Pointer command timing and hit tests
are unchanged. App's dispatcher, input guard and checkpoint flow are unchanged.

Native macOS arm64 Release build, 255 unit cases / 282,023 assertions, all five local
platform acceptance scenarios, formatting and whitespace checks passed. A local
production-scene fixture exercises all nine controls and eighteen focused release
routes at normal and Retina density. Checks cover intermediate hover colors, final
palette/geometry, common mouse/keyboard depression, clicks inside the original edge
but outside the depressed drawing, single release activation, drag-off neutrality,
blur/reset cancellation, exact neutral appearance after reset, view restoration and
continuous hover reversal. It produced 27 normal/mouse-held/key-held PNGs.

The reused 88-view scene fixture passed. All 69 static views are byte-identical to
9.1; nineteen timed animation frames differ with sampling time while passing their
stage/position checks. The tile easing, durations, merge/spawn state and floating
score code introduced or retained in 9.1 are unchanged. The combined dispatcher/
storage fixture passed 668 checks, including Win/Continue/higher tiles, terminal Win
→ Continue → Game Over, ordinary completions, Replay/Menu and one-time history/reopen.
The check count includes time-dependent polling and is not a unit-test case count.

The legacy fixture now sends focused menu/leaderboard activation releases and waits
for hover endpoints before comparing stable pixels. Initial fixture failures were
caused by comparing an in-flight hover with a settled screenshot, requiring strict
vertical shrink in a 44 px button despite pixel rounding, and PNG encoding delaying
the next animation sample past its stage. The ignored fixtures now allow 1.5 px
height rounding and save directional PNGs after sampling. Diagnostic logs are kept.
These corrections required no production fix and no permanent test target.

Native Computer Use QA used the actual unmodified app::run in an isolated wrapper/
profile. Sequential keyboard operations passed name entry, focused Scores/Space,
focused Back/Enter, focused Start/Enter, Playing New Game/Enter, reverse-focus Main
Menu/Space and Cmd+Q exit zero. The player and zero best score remain correct; no
score file is created for rounds with no improvement or completion. Physical mouse
click attempts returned the tool's noWindowsAvailable error despite successful
window observations, including after rebinding. They are not marked passed; mapped
pointer events passed separately. A concurrent held-key/observation attempt cleared
focus and cancelled activation; it is retained as a diagnostic, not a successful
native held-state capture. Sequential keyboard retry passed. Physical pointer QA
must be retried in 9.4 or the broader UI acceptance when the tool can send clicks.

Evidence is under `build/ui-motion-step-9.2/`: entry snapshots/hashes, build/unit/
platform/format logs, button/layout/combined fixtures, 27 button state renders,
88 scene renders, native observations/isolated profile, fixture diagnostics, source/
image/native reports, scoped diff and summary. A small ignored component preview
records the shared hover/hold/release renderer to a GIF; it is not an app asset.
Production changes stay in shared UI theme/primitives and the five scene .cpp/.hpp
files; this document and the Unreleased changelog record the increment. All core,
session/score/storage, paths/migration, App, settings, assets and build/test sources
retain their entry hashes. No branch, commit, push or Desktop replacement occurred.
Hosted Windows/Linux UI runs remain step 12. Step 9 stays open; 9.3/9.4 have not started.


## Roadmap step 9.3 — Panel and modal entry

Substep 9.3 is complete. Splash's form card, Leaderboard's table/back action,
Playing's right panel, Win's overlay/card and Game Over's full-client overlay/card
reveal together over 180 ms with smoothstep easing. Headings/decorations outside
the menu/table group and the playing board remain immediately visible. No layout,
hit region, palette, game rule or input delay changed.

A shared PanelReveal owns presentation only. Scenes expose onEnter separately from
input reset. App calls it on initial Splash and actual scene changes; Playing only
starts a reveal on entry from Splash. Continue, modal dismissal, same-scene restart,
blur, focus, hover, validation and sound changes cannot restart an existing panel.
The only App changes connect these UI lifecycle calls; dispatcher decisions,
checkpoint order, session logic and outcome/input guards retain their behavior.

The timer begins with the first rendered group, after initial GPU preparation.
A lazily allocated, reusable RenderTexture composites the whole group once with
premultiplied-alpha blending. Text edges, shadow overlap and the dim behind cards
therefore share the same opacity. The layer copies the caller's content view and
restores that exact view after full-surface composition, including Retina density.
Game Over continues to cover all client margins; Win retains its existing inset dim.
The final frame uses the original direct rendering path for exact settled pixels.
Allocation failure falls back to direct drawing. No asset, dependency, framework,
permanent test target, backend or storage code was added.

The native macOS arm64 Release build, 255 unit cases / 282,023 assertions, all five
local platform scenarios, format and whitespace checks passed. An ignored fixture
passed 173 reveal checks across all five screens at normal and Retina density:
first/mid/final opacity, delayed first draw, exact settled frames, reset neutrality,
repeat entry, immediate modal/navigation input, exact view preservation, target-size
reuse and Game Over's four corners. Every intermediate RGB channel matched the
single grouped blend of first/final frames within three rounding levels, including
text, shadows and overlapping dim/card areas. Thirty scene state PNGs and nineteen
16 ms preview frames were saved after timed sampling; a short GIF shows actual
production Game Over rendering, with no added application asset.

The previous 88-view fixture passed: all 69 static PNGs are byte-identical to 9.2;
nineteen timed tile frames vary with sampling while retaining their checks. Nine
button controls/eighteen focused release routes and the combined Win/Continue/
terminal outcome/checkpoint fixture passed (689 time-dependent polling checks).
Core/session/storage sources, button feedback and tile easing/timings are unchanged.

Native Computer Use ran the actual app::run through an isolated entry wrapper.
Name Reveal QA, focused Scores/Space, Back/Enter, Start/Enter, New Game/Enter,
Main Menu/Space and Cmd+Q exit zero passed, with five final-state screenshots.
The native tour verifies scene routing and final presentation; controlled production
renders verify the short fade's intermediate frames. No physical pointer claim is
made this step: the earlier noWindowsAvailable tool limitation remains a 9.4 retry.
Mapped pointer/button checks passed. Hosted Windows/Linux UI acceptance stays step 12.

Evidence is under build/ui-motion-step-9.3/: entry hashes/copies, build/unit/platform/
format logs, reveal/button/scene/combined fixtures, images/GIF, isolated native tour,
source/image/native reports, scoped diff and summary. Production changes are the
shared UI theme/primitives, five scene pairs and UI-only App entry hooks; this
status document and Unreleased changelog record the work. No branch, commit, push
or Desktop app replacement occurred. Step 9 remains open; 9.4 has not started.


## Roadmap step 9.4 — Score feedback and step 9 closure

Substep 9.4 and roadmap step 9 are complete for local macOS acceptance. The score
card's green +delta now uses emphasis weight, cubic ease-out upward travel and a
smoothstep fade. It remains fully opaque for the first 35% of its existing 850 ms
lifetime (about 298 ms), then fades to zero. Travel remains 22 logical pixels;
duration/hold/travel live in theme::motion. Width fitting precedes visual centering
so wide deltas stay centered. Rapid scoring moves display the latest move's delta
rather than overlapping several labels; the card continues to display the full
actual total. Non-scoring moves retain live feedback without generating a new label.
The existing effect collection, expiration, reset, move/input and outcome behavior
is preserved. Floating text never gates another move or delays a modal.

Production changes for this increment are PlayingScene::renderFloatingScores,
three theme motion constants and the existing lifetime alias in UiPrimitives.hpp.
Tile easing/90–120–140 ms stages, button feedback, panel reveal, scene handlers,
App's dispatcher/entry hooks, core, session, storage, paths/migration/settings,
assets, build and permanent test sources retain their entry hashes. No dependency,
framework, branch, commit, push or Desktop bundle replacement occurred.

The native macOS arm64 Release build, 255 unit cases / 282,023 assertions, all five
local platform scenarios, formatting and whitespace checks passed. An ignored
fixture passed 176 feedback checks at normal/Retina density: actual +8/+16 merges,
held opacity, monotonic upward travel, final fade/850 ms expiry, no input/outcome
blocking, rapid successive deltas, ten-digit fitting/centering, non-scoring moves,
input reset and restart/menu cleanup. Fourteen state PNGs and sixty 16 ms preview
frames show production rendering. PNGs are encoded after sampling; a short GIF is
preview evidence only. Two initial fixture diagnostics compared independently timed
subpixel glyphs as exact pixels and disallowed their one-logical-pixel antialiased
fringe at Retina density. The final checks compare bounds/ink coverage with 3%
rounding tolerance and allow that fringe while requiring reserved-space clearance.
Those fixture corrections required no production change; failed logs are retained.

The 88-view regression passed. Sixty-eight PNGs are byte-identical to 9.3; all twenty
differences are timed tile/score-feedback views. This includes the formerly identical
edge-settled view while +8196 is still alive, whose new weight/hold/easing is intended.
All sixty-four non-animation views and four non-scoring settled-board views are
unchanged. The nine-button/eighteen-focused-release fixture passed, as did all 173
panel-reveal checks. The combined dispatcher/lifecycle/storage acceptance passed
690 time-dependent polling checks: first Win, Continue, 4096/8192 without repeated
Win, new-round eligibility, terminal Win priority, subsequent Game Over, Replay/
Menu, one completion, preserved best/history and reopen. Polling counts are not
unit-test case counts.

Native Computer Use activated all nine actual buttons with physical pointer input:
Splash Start/Scores, Leaderboard Back, Playing New Game/Main Menu, Win Continue/
New Game and Game Over Replay/Main Menu. The ordinary tour uses production app::run
with only an isolated entry wrapper. Modal tours use ignored current-App copies
with deterministic public legal move prefixes and trace/profile instrumentation;
production has no seed or trace hook, and all current presentation hooks are kept.
A real final Left reaches Win at 21,196 after tile settlement. Continue preserves
the grid/score/round, while New Game resets to zero and retains best/player. Another
real final Left reaches ordinary Game Over at 2,108; Replay retains best and exactly
one completion. Main Menu followed by Scores shows exactly one Çağrı/2,108 row.
All tours exit zero. Profile files preserve one completion after close.

The old noWindowsAvailable tool error recurred in one relaunched Game Over menu
attempt. Rebinding and raising the window did not restore pointer input. That failed
tour was closed, kept as diagnostic evidence, and retried with a fresh isolated
bundle identity. The same pointer action and subsequent score-table tour passed.
This closes the earlier 9.2/9.3 pointer acceptance item without a production fix;
it does not claim the tool itself can never recur. Native tours verify actual
routing/final scenes; controlled production renders verify short intermediate
motion. Windows/Linux hosted UI acceptance remains step 12.

Evidence is under build/ui-motion-step-9.4/: entry hashes/copies, build/unit/platform/
format logs, feedback/reveal/button/layout/combined fixtures, state PNGs/preview GIF,
native wrappers and current-App seed fixtures, isolated profiles/traces/screenshots,
retained diagnostics, source/image/native reports, scoped diff and closure summary.
Step 9.1–9.4 is closed. The next roadmap step is 10, typography consistency; it has
not been started.

## Roadmap step 10 — Whole-UI quality audit and typography

The user expanded step 10 to review every UI change from steps 1–9 for production
quality. Substep 10.1 is complete: current typography roles, common components,
views, input/outcome routing, readability and prior evidence were inventoried.
No production code or asset changed. Confirmed defects, policy/coverage gaps,
acceptance criteria and the expanded 10.2–10.6 work packages are recorded in
[UI quality audit](ui-quality-audit.md).

The six substeps retain their sequence: 10.1 inventory/contract; 10.2 shared
components/rules; 10.3 menu/table; 10.4 Playing/board/motion; 10.5 outcomes/backdrop;
10.6 repeatable combined verification/closure. Typography is included in each
relevant package. Existing hierarchy and protected core/storage/headless boundaries
remain intact. An open finding is not waived by a previous successful test run.

### 10.2 — shared components and rules

Completed on 2026-10-13. Semantic card/table/Win/score-feedback roles retain approved
nominal sizes; bounded fitting now reports actual size and missed readability floors.
Full long card values receive wider numeric reservations inside unchanged card bounds.
Source labels remain intact, and input/icon/button copies center from glyph bounds.
Primary-pointer ownership now governs both feedback and commands, with matched
press/release, permanent drag-off cancellation, blur/reset/window-leave cancellation
and scene-crossing protection. Name input and sound row follow the same contract.

Release/unit/platform/format checks passed. Current 1×/2× shared-component, input,
button, reveal, score-feedback and combined lifecycle fixtures passed. Fresh static
menu/table frames retain the 10.1 pixels; long-score glyphs enlarge inside their
existing cards. Native tours activated all nine action buttons, including actual
Continue preserving the round and Replay retaining one completion; diagnostics and
successful retries are recorded separately. Native accessibility exposure is
feasible but remains an unresolved readiness gate. Detailed policy, source scope,
measurements, evidence and open findings are in [UI quality audit](ui-quality-audit.md).
The next substep is 10.3; full step-10 production closure remains open.

### 10.3 — menu and leaderboard quality pass

Completed on 2026-10-13. New names must use visible supported glyphs and keep their
complete input readable. Unsupported/invisible or too-wide edits stay correctable
in the original buffer, with explicit errors and a readable preview; attempted
19th-character input shows the 18-character limit. Focus remains distinct while an
error is present. Legacy leaderboard names use an explained display-only projection,
with original stored names/scores/IDs untouched. The table retains its columns,
medals, capped rows and full numeric values. Menu copy is Turkish; decorative text
shares the existing board palette without changing board rendering.

The final current-source menu/table matrix passes 1,725 checks, 74 normal/Retina
state PNGs and 48 mapped pointer scenario runs. Reveal, unchanged board/modal pixel
comparison, Release/unit/platform/format checks also pass. Native field/focus/limit,
legacy-table rendering and keyboard navigation/start are observed with unchanged
isolated profile bytes. Failed coordinate input and tool Unicode entry are recorded
separately; fresh native pointer button renewal and IME remain combined-acceptance
items. Detailed policy/evidence/open gates are in [UI quality audit](ui-quality-audit.md).
The next substep is 10.4. Full step-10 production/readiness closure remains open.

## Step 10.4 — Playing, board and motion quality

Completed on 2026-10-13. Accepted Restart/Menu intent now reaches App without clearing
round effects prematurely; checkpoint/reset ownership and pre-handler settled-modal
priority are preserved. Tile 8/16/32 and held+delta contrast, full long numeric text
and explained legacy player projection meet the shared contract. Text reservations
widen within established cells/cards; names retain nominal 22 with ellipsis and
stored UTF-8 stays intact. Adjacent simultaneous merges preserve 4 px separation with
a smooth 1.10909 peak, while isolated 1.17, motion durations/easing and fixed geometry
remain. The shared menu decoration 8 receives the same intentional text-color change.

The strengthened current 1×/2× matrix passes 5,469 checks and 178 PNGs; independent
whole-board adjacent/isolated pulse references and fixed name/caption reservations
pass. App-derived routing passes 64 profiles / 32,841 checks; the entry negative
control reproduces all ten original command losses. Two native windows exit zero;
Playing's three pointer controls and current keyboard/Continue/Replay routes pass.
Actual stores reopen with preserved best/history/raw identity/settings. Four of 22
whole-UI baseline frames remain identical; other differences stay in approved
board/player/decoration regions. Build, 255 unit cases / 282,023 assertions, five
platform scenarios and formatting pass. Failed fixture assumptions and native
capture/pointer attempts remain labeled diagnostics, not successful evidence.

Six existing application UI files and four documents changed; backend/profile/
asset/build boundaries and permanent tests remain untouched. A01/A02/A13 and
Playing's A04/A10 are complete. A03 and modal acceptance remain 10.5; combined,
maintained graphical, native accessibility/IME and other-platform readiness remain
open. See [UI quality audit](ui-quality-audit.md) for scope and evidence limits.
Next: **10.5 — outcomes and backdrop**.

## Step 10.5 — Outcomes and backdrop quality

Completed on 2026-10-13. GameOver's full-client primary background gestures now cancel
pending activation in all four margins. Win retains its existing content-inset
backdrop/viewport policy. Default modal geometry, icon/font hierarchy and motion
remain. GameOver cards no longer shift when Replay gains focus; both modals retain
stable normal-border-plus-padding hit bands through focus. Full score strings,
readability floors, restoration, caller view and 180 ms reveal pass.

Current routing passes 34,583 checks over 165 reopened profiles, including the entire
10.4 preservation matrix; a clean entry-App negative control reproduces all 16 old
margin failures. The strengthened 1×/2× modal matrix passes 1,575 checks / 94 PNGs.
Clean entry modal controls reproduce the old 0.5 px card shift and 403/404 hit errors.
All 22 unfocused whole-UI baselines remain byte/pixel-identical. Nine settled contrast
pairs, four zero-restoration pairs, build/format, 255 unit cases / 282,023 assertions
and five platform scenarios pass. Four native modal pointer routes and five held-
Space backdrop sequences pass; four windows exit zero and 79 native trace/store checks
retain round/grid/best/completion/name/default settings. Diagnostics are retained.

Only three existing application UI sources and four documents change. A03/A10/A14
are closed; A15 modal application is complete. Its remaining five action buttons
and Playing Sound row need the shared follow-up in 10.6, alongside maintained
acceptance, combined coverage and native accessible operation. IME/other-platform
readiness remains separate. The user deferred **10.6 — combined acceptance and
closure**; this substep does not declare full production readiness. See the current
[UI quality audit](ui-quality-audit.md) for evidence and remaining gates.


## Step 10.6.1 — Stable shared control geometry

Completed on 2026-10-13. A15's remaining five actions and Sound now use exact normal
SFML outline bounds independently of the thicker focus drawing. The shared helper
also owns the modal rule, preserving caller padding, hover/focus presentation and
403/404 ownership. Name input, rendering/timing, App and backend remain unchanged.

Both scene variants pass 1,730 checks / 682 targeted 1×/2× scenarios. The clean entry
reproduces 96 wrong commands; current has zero. All 68 paired scene/state PNGs are
byte/pixel-identical; each render variant passes 411 checks. Release, formatting,
255 unit cases / 282,023 assertions and five platform scenarios pass. Native menu
pointer/edge and Playing keyboard/reentry passes retain best/history/name/settings;
two QA windows exit zero and actual stores reopen cleanly. Playing coordinate-pointer
renewal remains 10.6.4 after recorded Computer Use noWindowsAvailable failures.

Seven UI files and four documents change; 128 of 139 entry files remain unchanged.
A15 is closed within the targeted defect scope. A07/A09/A08 remain respectively
10.6.2/10.6.3/10.6.4; IME and other-platform readiness remain separate. No build cleanup
or full readiness claim. Detailed evidence and limits: [UI quality audit](ui-quality-audit.md).


## Step 10.6.2–10.6.4 — Repeatable acceptance and native semantics

Completed locally on 2026-10-14. The opt-in repository graphical gate uses current
SFML scenes, the production
Inter font and 1×/2× views. A fresh system Release build passes 772 checks and
102 PNGs across all five scenes and ten controls, including independently checked
focus/hover/press drawing, Name validation and Sound state. Ordinary desktop and
headless presets leave this graphical target OFF.

Scene-owned semantics add a macOS AppKit provider for editable names, required/error
information, roles/state/focus, complete score/identity values, ranking/board cells
and four board moves. Modal trees remain exclusive. A bounded queue and scene/round
checks route native requests through existing App commands. Geometry, drawing,
motion and profile formats retain their established contracts; no UI framework or
production test hook is added. Eight portable bridge cases / 456 assertions pass.
Stable read-only cell-text children let VoiceOver read all live values. The semantic
host preserves keyboard focus through the SFML responder chain; a dedicated focus
refresh outside the queue lock also handles unchanged snapshots after repeated
physical clicks, without repeated semantic announcements.

Actual VoiceOver Name/validation/Start, board values/custom move, Sound, populated
ranking/Back and both outcome actions pass. Fresh physical Playing/menu/table
commands and Enter/Space/arrow delivery pass. The final native oracle passes
233 checks over seven closed runs / 473 trace records: a 214-check six-run base tour
and 19 checks from the separate latest focus proof. The base tour has nine VoiceOver
action targets plus one board move and six reopened profiles; the latest run adds
five physical Name clicks with VoiceOver off/on. All
four final CTest entries pass (21.39 s), with 255 core cases / 282,023 assertions,
eight bridge cases / 456 assertions, five platform scenarios and the graphical
matrix. Fresh headless acceptance, formatting and 26 executed Python controls pass;
one Ninja-dependent multi-config control is explicitly skipped. Actual user files
are preserved and VoiceOver/QA processes are restored. Step 11 edge checks have not
started; step 12 other-platform/IME/shaping/hosted proof and step 15.1 cleanup remain
separate. Current scope, revision differences and evidence:
[UI quality audit](ui-quality-audit.md).
