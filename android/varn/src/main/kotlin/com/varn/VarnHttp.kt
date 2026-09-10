package com.varn

import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import javax.net.ssl.HttpsURLConnection
import javax.net.ssl.SSLContext
import javax.net.ssl.X509TrustManager

/**
 * The platform HTTP stack, which is what makes an app's `network_security_config.xml` apply.
 *
 * Trust anchors, certificate pinning and the cleartext policy are declared there and enforced by the
 * platform on the way through, which a transport built on its own TLS stack can never honour.
 *
 * Every method here is called by the engine through JNI and is public only because JNI resolves it by name.
 * An application talks to the network through the `http` module in Lua, never through this object.
 */
object VarnHttp {

    /** What one request produced, laid out so the JNI side can read it without building objects of its own. */
    class Response {
        @JvmField var status: Int = 0
        @JvmField var headerNames: Array<String> = emptyArray()
        @JvmField var headerValues: Array<String> = emptyArray()
        @JvmField var body: ByteArray = ByteArray(0)
        @JvmField var error: String? = null
    }

    /** Receives a streamed body one chunk at a time, mirroring the engine's own streaming contract. */
    interface ChunkSink {
        fun onResponse(status: Int, headerNames: Array<String>, headerValues: Array<String>)
        fun onChunk(chunk: ByteArray, length: Int)
    }

    @JvmStatic
    fun perform(
        method: String,
        url: String,
        headerNames: Array<String>,
        headerValues: Array<String>,
        body: ByteArray?,
        timeoutSeconds: Int,
        verifyTls: Boolean,
        maxResponseBytes: Long,
    ): Response = execute(method, url, headerNames, headerValues, body, timeoutSeconds, verifyTls, maxResponseBytes, null)

    @JvmStatic
    fun performStream(
        method: String,
        url: String,
        headerNames: Array<String>,
        headerValues: Array<String>,
        body: ByteArray?,
        timeoutSeconds: Int,
        verifyTls: Boolean,
        maxResponseBytes: Long,
        sink: ChunkSink,
    ): Response = execute(method, url, headerNames, headerValues, body, timeoutSeconds, verifyTls, maxResponseBytes, sink)

    private fun execute(
        method: String,
        url: String,
        headerNames: Array<String>,
        headerValues: Array<String>,
        body: ByteArray?,
        timeoutSeconds: Int,
        verifyTls: Boolean,
        maxResponseBytes: Long,
        sink: ChunkSink?,
    ): Response {
        val response = Response()
        var connection: HttpURLConnection? = null

        try {
            val parsed = URL(url)
            if (parsed.protocol != "http" && parsed.protocol != "https") {
                response.error = "[VarnHttp] The URL scheme must be http or https."
                return response
            }

            val opened = parsed.openConnection() as HttpURLConnection
            connection = opened

            opened.requestMethod = method
            opened.connectTimeout = timeoutSeconds * 1000
            opened.readTimeout = timeoutSeconds * 1000
            // the engine hands a redirect to the caller, matching every other transport it ships
            opened.instanceFollowRedirects = false
            opened.useCaches = false

            if (!verifyTls && opened is HttpsURLConnection) {
                applyInsecureTls(opened)
            }

            for (index in headerNames.indices) {
                opened.setRequestProperty(headerNames[index], headerValues[index])
            }

            if (body != null && body.isNotEmpty()) {
                opened.doOutput = true
                opened.setFixedLengthStreamingMode(body.size)
                opened.outputStream.use { it.write(body) }
            }

            response.status = opened.responseCode
            collectHeaders(opened, response)

            // an error status answers through the error stream rather than the input one
            val stream: InputStream? = if (response.status >= 400) opened.errorStream else opened.inputStream
            readBody(stream, response, maxResponseBytes, sink)
        } catch (error: Throwable) {
            response.error = "[VarnHttp] " + (error.message ?: error.javaClass.simpleName)
        } finally {
            connection?.disconnect()
        }

        return response
    }

    private fun collectHeaders(connection: HttpURLConnection, response: Response) {
        val names = ArrayList<String>()
        val values = ArrayList<String>()

        for ((name, list) in connection.headerFields) {
            // the status line comes back under a null name, and the decoded body makes its encoding headers wrong
            if (name == null || describesEncodedBody(name)) {
                continue
            }

            for (value in list) {
                names.add(name)
                values.add(value)
            }
        }

        response.headerNames = names.toTypedArray()
        response.headerValues = values.toTypedArray()
    }

    // the platform decodes a compressed body before handing it over, so the headers describing the encoded form would lie
    private fun describesEncodedBody(name: String): Boolean {
        val lowered = name.lowercase()
        return lowered == "content-encoding" || lowered == "content-length"
    }

    private fun readBody(stream: InputStream?, response: Response, maxResponseBytes: Long, sink: ChunkSink?) {
        if (sink != null) {
            sink.onResponse(response.status, response.headerNames, response.headerValues)
        }

        if (stream == null) {
            return
        }

        val buffer = ByteArray(16 * 1024)
        val collected = if (sink == null) java.io.ByteArrayOutputStream() else null
        var total = 0L

        stream.use {
            while (true) {
                val read = it.read(buffer)
                if (read < 0) {
                    break
                }

                total += read
                if (maxResponseBytes > 0 && total > maxResponseBytes) {
                    response.error = "[VarnHttp] The response exceeded the maximum size allowed for it."
                    return
                }

                if (sink != null) {
                    sink.onChunk(buffer, read)
                } else {
                    collected?.write(buffer, 0, read)
                }
            }
        }

        if (collected != null) {
            response.body = collected.toByteArray()
        }
    }

    // an explicit opt-out for a development server, which is the only case the engine allows it for
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
 * Hands each streamed chunk straight back to the engine, so a long response is delivered as it arrives
 * instead of being collected first, which is what server-sent events and large downloads depend on.
 *
 * [handle] points at the native callbacks and stays valid for the length of the request.
 */
internal class NativeChunkSink(@JvmField val handle: Long) : VarnHttp.ChunkSink {
    external override fun onResponse(status: Int, headerNames: Array<String>, headerValues: Array<String>)
    external override fun onChunk(chunk: ByteArray, length: Int)
}
