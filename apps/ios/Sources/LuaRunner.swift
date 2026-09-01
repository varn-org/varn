import Foundation

// what one run produced, with the output already separated from the failure that ended it
struct RunOutcome {
    let output: String
    let failed: Bool
}

/// Runs a Lua chunk on the embedded engine and collects what it printed.
///
/// The C API answers with an exit code and nothing else, so the chunk is wrapped in a harness that
/// replaces `print` with one writing to a file the app reads back afterwards.
final class LuaRunner {
    private let workDirectory: URL
    private var current: VarnRuntime?
    private let lock = NSLock()

    init() {
        workDirectory = FileManager.default.temporaryDirectory.appendingPathComponent("varn", isDirectory: true)
        try? FileManager.default.createDirectory(at: workDirectory, withIntermediateDirectories: true)
    }

    var version: String { VarnRuntime.version }

    func run(source: String) -> RunOutcome {
        let scriptURL = workDirectory.appendingPathComponent("sample.lua")
        let outputURL = workDirectory.appendingPathComponent("output.txt")
        let scratchURL = workDirectory.appendingPathComponent("scratch", isDirectory: true)

        do {
            try source.write(to: scriptURL, atomically: true, encoding: .utf8)
            try "".write(to: outputURL, atomically: true, encoding: .utf8)
            try? FileManager.default.removeItem(at: scratchURL)
            try FileManager.default.createDirectory(at: scratchURL, withIntermediateDirectories: true)
        } catch {
            return RunOutcome(output: "error: \(error.localizedDescription)\n", failed: true)
        }

        guard let runtime = VarnRuntime() else {
            return RunOutcome(output: "error: could not create the engine\n", failed: true)
        }

        lock.lock()
        current = runtime
        lock.unlock()

        let code = runtime.run(source: harness(script: scriptURL, output: outputURL, scratch: scratchURL), chunkName: "=harness")

        lock.lock()
        current = nil
        lock.unlock()
        runtime.close()

        let captured = (try? String(contentsOf: outputURL, encoding: .utf8)) ?? ""
        // the harness reports a lua error through the output, so a non-zero code means the harness itself failed
        let text = captured.isEmpty && code != 0 ? "engine exited with code \(code)\n" : captured

        return RunOutcome(
            output: text,
            failed: code != 0 || text.hasPrefix("error: ") || text.contains("\nerror: ")
        )
    }

    // asks the engine to unwind the running chunk, which is how a long loop is cut short
    func stop() {
        lock.lock()
        let runtime = current
        lock.unlock()
        runtime?.stop()
    }

    // the paths come from the app's own temporary directory, so they carry nothing that could close the lua string
    private func harness(script: URL, output: URL, scratch: URL) -> String {
        """
        local out = assert(io.open([[\(output.path)]], "w"))

        local function report(message)
            out:write("error: ", tostring(message), "\\n")
            out:flush()
        end

        print = function(...)
            local parts = {}
            for i = 1, select("#", ...) do
                parts[i] = tostring((select(i, ...)))
            end
            out:write(table.concat(parts, "\\t"), "\\n")
            out:flush()
        end

        -- a sandboxed app has no writable working directory, so the one place a sample may write is named here
        SAMPLE_DIR = [[\(scratch.path)]]

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

        local file = assert(io.open([[\(script.path)]], "r"))
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
        """
    }
}
