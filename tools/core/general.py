from __future__ import annotations

import shutil
import subprocess
import zipfile
from argparse import Namespace
from pathlib import Path

from . import helper

_SOURCE_ROOTS = ("modules", "src")
_SOURCE_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".m", ".mm")


def clean(args: Namespace) -> None:
    build_dir = helper.PROJECT_DIR / args.build_dir
    if build_dir.exists():
        shutil.rmtree(build_dir)
        print(f"Removed \"{build_dir}\".")


def _is_vendored(path: Path) -> bool:
    return "vendor" in path.parts


def fmt(args: Namespace) -> None:
    files = []
    for root in _SOURCE_ROOTS:
        for path in sorted((helper.PROJECT_DIR / root).rglob("*")):
            if path.suffix in _SOURCE_SUFFIXES and not _is_vendored(path):
                files.append(str(path))

    if files:
        helper.run(["clang-format", "-i", *files])


def _source_files() -> list[Path]:
    # The archive holds what Git does not ignore, so build trees, local settings and secrets never leave the machine in it.
    listed = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=str(helper.PROJECT_DIR),
        capture_output=True,
        check=True,
    ).stdout.decode("utf-8")
    return sorted(Path(name) for name in listed.split("\0") if name)


def zip(args: Namespace) -> None:
    archive = helper.PROJECT_DIR.parent / f"{helper.PROJECT_DIR.name}.zip"
    root_name = helper.PROJECT_DIR.name

    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for relative in _source_files():
            path = helper.PROJECT_DIR / relative
            # A tracked file deleted in the working tree is not part of the sources any more.
            if not path.is_file():
                continue
            bundle.write(path, Path(root_name) / relative)

    print(f"Created \"{archive}\".")
