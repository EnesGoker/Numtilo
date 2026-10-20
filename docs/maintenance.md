# Maintenance Playbook

This document defines the routine maintenance and optional hardening steps for this repository.

## A. Routine Maintenance

### Weekly

1. Review open Dependabot PRs.
2. Confirm CI is green on each PR.
3. Merge safe dependency updates through PR flow.

Commands:

```bash
gh pr list --state open --limit 30
gh pr view <PR_NUMBER>
```

### Per Feature/Bugfix

1. Create a branch from `main`.
2. Implement the change.
3. Open a PR and wait for required checks.
4. Merge only after CI is green.

Commands:

```bash
git checkout -b feat/<name>
# edit
git add .
git commit -m "feat: <summary>"
git push -u origin feat/<name>
```

### UI verification and evidence

At an explicitly approved UI acceptance checkpoint, run the maintained current-build
graphical acceptance in a native session using the matching system/vcpkg UI Release
configure, build and test presets in [testing.md](testing.md#maintained-graphical-acceptance-step-1062). Retain the JSON
report and 1×/2× images; investigate failures before changing an oracle. Pair
controlled layout/input results with relevant native pointer, keyboard, focus and
accessibility checks. Painted screenshots or AX trees alone do not prove those
paths. Keep the ordinary unit/platform checks and independent headless audit intact.

QA writes belong in owned temporary profiles. Preserve actual user data, runtime
JSONs, untracked production sources and recorded evidence. Build/evidence cleanup is
a separate 15.1 task after the versioned regression entry points and required proof
are retained; inspect live build dependencies before removing obsolete output.

### Approved roadmap workflow from step 11

The user's workflow, recorded on 2026-10-14, governs roadmap substeps. Step 11 has
completed the [11.1 inventory and 11.2/11.3 scenario preparation](ui-edge-case-plan.md),
with successful builds and no scenario execution during preparation. Step 11.4 checkpoint
execution was explicitly authorized: automated gates passed, with one Ninja
environment skip. The user subsequently reported all six E11-03 manual steps
worked without problems. Step 11 is closed in its local scope; partial automation
and user-reported manual acceptance remain separate. No product defect was diagnosed,
source/oracle correction performed or extra test/automation started for closure.
For each explicitly
approved substep, implement only its scope,
review code quality, build and fix build errors, give a brief result and stop. Wait
for approval before starting the next substep.

Do not run gameplay tours, screenshots, VoiceOver, UI clicks or regression suites
during ordinary substeps. Run checkpoint tests after all approved substeps only
with explicit user approval. If a defect is found, report it, obtain approval for
the fix, apply it and verify that fix within the approved scope. This workflow
supersedes the routine verification cadence above for roadmap work.

## B. Optional Hardening (Cila)

Apply these gradually to avoid blocking solo development.

1. Signed commits (GPG/SSH/Sigstore).
2. Tighten review rules (for team mode).
3. Supply-chain hardening (attestation, advanced SBOM workflows).

## Guardrails

1. Do not push directly to `main`; use PRs.
2. Keep required checks stable before changing branch protection.
3. Treat broken CI as highest-priority maintenance work.

## Required CI checks

The local `scripts/apply_branch_protection.sh` profiles preserve these existing check names:

- `Format Check`
- `Static Analysis (ubuntu-latest)`
- `Build and Test (ubuntu-latest)`
- `Build and Test (macos-latest)`
- `Build and Test (windows-latest)`
- `Sanitizers (ubuntu-latest)`
- `Coverage (ubuntu-latest)`

Step 6.6 adds six contexts, one for each Linux/macOS/Windows and Debug/Release headless job:

| Runner | Debug check | Release check |
| --- | --- | --- |
| Ubuntu | `Headless Build and Test (ubuntu-latest, Debug)` | `Headless Build and Test (ubuntu-latest, Release)` |
| macOS | `Headless Build and Test (macos-latest, Debug)` | `Headless Build and Test (macos-latest, Release)` |
| Windows | `Headless Build and Test (windows-latest, Debug)` | `Headless Build and Test (windows-latest, Release)` |

Both profiles therefore specify 13 contexts. Editing this local script does not change the
repository's GitHub protection settings. Confirm the new checks appear and pass on a hosted
run before deliberately applying an updated protection profile; keep the workflow job names
and both profile lists synchronized. No remote protection update or hosted CI run was performed
during local step 6.6 implementation. Its actual validation scope is recorded in
[testing.md](testing.md#step-66-ci-validation).
