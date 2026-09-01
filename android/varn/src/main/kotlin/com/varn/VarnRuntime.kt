package com.varn

// thin kotlin wrapper over the native varn runtime whose close() releases the native handle
class VarnRuntime : AutoCloseable {
    private var handle: Long = nativeNew()

    fun runFile(path: String): Int = nativeRunFile(handle, path)

    fun runString(source: String, chunkName: String = "=(embedded)"): Int =
        nativeRunString(handle, source, chunkName)

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
        @JvmStatic private external fun nativeRunFile(handle: Long, path: String): Int
        @JvmStatic private external fun nativeRunString(handle: Long, source: String, chunkName: String): Int
        @JvmStatic private external fun nativeStop(handle: Long)
        @JvmStatic private external fun nativeFree(handle: Long)
        @JvmStatic private external fun nativeVersion(): String
    }
}
