"""Build, test and publish Varn on every target from one command."""

from __future__ import annotations

import argparse
import os

from tools.core import android, apple, apps, bench, cxx, general, helper, lib, site, tests, wasm


def _common(
    parser: argparse.ArgumentParser, default_build_dir: str | None = None
) -> None:
    parser.add_argument(
        "--build-dir", default=default_build_dir, help="Build directory."
    )

    parser.add_argument(
        "--config", default="Release", help="CMake build type (default: \"Release\")."
    )


def _opt_build(parser: argparse.ArgumentParser) -> None:
    _common(parser, "build")
    parser.add_argument("--arch", help="Override for \"CMAKE_OSX_ARCHITECTURES\" (for example \"x86_64\").")
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="Extra \"-D\" cache entry forwarded to \"cmake\" (repeatable).",
    )


def _opt_lib(parser: argparse.ArgumentParser) -> None:
    _common(parser, "build/lib")
    parser.add_argument("--prefix", help="Value of \"CMAKE_INSTALL_PREFIX\" for a later \"--install\".")
    parser.add_argument(
        "--install",
        action="store_true",
        help="Install the \"varn\" component into the prefix after building.",
    )


def _opt_android(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument(
        "--api", type=int, default=24, help="Android min SDK (default: 24)."
    )


def _opt_wasm(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument("--zip", help="Either \"ON\" or \"OFF\".")


def _opt_apple(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--family", help="Build only the slices of this family, such as \"ios\", without assembling the XCFramework.")
    selection.add_argument(
        "--assemble",
        action="store_true",
        help="Assemble \"varn.xcframework\" from the slices that the families already built.",
    )


def _opt_test(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build", help="Build directory holding \"bin/varn\" (default: \"build\")."
    )
    parser.add_argument(
        "--timeout",
        type=_positive,
        default=300,
        help="Seconds one Lua test may run before it fails (default: 300).",
    )


def _opt_test_cpp(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build/test", help="Build directory for the C++ tests (default: \"build/test\")."
    )
    parser.add_argument(
        "--config", default="Release", help="CMake build type (default: \"Release\")."
    )
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="Extra \"-D\" cache entry forwarded to \"cmake\" (repeatable).",
    )


def _opt_clean(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build", help="Build directory (default: \"build\")."
    )


def _positive(text: str) -> int:
    value = int(text)
    if value < 1:
        raise argparse.ArgumentTypeError(f"The value \"{text}\" must be a whole number of at least 1.")
    return value


def _opt_bench(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--connections", type=int, default=256, help="Concurrent connections (default: 256).")
    parser.add_argument("--duration", default="10s", help="Duration of each \"wrk\" route run (default: \"10s\").")
    parser.add_argument("--workers", type=int, default=1, help="Worker processes per runtime (default: 1).")
    parser.add_argument("--pool-size", type=int, default=64, help="Database connection pool size (default: 64).")


def _opt_site(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument("--zip", help="Either \"ON\" or \"OFF\".")
    parser.add_argument(
        "--remote",
        default="git@github.com:varn-org/website.git",
        help="Target Git remote (default: the \"varn-org/website\" repository over SSH).",
    )
    parser.add_argument("--branch", default="main", help="Target branch (default: \"main\").")
    parser.add_argument("--message", help="Commit message (default: derived from the source commit).")
    parser.add_argument("--dry-run", action="store_true", help="Build and commit locally without pushing.")


def _opt_fetch_native(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--version",
        default="latest",
        help="Release tag to fetch, or \"latest\" (default: \"latest\").",
    )
    parser.add_argument(
        "--platform",
        choices=["all", "android", "ios"],
        default="all",
        help="Which artifact to fetch (default: \"all\").",
    )


# Each task maps to its handler, its options configurator and the one-line help shown by `--help`.
_TASKS = {
    "build": (
        cxx.build,
        _opt_build,
        "Build the native \"varn\" executable for the current desktop OS.",
    ),
    "test": (
        tests.run,
        _opt_test,
        "Run the Lua test suite against the built \"varn\" binary.",
    ),
    "test-cpp": (
        tests.run_cpp,
        _opt_test_cpp,
        "Build and run the native C++ test target (googletest).",
    ),
    "bench": (
        bench.run,
        _opt_bench,
        "Run the Varn vs Node vs Python benchmark (plaintext, JSON, MySQL, Redis) in Docker.",
    ),
    "apple": (apple.build, _opt_apple, "Build \"varn.xcframework\" for all Apple slices."),
    "lib": (
        lib.build,
        _opt_lib,
        "Build the embeddable Varn shared library as a \"find_package(varn)\" package. Add \"--install\" to place the \"varn\" component into \"--prefix\".",
    ),
    "android": (
        android.build,
        _opt_android,
        "Build the Android AAR (\"libvarn.so\" for every ABI).",
    ),
    "wasm": (wasm.build, _opt_wasm, "Build the \"varn-wasm\" target with Emscripten."),
    "app-wasm": (
        wasm.app,
        _opt_wasm,
        "Build the wasm engine, then bundle the browser app into \"apps/wasm/dist\".",
    ),
    "serve": (
        wasm.serve,
        _opt_wasm,
        "Build the wasm engine, then start the Vite dev server for \"apps/wasm\".",
    ),
    "site-deploy": (
        site.deploy,
        _opt_site,
        "Build the production wasm site and publish it to the \"varn-org/website\" repository.",
    ),
    "format": (
        general.fmt,
        None,
        "Run \"clang-format\" over the \"modules/\" and \"src/\" sources.",
    ),
    "clean": (general.clean, _opt_clean, "Remove the build directory."),
    "zip": (general.zip, None, "Create a source archive beside the repo."),
    "fetch-native": (
        apps.fetch,
        _opt_fetch_native,
        "Download the released AAR and XCFramework the mobile apps link against.",
    ),
}


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="varn", description=__doc__)
    tasks = parser.add_subparsers(dest="task", metavar="task", required=True)

    for name, (_, configure, help_text) in _TASKS.items():
        task = tasks.add_parser(name, help=help_text, description=help_text)
        task.add_argument(
            "--jobs",
            type=_positive,
            default=helper.DEFAULT_JOBS,
            help=f"Parallel build jobs (default: {helper.DEFAULT_JOBS}).",
        )
        if configure:
            configure(task)

    return parser


def main() -> None:
    args = _build_parser().parse_args()

    # A build that a dependency runs by itself, such as the one of OpenSSL, keeps to the same job count as the build around it.
    os.environ["CMAKE_BUILD_PARALLEL_LEVEL"] = str(args.jobs)
    _TASKS[args.task][0](args)


if __name__ == "__main__":
    main()
