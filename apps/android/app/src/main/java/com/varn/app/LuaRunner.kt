package com.varn.app

import com.varn.VarnRuntime
import java.io.File
import org.json.JSONTokener

// what one run produced, with the output already separated from the failure that ended it
data class RunOutcome(val output: String, val failed: Boolean)

/**
 * Runs a Lua chunk on the embedded engine and reports each line it prints as it is printed.
 *
 * The runtime answers a run with an exit code and nothing else, so the chunk is wrapped in a harness
 * that replaces `print` with one handing every line to the host, which is what lets the console fill
 * while a sample that waits on the network is still running.
 */
class LuaRunner(private val workDir: File) {

    @Volatile
    private var current: VarnRuntime? = null

    fun version(): String = VarnRuntime.version()

    fun run(source: String, onLine: (String, Boolean) -> Unit): RunOutcome {
        val scriptFile = File(workDir, "sample.lua")
        val scratch = File(workDir, "scratch")

        scriptFile.writeText(source)
        scratch.deleteRecursively()
        scratch.mkdirs()

        val runtime = VarnRuntime()
        current = runtime

        val captured = StringBuilder()
        runtime.register("emit") { argument ->
            val line = decodeLine(argument)
            captured.append(line).append('\n')
            onLine(line, line.startsWith("error: "))
            null
        }

        val code = try {
            runtime.runString(harness(scriptFile, scratch), "=harness")
        } finally {
            current = null
            runtime.close()
        }

        val output = captured.toString()
        // the harness reports a lua error as an output line, so a non-zero code means the harness itself failed
        return RunOutcome(
            output = output.ifEmpty { if (code == 0) "" else "engine exited with code $code\n" },
            failed = code != 0 || output.contains("\nerror: ") || output.startsWith("error: "),
        )
    }

    /**
     * Turns the JSON the engine hands a host function back into the line the script printed.
     *
     * Everything crossing the bridge is JSON, so a printed line arrives quoted and with its tabs and
     * newlines escaped. A value that is not a JSON string is a bug in the harness rather than output,
     * so it is shown as it arrived instead of being hidden.
     */
    private fun decodeLine(argument: String): String =
        runCatching { JSONTokener(argument).nextValue() as? String }.getOrNull() ?: argument

    // asks the engine to unwind the running chunk, which is how a long loop is cut short
    fun stop() {
        current?.stop()
    }

    // the paths come from the app's own cache directory, so they carry nothing that could close the lua string
    private fun harness(script: File, scratch: File): String = """
        local function emit(line)
            host.emit(line)
        end

        local function report(message)
            emit("error: " .. tostring(message))
        end

        print = function(...)
            local parts = {}
            for i = 1, select("#", ...) do
                parts[i] = tostring((select(i, ...)))
            end
            emit(table.concat(parts, "\t"))
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
