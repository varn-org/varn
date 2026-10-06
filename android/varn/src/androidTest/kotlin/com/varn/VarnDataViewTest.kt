package com.varn

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlin.concurrent.thread
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class VarnDataViewTest {
    init {
        VarnRuntime.version()
    }

    @Test(timeout = 120_000)
    fun aCloseOnAnotherThreadNeverLetsACallReachAFreedView() {
        repeat(20_000) {
            val view = VarnDataView.allocate(64)
            val closer = thread { view.close() }
            try {
                while (true) {
                    assertEquals(64L, view.size())
                    view.retain().close()
                }
            } catch (closed: IllegalStateException) {
                assertEquals("The data view was closed.", closed.message)
            }
            closer.join()
        }
    }

    @Test
    fun aTokenWhosePointerHasMoreThanSixtyFourBitsNamesNoView() {
        VarnDataView.allocate(8).use { view ->
            val token = view.token()
            val digits = token.getString("pointer").substring(2).padStart(16, '0')
            val forged = JSONObject().put("\$view", token.getLong("\$view")).put("pointer", "0x1$digits").put("size", token.getLong("size"))

            assertThrows(JSONException::class.java) { VarnDataView.acquire(forged) }
        }
    }

    @Test
    fun aTokenWithANegativeSizeNamesNoView() {
        VarnDataView.allocate(8).use { view ->
            val forged = view.token().put("size", -1L)

            assertThrows(JSONException::class.java) { VarnDataView.acquire(forged) }
        }
    }
}
