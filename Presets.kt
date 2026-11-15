package com.waveforge.synth

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class Preset(val name: String, val params: Map<String, Float>, val factory: Boolean)

object Presets {
    private fun parse(obj: JSONObject, factory: Boolean): Preset {
        val po = obj.getJSONObject("params")
        val map = LinkedHashMap<String, Float>()
        val it = po.keys()
        while (it.hasNext()) {
            val k = it.next()
            if (P.idOrNull(k) != null) map[k] = po.getDouble(k).toFloat()
        }
        return Preset(obj.getString("name"), map, factory)
    }

    fun factory(ctx: Context): List<Preset> {
        val text = ctx.assets.open("presets/factory.json").bufferedReader().use { it.readText() }
        val arr = JSONArray(text)
        return (0 until arr.length()).map { parse(arr.getJSONObject(it), true) }
    }

    private fun dir(ctx: Context): File = File(ctx.filesDir, "presets").also { it.mkdirs() }

    fun user(ctx: Context): List<Preset> =
        (dir(ctx).listFiles { f -> f.extension == "json" } ?: emptyArray())
            .sortedBy { it.name.lowercase() }
            .mapNotNull { f ->
                try { parse(JSONObject(f.readText()), false) } catch (_: Exception) { null }
            }

    private fun fileFor(ctx: Context, name: String): File =
        File(dir(ctx), name.replace(Regex("[^A-Za-z0-9 _-]"), "_").trim().ifEmpty { "preset" } + ".json")

    fun save(ctx: Context, name: String) {
        val po = JSONObject()
        for ((k, v) in Synth.snapshot()) po.put(k, v.toDouble())
        val o = JSONObject().put("name", name).put("params", po)
        fileFor(ctx, name).writeText(o.toString())
        Synth.presetName = name
    }

    fun delete(ctx: Context, name: String) { fileFor(ctx, name).delete() }
}
