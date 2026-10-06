from __future__ import annotations

from argparse import Namespace

from . import helper


def build(args: Namespace) -> None:
    build_dir = helper.PROJECT_DIR / (args.build_dir or "build/lib")

    command = [
        "cmake", "-S", ".", "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={args.config}",
        "-DVARN_TARGET=lib",
    ]

    # The user may pin the install prefix at configure time so a later `--install` lands there.
    if args.prefix:
        command.append(f"-DCMAKE_INSTALL_PREFIX={args.prefix}")

    helper.run(command)
    helper.run(["cmake", "--build", str(build_dir), "--config", args.config, "-j", str(helper.jobs())])

    # Only the `varn` component is installed so the prefix holds just the library, the header and the CMake package.
    if args.install:
        helper.run(["cmake", "--install", str(build_dir), "--config", args.config, "--component", "varn"])
