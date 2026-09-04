package com.varn.app

import android.os.Bundle
import android.text.Spannable
import android.text.SpannableString
import android.text.style.ForegroundColorSpan
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private lateinit var runner: LuaRunner
    private lateinit var samples: List<Sample>

    private lateinit var sampleSpinner: Spinner
    private lateinit var codeInput: EditText
    private lateinit var consoleOutput: TextView
    private lateinit var consoleScroll: ScrollView
    private lateinit var statusLine: TextView
    private lateinit var runButton: Button

    @Volatile
    private var running = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)

        val root = findViewById<View>(R.id.main)
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            view.setPadding(bars.left + view.paddingLeft, bars.top, bars.right + view.paddingRight, maxOf(bars.bottom, ime.bottom))
            insets
        }

        sampleSpinner = findViewById(R.id.sampleSpinner)
        codeInput = findViewById(R.id.codeInput)
        consoleOutput = findViewById(R.id.consoleOutput)
        consoleScroll = findViewById(R.id.consoleScroll)
        statusLine = findViewById(R.id.statusLine)
        runButton = findViewById(R.id.runButton)

        runner = LuaRunner(cacheDir)
        samples = Samples.load(this)

        findViewById<TextView>(R.id.engineVersion).text = getString(R.string.app_name) + " engine " + runner.version()
        statusLine.text = getString(R.string.status_ready)

        bindSamples()
        bindActions()
    }

    private fun bindSamples() {
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, samples.map { it.title })
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        sampleSpinner.adapter = adapter

        sampleSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                codeInput.setText(samples[position].code)
            }

            override fun onNothingSelected(parent: AdapterView<*>?) = Unit
        }

        codeInput.setText(samples.first().code)
    }

    private fun bindActions() {
        runButton.setOnClickListener { runCurrentSource() }

        // the return key stays a newline in a code editor, so tapping away is what closes the keyboard
        findViewById<View>(R.id.main).setOnClickListener { dismissKeyboard() }

        findViewById<Button>(R.id.stopButton).setOnClickListener {
            if (running) {
                runner.stop()
                append(getString(R.string.status_stopped), error = false)
                statusLine.text = getString(R.string.status_stopped)
            }
        }

        findViewById<Button>(R.id.clearButton).setOnClickListener {
            consoleOutput.text = ""
            statusLine.text = getString(R.string.status_cleared)
        }
    }

    // the engine runs the chunk to completion, so it goes on a background thread and the ui is updated from the result
    private fun runCurrentSource() {
        if (running) {
            return
        }

        dismissKeyboard()
        running = true
        runButton.isEnabled = false
        statusLine.text = getString(R.string.status_running)

        val source = codeInput.text.toString()

        thread(name = "varn-run") {
            val outcome = try {
                // the console fills as the chunk prints rather than after it finishes, which matters for a sample that waits on the network
                runner.run(source) { line, isError ->
                    runOnUiThread { append(line, error = isError) }
                }
            } catch (error: Throwable) {
                val message = "error: ${error.message}"
                runOnUiThread { append(message, error = true) }
                RunOutcome(message + "\n", failed = true)
            }

            runOnUiThread {
                statusLine.text = getString(if (outcome.failed) R.string.status_failed else R.string.status_finished)
                runButton.isEnabled = true
                running = false
            }
        }
    }

    private fun dismissKeyboard() {
        val service = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        service.hideSoftInputFromWindow(codeInput.windowToken, 0)
        codeInput.clearFocus()
    }

    private fun append(line: String, error: Boolean) {
        val rendered = SpannableString(line + "\n")
        if (error) {
            val color = ContextCompat.getColor(this, R.color.varn_error)
            rendered.setSpan(ForegroundColorSpan(color), 0, rendered.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }

        consoleOutput.append(rendered)
        consoleScroll.post { consoleScroll.fullScroll(View.FOCUS_DOWN) }
    }
}
