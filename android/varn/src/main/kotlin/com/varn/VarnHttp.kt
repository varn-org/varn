package com.varn

import java.io.InputStream
import java.net.HttpURLConnection
import java.net.MalformedURLException
import java.net.ProtocolException
import java.net.SocketTimeoutException
import java.net.URL
import javax.net.ssl.HttpsURLConnection
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLException
import javax.net.ssl.X509TrustManager

/**
 * The platform HTTP stack, which is what makes an app's `network_security_config.xml` apply.
 *
 * Trust anchors, certificate pinning and the cleartext policy are declared there and enforced by the platform on the way through, which a transport built on its own TLS stack can never honour.
 * A connection whose body was read to its end goes back to the pool of the platform, so the next request to the same host skips the TCP and TLS handshakes.
 *
 * Every method here is called by the engine through JNI and is public only because JNI resolves it by name.
 * An application talks to the network through the `http` module in Lua, never through this object.
 */
object VarnHttp {

    /** How one request ended, laid out so the JNI side can read it without building objects of its own. */
    class Outcome {
        @JvmField var error: String? = null
        @JvmField var kind: Int = KIND_NONE
    }

    // The kinds of failure the engine tells apart, numbered the same way on the JNI side.
    private const val KIND_NONE = 0
    private const val KIND_NETWORK = 1
    private const val KIND_TLS = 2
    private const val KIND_TIMEOUT = 3
    private const val KIND_INVALID = 4

    private const val PIECE_BYTES = 64 * 1024

    @JvmStatic
    fun perform(
        method: String,
        url: String,
        headerNames: Array<String>,
        headerValues: Array<String>,
        body: ByteArray?,
        streamedBody: Boolean,
        bodyLength: Long,
        timeoutMilliseconds: Int,
        verifyTls: Boolean,
        exchange: NativeExchange,
    ): Outcome {
        val outcome = Outcome()
        var connection: HttpURLConnection? = null
        var reusable = false

        try {
            val parsed = URL(url)
            if (parsed.protocol != "http" && parsed.protocol != "https") {
                outcome.error = "[VarnHttp] The URL scheme must be \"http\" or \"https\"."
                outcome.kind = KIND_INVALID
                return outcome
            }

            val opened = parsed.openConnection() as HttpURLConnection
            connection = opened
            if (!exchange.attach(opened)) {
                return outcome
            }

            opened.requestMethod = method
            opened.connectTimeout = timeoutMilliseconds
            opened.readTimeout = timeoutMilliseconds
            // The engine hands a redirect to the caller, matching every other transport it ships.
            opened.instanceFollowRedirects = false
            opened.useCaches = false

            if (!verifyTls && opened is HttpsURLConnection) {
                applyInsecureTls(opened)
            }

            for (index in headerNames.indices) {
                opened.addRequestProperty(headerNames[index], headerValues[index])
            }

            if (body != null && body.isNotEmpty()) {
                opened.doOutput = true
                opened.setFixedLengthStreamingMode(body.size)
                opened.outputStream.use { it.write(body) }
            } else if (streamedBody) {
                sendSource(opened, bodyLength, exchange)
            }

            val status = opened.responseCode
            val names = ArrayList<String>()
            val values = ArrayList<String>()
            val encoded = collectHeaders(opened, names, values)
            val bodiless = method == "HEAD" || status < 200 || status == 204 || status == 304
            // The length of a decoded body is known only when nothing was decoded.
            val contentLength = if (bodiless) 0L else if (encoded) -1L else opened.contentLengthLong

            if (!exchange.onHead(status, names.toTypedArray(), values.toTypedArray(), contentLength)) {
                return outcome
            }

            // An error status answers through the error stream rather than the input one.
            val stream: InputStream? = if (status >= 400) opened.errorStream else opened.inputStream
            reusable = readBody(stream, exchange)
        } catch (error: Throwable) {
            if (!exchange.isCancelled) {
                outcome.error = "[VarnHttp] " + (error.message ?: error.javaClass.simpleName)
                outcome.kind = kindOf(error)
            }
        } finally {
            // Only a connection left halfway is dropped, since one read to its end is what the platform keeps alive.
            if (!reusable) {
                connection?.disconnect()
            }
        }

        return outcome
    }

    private fun sendSource(connection: HttpURLConnection, bodyLength: Long, exchange: NativeExchange) {
        connection.doOutput = true
        if (bodyLength >= 0) {
            connection.setFixedLengthStreamingMode(bodyLength)
        } else {
            connection.setChunkedStreamingMode(0)
        }

        val buffer = ByteArray(PIECE_BYTES)
        connection.outputStream.use {
            while (true) {
                val length = exchange.readBody(buffer)
                if (length <= 0) {
                    break
                }

                it.write(buffer, 0, length)
            }
        }
    }

    // Answers whether the body was decoded, since its encoding headers are dropped along with the length of the encoded form.
    private fun collectHeaders(connection: HttpURLConnection, names: ArrayList<String>, values: ArrayList<String>): Boolean {
        var encoded = false

        for ((name, list) in connection.headerFields) {
            // The status line comes back under a null name.
            if (name == null) {
                continue
            }

            val lowered = name.lowercase()
            if (lowered == "content-encoding") {
                encoded = true
            }

            if (lowered == "content-encoding" || lowered == "content-length") {
                continue
            }

            for (value in list) {
                names.add(name)
                values.add(value)
            }
        }

        return encoded
    }

    // Answers whether the body was read to its end, which is what lets the platform reuse the connection.
    private fun readBody(stream: InputStream?, exchange: NativeExchange): Boolean {
        if (stream == null) {
            return true
        }

        val buffer = ByteArray(PIECE_BYTES)
        stream.use {
            var read = it.read(buffer)
            while (read >= 0) {
                if (read > 0 && !exchange.onChunk(buffer, read)) {
                    return false
                }

                read = it.read(buffer)
            }
        }

        return true
    }

    private fun kindOf(error: Throwable): Int = when (error) {
        is SocketTimeoutException -> KIND_TIMEOUT
        is SSLException -> KIND_TLS
        is MalformedURLException, is ProtocolException, is IllegalArgumentException -> KIND_INVALID
        else -> KIND_NETWORK
    }

    // This is an explicit opt-out for a development server, which is the only case the engine allows it for.
    private fun applyInsecureTls(connection: HttpsURLConnection) {
        val trustEverything = object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) = Unit
            override fun checkServerTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) = Unit
            override fun getAcceptedIssuers(): Array<java.security.cert.X509Certificate> = emptyArray()
        }

        val context = SSLContext.getInstance("TLS")
        context.init(null, arrayOf(trustEverything), java.security.SecureRandom())

        connection.sslSocketFactory = context.socketFactory
        connection.setHostnameVerifier { _, _ -> true }
    }
}

/**
 * One request as the engine sees it, whose head and body chunks go straight back to the engine as they arrive and which the engine cancels from any thread.
 *
 * The value [handle] points at the native side of the request and stays valid for the length of [VarnHttp.perform].
 * The class is public only because [VarnHttp.perform] takes it and JNI resolves both by name.
 */
class NativeExchange(@JvmField val handle: Long) {
    @Volatile private var connection: HttpURLConnection? = null
    @Volatile private var cancelled = false

    val isCancelled: Boolean
        get() = cancelled

    /** Answers whether the request may go on, which is false once the engine cancelled it. */
    fun attach(opened: HttpURLConnection): Boolean {
        connection = opened
        return !cancelled
    }

    /** Ends the request wherever it waits, since a closed connection makes a blocked read or write fail at once. */
    fun cancel() {
        cancelled = true
        connection?.disconnect()
    }

    external fun onHead(status: Int, headerNames: Array<String>, headerValues: Array<String>, contentLength: Long): Boolean
    external fun onChunk(chunk: ByteArray, length: Int): Boolean
    external fun readBody(buffer: ByteArray): Int
}
