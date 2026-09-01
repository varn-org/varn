from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

# repository root three levels up from tools/core/helper.py
PROJECT_DIR = Path(__file__).resolve().parents[2]


def jobs() -> int:
    # leave a couple of cores free so heavy native builds do not freeze the machine
    return max(1, (os.cpu_count() or 4) - 2)


def run(args: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    printable = " ".join(str(a) for a in args)
    print(f"> {printable}")

    result = subprocess.run([str(a) for a in args], cwd=str(cwd or PROJECT_DIR), env=env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def environ_with(**values: str) -> dict[str, str]:
    env = dict(os.environ)
    env.update(values)
    return env
