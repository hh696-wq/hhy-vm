#!/usr/bin/env python3

import csv
import json
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
BUILD = ROOT / ".build"
RESULTS = ROOT / "results"


def version(command: list[str]) -> str:
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    return (result.stdout or result.stderr).splitlines()[0].strip()


def execute(command: list[str]) -> tuple[str, float]:
    started = time.perf_counter_ns()
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000
    return result.stdout.strip(), elapsed_ms


def main() -> None:
    config_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "config.json"
    config = json.loads(config_path.read_text(encoding="utf-8"))
    BUILD.mkdir(exist_ok=True)
    RESULTS.mkdir(exist_ok=True)

    go_binary = BUILD / "algorithms-go"
    subprocess.run(["go", "build", "-o", str(go_binary), str(ROOT / "algorithms.go")], check=True)

    commands = {
        "HHY": [str(REPO / "build" / "hhy"), "run", str(ROOT / "algorithms.hhy")],
        "Go": [str(go_binary)],
        "Python": [sys.executable, str(ROOT / "algorithms.py")],
        "PHP": ["php", str(ROOT / "algorithms.php")],
    }
    rows: list[dict[str, object]] = []

    for case in config["cases"]:
        outputs: dict[str, str] = {}
        for language, base_command in commands.items():
            command = [*base_command, case["algorithm"], str(case["input"])]
            for _ in range(config["warmups"]):
                execute(command)
            samples = [execute(command) for _ in range(config["runs"])]
            result_values = {output for output, _ in samples}
            if len(result_values) != 1:
                raise RuntimeError(f"unstable result for {language} {case['algorithm']}: {result_values}")
            output = result_values.pop()
            outputs[language] = output
            durations = [duration for _, duration in samples]
            rows.append({
                "algorithm": case["algorithm"],
                "input": case["input"],
                "language": language,
                "median_ms": round(statistics.median(durations), 3),
                "min_ms": round(min(durations), 3),
                "max_ms": round(max(durations), 3),
                "result": output.split()[-1],
                "runs": config["runs"],
            })
        if len(set(outputs.values())) != 1:
            raise RuntimeError(f"result mismatch for {case['algorithm']}: {outputs}")

    csv_path = RESULTS / "benchmark.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    report = {
        "methodology": {
            "clock": "time.perf_counter_ns around each subprocess",
            "warmups": config["warmups"],
            "runs": config["runs"],
            "statistic": "median wall-clock milliseconds",
            "go_build_included": False,
            "process_startup_included": True,
        },
        "environment": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "hhy": version([str(REPO / "build" / "hhy"), "--version"]),
            "go": version(["go", "version"]),
            "python": version([sys.executable, "--version"]),
            "php": version(["php", "--version"]),
        },
        "results": rows,
    }
    (RESULTS / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print("Algorithm benchmark complete")
    for case in config["cases"]:
        print(f"\n{case['algorithm']} input={case['input']}")
        for row in rows:
            if row["algorithm"] == case["algorithm"]:
                print(f"  {row['language']:<6} {row['median_ms']:>10.3f} ms  result={row['result']}")
    print(f"\nCSV {csv_path}")
    print(f"Report {RESULTS / 'report.json'}")


if __name__ == "__main__":
    main()
