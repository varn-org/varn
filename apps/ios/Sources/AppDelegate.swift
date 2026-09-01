import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = PlaygroundViewController()
        // the app commits to one palette, so the system appearance never repaints it
        window.overrideUserInterfaceStyle = .dark
        window.makeKeyAndVisible()
        self.window = window
        return true
    }
}
