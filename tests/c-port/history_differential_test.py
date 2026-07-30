#!/usr/bin/env python3
"""Generate long deterministic stage-10 history/lifecycle sequences and compare engines."""

from __future__ import annotations

import argparse
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


SEEDS = [
    0x10A11CE,
    0x10B4A2C,
    0x10C0FFEE,
    0x10D37E2,
    0x10E501D,
    0x10F17E5,
    0x1012345,
    0x1065432,
]
RANDOM_OPERATIONS_PER_SEED = 256


def flat_level(width: int, height: int, player_x: int, player_y: int,
               player_id: int) -> dict[str, Any]:
    return {
        "format": "game-rules-level",
        "version": 1,
        "coordinateSystem": {
            "origin": {"x": 0, "y": 0},
            "positiveX": "east",
            "positiveY": "north",
        },
        "width": width,
        "height": height,
        "cells": [
            {"coordinate": {"x": x, "y": y}, "type": "flat", "elevation": 0}
            for y in range(height)
            for x in range(width)
        ],
        "fixtures": [],
        "entities": [{
            "id": str(player_id),
            "type": "player",
            "coordinate": {"x": player_x, "y": player_y},
            "bottomHalfSteps": 0,
        }],
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference_runner", type=Path)
    parser.add_argument("candidate_runner", type=Path)
    parser.add_argument("comparator", type=Path)
    return parser.parse_args()


def run_runner(runner: Path, transcript: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(runner), str(transcript)], check=False,
                          capture_output=True, text=True)


def main() -> int:
    args = parse_args()
    repository = Path(__file__).resolve().parents[2]
    contracts = repository / "tests" / "contracts"
    stress_level = contracts / "engine_hardening" / "v1" / "rewind-stress-level.json"
    terminal_level = contracts / "engine_hardening" / "v1" / "terminal-level.json"
    browser_level = contracts / "browser_vertical_slice" / "v1" / "level.json"
    malformed = contracts / "engine_hardening" / "v1" / "malformed.json"
    loss_level = repository / "tests" / "c-port" / "levels" / "fall-chain-loss.json"

    with tempfile.TemporaryDirectory(prefix="game-rules-stage10-") as temporary:
        root = Path(temporary)
        open_level = root / "open-level.json"
        replacement_level = root / "replacement-level.json"
        open_level.write_text(json.dumps(flat_level(7, 5, 3, 2, 1), separators=(",", ":")),
                              encoding="utf-8")
        replacement_level.write_text(
            json.dumps(flat_level(9, 3, 4, 1, 99), separators=(",", ":")),
            encoding="utf-8")

        level_paths = [open_level, replacement_level, stress_level,
                       terminal_level, browser_level, loss_level]
        directions = ["north", "east", "south", "west", "invalid"]
        lines = ["# Generated stage-10 mixed history, replacement, and lifecycle corpus."]

        for seed in SEEDS:
            randomizer = random.Random(seed)
            lines.extend([
                "create-engine|-",
                f"load|-|{stress_level}",
                "move|-|east", "move|-|north", "move|-|east", "move|-|east",
                "move|-|east", "move|-|east", "move|-|north", "move|-|east",
                "move|-|west",
                "rewind|-", "rewind|-", "rewind|-",
                "move|-|north",
                f"load|-|{malformed}",
                "get-state|-",
                f"load|-|{browser_level}",
                "move|-|east", "rewind|-", "rewind|-",
                f"load|-|{terminal_level}",
                "move|-|west", "rewind|-",
                f"load|-|{loss_level}",
                "move|-|east", "rewind|-",
                "destroy-engine|-", "get-state|-", "create-engine|-",
            ])

            for _ in range(RANDOM_OPERATIONS_PER_SEED):
                choice = randomizer.random()
                if choice < 0.42:
                    lines.append(f"move|-|{randomizer.choice(directions)}")
                elif choice < 0.60:
                    lines.append("rewind|-")
                elif choice < 0.72:
                    lines.append("get-state|-")
                elif choice < 0.84:
                    lines.append(f"load|-|{randomizer.choice(level_paths)}")
                elif choice < 0.92:
                    lines.append(f"load|-|{malformed}")
                elif choice < 0.96:
                    lines.append("destroy-engine|-")
                else:
                    lines.append("create-engine|-")

            lines.extend(["destroy-engine|-", "create-engine|-", f"load|-|{open_level}"])

        transcript = root / "stage10-history-lifecycle.txt"
        transcript.write_text("\n".join(lines) + "\n", encoding="utf-8")
        operation_count = sum(1 for line in lines if line and not line.startswith("#"))

        compared = subprocess.run(
            [sys.executable, str(args.comparator), str(args.reference_runner),
             str(args.candidate_runner), str(transcript)],
            check=False, capture_output=True, text=True)
        if compared.returncode != 0:
            print(compared.stdout, end="")
            print(compared.stderr, end="", file=sys.stderr)
            return compared.returncode

        first = run_runner(args.candidate_runner, transcript)
        second = run_runner(args.candidate_runner, transcript)
        if first.returncode != 0 or second.returncode != 0:
            print(first.stderr or second.stderr, end="", file=sys.stderr)
            return 2
        if first.stdout != second.stdout:
            print("candidate output changed across identical seeded executions", file=sys.stderr)
            return 1

        print(f"matched {operation_count} mixed operations across {len(SEEDS)} seeds; "
              "candidate repeat was byte-identical")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
