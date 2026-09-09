# Runtime workload regression fixtures

This directory contains the programs, local servers, assertions and input fixtures
needed by the seven existing AST/Bytecode workload regressions. Full application
READMEs, launchers, non-test configuration and generated output stay outside the
Core repository. These are regression snapshots, not independently released apps.

Run `make workload-test` from the repository root. The browser case additionally
requires `npm ci --prefix tests/workloads/my-crawler/renderer` and a Playwright
Chromium installation; use `HHY_WORKLOAD_BROWSER=1 make workload-test` to include it.
CI installs the browser and continues to run all seven cases. Resource and cache
evaluation scripts use the same fixture paths. Output directories are ignored.

Do not remove test inputs merely because they look like generated files: the
asset-governance fixture intentionally includes old temporary files and cache data.
