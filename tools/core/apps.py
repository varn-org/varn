from __future__ import annotations

import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
from argparse import Namespace
from pathlib import Path

from . import helper

RELEASES = "https://github.com/varn-org/varn/releases"

APPS = helper.PROJECT_DIR / "apps"
ANDROID_LIBS = APPS / "android" / "app" / "libs"
IOS_FRAMEWORKS = APPS / "ios" / "Frameworks"

# the apps carry no version of their own, so both read the one the fetched engine reports
VERSION_FILE = APPS / "varn-version.txt"
IOS_XCCONFIG = APPS / "ios" / "Version.xcconfig"


def _download(url: str, target: Path) -> None:
    print(f"> fetching {url}")
    try:
        urllib.request.urlretrieve(url, target)
    except Exception as error:
        raise SystemExit(f"could not download {url}: {error}")


def _asset_url(version: str, name: str) -> str:
    tag = "latest/download" if version == "latest" else f"download/{version}"
    return f"{RELEASES}/{tag}/{name}"


# each archive holds one directory, so the wanted member is lifted out of it into the destination
def _extract(archive: Path, member_suffix: str, destination: Path) -> Path:
    with tempfile.TemporaryDirectory() as scratch:
        with tarfile.open(archive) as tf:
            tf.extractall(scratch)

        found = next((p for p in Path(scratch).rglob(f"*{member_suffix}")), None)
        if found is None:
            raise SystemExit(f"{archive.name} did not contain a {member_suffix}")

        destination.mkdir(parents=True, exist_ok=True)
        target = destination / found.name
        if target.exists():
            shutil.rmtree(target) if target.is_dir() else target.unlink()

        shutil.move(str(found), str(target))
        return target


# the latest release redirects to its own tag, which resolves the name without spending api quota
def _resolve_tag(version: str) -> str:
    if version != "latest":
        return version

    with urllib.request.urlopen(f"{RELEASES}/latest") as response:
        return response.url.rstrip("/").rsplit("/", 1)[-1]


# the framework states the engine version in its plist, which is the same number the binary reports
def _version_from_xcframework(xcframework: Path) -> str:
    plist = next(xcframework.glob("*/*.framework/Info.plist"), None)
    if plist is None:
        raise SystemExit(f"{xcframework.name} has no framework plist to read the version from")

    text = subprocess.run(
        ["plutil", "-extract", "CFBundleShortVersionString", "raw", "-o", "-", str(plist)],
        capture_output=True,
        text=True,
        check=True,
    )
    return text.stdout.strip()


def _write_version(version: str) -> None:
    VERSION_FILE.write_text(version + "\n")
    IOS_XCCONFIG.write_text(f"MARKETING_VERSION = {version}\n")
    print(f"  engine version {version}")


def fetch(args: Namespace) -> None:
    """Download the released native artifacts the mobile apps link against."""
    wanted = {"android", "ios"} if args.platform == "all" else {args.platform}
    version = None

    with tempfile.TemporaryDirectory() as scratch:
        if "android" in wanted:
            archive = Path(scratch) / "varn-android-aar.tar.gz"
            _download(_asset_url(args.version, "varn-android-aar.tar.gz"), archive)
            placed = _extract(archive, ".aar", ANDROID_LIBS)
            print(f"  {placed.relative_to(helper.PROJECT_DIR)}")

        if "ios" in wanted:
            archive = Path(scratch) / "varn-apple-xcframework.tar.gz"
            _download(_asset_url(args.version, "varn-apple-xcframework.tar.gz"), archive)
            placed = _extract(archive, ".xcframework", IOS_FRAMEWORKS)
            print(f"  {placed.relative_to(helper.PROJECT_DIR)}")

            # the artifact states it exactly, so nothing has to be inferred from the tag it shipped under
            version = _version_from_xcframework(placed)

    # only an android-only fetch has no artifact to read the version out of, so the tag names it there
    _write_version(version or _resolve_tag(args.version).lstrip("v"))
