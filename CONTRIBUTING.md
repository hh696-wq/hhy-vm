# Contributing to HHY

Core changes must preserve the frozen language semantics, explicit AST fallback,
resource limits, cancellation and structured errors. Read [the specification](docs/HHY_V1.en.md)
and [Runtime governance](docs/RUNTIME_GOVERNANCE.md) before changing these boundaries.

## Build and validate

Install the dependencies described in [INSTALL.md](INSTALL.md), then run:

```sh
make
make test
make quality
```

Compiler changes additionally use `make test-compiler`; Runtime changes require
`make test-debug` and appropriate GC/fuzz coverage. `make workload-test` checks
real-workload fixtures under both engines. Browser fixture setup is documented in
[tests/workloads](tests/workloads/README.md). Host-capability skips are not passes;
CI must run the relevant coverage on capable runners.

## Repository scope

The tracked top-level directories are `.github`, `src`, `include`, `compiler`,
`tests`, `benchmarks`, `scripts`, `extensions`, `examples`, `docs`, and `Formula`.
Keep generated binaries, dependencies, full standalone apps, editor plugins,
website sources, videos and private analysis outside the Core commit surface.
Binary packages belong in GitHub Releases; CI reports belong in workflow artifacts.
Only the necessary small documentation images belong in `docs/assets`.

Use focused commits describing behavior and validation. Check `git diff --check`
and `python3 scripts/check-repository.py` before submission. Update public docs
alongside API changes. Release versions and digests require the existing release
workflow; repository housekeeping does not require a new language version.
