#!/usr/bin/env python3
"""Run one transcript through reference and C candidates and show the first difference."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Operation:
    name: str
    line: int


@dataclass(frozen=True)
class Difference:
    path: str
    reference: Any
    candidate: Any
    detail: str


def operations(path: Path) -> list[Operation]:
    result: list[Operation] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line and not line.startswith("#"):
            result.append(Operation(line.split("|", 1)[0], number))
    return result


def run(runner: Path, transcript: Path, label: str) -> list[Any]:
    completed = subprocess.run(
        [str(runner), str(transcript)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{label} runner exited {completed.returncode}:\n{completed.stderr.rstrip()}"
        )
    values: list[Any] = []
    for index, line in enumerate(completed.stdout.splitlines(), 1):
        try:
            values.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise RuntimeError(
                f"{label} runner emitted invalid JSON on output line {index}: {error}"
            ) from error
    return values


def child_path(path: str, key: str) -> str:
    return f"{path}.{key}" if key.isidentifier() else f"{path}[{json.dumps(key)}]"


def first_difference(reference: Any, candidate: Any, path: str = "$") -> Difference | None:
    if type(reference) is not type(candidate):
        return Difference(path, reference, candidate, "value types differ")
    if isinstance(reference, dict):
        for key in reference:
            if key not in candidate:
                return Difference(child_path(path, key), reference[key], None, "candidate key is missing")
            difference = first_difference(reference[key], candidate[key], child_path(path, key))
            if difference is not None:
                return difference
        for key in candidate:
            if key not in reference:
                return Difference(child_path(path, key), None, candidate[key], "candidate has an extra key")
        return None
    if isinstance(reference, list):
        shared = min(len(reference), len(candidate))
        for index in range(shared):
            difference = first_difference(reference[index], candidate[index], f"{path}[{index}]")
            if difference is not None:
                return difference
        if len(reference) != len(candidate):
            return Difference(f"{path}.length", len(reference), len(candidate), "array lengths differ")
        return None
    if reference != candidate:
        return Difference(path, reference, candidate, "values differ")
    return None


def render(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expect-difference", action="store_true")
    parser.add_argument("--expected-operation", type=int, help="one-based operation index")
    parser.add_argument("--expected-path")
    parser.add_argument("reference_runner", type=Path)
    parser.add_argument("candidate_runner", type=Path)
    parser.add_argument("transcript", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        described = operations(args.transcript)
        reference = run(args.reference_runner, args.transcript, "reference")
        candidate = run(args.candidate_runner, args.transcript, "candidate")
    except (OSError, RuntimeError) as error:
        print(f"comparison error: {error}", file=sys.stderr)
        return 2

    if len(reference) != len(candidate) or len(reference) != len(described):
        print(
            "comparison error: operation/output counts differ "
            f"(transcript={len(described)}, reference={len(reference)}, candidate={len(candidate)})",
            file=sys.stderr,
        )
        return 2

    for zero_index, (reference_value, candidate_value) in enumerate(zip(reference, candidate)):
        difference = first_difference(reference_value, candidate_value)
        if difference is None:
            continue
        one_index = zero_index + 1
        operation = described[zero_index]
        print(
            f"difference at operation {one_index} ({operation.name}, transcript line {operation.line}), "
            f"path {difference.path}: {difference.detail}\n"
            f"  reference: {render(difference.reference)}\n"
            f"  candidate: {render(difference.candidate)}"
        )
        if not args.expect_difference:
            return 1
        if args.expected_operation is not None and args.expected_operation != one_index:
            print("difference did not occur at the expected operation", file=sys.stderr)
            return 1
        if args.expected_path is not None and args.expected_path != difference.path:
            print("difference did not occur at the expected field path", file=sys.stderr)
            return 1
        print("expected incomplete-port difference observed")
        return 0

    if args.expect_difference:
        print("expected an incomplete-port difference, but outputs matched", file=sys.stderr)
        return 1
    print(f"matched {len(reference)} operations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
