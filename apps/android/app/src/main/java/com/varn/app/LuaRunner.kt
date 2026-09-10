package com.varn.app

import android.os.Handler
import android.os.Looper
import com.varn.VarnRuntime
import java.io.File

// what one run produced, with the output already separated from the failure that ended it
data class RunOutcome(val output: String, val failed: Boolean)

/**
 * Runs a Lua chunk on the embedded engine, driven by the app's own looper.
 *
 * The engine is never given a thread of its own. The chunk is loaded and then advanced one tick at a
 * time on the main looper, so everything the script does — a `print`, a host call, a coroutine
 * resuming after the network answered — happens on the thread that owns the interface, as it happens,
 * rather than after the script has finished.
 */
class LuaRunner(private val workDir: File) {

    private val handler = Handler(Looper.getMainLooper())
    private var runtime: VarnRuntime? = null
    private var onLine: ((String, Boolean) -> Unit)? = null
    private var onFinished: ((RunOutcome) -> Unit)? = null
    private val captured = StringBuilder()
    private var sawError = false

    fun version(): String = VarnRuntime.version()

    val isRunning: Boolean
        get() = runtime != null

    /** Starts a chunk and returns at once, reporting each line through [onLine] as the script produces it. */
    fun start(source: String, onLine: (String, Boolean) -> Unit, onFinished: (RunOutcome) -> Unit) {
        stop()

        val scriptFile = File(workDir, "sample.lua")
        val scratch = File(workDir, "scratch")
        scriptFile.writeText(source)
        scratch.deleteRecursively()
        scratch.mkdirs()

        this.onLine = onLine
        this.onFinished = onFinished
        captured.setLength(0)
        sawError = false

        val engine = VarnRuntime()
        runtime = engine

        // The engine writes every line to logcat already, and this mirrors it into the app's own console.
        VarnRuntime.setConsole { level, message ->
            handler.post { report(message, level >= WARN_LEVEL) }
        }

        val code = engine.loadString(harness(scriptFile, scratch), "=harness")
        if (code != 0) {
            report("the engine rejected the chunk with code $code", true)
            finish()
            return
        }

        handler.post(::tick)
    }

    /** Asks the engine to unwind, which is how a long loop is cut short. */
    fun stop() {
        runtime?.stop()
        if (runtime != null) {
            finish()
        }
    }

    private fun tick() {
        val engine = runtime ?: return
        if (engine.poll()) {
            handler.post(::tick)
            return
        }

        finish()
    }

    private fun report(line: String, isError: Boolean) {
        if (isError) {
            sawError = true
        }

        captured.append(line).append('\n')
        onLine?.invoke(line, isError)
    }

    private fun finish() {
        handler.removeCallbacksAndMessages(null)
        VarnRuntime.setConsole(null)

        runtime?.close()
        runtime = null

        val finished = onFinished
        val outcome = RunOutcome(captured.toString(), sawError)
        onLine = null
        onFinished = null
        finished?.invoke(outcome)
    }

    // the paths come from the app's own cache directory, so they carry nothing that could close the lua string
    private fun harness(script: File, scratch: File): String = """
        local log = require("log")

        local function report(message)
            log.error(tostring(message))
        end

        -- A sandboxed app has no writable working directory, so the one place a sample may write is named here.
        SAMPLE_DIR = [[${scratch.absolutePath}]]

        -- An error inside a background coroutine never reaches a pcall around the chunk, so each entry point reports its own.
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

    private companion object {
        // A warning and an error share the tint, since both mean the sample did not do what it set out to.
        const val WARN_LEVEL = 2
    }
}
