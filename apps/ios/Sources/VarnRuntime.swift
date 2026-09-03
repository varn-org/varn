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

        // clang-format off
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
        // clang-format on

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
        handle.map { varn_runtime_retain($0) }
    }

    /// Gives back one retain, letting the loop finish when nothing else holds it.
    func release() {
        handle.map { varn_runtime_release($0) }
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
