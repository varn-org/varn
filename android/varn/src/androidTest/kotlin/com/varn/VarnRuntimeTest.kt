package com.varn

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread
import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class VarnRuntimeTest {
    private val grinning = "😀"

    @Test
    fun aCharacterOutsideTheBasicPlaneCrossesBothWaysAsItsFourBytes() {
        VarnRuntime().use { runtime ->
            val arguments = CopyOnWriteArrayList<String>()
            val reports = CopyOnWriteArrayList<String>()
            runtime.register("echo") { argument ->
                arguments.add(argument)
                JSONObject.quote(grinning)
            }
            runtime.register("report") { argument ->
                reports.add(argument)
                "null"
            }

            val source = """
                local answer = host.echo("$grinning")
                host.report({ sourceLength = #"$grinning", answerLength = #answer, same = answer == "$grinning" })
            """
            assertEquals(0, runtime.runString(source))

            assertEquals(grinning, JSONTokener(arguments.single()).nextValue())
            val report = JSONObject(reports.single())
            assertEquals(4, report.getInt("sourceLength"))
            assertEquals(4, report.getInt("answerLength"))
            assertTrue(report.getBoolean("same"))
        }
    }

    @Test
    fun anEventCarriesACharacterOutsideTheBasicPlane() {
        VarnRuntime().use { runtime ->
            val reports = CopyOnWriteArrayList<String>()
            runtime.register("report") { argument ->
                reports.add(argument)
                "null"
            }

            assertEquals(0, runtime.loadString("host.on('text', function(value) host.report(#value) end)"))
            assertEquals(0, runtime.emit("text", JSONObject.quote(grinning)))
            while (runtime.poll() && reports.isEmpty()) {
                Thread.yield()
            }

            assertEquals(listOf("4"), reports)
        }
    }

    @Test(timeout = 60_000)
    fun aConsoleSinkReplacedWhileLinesArriveIsNeverUsedAfterItIsReleased() {
        VarnRuntime().use { runtime ->
            val lines = AtomicInteger()
            val writer = thread { runtime.runString("for i = 1, 5000 do print(i) end") }
            while (writer.isAlive) {
                VarnRuntime.setConsole { _, _ -> lines.incrementAndGet() }
            }
            VarnRuntime.setConsole(null)
            writer.join()

            assertTrue(lines.get() > 0)
        }
    }

    @Test(timeout = 60_000)
    fun aCloseWaitsForAnEmitOnAnotherThreadAndLaterCallsAnswerANullRuntime() {
        repeat(50) {
            val runtime = VarnRuntime()
            val emitting = thread {
                while (runtime.emit("tick", "1") == 0) {
                    Thread.yield()
                }
            }
            runtime.close()
            emitting.join()

            assertEquals(2, runtime.emit("tick"))
            assertFalse(runtime.poll())
            assertEquals(-1L, runtime.idle())
        }
    }

    @Test
    fun aRuntimeRefusesToBeClosedFromInsideItsOwnCall() {
        VarnRuntime().use { runtime ->
            val failures = CopyOnWriteArrayList<Throwable>()
            runtime.register("close") {
                try {
                    runtime.close()
                } catch (failure: IllegalStateException) {
                    failures.add(failure)
                }
                "null"
            }

            assertEquals(0, runtime.runString("host.close()"))
            assertEquals("A runtime cannot be closed from inside one of its own calls.", failures.single().message)
        }
    }

    @Test(timeout = 60_000)
    fun aPumpedRuntimeIsWokenByAnEventFromAnotherThread() {
        VarnRuntime().use { runtime ->
            val reports = CopyOnWriteArrayList<String>()
            runtime.register("report") { argument ->
                reports.add(argument)
                "null"
            }
            val woken = CountDownLatch(1)
            runtime.setWake { woken.countDown() }

            assertEquals(0, runtime.loadString("host.on('ping', function(value) host.report(value) end)"))
            val result = VarnRuntime.PollResult()
            while (runtime.pollBudget(1_000_000, result)) {
                Thread.yield()
            }
            assertEquals(-1L, runtime.idle())

            thread { runtime.emit("ping", "7") }.join()
            assertTrue(woken.await(10, TimeUnit.SECONDS))
            assertEquals(0L, runtime.idle())

            runtime.pollBudget(10_000_000, result)
            assertTrue(result.ran)
            assertEquals(listOf("7"), reports)
            runtime.setWake(null)
        }
    }

    @Test
    fun aHostFunctionAnswersWithAViewItReleasesBeforeItReturns() {
        VarnRuntime().use { runtime ->
            val reports = CopyOnWriteArrayList<String>()
            val views = CopyOnWriteArrayList<VarnDataView>()
            runtime.register("frame") {
                val view = VarnDataView.allocate(3)
                view.buffer().put(byteArrayOf(1, 2, 3))
                val answer = JSONObject().put("frame", view.token()).toString()
                assertEquals(0, runtime.releaseDataview(view))
                views.add(view)
                answer
            }
            runtime.register("report") { argument ->
                reports.add(argument)
                "null"
            }

            assertEquals(0, runtime.runString("local frame = host.frame().frame host.report({ frame[0], frame[1], frame[2], #frame })"))

            assertTrue(views.single().isClosed)
            assertEquals("[1,2,3,3]", JSONArray(reports.single()).toString())
        }
    }
}
