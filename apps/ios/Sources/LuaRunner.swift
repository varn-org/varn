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

    func run(source: String, onLine: @escaping (String, Bool) -> Void) -> RunOutcome {
        let scriptURL = workDirectory.appendingPathComponent("sample.lua")
        let scratchURL = workDirectory.appendingPathComponent("scratch", isDirectory: true)

        do {
            try source.write(to: scriptURL, atomically: true, encoding: .utf8)
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

        // the console fills as the chunk prints rather than after it finishes, which matters for a sample that waits on the network
        var captured = ""
        runtime.register("emit") { line in
            let isError = line.hasPrefix("error: ")
            captured += line + "\n"
            DispatchQueue.main.async { onLine(line, isError) }
            return nil
        }

        let code = runtime.run(source: harness(script: scriptURL, scratch: scratchURL), chunkName: "=harness")

        lock.lock()
        current = nil
        lock.unlock()
        runtime.close()

        if captured.isEmpty && code != 0 {
            let line = "engine exited with code \(code)"
            DispatchQueue.main.async { onLine(line, true) }
            captured = line + "\n"
        }

        // the harness reports a lua error as an output line, so a non-zero code means the harness itself failed
        return RunOutcome(
            output: captured,
            failed: code != 0 || captured.hasPrefix("error: ") || captured.contains("\nerror: ")
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
    // print is routed to the host so the console fills live, and the paths come from the app's own directory
    private func harness(script: URL, scratch: URL) -> String {
        """
        local function emit(line)
            host.emit(line)
        end

        print = function(...)
            local parts = {}
            for i = 1, select("#", ...) do
                parts[i] = tostring((select(i, ...)))
            end
            emit(table.concat(parts, "\t"))
        end

        SAMPLE_DIR = [[\(scratch.path)]]

        local async = require("async")
        local realRun, realSpawn = async.run, async.spawn

        async.run = function(fn)
            return realRun(function()
                local ok, err = pcall(fn)
                if not ok then
                    emit("error: " .. tostring(err))
                end
            end)
        end

        async.spawn = function(fn)
            return realSpawn(function()
                local ok, err = pcall(fn)
                if not ok then
                    emit("error: " .. tostring(err))
                end
            end)
        end

        local file = assert(io.open([[\(script.path)]], "r"))
        local source = file:read("a")
        file:close()

        local chunk, loadError = load(source, "=sample")
        if not chunk then
            emit("error: " .. tostring(loadError))
        else
            local ok, runError = pcall(chunk)
            if not ok then
                emit("error: " .. tostring(runError))
            end
        end
        """
    }
}
