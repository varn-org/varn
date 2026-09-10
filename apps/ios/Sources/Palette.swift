import UIKit

// one palette only, so the app looks the same whatever the system appearance is set to
enum Palette {
    static let background = UIColor(red: 0.035, green: 0.035, blue: 0.043, alpha: 1)
    static let surface = UIColor(red: 0.075, green: 0.075, blue: 0.086, alpha: 1)
    static let border = UIColor(red: 0.153, green: 0.153, blue: 0.165, alpha: 1)
    static let field = UIColor(red: 0.043, green: 0.043, blue: 0.055, alpha: 1)
    static let text = UIColor(red: 0.894, green: 0.894, blue: 0.906, alpha: 1)
    static let textMuted = UIColor(red: 0.631, green: 0.631, blue: 0.667, alpha: 1)
    static let textFaint = UIColor(red: 0.443, green: 0.443, blue: 0.478, alpha: 1)
    static let accent = UIColor(red: 0.024, green: 0.714, blue: 0.831, alpha: 1)
    static let onAccent = UIColor(red: 0.035, green: 0.035, blue: 0.043, alpha: 1)
    static let output = UIColor(red: 0.655, green: 0.953, blue: 0.816, alpha: 1)
    static let error = UIColor(red: 0.973, green: 0.443, blue: 0.443, alpha: 1)
}
