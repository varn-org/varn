package com.varn

// thin kotlin wrapper over the native varn runtime whose close() releases the native handle
class VarnRuntime : AutoCloseable {
    private var handle: Long = nativeNew()

    /**
     * A native function Lua reaches through the global `host` table.
     *
     * The argument and the answer are json, and the call arrives on the thread that entered the runtime,
     * so anything touching the user interface has to be posted to the main thread from here.
     */
    fun interface HostFunction {
        fun call(argument: String): String?
    }

    fun register(name: String, function: HostFunction): Int = nativeRegister(handle, name, function)

    /**
     * Delivers an event to every Lua handler registered for [name] through `host.on`.
     *
     * It is safe from any thread: the call is posted to the event loop and Lua is only touched there.
     */
    fun emit(name: String, jsonArgument: String = "null"): Int = nativeEmit(handle, name, jsonArgument)

    /** Keeps the event loop running, so the runtime waits for events instead of exiting once the script returns. */
    fun retain(): Int = nativeRetain(handle)

    /** Gives back one retain, letting the loop finish when nothing else holds it. */
    fun release(): Int = nativeRelease(handle)

    fun runFile(path: String): Int = nativeRunFile(handle, path)

    fun runString(source: String, chunkName: String = "=(embedded)"): Int =
        nativeRunString(handle, source, chunkName)

    /**
     * Runs a chunk and hands control straight back, leaving whatever it armed for [poll] to drive.
     *
     * This is what an application uses when its own run loop owns the thread, so every host call the
     * script makes arrives on that thread rather than on one of the engine's.
     */
    fun loadFile(path: String): Int = nativeLoadFile(handle, path)

    fun loadString(source: String, chunkName: String = "=(embedded)"): Int =
        nativeLoadString(handle, source, chunkName)

    /** Advances the runtime once without blocking, answering true while something can still make progress. */
    fun poll(): Boolean = nativePoll(handle) == 1

    fun stop() = nativeStop(handle)

    override fun close() {
        if (handle != 0L) {
            nativeFree(handle)
            handle = 0L
        }
    }

    companion object {
        init {
            System.loadLibrary("varn")
        }

        fun version(): String = nativeVersion()

        @JvmStatic private external fun nativeNew(): Long
        @JvmStatic private external fun nativeRegister(handle: Long, name: String, function: HostFunction): Int
        @JvmStatic private external fun nativeEmit(handle: Long, name: String, jsonArgument: String): Int
        @JvmStatic private external fun nativeRetain(handle: Long): Int
        @JvmStatic private external fun nativeRelease(handle: Long): Int
        @JvmStatic private external fun nativeRunFile(handle: Long, path: String): Int
        @JvmStatic private external fun nativeRunString(handle: Long, source: String, chunkName: String): Int
        @JvmStatic private external fun nativeLoadFile(handle: Long, path: String): Int
        @JvmStatic private external fun nativeLoadString(handle: Long, source: String, chunkName: String): Int
        @JvmStatic private external fun nativePoll(handle: Long): Int
        @JvmStatic private external fun nativeStop(handle: Long)
        @JvmStatic private external fun nativeFree(handle: Long)
        @JvmStatic private external fun nativeVersion(): String
    }
}
