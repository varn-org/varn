from __future__ import annotations

import os
from argparse import Namespace

from . import helper


def build(args: Namespace) -> None:
    android_dir = helper.PROJECT_DIR / "android"

    # gradle drives the per-abi native build (externalNativeBuild + abiFilters) and packs the aar
    gradlew = android_dir / ("gradlew.bat" if os.name == "nt" else "gradlew")
    launcher = [str(gradlew)] if gradlew.exists() else ["gradle"]

    variant = "assembleDebug" if args.config.lower() == "debug" else "assembleRelease"

    helper.run(
        launcher + [f":varn:{variant}", f"-PvarnMinSdk={args.api}", f"--max-workers={helper.jobs()}"],
        cwd=android_dir,
    )
    print("aar written under android/varn/build/outputs/aar/")
