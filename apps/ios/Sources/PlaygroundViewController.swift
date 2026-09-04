import UIKit

final class PlaygroundViewController: UIViewController {

    private let runner = LuaRunner()
    private lazy var samples = Samples.load()
    private var selected = 0
    private var running = false

    private let samplePicker = UIButton(type: .system)
    private let editor = UITextView()
    private let console = UITextView()
    private let status = UILabel()
    private let runButton = UIButton(type: .system)
    private let stopButton = UIButton(type: .system)
    private let clearButton = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = Palette.background

        buildLayout()
        bindActions()

        if let first = samples.first {
            apply(sample: first)
        } else {
            editor.text = "print(\"no samples were bundled\")"
            samplePicker.setTitle("No samples", for: .normal)
        }

        status.text = "Ready."
    }

    // MARK: layout

    private func buildLayout() {
        let title = UILabel()
        title.text = "Varn"
        title.textColor = Palette.text
        title.font = .systemFont(ofSize: 22, weight: .bold)

        let engine = UILabel()
        engine.text = "Varn engine \(runner.version)"
        engine.textColor = Palette.textFaint
        engine.font = .systemFont(ofSize: 12)

        // a button configuration replaces the content insets and title attributes that were deprecated in ios 15
        var picker = UIButton.Configuration.plain()
        picker.contentInsets = NSDirectionalEdgeInsets(top: 10, leading: 12, bottom: 10, trailing: 12)
        picker.baseForegroundColor = Palette.text
        picker.titleTextAttributesTransformer = UIConfigurationTextAttributesTransformer { attributes in
            var updated = attributes
            updated.font = .systemFont(ofSize: 14)
            return updated
        }

        samplePicker.configuration = picker
        samplePicker.contentHorizontalAlignment = .leading
        style(samplePicker, background: Palette.field)
        samplePicker.showsMenuAsPrimaryAction = true

        editor.font = .monospacedSystemFont(ofSize: 13, weight: .regular)
        editor.textColor = Palette.text
        editor.backgroundColor = Palette.field
        editor.autocorrectionType = .no
        editor.autocapitalizationType = .none
        editor.smartQuotesType = .no
        editor.smartDashesType = .no
        editor.textContainerInset = UIEdgeInsets(top: 10, left: 8, bottom: 10, right: 8)
        style(editor, background: Palette.field)

        console.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        console.textColor = Palette.output
        console.backgroundColor = Palette.field
        console.isEditable = false
        console.textContainerInset = UIEdgeInsets(top: 10, left: 8, bottom: 10, right: 8)
        style(console, background: Palette.field)

        status.textColor = Palette.textFaint
        status.font = .systemFont(ofSize: 12)

        configure(runButton, title: "Run", background: Palette.accent, foreground: Palette.onAccent)
        configure(stopButton, title: "Stop", background: Palette.surface, foreground: Palette.text)
        configure(clearButton, title: "Clear", background: Palette.surface, foreground: Palette.text)

        let buttons = UIStackView(arrangedSubviews: [runButton, stopButton, clearButton])
        buttons.distribution = .fillEqually
        buttons.spacing = 8

        let editorCard = card(with: [label("Sample"), samplePicker, label("Editor"), editor, buttons])
        editorCard.setCustomSpacing(12, after: samplePicker)
        editorCard.setCustomSpacing(12, after: editor)

        let consoleCard = card(with: [label("Console"), console, status])

        let stack = UIStackView(arrangedSubviews: [title, engine, editorCard, consoleCard])
        stack.axis = .vertical
        stack.spacing = 12
        stack.setCustomSpacing(2, after: title)
        stack.setCustomSpacing(16, after: engine)
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)

        // the content follows the keyboard guide so the console is never left underneath the keyboard
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
            stack.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            stack.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            stack.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor, constant: -16),
            editor.heightAnchor.constraint(equalToConstant: 180),
        ])

        // the return key stays a newline in a code editor, so tapping away is what closes the keyboard
        let dismiss = UITapGestureRecognizer(target: self, action: #selector(dismissKeyboard))
        dismiss.cancelsTouchesInView = false
        view.addGestureRecognizer(dismiss)
    }

    @objc private func dismissKeyboard() {
        view.endEditing(true)
    }

    private func card(with views: [UIView]) -> UIStackView {
        let inner = UIStackView(arrangedSubviews: views)
        inner.axis = .vertical
        inner.spacing = 4
        inner.isLayoutMarginsRelativeArrangement = true
        inner.layoutMargins = UIEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        inner.backgroundColor = Palette.surface
        inner.layer.cornerRadius = 16
        inner.layer.borderWidth = 1
        inner.layer.borderColor = Palette.border.cgColor
        return inner
    }

    private func label(_ text: String) -> UILabel {
        let view = UILabel()
        view.text = text
        view.textColor = Palette.textMuted
        view.font = .systemFont(ofSize: 12)
        return view
    }

    private func style(_ view: UIView, background: UIColor) {
        view.backgroundColor = background
        view.layer.cornerRadius = 12
        view.layer.borderWidth = 1
        view.layer.borderColor = Palette.border.cgColor
    }

    private func configure(_ button: UIButton, title: String, background: UIColor, foreground: UIColor) {
        button.setTitle(title, for: .normal)
        button.setTitleColor(foreground, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 15, weight: .semibold)
        button.backgroundColor = background
        button.layer.cornerRadius = 10
        button.heightAnchor.constraint(equalToConstant: 44).isActive = true
    }

    // MARK: actions

    private func bindActions() {
        samplePicker.menu = UIMenu(children: samples.enumerated().map { index, sample in
            UIAction(title: sample.title) { [weak self] _ in
                self?.selected = index
                self?.apply(sample: sample)
            }
        })

        runButton.addTarget(self, action: #selector(runTapped), for: .touchUpInside)
        stopButton.addTarget(self, action: #selector(stopTapped), for: .touchUpInside)
        clearButton.addTarget(self, action: #selector(clearTapped), for: .touchUpInside)
    }

    private func apply(sample: Sample) {
        samplePicker.setTitle(sample.title, for: .normal)
        editor.text = sample.code
    }

    // The engine is advanced by the app's own run loop, so the chunk stays on this thread and nothing is dispatched.
    @objc private func runTapped() {
        guard !running else {
            return
        }

        dismissKeyboard()
        running = true
        runButton.isEnabled = false
        status.text = "Running…"

        runner.start(
            source: editor.text ?? "",
            onLine: { [weak self] line, isError in
                self?.append(line: line, error: isError)
            },
            onFinished: { [weak self] failed in
                guard let self else {
                    return
                }

                self.status.text = failed ? "Finished with errors." : "Finished."
                self.runButton.isEnabled = true
                self.running = false
            }
        )
    }

    @objc private func stopTapped() {
        guard running else {
            return
        }
        runner.stop()
        append(line: "stopped.", error: false)
        status.text = "Stopped."
    }

    @objc private func clearTapped() {
        console.text = ""
        status.text = "Console cleared."
    }


    // an error line is tinted so a failure stands out from the printed output around it
    private func append(line: String, error: Bool) {
        let attributed = NSMutableAttributedString(attributedString: console.attributedText ?? NSAttributedString())
        attributed.append(NSAttributedString(
            string: line + "\n",
            attributes: [
                .font: UIFont.monospacedSystemFont(ofSize: 12, weight: .regular),
                .foregroundColor: error ? Palette.error : Palette.output,
            ]
        ))

        console.attributedText = attributed
        console.scrollRangeToVisible(NSRange(location: attributed.length, length: 0))
    }
}
