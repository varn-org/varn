import json
import sys


class CompilerCacheReport:
    """Reads the statistics that "sccache --show-stats --stats-format json" writes and prints a short report of the job in Markdown."""

    @staticmethod
    def main() -> None:
        stats = json.load(sys.stdin)["stats"]
        hits = sum(stats["cache_hits"]["counts"].values())
        misses = sum(stats["cache_misses"]["counts"].values())
        failures = stats["compile_fails"]
        compiled = misses - failures
        uncacheable = stats["requests_not_cacheable"]
        cacheable = hits + compiled
        rate = 100 * hits / cacheable if cacheable else 0.0
        reading = CompilerCacheReport.seconds(stats["cache_read_hit_duration"])
        compiling = CompilerCacheReport.seconds(stats["compiler_write_duration"])

        print("### Compiler cache\n")
        print(f"- {hits} of {cacheable} compiles came from the cache ({rate:.2f} %), and {compiled} compiled because the cache held no object for them.")
        print(f"- Reading the objects from the cache took {reading:.0f} seconds in all, {reading / hits if hits else 0:.3f} seconds each, and the compiles that missed took {compiling:.0f} seconds.")
        if failures:
            print(f"- {failures} compiles failed, which are the checks of the configure steps that probe the compiler.")
        if uncacheable:
            reasons = ", ".join(f'"{reason}" {count}' for reason, count in sorted(stats["not_cached"].items(), key=lambda item: -item[1]))
            print(f"- {uncacheable} calls could not use the cache: {reasons}.")

    @staticmethod
    def seconds(duration: dict) -> float:
        return duration["secs"] + duration["nanos"] / 1e9


if __name__ == "__main__":
    CompilerCacheReport.main()
