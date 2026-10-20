# Contributing

## Workflow

The new Numtilo GitHub repository is pending creation. GitHub fork/PR and branch-protection
steps below apply once its actual repository address is available.

1. Fork/branch from `main`.
2. Make a focused change.
3. Run format, build, and tests locally.
4. Open a PR with a clear summary.

## Branch Naming

Use short descriptive names, for example:

- `feat/ui-gameover-overlay`
- `fix/core-merge-rule`
- `chore/docs-testing`

## Commit Messages

Use Conventional Commits:

- `feat: add game over new game and quit actions`
- `fix: clear spawn animation state on menu transition`
- `docs: add architecture and testing guides`
- `chore: pin sfml manifest version`

## Local Quality Checklist

```bash
./scripts/check_format.sh
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
ctest --test-dir build/vcpkg-debug -C Debug --output-on-failure
```

`-C Debug` selects the built configuration for multi-config generators such as Visual Studio.

## Pull Request Checklist

- [ ] Scope is small and focused.
- [ ] No unrelated files changed.
- [ ] `./scripts/check_format.sh` passes.
- [ ] Build succeeds.
- [ ] Tests pass (`ctest ...`).
- [ ] Docs/README updated if behavior changed.
- [ ] Changelog updated under `Unreleased`.

## Engineering Rules

- Keep core logic SFML-independent (`src/core`).
- New gameplay rules require unit tests.
- Avoid platform-specific hardcoded paths.
- Prefer deterministic behavior for reproducible bug reports.

## Branch Protection (Maintainers)

Apply branch protection rules for `main` (required checks + review gate):

```bash
./scripts/apply_branch_protection.sh OWNER/REPO
```

Replace `OWNER/REPO` with the verified repository after creation. If `origin` points to
that GitHub repository, the repo argument can be omitted:

```bash
./scripts/apply_branch_protection.sh
```
