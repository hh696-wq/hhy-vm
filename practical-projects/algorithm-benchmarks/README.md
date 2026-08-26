# HHY algorithm timing benchmark against Go, Python, and PHP

This project implements the same three CPU-bound algorithms in HHY, Go, Python, and PHP, verifies identical outputs, and compares independent-process wall-clock time.

## Workloads

- naive recursive `fibonacci(22)` → `17711`;
- trial-division `prime_count(3000)` → `430`;
- 70×70 Euclidean `gcd_grid(70)` checksum → `14197`.

## Methodology

- Apple Silicon arm64 on macOS 26.6.2;
- HHY 1.1.0, Go 1.27.0, Python 3.14.7, and PHP 8.5.9;
- one warm-up and five measured runs per language/workload;
- `time.perf_counter_ns` around each subprocess, reporting median wall-clock milliseconds;
- process startup is included; Go compilation happens before timing;
- the harness requires every implementation to print exactly the same algorithm and checksum.

## Measured medians

| Workload | HHY | Go | Python | PHP |
|---|---:|---:|---:|---:|
| Fibonacci(22) | 1850.439 ms | 2.185 ms | 15.447 ms | 32.601 ms |
| Prime count(3000) | 666.554 ms | 2.113 ms | 15.210 ms | 31.969 ms |
| GCD grid(70) | 890.378 ms | 2.276 ms | 14.793 ms | 32.598 ms |

These results describe only this machine, these implementations, and these inputs. Startup dominates much of the short Go, Python, and PHP runs; HHY spends most of its time interpreting dense loops and calls. HHY is currently better suited to filesystem, process, HTTP, and structured-data orchestration than numeric hot loops.

## Run

From the repository root:

```sh
sh practical-projects/algorithm-benchmarks/run.sh
```

The harness writes `results/benchmark.csv` and `results/report.json` with the methodology, environment, checksums, and complete timings.
