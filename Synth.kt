package com.waveforge.synth

import android.content.Context
import android.content.Intent
import android.media.AudioManager
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlin.math.roundToInt

/** App-wide synth state. UI reads [values]; every change is forwarded to the native engine. */
object Synth {
    val values = Array(P.count) { mutableFloatStateOf(P.defs[it].def) }

    var ready by mutableStateOf(false)
        private set
    var audioOk by mutableStateOf(true)
        private set
    var presetName by mutableStateOf("Init")

    private var started = false

    @Synchronized
    fun init(context: Context) {
        if (started) return
        started = true
        val app = context.applicationContext
        val am = app.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val sr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toIntOrNull() ?: 48000
        try {
            app.startForegroundService(Intent(app, KeepAliveService::class.java))
        } catch (_: Exception) {
            // not allowed from background: audio still works while the process is alive
        }
        Thread({
            NativeSynth.create(sr)
            for (i in 0 until P.count) NativeSynth.setParam(i, values[i].floatValue)
            audioOk = NativeSynth.startAudio()
            ready = true
        }, "synth-init").start()
    }

    fun restartAudio() { audioOk = NativeSynth.startAudio() }

    fun get(id: Int): Float = values[id].floatValue
    fun getKey(key: String): Float = values[P.id(key)].floatValue

    fun set(id: Int, value: Float) {
        val d = P.defs[id]
        var v = value.coerceIn(d.lo, d.hi)
        if (d.kind != 'f') v = v.roundToInt().toFloat()
        values[id].floatValue = v
        if (ready) NativeSynth.setParam(id, v)
    }

    fun setKey(key: String, value: Float) = set(P.id(key), value)

    fun noteOn(note: Int, velocity: Int = 100) = midi(0x90, note, velocity)
    fun noteOff(note: Int) = midi(0x80, note, 0)
    fun midi(status: Int, d1: Int, d2: Int) { if (ready) NativeSynth.midi(status, d1, d2) }
    fun panic() = midi(0xB0, 123, 0)

    /** Applies a preset: everything except tempo/master-independent state returns to default first. */
    fun loadPreset(name: String, params: Map<String, Float>) {
        panic()
        for (i in 0 until P.count) {
            val d = P.defs[i]
            if (d.key == "bpm") continue
            set(i, params[d.key] ?: d.def)
        }
        presetName = name
    }

    /** Only values that differ from the default, by key. */
    fun snapshot(): Map<String, Float> {
        val out = LinkedHashMap<String, Float>()
        for (i in 0 until P.count) {
            val d = P.defs[i]
            val v = values[i].floatValue
            if (Math.abs(v - d.def) > 1e-4f) out[d.key] = v
        }
        return out
    }
}
