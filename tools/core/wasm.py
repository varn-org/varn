from __future__ import annotations

from argparse import Namespace

from . import helper


def _build_dir(args: Namespace) -> str:
    return args.build_dir or "build/wasm"


def build(args: Namespace) -> None:
    build_dir = _build_dir(args)

    command = ["emcmake", "cmake", "-B", build_dir, "-S", ".", f"-DCMAKE_BUILD_TYPE={args.config}"]
    if args.zip is not None:
        command.append(f"-DVARN_ENABLE_ZIP={args.zip}")
    helper.run(command)

    helper.run(["cmake", "--build", build_dir, "--config", args.config, "-j", str(helper.jobs())])


# builds varn_wasm then drives the vite project where "build" bundles dist/ and "dev" serves it
def _vite(args: Namespace, script: str) -> None:
    build(args)

    app_dir = helper.PROJECT_DIR / "apps" / "wasm"
    wasm_dir = (helper.PROJECT_DIR / _build_dir(args) / "bin").resolve()
    env = helper.environ_with(VARN_WASM_DIR=str(wasm_dir))

    helper.run(["npm", "install"], cwd=app_dir, env=env)
    helper.run(["npm", "run", script], cwd=app_dir, env=env)


def app(args: Namespace) -> None:
    _vite(args, "build")


def serve(args: Namespace) -> None:
    _vite(args, "dev")
