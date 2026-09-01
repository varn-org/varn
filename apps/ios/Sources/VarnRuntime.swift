import Foundation
import varn

/// A Swift face for the engine's C API, owning the native handle for its lifetime.
///
/// The Android side ships an equivalent wrapper inside the AAR; on Apple the framework exposes the
/// plain C entry points, so the ownership rules live here instead.
final class VarnRuntime {
    private var handle: OpaquePointer?

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
    }
}
