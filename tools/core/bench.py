from __future__ import annotations

from argparse import Namespace

from . import helper


def run(args: Namespace) -> None:
    # mysql + redis behind varn, node and python, all on one docker network.
    # the bench service builds varn for linux and drives /plaintext, /json, /db (mysql) and /cache (redis) with wrk, printing the comparison table at the end so the result is reproducible with one command.
    bench_dir = helper.PROJECT_DIR / "bench"
    env = helper.environ_with(
        CONNECTIONS=str(args.connections),
        DURATION=args.duration,
        WORKERS=str(args.workers),
        JOBS=str(args.jobs),
        POOL_SIZE=str(args.pool_size),
    )

    helper.run(
        ["docker", "compose", "run", "--rm", "--build", "bench"],
        cwd=bench_dir,
        env=env,
    )
