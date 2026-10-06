package com.varn

import java.util.concurrent.locks.ReentrantReadWriteLock
import kotlin.concurrent.read
import kotlin.concurrent.write

/**
 * A thin Kotlin wrapper over the native Varn runtime, whose pools start at most [taskThreads] and [ioThreads] threads, where zero keeps the default.
 *
 * The method [close] waits for the calls that other threads are making, so an [emit] from any thread never meets a freed runtime, and every call after it answers as the C API answers a null runtime.
 */
class VarnRuntime @JvmOverloads constructor(taskThreads: Int = 0, ioThreads: Int = 0) : AutoCloseable {
    private val guard = ReentrantReadWriteLock()
    private var handle: Long

    init {
        require(taskThreads >= 0 && ioThreads >= 0) { "A limit of threads is zero, which keeps the default, or more." }
        handle = nativeNew(taskThreads, ioThreads)
        check(handle != 0L) { "The runtime could not be created." }
    }

    /**
     * A native function Lua reaches through the global `host` table.
     *
     * The argument and the answer are JSON, and the call arrives on the thread that entered the runtime, so anything touching the user interface has to be posted to the main thread from here.
     */
    fun interface HostFunction {
        fun call(argument: String): String?
    }

    /** Receives every line the engine writes, at the level it was written with. */
    fun interface ConsoleSink {
        fun onLine(level: Int, message: String)
    }

    /** What one [pollBudget] did, which the runtime fills in place so a host that polls every frame allocates nothing. */
    class PollResult {
        /** Whether any job, timer or socket callback ran. */
        var ran: Boolean = false
            private set

        /** Whether anything can still make progress. */
        var pending: Boolean = false
            private set

        /** The milliseconds the host can sleep, as [idle] answers them. */
        var idleMilliseconds: Long = -1L
            private set
    }

    fun register(name: String, function: HostFunction): Int = withHandle { nativeRegister(it, name, function) }

    /**
     * Delivers an event to every Lua handler registered for [name] through `host.on`.
     *
     * It is safe from any thread: the call is posted to the event loop and Lua is only touched there.
     */
    fun emit(name: String, jsonArgument: String = "null"): Int = withHandle { nativeEmit(it, name, jsonArgument) }

    /** Keeps the event loop running, so the runtime waits for events instead of exiting once the script returns. */
    fun retain(): Int = withHandle { nativeRetain(it) }

    /** Gives back one retain, letting the loop finish when nothing else holds it. */
    fun release(): Int = withHandle { nativeRelease(it) }

    fun runFile(path: String): Int = withHandle { nativeRunFile(it, path) }

    fun runString(source: String, chunkName: String = "=(embedded)"): Int = withHandle { nativeRunString(it, source, chunkName) }

    /**
     * Runs a chunk and hands control straight back, leaving whatever it armed for [poll] to drive.
     *
     * This is what an application uses when its own run loop owns the thread, so every host call the script makes arrives on that thread rather than on one of the engine's.
     */
    fun loadFile(path: String): Int = withHandle { nativeLoadFile(it, path) }

    fun loadString(source: String, chunkName: String = "=(embedded)"): Int = withHandle { nativeLoadString(it, source, chunkName) }

    /** Advances the runtime once without blocking, answering true while something can still make progress. */
    fun poll(): Boolean = withHandle { nativePoll(it) == 1 }

    /**
     * Advances the runtime without blocking, repeating passes while they make progress and [budgetNanoseconds] lasts, so a frame bounds the time it gives the runtime.
     *
     * It answers true while something can still make progress, and fills [result] when one is given.
     */
    fun pollBudget(budgetNanoseconds: Long, result: PollResult? = null): Boolean {
        require(budgetNanoseconds >= 0) { "A budget is zero or more nanoseconds." }
        return withHandle { nativePollBudget(it, budgetNanoseconds, result) == 1 }
    }

    /**
     * Calls [wake] whenever work reaches the runtime from another thread, so a host that sleeps between polls knows to poll again.
     *
     * The wake runs on the thread that brought the work, so it only signals the thread that polls, such as through a `Handler`, and never calls [setWake] itself. Pass null to stop, which returns once no call of the previous wake still runs.
     */
    fun setWake(wake: Runnable?) = withHandle { nativeSetWake(it, wake) }

    /** Answers how many milliseconds the runtime can wait before it has work of its own: 0 while work is ready, the time to its next timer while one is armed, and -1 when only a wake can bring work. */
    fun idle(): Long = withHandle { nativeIdle(it) }

    /**
     * Gives the reference of [view] back on the loop thread once the work the loop runs now is done, so a host function answers with a view it made for the answer and lets it go before it returns.
     *
     * The view is closed by the call, while a closed runtime answers 2 and leaves the view open.
     */
    fun releaseDataview(view: VarnDataView): Int = withHandle {
        if (it == 0L) nativeReleaseDataview(0L, 0L) else nativeReleaseDataview(it, view.detach())
    }

    /** Asks the runtime to end, from any thread, so a [runFile] or [runString] that runs returns. */
    fun stop() = withHandle { nativeStop(it) }

    /** Stops the runtime, waits for the calls that other threads are making and frees it, and closing again does nothing. */
    override fun close() {
        check(guard.readHoldCount == 0) { "A runtime cannot be closed from inside one of its own calls." }

        guard.read { nativeStop(handle) }
        guard.write {
            if (handle != 0L) {
                nativeFree(handle)
                handle = 0L
            }
        }
    }

    private inline fun <T> withHandle(call: (Long) -> T): T = guard.read { call(handle) }

    companion object {
        init {
            System.loadLibrary("varn")
        }

        fun version(): String = nativeVersion()

        /**
         * Mirrors every line the engine writes into [sink], which already reached Logcat as well.
         *
         * Pass null to stop receiving. The sink is process wide, as the logger it mirrors is, and a line already on its way when the sink changes may still reach the previous one.
         */
        fun setConsole(sink: ConsoleSink?) = nativeSetConsole(sink)

        @JvmStatic private external fun nativeSetConsole(sink: ConsoleSink?)

        @JvmStatic private external fun nativeNew(taskThreads: Int, ioThreads: Int): Long
        @JvmStatic private external fun nativeRegister(handle: Long, name: String, function: HostFunction): Int
        @JvmStatic private external fun nativeEmit(handle: Long, name: String, jsonArgument: String): Int
        @JvmStatic private external fun nativeRetain(handle: Long): Int
        @JvmStatic private external fun nativeRelease(handle: Long): Int
        @JvmStatic private external fun nativeRunFile(handle: Long, path: String): Int
        @JvmStatic private external fun nativeRunString(handle: Long, source: String, chunkName: String): Int
        @JvmStatic private external fun nativeLoadFile(handle: Long, path: String): Int
        @JvmStatic private external fun nativeLoadString(handle: Long, source: String, chunkName: String): Int
        @JvmStatic private external fun nativePoll(handle: Long): Int
        @JvmStatic private external fun nativePollBudget(handle: Long, budgetNanoseconds: Long, result: PollResult?): Int
        @JvmStatic private external fun nativeSetWake(handle: Long, wake: Runnable?)
        @JvmStatic private external fun nativeIdle(handle: Long): Long
        @JvmStatic private external fun nativeReleaseDataview(handle: Long, view: Long): Int
        @JvmStatic private external fun nativeStop(handle: Long)
        @JvmStatic private external fun nativeFree(handle: Long)
        @JvmStatic private external fun nativeVersion(): String
    }
}
