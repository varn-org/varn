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
    # A macOS framework is versioned (`Versions/A/varn`), unlike the flat layout elsewhere.
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

    # The arch is pinned when the platform default is a fat or legacy set (the watchOS device).
    if "archs" in sl:
        command.append(f"-DARCHS={sl['archs']}")

    helper.run(command)
    helper.run(["cmake", "--build", str(build_dir), "--config", args.config, "-j", str(args.jobs)])

    return build_dir / "lib" / "varn.framework"


# Clang reads a framework module only from `Modules/module.modulemap`, and in the versioned layout CMake links `Headers` and `Resources` into the framework root but never `Modules`.
def _link_modules(framework: Path) -> None:
    versioned = framework / "Versions" / "A" / "Modules"
    root = framework / "Modules"
    if versioned.is_dir() and not root.exists():
        root.symlink_to(Path("Versions") / "Current" / "Modules")


def _fuse_group(group: str, frameworks: list[Path], groups_root: Path) -> Path:
    group_framework = groups_root / group / "varn.framework"
    if group_framework.parent.exists():
        shutil.rmtree(group_framework.parent)
    group_framework.parent.mkdir(parents=True, exist_ok=True)

    # The first arch is the base, then `lipo` merges every arch binary into it.
    shutil.copytree(frameworks[0], group_framework, symlinks=True)
    _link_modules(group_framework)
    if len(frameworks) > 1:
        binaries = [str(_framework_binary(fw)) for fw in frameworks]
        helper.run(["lipo", "-create", "-output", str(_framework_binary(group_framework)), *binaries])

    return group_framework


def _assemble(out_root: Path) -> None:
    # Archs of the same group are fused, then one framework per group is bundled into the XCFramework.
    grouped: dict[str, list[Path]] = {}
    for sl in targets.slices:
        framework = _slice_dir(out_root, sl) / "lib" / "varn.framework"
        if not framework.is_dir():
            print(f"The slice \"{sl['platform']}\" has no framework under \"{framework}\". Build every family first.")
            raise SystemExit(1)
        grouped.setdefault(sl["group"], []).append(framework)

    groups_root = out_root / "groups"
    framework_args: list[str] = []
    for group, frameworks in grouped.items():
        fused = _fuse_group(group, frameworks, groups_root)
        framework_args += ["-framework", str(fused)]

    xcframework = out_root / "varn.xcframework"
    if xcframework.exists():
        shutil.rmtree(xcframework)

    helper.run(["xcodebuild", "-create-xcframework", *framework_args, "-output", str(xcframework)])
    print(f"Created \"{xcframework}\".")


def build(args: Namespace) -> None:
    out_root = helper.PROJECT_DIR / (args.build_dir or "build/apple")

    if args.assemble:
        _assemble(out_root)
        return

    families = {sl["family"] for sl in targets.slices}
    if args.family is not None and args.family not in families:
        print(f"The family \"{args.family}\" names no Apple slices. Use one of \"{', '.join(sorted(families))}\".")
        raise SystemExit(1)

    for sl in targets.slices:
        if args.family is None or sl["family"] == args.family:
            _build_slice(args, sl, out_root)

    # A family builds its slices alone, and the job that has every family assembles them.
    if args.family is None:
        _assemble(out_root)
