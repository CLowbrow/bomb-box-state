#!/usr/bin/env python3
"""Generate deterministic explosion scenarios and compare both engines."""

from __future__ import annotations

import argparse
import copy
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


def flat(x: int, y: int, elevation: int = 0) -> dict[str, Any]:
    return {"coordinate": {"x": x, "y": y}, "type": "flat", "elevation": elevation}


def ramp(x: int, y: int, low: str, elevation: int) -> dict[str, Any]:
    return {
        "coordinate": {"x": x, "y": y},
        "type": "ramp",
        "lowDirection": low,
        "lowElevation": elevation,
    }


def entity(identifier: int, kind: str, x: int, y: int, bottom: int) -> dict[str, Any]:
    return {
        "id": str(identifier),
        "type": kind,
        "coordinate": {"x": x, "y": y},
        "bottomHalfSteps": bottom,
    }


def fixture(kind: str, x: int, y: int, color: str | None = None) -> dict[str, Any]:
    value: dict[str, Any] = {"coordinate": {"x": x, "y": y}, "type": kind}
    if color is not None:
        value["color"] = color
    return value


def grid(width: int, height: int, elevation: int = 0) -> list[dict[str, Any]]:
    return [flat(x, y, elevation) for y in range(height) for x in range(width)]


def level(width: int,
          height: int,
          cells: list[dict[str, Any]],
          entities: list[dict[str, Any]],
          fixtures: list[dict[str, Any]] | None = None) -> dict[str, Any]:
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
        "cells": cells,
        "fixtures": fixtures or [],
        "entities": entities,
    }


def flat_line(elevations: list[int], entities: list[dict[str, Any]],
              fixtures: list[dict[str, Any]] | None = None) -> dict[str, Any]:
    return level(len(elevations), 1,
                 [flat(x, 0, elevation) for x, elevation in enumerate(elevations)],
                 entities, fixtures)


def authored_scenarios() -> list[tuple[str, dict[str, Any], list[str]]]:
    scenarios: list[tuple[str, dict[str, Any], list[str]]] = []
    scenarios.append(("adjacent-pop-fall", flat_line(
        [2, 2, 0, 0, -1],
        [entity(1, "player", 0, 0, 4), entity(8, "barrel", 1, 0, 4),
         entity(3, "box", 3, 0, 0)]), ["east"]))
    scenarios.append(("blast-height-stack-drop", flat_line(
        [0, 1, 0, 0, 0],
        [entity(1, "player", 4, 0, 0), entity(8, "barrel", 1, 0, 4),
         entity(2, "box", 2, 0, 0), entity(3, "box", 2, 0, 2),
         entity(4, "box", 2, 0, 4)]), []))
    scenarios.append(("same-cell-vertical-chain", flat_line(
        [0, 0],
        [entity(1, "player", 1, 0, 0), entity(9, "barrel", 0, 0, 0),
         entity(8, "barrel", 0, 0, 4)]), []))
    scenarios.append(("same-cell-simultaneous-sources", flat_line(
        [0, 0],
        [entity(4, "barrel", 0, 0, 4), entity(1, "player", 1, 0, 0),
         entity(8, "barrel", 0, 0, 2)]), []))
    scenarios.append(("middle-support-fatal-fall", flat_line(
        [0, 1, 0, 0],
        [entity(1, "player", 2, 0, 4), entity(8, "barrel", 1, 0, 4),
         entity(2, "box", 2, 0, 0), entity(3, "box", 2, 0, 2)]), []))
    scenarios.append(("blocked-barrel-chain", flat_line(
        [1, 0, 0, 0],
        [entity(1, "player", 0, 0, 2), entity(8, "barrel", 1, 0, 2),
         entity(9, "barrel", 2, 0, 0)],
        [fixture("door", 3, 0, "red")]), []))
    scenarios.append(("explosion-fixture-order", flat_line(
        [0, 0, 0],
        [entity(1, "player", 2, 0, 0), entity(8, "barrel", 0, 0, 2)],
        [fixture("switch", 0, 0, "red"), fixture("door", 1, 0, "red")]), []))

    ramp_stack_cells = [flat(0, 0, 0), ramp(1, 0, "west", 0), flat(2, 0, 1)]
    scenarios.append(("armed-ramp-stack-settles", level(
        3, 1, ramp_stack_cells,
        [entity(8, "barrel", 1, 0, 2), entity(1, "player", 1, 0, 4)]), []))
    scenarios.append(("ramp-endpoint-connectivity", level(
        3, 1, ramp_stack_cells,
        [entity(2, "box", 0, 0, 0), entity(3, "box", 1, 0, 1),
         entity(1, "player", 1, 0, 3), entity(4, "box", 2, 0, 2),
         entity(8, "barrel", 2, 0, 6)]), []))
    perpendicular = grid(3, 2)
    perpendicular[1] = ramp(1, 0, "west", 0)
    perpendicular[2] = flat(2, 0, 1)
    scenarios.append(("perpendicular-ramp-edge-ignored", level(
        3, 2, perpendicular,
        [entity(1, "player", 1, 0, 1), entity(2, "box", 0, 0, 0),
         entity(8, "barrel", 1, 1, 2)]), []))
    scenarios.append(("connected-ramp-centers", level(
        4, 1,
        [flat(0, 0, 0), ramp(1, 0, "west", 0), ramp(2, 0, "west", 1),
         flat(3, 0, 2)],
        [entity(2, "box", 0, 0, 0), entity(8, "barrel", 1, 0, 3),
         entity(1, "player", 2, 0, 3)]), []))

    lanes: list[dict[str, Any]] = []
    for y in range(3):
        lanes.extend([flat(0, y, 0), ramp(1, y, "west", 0), flat(2, y, 1)])
    scenarios.append(("matching-ramp-lane-slide", level(
        3, 3, lanes,
        [entity(2, "box", 0, 0, 0), entity(8, "barrel", 1, 0, 3),
         entity(3, "box", 0, 1, 0), entity(9, "box", 1, 1, 1),
         entity(1, "player", 2, 2, 2)]), []))

    scenarios.append(("opposing-impulses", level(
        5, 2, grid(5, 2),
        [entity(1, "player", 2, 1, 0), entity(8, "barrel", 1, 0, 2),
         entity(3, "box", 2, 0, 0), entity(4, "barrel", 3, 0, 2)]), []))
    scenarios.append(("overlapping-destinations", level(
        5, 2, grid(5, 2),
        [entity(1, "player", 2, 1, 0), entity(8, "barrel", 0, 0, 2),
         entity(2, "box", 1, 0, 0), entity(3, "box", 3, 0, 0),
         entity(9, "barrel", 4, 0, 2)]), []))
    split_height_cells = grid(5, 2)
    split_height_cells[3] = flat(3, 0, 1)
    split_height_cells[4] = flat(4, 0, 1)
    scenarios.append(("nonoverlap-different-heights", level(
        5, 2, split_height_cells,
        [entity(1, "player", 2, 1, 0), entity(8, "barrel", 0, 0, 2),
         entity(2, "box", 1, 0, 0), entity(3, "box", 3, 0, 2),
         entity(9, "barrel", 4, 0, 4)]), []))
    scenarios.append(("direction-conflict-before-blockage", level(
        4, 4, grid(4, 4),
        [entity(1, "player", 3, 3, 0), entity(2, "box", 1, 0, 0),
         entity(7, "barrel", 1, 1, 0), entity(8, "barrel", 2, 1, 2),
         entity(9, "barrel", 1, 2, 2)]), []))
    scenarios.append(("blast-fall-next-wave", flat_line(
        [2, 2, 0, 0, -1],
        [entity(1, "player", 0, 0, 4), entity(8, "barrel", 1, 0, 4),
         entity(9, "barrel", 3, 0, 0)]), ["east"]))
    scenarios.append(("simultaneous-secondary-wave", level(
        5, 5, grid(5, 5),
        [entity(1, "player", 2, 4, 0), entity(8, "barrel", 2, 2, 2),
         entity(9, "barrel", 1, 2, 0), entity(7, "barrel", 3, 2, 0)]), []))
    fall_slide_cells = grid(5, 2)
    for index in range(3):
        fall_slide_cells[index] = flat(index, 0, 1)
    fall_slide_cells[3] = ramp(3, 0, "east", 0)
    scenarios.append(("blast-fall-slide-next-wave", level(
        5, 2, fall_slide_cells,
        [entity(1, "player", 0, 1, 0), entity(8, "barrel", 1, 0, 4),
         entity(9, "barrel", 2, 0, 2)]), []))
    scenarios.append(("simultaneous-player-death", level(
        3, 3, grid(3, 3),
        [entity(1, "player", 1, 1, 0), entity(8, "barrel", 1, 2, 2),
         entity(9, "barrel", 2, 1, 2)]), []))
    scenarios.append(("terminal-win-cancels-unstable-barrels", level(
        4, 2, grid(4, 2),
        [entity(1, "player", 0, 0, 0), entity(8, "barrel", 2, 1, 4),
         entity(9, "barrel", 3, 1, 6)],
        [fixture("exit", 0, 0)]), []))
    return scenarios


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference_runner", type=Path)
    parser.add_argument("candidate_runner", type=Path)
    parser.add_argument("comparator", type=Path)
    return parser.parse_args()


def run_json(runner: Path, transcript: Path) -> list[Any]:
    completed = subprocess.run([str(runner), str(transcript)], check=False,
                               capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or "candidate runner failed")
    return [json.loads(line) for line in completed.stdout.splitlines()]


def main() -> int:
    args = parse_args()
    seeds = [0x09A11CE, 0x09B1A57, 0x09C4A17, 0x09D37E2,
             0x09E501D, 0x09F17E5, 0x0912345, 0x0965432]
    seeded_templates = [
        "overlapping-destinations",
        "direction-conflict-before-blockage",
        "simultaneous-secondary-wave",
        "blast-fall-slide-next-wave",
        "simultaneous-player-death",
        "terminal-win-cancels-unstable-barrels",
        "nonoverlap-different-heights",
        "same-cell-vertical-chain",
    ]
    authored = authored_scenarios()
    authored_by_name = {name: value for name, value, moves in authored if not moves}
    with tempfile.TemporaryDirectory(prefix="game-rules-explosions-") as temporary:
        root = Path(temporary)
        transcript = root / "explosions.txt"
        lines = ["# Generated explosion parity and deterministic-seed corpus."]
        equivalent_pairs: list[tuple[int, int]] = []
        operation_count = 0

        for name, value, moves in authored:
            path = root / f"{name}.json"
            path.write_text(json.dumps(value, separators=(",", ":")), encoding="utf-8")
            lines.append(f"load|-|{path.name}")
            operation_count += 1
            for direction in moves:
                lines.append(f"move|-|{direction}")
                operation_count += 1

        for seed, template_name in zip(seeds, seeded_templates):
            randomizer = random.Random(seed)
            canonical = copy.deepcopy(authored_by_name[template_name])
            ids = randomizer.sample(range(2, 1_000_000), len(canonical["entities"]))
            for current, identifier in zip(canonical["entities"], ids):
                current["id"] = str(identifier)
            reordered = copy.deepcopy(canonical)
            randomizer.shuffle(reordered["cells"])
            randomizer.shuffle(reordered["entities"])
            canonical_path = root / f"stress-{seed:08x}-{template_name}-canonical.json"
            reordered_path = root / f"stress-{seed:08x}-{template_name}-reordered.json"
            canonical_path.write_text(json.dumps(canonical, separators=(",", ":")),
                                      encoding="utf-8")
            reordered_path.write_text(json.dumps(reordered, separators=(",", ":")),
                                      encoding="utf-8")
            first = operation_count
            lines.append(f"load|-|{canonical_path.name}")
            operation_count += 1
            second = operation_count
            lines.append(f"load|-|{reordered_path.name}")
            operation_count += 1
            third = operation_count
            lines.append(f"load|-|{canonical_path.name}")
            operation_count += 1
            equivalent_pairs.extend([(first, second), (first, third)])

        transcript.write_text("\n".join(lines) + "\n", encoding="utf-8")
        compared = subprocess.run(
            [sys.executable, str(args.comparator), str(args.reference_runner),
             str(args.candidate_runner), str(transcript)],
            check=False, capture_output=True, text=True)
        if compared.returncode != 0:
            print(compared.stdout, end="")
            print(compared.stderr, end="")
            return compared.returncode

        candidate = run_json(args.candidate_runner, transcript)
        for left, right in equivalent_pairs:
            if candidate[left] != candidate[right]:
                print(f"seeded deterministic mismatch at operations {left + 1} and {right + 1}")
                return 1
        print(f"matched {operation_count} explosion operations across {len(seeds)} repeated seeds")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
