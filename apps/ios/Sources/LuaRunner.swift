import Foundation
import QuartzCore

/// Runs a Lua chunk on the embedded engine, driven by the app's own run loop.
///
/// The engine is never given a thread of its own. The chunk is loaded and then advanced one tick at a
/// time from a display link on the main thread, so everything the script does — a `print`, a host call,
/// a coroutine resuming after the network answered — happens on the thread that owns the interface, as
/// it happens, rather than after the script has finished.
final class LuaRunner {
    private let workDirectory: URL
    private var runtime: VarnRuntime?
    private var displayLink: CADisplayLink?
    private var onLine: ((String, Bool) -> Void)?
    private var onFinished: ((Bool) -> Void)?
    private var sawError = false

    init() {
        workDirectory = FileManager.default.temporaryDirectory.appendingPathComponent("varn", isDirectory: true)
        try? FileManager.default.createDirectory(at: workDirectory, withIntermediateDirectories: true)
    }

    var version: String { VarnRuntime.version }

    var isRunning: Bool { runtime != nil }

    /// Starts a chunk and returns at once, reporting each line through `onLine` as the script produces it.
    func start(
        source: String,
        onLine: @escaping (String, Bool) -> Void,
        onFinished: @escaping (Bool) -> Void
    ) {
        stop()

        let scratchURL = workDirectory.appendingPathComponent("scratch", isDirectory: true)
        try? FileManager.default.removeItem(at: scratchURL)
        try? FileManager.default.createDirectory(at: scratchURL, withIntermediateDirectories: true)

        guard let runtime = VarnRuntime() else {
            onLine("could not create the engine", true)
            onFinished(true)
            return
        }

        self.runtime = runtime
        self.onLine = onLine
        self.onFinished = onFinished
        sawError = false

        // The engine writes every line to the platform console already, and this mirrors it into the app's own.
        VarnRuntime.setConsole { [weak self] level, message in
            self?.report(message, isError: level >= Self.warnLevel)
        }

        let code = runtime.load(source: Self.harness(source: source, scratch: scratchURL), chunkName: "=harness")
        if code != 0 {
            report("the engine rejected the chunk with code \(code)", isError: true)
            finish()
            return
        }

        let link = CADisplayLink(target: self, selector: #selector(tick))
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    /// Asks the engine to unwind, which is how a long loop is cut short.
    func stop() {
        guard let runtime else {
            return
        }

        runtime.stop()
        finish()
    }

    @objc private func tick() {
        guard let runtime else {
            return
        }

        if !runtime.poll() {
            finish()
        }
    }

    // A warning and an error share the tint, since both mean the sample did not do what it set out to.
    private static let warnLevel: Int32 = 3

    private func report(_ line: String, isError: Bool) {
        if isError {
            sawError = true
        }

        onLine?(line, isError)
    }

    private func finish() {
        displayLink?.invalidate()
        displayLink = nil
        VarnRuntime.setConsole(nil)

        runtime?.close()
        runtime = nil

        let finished = onFinished
        onLine = nil
        onFinished = nil
        finished?(sawError)
    }

    // The scratch path comes from the app's own temporary directory, so it carries nothing that could close the lua string.
    private static func harness(source: String, scratch: URL) -> String {
        """
        local log = require("log")

        local function report(message)
            log.error(tostring(message))
        end

        -- A sandboxed app has no writable working directory, so the one place a sample may write is named here.
        SAMPLE_DIR = [[\(scratch.path)]]

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

        local chunk, loadError = load([==[\(source)]==], "=sample")
        if not chunk then
            report(loadError)
        else
            local ok, runError = pcall(chunk)
            if not ok then
                report(runError)
            end
        end
        """
    }
}
