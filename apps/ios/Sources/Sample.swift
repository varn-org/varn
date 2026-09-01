import Foundation

// one entry of the shared sample set, which the manifest beside the lua files describes
struct Sample {
    let module: String
    let label: String
    let code: String

    var title: String { "\(module) · \(label)" }
}

enum Samples {
    private struct Manifest: Decodable {
        struct Entry: Decodable {
            let module: String
            let label: String
            let file: String
        }

        let samples: [Entry]
    }

    // the samples arrive as a folder reference, so they keep their directory inside the bundle
    static func load() -> [Sample] {
        guard let root = Bundle.main.url(forResource: "samples", withExtension: nil),
              let manifestData = try? Data(contentsOf: root.appendingPathComponent("manifest.json")),
              let manifest = try? JSONDecoder().decode(Manifest.self, from: manifestData)
        else {
            return []
        }

        return manifest.samples.compactMap { entry in
            let path = root.appendingPathComponent("lua").appendingPathComponent(entry.file)
            guard let code = try? String(contentsOf: path, encoding: .utf8) else {
                return nil
            }
            return Sample(module: entry.module, label: entry.label, code: code)
        }
    }
}
