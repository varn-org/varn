from __future__ import annotations

import os
from argparse import Namespace

from . import helper


def build(args: Namespace) -> None:
    android_dir = helper.PROJECT_DIR / "android"

    # Gradle drives the per-ABI native build (`externalNativeBuild` plus `abiFilters`) and packs the AAR.
    gradlew = android_dir / ("gradlew.bat" if os.name == "nt" else "gradlew")
    launcher = [str(gradlew)] if gradlew.exists() else ["gradle"]

    variant = "assembleDebug" if args.config.lower() == "debug" else "assembleRelease"

    helper.run(
        launcher + [f":varn:{variant}", f"-PvarnMinSdk={args.api}", f"--max-workers={helper.jobs()}"],
        cwd=android_dir,
    )
    print("The AAR is written under \"android/varn/build/outputs/aar/\".")
