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

    # let the user pin the install prefix at configure time so a later --install lands there.
    if args.prefix:
        command.append(f"-DCMAKE_INSTALL_PREFIX={args.prefix}")

    helper.run(command)
    helper.run(["cmake", "--build", str(build_dir), "--config", args.config, "-j", str(helper.jobs())])

    # install only the `varn` component so the prefix stays library, header and cmake package.
    if args.install:
        helper.run(["cmake", "--install", str(build_dir), "--config", args.config, "--component", "varn"])
