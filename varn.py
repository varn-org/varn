from __future__ import annotations

import argparse

from tools.core import android, apple, apps, bench, cxx, general, lib, site, tests, wasm


def _common(
    parser: argparse.ArgumentParser, default_build_dir: str | None = None
) -> None:
    parser.add_argument(
        "--build-dir", default=default_build_dir, help="build directory"
    )

    parser.add_argument(
        "--config", default="Release", help="CMake build type (default: Release)"
    )


def _opt_build(parser: argparse.ArgumentParser) -> None:
    _common(parser, "build")
    parser.add_argument("--arch", help="CMAKE_OSX_ARCHITECTURES override (e.g. x86_64)")
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="extra -D cache entry forwarded to cmake (repeatable)",
    )


def _opt_lib(parser: argparse.ArgumentParser) -> None:
    _common(parser, "build/lib")
    parser.add_argument("--prefix", help="CMAKE_INSTALL_PREFIX for a later --install")
    parser.add_argument(
        "--install",
        action="store_true",
        help="install the varn component into the prefix after building",
    )


def _opt_android(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument(
        "--api", type=int, default=24, help="Android min SDK (default: 24)"
    )


def _opt_wasm(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument("--zip", help="ON or OFF")


def _opt_test(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build", help="build directory holding bin/varn (default: build)"
    )


def _opt_test_cpp(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build/test", help="build directory for the C++ tests (default: build/test)"
    )
    parser.add_argument(
        "--config", default="Release", help="CMake build type (default: Release)"
    )
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="extra -D cache entry forwarded to cmake (repeatable)",
    )


def _opt_clean(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir", default="build", help="build directory (default: build)"
    )


def _opt_bench(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--connections", type=int, default=256, help="concurrent connections (default: 256)")
    parser.add_argument("--duration", default="10s", help="wrk duration per route (default: 10s)")
    parser.add_argument("--workers", type=int, default=1, help="worker processes per runtime (default: 1)")
    parser.add_argument("--jobs", type=int, default=4, help="parallel build jobs for varn (default: 4)")
    parser.add_argument("--pool-size", type=int, default=64, help="db connection pool size (default: 64)")


def _opt_site(parser: argparse.ArgumentParser) -> None:
    _common(parser)
    parser.add_argument("--zip", help="ON or OFF")
    parser.add_argument(
        "--remote",
        default="git@github.com:varn-org/website.git",
        help="target git remote (default: the varn-org/website repo over ssh)",
    )
    parser.add_argument("--branch", default="main", help="target branch (default: main)")
    parser.add_argument("--message", help="commit message (default: auto from source commit)")
    parser.add_argument("--dry-run", action="store_true", help="build and commit locally without pushing")


# task -> (handler, options configurator, one-line help shown in --help)
def _opt_fetch_native(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--version",
        default="latest",
        help="release tag to fetch, or 'latest' (default: latest)",
    )
    parser.add_argument(
        "--platform",
        choices=["all", "android", "ios"],
        default="all",
        help="which artifact to fetch (default: all)",
    )


_TASKS = {
    "build": (
        cxx.build,
        _opt_build,
        "Build the native varn executable for the current desktop OS.",
    ),
    "test": (
        tests.run,
        _opt_test,
        "Run the Lua test suite against the built varn binary.",
    ),
    "test-cpp": (
        tests.run_cpp,
        _opt_test_cpp,
        "Build and run the native C++ test target (googletest).",
    ),
    "bench": (
        bench.run,
        _opt_bench,
        "Run the Varn vs Node vs Python benchmark (plaintext, json, MySQL, Redis) in Docker.",
    ),
    "apple": (apple.build, _common, "Build varn.xcframework for all Apple slices."),
    "lib": (
        lib.build,
        _opt_lib,
        "Build the embeddable varn shared library (find_package(varn) package); add --install to place the varn component into --prefix.",
    ),
    "android": (
        android.build,
        _opt_android,
        "Build the Android .aar (libvarn.so for every ABI).",
    ),
    "wasm": (wasm.build, _opt_wasm, "Build the varn_wasm target with Emscripten."),
    "app-wasm": (
        wasm.app,
        _opt_wasm,
        "Build wasm, then bundle the browser app into apps/wasm/dist.",
    ),
    "serve": (
        wasm.serve,
        _opt_wasm,
        "Build wasm, then start the Vite dev server for apps/wasm.",
    ),
    "site-deploy": (
        site.deploy,
        _opt_site,
        "Build the production wasm site and publish it to the varn-org/website repo.",
    ),
    "format": (
        general.fmt,
        None,
        "Run clang-format over the modules/ and src/ sources.",
    ),
    "clean": (general.clean, _opt_clean, "Remove the build directory."),
    "zip": (general.zip, None, "Create a source archive beside the repo."),
    "fetch-native": (
        apps.fetch,
        _opt_fetch_native,
        "Download the released aar and xcframework the mobile apps link against.",
    ),
}


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="varn", description=__doc__)
    tasks = parser.add_subparsers(dest="task", metavar="task", required=True)

    for name, (_, configure, help_text) in _TASKS.items():
        task = tasks.add_parser(name, help=help_text, description=help_text)
        if configure:
            configure(task)

    return parser


def main() -> None:
    args = _build_parser().parse_args()
    _TASKS[args.task][0](args)


if __name__ == "__main__":
    main()
