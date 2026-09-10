from __future__ import annotations

from argparse import Namespace

from . import helper


def build(args: Namespace) -> None:
    build_dir = args.build_dir or "build"

    command = ["cmake", "-B", build_dir, "-S", ".", f"-DCMAKE_BUILD_TYPE={args.config}"]
    if args.arch:
        command.append(f"-DCMAKE_OSX_ARCHITECTURES={args.arch}")
    for define in args.define:
        command.append(f"-D{define}")

    helper.run(command)
    helper.run(["cmake", "--build", build_dir, "--config", args.config, "-j", str(helper.jobs())])
