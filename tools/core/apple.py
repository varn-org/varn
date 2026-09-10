from __future__ import annotations

import shutil
from argparse import Namespace
from pathlib import Path

from . import helper
from .targets import apple as targets

_TOOLCHAIN = "cmake/ios/ios.toolchain.cmake"


def _slice_dir(out_root: Path, sl: dict) -> Path:
    return out_root / "slices" / sl["group"] / sl["platform"]


def _framework_binary(framework_dir: Path) -> Path:
    # macOS frameworks are versioned (Versions/A/Varn), unlike the flat layout elsewhere
    versioned = framework_dir / "Versions" / "A" / "varn"
    return versioned if versioned.exists() else framework_dir / "varn"


def _build_slice(args: Namespace, sl: dict, out_root: Path) -> Path:
    build_dir = _slice_dir(out_root, sl)

    command = [
        "cmake", "-S", ".", "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={args.config}",
        f"-DCMAKE_TOOLCHAIN_FILE={_TOOLCHAIN}",
        f"-DPLATFORM={sl['platform']}",
        f"-DDEPLOYMENT_TARGET={sl['deployment_target']}",
        "-DVARN_TARGET=apple",
    ]

    # pin the arch when the platform default is a fat or legacy set (watchos device)
    if "archs" in sl:
        command.append(f"-DARCHS={sl['archs']}")

    helper.run(command)
    helper.run(["cmake", "--build", str(build_dir), "--config", args.config, "-j", str(helper.jobs())])

    return build_dir / "lib" / "varn.framework"


# clang reads a framework module only from Modules/module.modulemap, which a cached build tree and the versioned
# layout each break in their own way: the tree keeps a map that was renamed, and cmake links Headers and Resources
# into the framework root but never Modules
def _normalize_modules(framework: Path) -> None:
    for stale in framework.rglob("*.modulemap"):
        if stale.name != "module.modulemap":
            stale.unlink()

    versioned = framework / "Versions" / "A" / "Modules"
    root = framework / "Modules"
    if versioned.is_dir() and not root.exists():
        root.symlink_to(Path("Versions") / "Current" / "Modules")


def _fuse_group(group: str, frameworks: list[Path], groups_root: Path) -> Path:
    group_framework = groups_root / group / "varn.framework"
    if group_framework.parent.exists():
        shutil.rmtree(group_framework.parent)
    group_framework.parent.mkdir(parents=True, exist_ok=True)

    # take the first arch as the base, then lipo every arch binary into it
    shutil.copytree(frameworks[0], group_framework, symlinks=True)
    _normalize_modules(group_framework)
    if len(frameworks) > 1:
        binaries = [str(_framework_binary(fw)) for fw in frameworks]
        helper.run(["lipo", "-create", "-output", str(_framework_binary(group_framework)), *binaries])

    return group_framework


def build(args: Namespace) -> None:
    out_root = helper.PROJECT_DIR / (args.build_dir or "build/apple")

    # build every slice, grouped by xcframework library
    grouped: dict[str, list[Path]] = {}
    for sl in targets.slices:
        framework = _build_slice(args, sl, out_root)
        grouped.setdefault(sl["group"], []).append(framework)

    # fuse same-group archs, then bundle one framework per group into the xcframework
    groups_root = out_root / "groups"
    framework_args: list[str] = []
    for group, frameworks in grouped.items():
        fused = _fuse_group(group, frameworks, groups_root)
        framework_args += ["-framework", str(fused)]

    xcframework = out_root / "varn.xcframework"
    if xcframework.exists():
        shutil.rmtree(xcframework)

    helper.run(["xcodebuild", "-create-xcframework", *framework_args, "-output", str(xcframework)])
    print(f"created {xcframework}")
