import Foundation
import varn

/// A Swift face for the engine's C API, owning the native handle for its lifetime.
///
/// The Android side ships an equivalent wrapper inside the AAR; on Apple the framework exposes the
/// plain C entry points, so the ownership rules live here instead.
final class VarnRuntime {
    /// Keeps a registered handler alive and owns the buffer its answer is returned through.
    ///
    /// The C API copies the answer as soon as the call returns, so one buffer per handler is enough.
    private final class HostBox {
        let handler: (String) -> String?
        var answer: UnsafeMutablePointer<CChar>?

        init(_ handler: @escaping (String) -> String?) {
            self.handler = handler
        }

        deinit {
            answer.map { free($0) }
        }
    }

    private var handle: OpaquePointer?
    private var boxes: [HostBox] = []

    init?() {
        guard let created = varn_runtime_new() else {
            return nil
        }
        handle = created
    }

    deinit {
        close()
    }

    /// Receives every line the engine writes, at the level a browser console would have used.
    ///
    /// The engine has already sent the line to the platform console, so this is a mirror for an
    /// interface that wants to show it, not the only place it appears.
    static func setConsole(_ sink: ((Int32, String) -> Void)?) {
        guard let sink else {
            consoleSink = nil
            varn_set_console(nil, nil)
            return
        }

        consoleSink = sink
        varn_set_console({ level, message, _ in
            guard let message else {
                return
            }
            VarnRuntime.consoleSink?(level, String(cString: message))
        }, nil)
    }

    private nonisolated(unsafe) static var consoleSink: ((Int32, String) -> Void)?

    static var version: String {
        guard let text = varn_version() else {
            return "unknown"
        }
        return String(cString: text)
    }

    /// Exposes a native function to Lua under the global `host` table.
    ///
    /// The argument and the answer are json, and the call arrives on the thread that entered the runtime,
    /// so anything touching the user interface has to be dispatched to the main queue from the handler.
    @discardableResult
    func register(_ name: String, _ handler: @escaping (String) -> String?) -> Bool {
        guard let handle else {
            return false
        }

        let box = HostBox(handler)
        boxes.append(box)

        let code = varn_runtime_register(handle, name, { argument, context in
            guard let context else {
                return nil
            }

            let box = Unmanaged<HostBox>.fromOpaque(context).takeUnretainedValue()
            let text = argument.map { String(cString: $0) } ?? "null"

            guard let answer = box.handler(text) else {
                return nil
            }

            box.answer.map { free($0) }
            box.answer = strdup(answer)
            return UnsafePointer(box.answer)
        }, Unmanaged.passUnretained(box).toOpaque())

        return code == 0
    }

    /// Delivers an event to every Lua handler registered for `name` through `host.on`.
    ///
    /// It is safe from any thread: the call is posted to the event loop and Lua is only touched there.
    @discardableResult
    func emit(_ name: String, _ jsonArgument: String = "null") -> Bool {
        guard let handle else {
            return false
        }
        return varn_runtime_emit(handle, name, jsonArgument) == 0
    }

    /// Keeps the event loop running, so the runtime waits for events instead of exiting once the script returns.
    func retain() {
        guard let handle else {
            return
        }
        varn_runtime_retain(handle)
    }

    /// Gives back one retain, answering false when there was none left to give.
    @discardableResult
    func release() -> Bool {
        guard let handle else {
            return false
        }
        return varn_runtime_release(handle) == 0
    }

    /// Runs a chunk and hands control straight back, leaving whatever it armed for `poll` to drive.
    ///
    /// This is what an application uses when its own run loop owns the thread, so every host call the
    /// script makes arrives on that thread rather than on one of the engine's.
    @discardableResult
    func load(source: String, chunkName: String = "=(embedded)") -> Int32 {
        guard let handle else {
            return -1
        }
        return varn_runtime_load_string(handle, source, chunkName)
    }

    @discardableResult
    func load(file path: String) -> Int32 {
        guard let handle else {
            return -1
        }
        return varn_runtime_load_file(handle, path)
    }

    /// Advances the runtime once without blocking, answering whether anything can still make progress.
    @discardableResult
    func poll() -> Bool {
        guard let handle else {
            return false
        }
        return varn_runtime_poll(handle) == 1
    }

    @discardableResult
    func run(source: String, chunkName: String = "=(embedded)") -> Int32 {
        guard let handle else {
            return -1
        }
        return varn_runtime_run_string(handle, source, chunkName)
    }

    @discardableResult
    func run(file path: String) -> Int32 {
        guard let handle else {
            return -1
        }
        return varn_runtime_run_file(handle, path)
    }

    // asks the engine to unwind, which is safe to call from another thread while a chunk runs
    func stop() {
        guard let handle else {
            return
        }
        varn_runtime_stop(handle)
    }

    func close() {
        guard let handle else {
            return
        }
        varn_runtime_free(handle)
        self.handle = nil
        boxes.removeAll()
    }
}
