## Summary

- What changed?
- Why was this needed?

## Validation

- [ ] `./scripts/check_format.sh`
- [ ] Headless configure/build/test preset chain (`vcpkg-headless-debug` or `vcpkg-headless-release`).
- [ ] `cmake --build --preset build-vcpkg-debug`
- [ ] `ctest --test-dir build/vcpkg-debug --output-on-failure`
- [ ] For build/dependency/CI changes: guarded headless configuration check and supported-platform fresh dependency audits; attach results and state local/hosted scope.

## Checklist

- [ ] Scope is focused (no unrelated changes).
- [ ] Tests were added/updated for behavior changes.
- [ ] Docs/README/CHANGELOG updated when needed.
- [ ] CI is expected to pass (`format`, `static-analysis`, `headless` Debug/Release matrix, `build-and-test`, `sanitizers`, `coverage`).
