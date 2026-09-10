from __future__ import annotations

import shutil
import zipfile
from argparse import Namespace
from pathlib import Path

from . import helper

_SOURCE_ROOTS = ("modules", "src")
_SOURCE_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".m", ".mm")
_ZIP_EXCLUDED = ("build", ".git", "node_modules", "dist")


def clean(args: Namespace) -> None:
    build_dir = helper.PROJECT_DIR / args.build_dir
    if build_dir.exists():
        shutil.rmtree(build_dir)
        print(f"removed {build_dir}")


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


def _is_excluded(relative: Path) -> bool:
    return any(part in _ZIP_EXCLUDED for part in relative.parts)


def zip(args: Namespace) -> None:
    archive = helper.PROJECT_DIR.parent / f"{helper.PROJECT_DIR.name}.zip"
    root_name = helper.PROJECT_DIR.name

    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(helper.PROJECT_DIR.rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(helper.PROJECT_DIR)
            if _is_excluded(relative):
                continue
            bundle.write(path, Path(root_name) / relative)

    print(f"created {archive}")
