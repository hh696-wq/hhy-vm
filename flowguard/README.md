# FlowGuard

[中文说明](README.zh-CN.md)

FlowGuard is a complete repository health and quality-gate application written in HHY v1.0. It demonstrates that HHY can coordinate real filesystem, process, HTTP, Stream, module, error, and reporting workloads—not just small syntax examples.

## What it checks

- required project files;
- file inventory and oversized assets;
- redacted credential-pattern scanning;
- concurrent quality commands;
- concurrent HTTP health checks;
- structured JSON reports and stable exit codes.

## Run the healthy fixture

Start the deterministic local API in one terminal:

```sh
python3 test-server.py
```

Then run FlowGuard from this directory:

```sh
../build/hhy run flowguard.hhy \
  fixtures/healthy-project \
  config/healthy.json \
  output/healthy-report.json
```

FlowGuard exits with `0` when no check fails. Warnings are retained in the report but do not fail the gate.

## Run all self-tests

```sh
sh self-test.sh
```

The test runs both fixtures. The healthy project must pass; the risky project intentionally has a missing `LICENSE`, a fake credential pattern, a failing command, and an unhealthy endpoint, so it must return exit code `1`. Generated reports are written to `output/` and ignored by Git.

Override the interpreter when needed:

```sh
HHY_BIN=/usr/local/bin/hhy sh self-test.sh
```

## Use it on another project

Copy a config, replace the required files, commands, health endpoints, and size limit, then run. The current example accepts `256b`, `1kib`, `4kib`, or `1mib` for `limits.large_file`:

```sh
hhy run \
  --limit max_runtime=2min \
  --limit max_memory=256mib \
  --limit max_processes=8 \
  flowguard.hhy /path/to/project config/my-project.json report.json
```

Command arguments are arrays and are passed directly to `run`; FlowGuard does not use `shell`. Credential findings never include matching file contents.
