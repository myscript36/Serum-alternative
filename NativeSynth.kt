package com.waveforge.synth

/** JNI bridge to the C++ engine (libwaveforge.so). */
object NativeSynth {
    init { System.loadLibrary("waveforge") }

    external fun create(sampleRate: Int)
    external fun startAudio(): Boolean
    external fun stopAudio()
    external fun setParam(id: Int, value: Float)
    external fun midi(status: Int, d1: Int, d2: Int)
    external fun scope(out: FloatArray)
    external fun peak(): Float
    external fun voices(): Int
    external fun tableFrame(table: Int, pos: Float, out: FloatArray)
    external fun paramCount(): Int
}
