from __future__ import annotations

import datetime
import shutil
import subprocess
import tempfile
from argparse import Namespace
from pathlib import Path

from . import helper, wasm


def deploy(args: Namespace) -> None:
    # build a fresh production bundle (the wasm engine plus the vite app) into apps/wasm/dist
    wasm.app(args)

    dist = helper.PROJECT_DIR / "apps" / "wasm" / "dist"
    if not (dist / "index.html").exists():
        raise SystemExit(f"no built site found at {dist}")

    work = Path(tempfile.mkdtemp(prefix="varn-website-"))
    print(f"staging the production site in {work}")

    # start from the published repo so history is kept, falling back to a fresh repo when it is empty
    cloned = (
        subprocess.run(["git", "clone", "--depth", "1", "--branch", args.branch, args.remote, str(work)]).returncode == 0
    )
    if not cloned:
        helper.run(["git", "init", "-b", args.branch], cwd=work)
        helper.run(["git", "remote", "add", "origin", args.remote], cwd=work)

    # replace the tracked content with the fresh build, keeping the .git directory
    for entry in work.iterdir():
        if entry.name == ".git":
            continue
        if entry.is_dir():
            shutil.rmtree(entry)
        else:
            entry.unlink()

    shutil.copytree(dist, work, dirs_exist_ok=True)
    (work / ".nojekyll").write_text("")

    helper.run(["git", "add", "-A"], cwd=work)

    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=str(work)).returncode == 0:
        print("the published site already matches this build; nothing to do")
        return

    head = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=str(helper.PROJECT_DIR),
        capture_output=True,
        text=True,
    ).stdout.strip()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    message = args.message or f"deploy site from {head or 'local'} ({stamp})"
    helper.run(["git", "commit", "-m", message], cwd=work)

    if args.dry_run:
        print(f"dry run: committed in {work} but not pushed (drop --dry-run to publish)")
        return

    helper.run(["git", "push", "origin", f"HEAD:{args.branch}"], cwd=work)
    print(f"published to {args.remote} ({args.branch})")
