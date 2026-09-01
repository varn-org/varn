package com.varn.app

import android.content.Context
import org.json.JSONObject

// one entry of the shared sample set, which the manifest beside the lua files describes
data class Sample(val module: String, val label: String, val code: String) {
    val title: String get() = "$module · $label"
}

object Samples {
    private const val MANIFEST = "manifest.json"

    // the samples are assets shared with the other apps, so the list here is whatever the manifest declares
    fun load(context: Context): List<Sample> {
        val manifest = context.assets.open(MANIFEST).bufferedReader().use { it.readText() }
        val entries = JSONObject(manifest).getJSONArray("samples")

        return (0 until entries.length()).map { index ->
            val entry = entries.getJSONObject(index)
            val file = entry.getString("file")
            Sample(
                module = entry.getString("module"),
                label = entry.getString("label"),
                code = context.assets.open("lua/$file").bufferedReader().use { it.readText() },
            )
        }
    }
}
