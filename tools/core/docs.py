from __future__ import annotations

import re
import sys
from argparse import Namespace
from pathlib import Path

from . import helper

_EXAMPLES_HEADING = "## Examples"


def _reference_page(module: str) -> Path:
    return helper.PROJECT_DIR / "docs" / "lua-api" / f"{module}.md"


def _examples_dir(module: str) -> Path:
    return helper.PROJECT_DIR / "modules" / module / "lua" / "examples"


def _documented_modules() -> list[str]:
    modules = []
    for module_dir in sorted((helper.PROJECT_DIR / "modules").iterdir()):
        if _examples_dir(module_dir.name).is_dir() and _reference_page(module_dir.name).exists():
            modules.append(module_dir.name)

    return modules


def _render(module: str) -> str:
    blocks = [_EXAMPLES_HEADING, ""]
    for example in sorted(_examples_dir(module).glob("*.lua")):
        blocks.append(f"### `{example.name}`")
        blocks.append("")
        blocks.append("```lua")
        blocks.append(example.read_text().strip())
        blocks.append("```")
        blocks.append("")

    return "\n".join(blocks)


def _rewrite(module: str) -> str | None:
    page = _reference_page(module)
    text = page.read_text()

    start = text.find(f"\n{_EXAMPLES_HEADING}\n")
    if start < 0:
        raise SystemExit(f"{page} has no '{_EXAMPLES_HEADING}' section to fill")

    start += 1
    following = re.search(r"^## (?!Examples)", text[start:], re.M)
    end = start + following.start() if following else len(text)

    updated = text[:start] + _render(module) + text[end:]
    return updated if updated != text else None


def sync(args: Namespace) -> None:
    stale = []
    for module in _documented_modules():
        updated = _rewrite(module)
        if updated is None:
            continue

        stale.append(module)
        if not args.check:
            _reference_page(module).write_text(updated)

    if args.check and stale:
        print("the reference pages no longer match the examples on disk: " + ", ".join(stale))
        print("run: python3 varn.py docs")
        sys.exit(1)

    if not stale:
        print("every reference page already inlines its examples verbatim")
        return

    print(f"updated {len(stale)} reference page(s): {', '.join(stale)}")
