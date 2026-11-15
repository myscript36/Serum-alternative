package com.waveforge.synth

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiDeviceService
import android.media.midi.MidiManager
import android.media.midi.MidiOutputPort
import android.media.midi.MidiReceiver
import android.os.Handler
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/** Splits a raw MIDI byte stream into channel messages and forwards them to the synth. */
object MidiParser {
    private var status = 0
    private val data = IntArray(2)
    private var n = 0

    @Synchronized
    fun feed(b: ByteArray, off: Int, count: Int) {
        for (i in off until off + count) {
            val v = b[i].toInt() and 0xFF
            if (v >= 0xF8) continue                       // realtime
            if (v >= 0xF0) { status = 0; n = 0; continue } // system / sysex
            if (v >= 0x80) { status = v; n = 0; continue }
            if (status == 0) continue
            data[n++] = v
            val hi = status and 0xF0
            val need = if (hi == 0xC0 || hi == 0xD0) 1 else 2
            if (n >= need) {
                Synth.midi(status, data[0], if (need == 2) data[1] else 0)
                n = 0
            }
        }
    }
}

/** Lets other Android apps (DAWs, sequencers) send MIDI to WaveForge as a virtual MIDI device. */
class SynthMidiService : MidiDeviceService() {
    override fun onCreate() {
        super.onCreate()
        Synth.init(applicationContext)
    }

    override fun onGetInputPortReceivers(): Array<MidiReceiver> = arrayOf(object : MidiReceiver() {
        override fun onSend(msg: ByteArray, offset: Int, count: Int, timestamp: Long) {
            MidiParser.feed(msg, offset, count)
        }
    })
}

/** Connects USB / Bluetooth MIDI controllers. */
object MidiInput {
    var devices by mutableStateOf<List<String>>(emptyList())
        private set

    private val ports = HashMap<Int, List<MidiOutputPort>>()
    private val names = HashMap<Int, String>()
    private var manager: MidiManager? = null
    private var started = false

    private val receiver = object : MidiReceiver() {
        override fun onSend(msg: ByteArray, offset: Int, count: Int, timestamp: Long) {
            MidiParser.feed(msg, offset, count)
        }
    }

    private fun publish() { devices = names.values.toList() }

    private fun open(info: MidiDeviceInfo, handler: Handler) {
        if (info.outputPortCount == 0 || ports.containsKey(info.id)) return
        val product = info.properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT) ?: ""
        if (product == "WaveForge Synth") return  // never connect to ourselves
        val mm = manager ?: return
        mm.openDevice(info, { dev ->
            if (dev != null) {
                val list = ArrayList<MidiOutputPort>()
                for (p in 0 until info.outputPortCount) {
                    val port = dev.openOutputPort(p)
                    if (port != null) { port.connect(receiver); list.add(port) }
                }
                ports[info.id] = list
                names[info.id] = info.properties.getString(MidiDeviceInfo.PROPERTY_NAME) ?: "MIDI device"
                publish()
            }
        }, handler)
    }

    fun start(context: Context) {
        if (started) return
        started = true
        val mm = context.applicationContext.getSystemService(Context.MIDI_SERVICE) as? MidiManager ?: return
        manager = mm
        val handler = Handler(Looper.getMainLooper())
        for (info in mm.devices) open(info, handler)
        mm.registerDeviceCallback(object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: MidiDeviceInfo) { open(info, handler) }
            override fun onDeviceRemoved(info: MidiDeviceInfo) {
                ports.remove(info.id)?.forEach { try { it.close() } catch (_: Exception) {} }
                names.remove(info.id)
                publish()
            }
        }, handler)
    }
}
