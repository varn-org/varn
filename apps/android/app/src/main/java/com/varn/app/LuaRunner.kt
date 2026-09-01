package com.varn.app

import com.varn.VarnRuntime
import java.io.File

// what one run produced, with the output already separated from the failure that ended it
data class RunOutcome(val output: String, val failed: Boolean)

/**
 * Runs a Lua chunk on the embedded engine and collects what it printed.
 *
 * The runtime the AAR exposes answers with an exit code and nothing else, so the chunk is wrapped in
 * a harness that replaces `print` with one writing to a file the app reads back afterwards.
 */
class LuaRunner(private val workDir: File) {

    @Volatile
    private var current: VarnRuntime? = null

    fun version(): String = VarnRuntime.version()

    fun run(source: String): RunOutcome {
        val scriptFile = File(workDir, "sample.lua")
        val outputFile = File(workDir, "output.txt")
        val scratch = File(workDir, "scratch")

        scriptFile.writeText(source)
        outputFile.writeText("")
        scratch.deleteRecursively()
        scratch.mkdirs()

        val runtime = VarnRuntime()
        current = runtime

        val code = try {
            runtime.runString(harness(scriptFile, outputFile, scratch), "=harness")
        } finally {
            current = null
            runtime.close()
        }

        val captured = if (outputFile.exists()) outputFile.readText() else ""
        // the harness reports a lua error through the output, so a non-zero code means the harness itself failed
        return RunOutcome(
            output = captured.ifEmpty { if (code == 0) "" else "engine exited with code $code\n" },
            failed = code != 0 || captured.contains("\nerror: ") || captured.startsWith("error: "),
        )
    }

    // asks the engine to unwind the running chunk, which is how a long loop is cut short
    fun stop() {
        current?.stop()
    }

    // the paths come from the app's own cache directory, so they carry nothing that could close the lua string
    private fun harness(script: File, output: File, scratch: File): String = """
        local out = assert(io.open([[${output.absolutePath}]], "w"))

        local function report(message)
            out:write("error: ", tostring(message), "\n")
            out:flush()
        end

        print = function(...)
            local parts = {}
            for i = 1, select("#", ...) do
                parts[i] = tostring((select(i, ...)))
            end
            out:write(table.concat(parts, "\t"), "\n")
            out:flush()
        end

        -- a sandboxed app has no writable working directory, so the one place a sample may write is named here
        SAMPLE_DIR = [[${scratch.absolutePath}]]

        -- an error inside a background coroutine never reaches the pcall below, so each entry point reports its own
        local async = require("async")
        local realRun, realSpawn = async.run, async.spawn

        async.run = function(fn)
            return realRun(function()
                local ok, err = pcall(fn)
                if not ok then
                    report(err)
                end
            end)
        end

        async.spawn = function(fn)
            return realSpawn(function()
                local ok, err = pcall(fn)
                if not ok then
                    report(err)
                end
            end)
        end

        local file = assert(io.open([[${script.absolutePath}]], "r"))
        local source = file:read("a")
        file:close()

        local chunk, loadError = load(source, "=sample")
        if not chunk then
            report(loadError)
        else
            local ok, runError = pcall(chunk)
            if not ok then
                report(runError)
            end
        end
    """.trimIndent()
}
