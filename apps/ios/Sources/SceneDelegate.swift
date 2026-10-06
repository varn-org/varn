import UIKit

/// Owns the window the app shows, which is what the scene lifecycle asks of an application.
final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?

    func scene(
        _ scene: UIScene,
        willConnectTo session: UISceneSession,
        options connectionOptions: UIScene.ConnectionOptions
    ) {
        guard let windowScene = scene as? UIWindowScene else {
            return
        }

        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = PlaygroundViewController()
        // The app commits to one palette, so the system appearance never repaints it.
        window.overrideUserInterfaceStyle = .dark
        window.makeKeyAndVisible()
        self.window = window
    }
}
