from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

# The repository root sits three levels up from `tools/core/helper.py`.
PROJECT_DIR = Path(__file__).resolve().parents[2]


# Builds run four jobs unless a task is given "--jobs", since a build on every core can freeze the machine.
DEFAULT_JOBS = 4


def run(args: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    printable = " ".join(str(a) for a in args)
    print(f"> {printable}", flush=True)

    result = subprocess.run([str(a) for a in args], cwd=str(cwd or PROJECT_DIR), env=env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def environ_with(**values: str) -> dict[str, str]:
    env = dict(os.environ)
    env.update(values)
    return env
