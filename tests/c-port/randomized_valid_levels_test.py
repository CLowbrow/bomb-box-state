#!/usr/bin/env python3
"""Generate valid levels/transcripts and compare complete reference/C17 responses."""

from __future__ import annotations

import copy
import json
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


SEEDS = [
    0xC170001,
    0xC17002B,
    0xC1700D3,
    0xC171337,
    0xC17A110,
    0xC17BEEF,
    0xC17CAFE,
    0xC17F00D,
]


def generate_level(seed: int) -> tuple[dict[str, Any], dict[str, Any], list[str]]:
    rng = random.Random(seed)
    width = rng.randint(3, 8)
    height = rng.randint(2, 5)
    origin_x = rng.randint(-20, 20)
    origin_y = rng.randint(-20, 20)
    coordinates = [
        {"x": origin_x + x, "y": origin_y + y}
        for y in range(height)
        for x in range(width)
    ]
    cells = [
        {"coordinate": copy.deepcopy(coordinate), "type": "flat", "elevation": 0}
        for coordinate in coordinates
    ]

    available = coordinates.copy()
    rng.shuffle(available)
    fixture_count = min(len(available) // 4, rng.randint(0, 6))
    colors = ["red", "green", "blue", "yellow"]
    fixtures: list[dict[str, Any]] = []
    for index in range(fixture_count):
        kind = "switch" if index % 2 == 0 else "door"
        fixtures.append({
            "coordinate": copy.deepcopy(available.pop()),
            "type": kind,
            "color": rng.choice(colors),
        })

    occupied = coordinates.copy()
    rng.shuffle(occupied)
    entity_count = min(len(occupied), rng.randint(1, 1 + len(occupied) // 2))
    identifiers = rng.sample(range(2, 1_000_000_000), max(0, entity_count - 1))
    if seed == SEEDS[-1] and identifiers:
        identifiers[-1] = 18_446_744_073_709_551_615
    entities: list[dict[str, Any]] = [{
        "id": "1",
        "type": "player",
        "coordinate": copy.deepcopy(occupied.pop()),
        "bottomHalfSteps": 0,
    }]
    for identifier in identifiers:
        entities.append({
            "id": str(identifier),
            "type": rng.choice(["box", "barrel"]),
            "coordinate": copy.deepcopy(occupied.pop()),
            "bottomHalfSteps": 0,
        })

    canonical = {
        "format": "game-rules-level",
        "version": 1,
        "coordinateSystem": {
            "origin": {"x": origin_x, "y": origin_y},
            "positiveX": rng.choice(["east", "west"]),
            "positiveY": rng.choice(["north", "south"]),
        },
        "width": width,
        "height": height,
        "cells": cells,
        "fixtures": fixtures,
        "entities": entities,
    }
    reordered = copy.deepcopy(canonical)
    rng.shuffle(reordered["cells"])
    rng.shuffle(reordered["fixtures"])
    rng.shuffle(reordered["entities"])

    operations = ["get-state|-", "rewind|-"]
    choices = ["north", "east", "south", "west", "invalid"]
    for _ in range(128):
        value = rng.random()
        if value < 0.76:
            operations.append(f"move|-|{rng.choice(choices)}")
        elif value < 0.91:
            operations.append("rewind|-")
        else:
            operations.append("get-state|-")
    return canonical, reordered, operations


def run(runner: Path, transcript: Path, locale: str) -> tuple[str, list[Any]]:
    environment = os.environ.copy()
    environment["LC_ALL"] = locale
    completed = subprocess.run(
        [str(runner), str(transcript)],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or f"runner exited {completed.returncode}")
    return completed.stdout, [json.loads(line) for line in completed.stdout.splitlines()]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: randomized_valid_levels_test.py <reference-runner> <candidate-runner>",
              file=sys.stderr)
        return 2
    reference_runner = Path(sys.argv[1])
    candidate_runner = Path(sys.argv[2])
    operation_count = 0
    with tempfile.TemporaryDirectory(prefix="game-rules-c17-random-") as temporary:
        root = Path(temporary)
        for seed in SEEDS:
            canonical, reordered, operations = generate_level(seed)
            level_paths = []
            for label, value in (("canonical", canonical), ("reordered", reordered)):
                level_path = root / f"{seed:08x}-{label}.json"
                level_path.write_text(json.dumps(value, separators=(",", ":")), encoding="utf-8")
                transcript = root / f"{seed:08x}-{label}.txt"
                transcript.write_text(
                    "\n".join([f"load|-|{level_path.name}", *operations]) + "\n",
                    encoding="utf-8",
                )
                level_paths.append(transcript)

            reference_c_text, reference_c = run(reference_runner, level_paths[0], "C")
            candidate_c_text, candidate_c = run(candidate_runner, level_paths[0], "C")
            reference_utf8_text, reference_utf8 = run(
                reference_runner, level_paths[1], "en_US.UTF-8")
            candidate_utf8_text, candidate_utf8 = run(
                candidate_runner, level_paths[1], "en_US.UTF-8")
            if reference_c != candidate_c or reference_utf8 != candidate_utf8:
                print(f"reference/C17 mismatch for seed 0x{seed:08x}", file=sys.stderr)
                return 1
            if reference_c != reference_utf8 or candidate_c != candidate_utf8:
                print(f"input-order or locale mismatch for seed 0x{seed:08x}", file=sys.stderr)
                return 1

            candidate_repeat_text, candidate_repeat = run(candidate_runner, level_paths[0], "C")
            if candidate_repeat != candidate_c or candidate_repeat_text != candidate_c_text:
                print(f"candidate repeat mismatch for seed 0x{seed:08x}", file=sys.stderr)
                return 1
            if reference_c_text != candidate_c_text or reference_utf8_text != candidate_utf8_text:
                print(f"byte serialization mismatch for seed 0x{seed:08x}", file=sys.stderr)
                return 1
            operation_count += len(reference_c) + len(reference_utf8)

    print(f"matched {operation_count} operations across {len(SEEDS)} generated valid-level seeds")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
