package com.waveforge.synth

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

private val WHITE = intArrayOf(0, 2, 4, 5, 7, 9, 11)
private val BLACK_AFTER = setOf(0, 1, 3, 4, 5)  // C D F G A are followed by a black key

@Composable
private fun Key(note: Int, white: Boolean, modifier: Modifier) {
    var down by remember { mutableStateOf(false) }
    val color = when {
        down -> Accent
        white -> Color(0xFFF2F2F8)
        else -> Color(0xFF15151F)
    }
    Box(
        modifier
            .background(color)
            .border(1.dp, Color(0xFF55556A))
            .pointerInput(note) {
                detectTapGestures(onPress = {
                    down = true
                    Synth.noteOn(note)
                    try { tryAwaitRelease() } finally {
                        down = false
                        Synth.noteOff(note)
                    }
                })
            }
    )
}

/** Multi-touch on-screen piano. [baseNote] is the MIDI note of the lowest C. */
@Composable
fun PianoKeyboard(baseNote: Int, modifier: Modifier = Modifier) {
    BoxWithConstraints(modifier.fillMaxWidth().height(120.dp)) {
        val keyW: Dp = 38.dp
        val count = (maxWidth / keyW).toInt().coerceIn(7, 28)
        val kw = maxWidth / count
        Row(Modifier.fillMaxSize()) {
            for (i in 0 until count) {
                val note = baseNote + (i / 7) * 12 + WHITE[i % 7]
                if (note <= 127) Key(note, true, Modifier.width(kw).fillMaxHeight())
            }
        }
        for (i in 0 until count) {
            if ((i % 7) in BLACK_AFTER && i < count - 1) {
                val note = baseNote + (i / 7) * 12 + WHITE[i % 7] + 1
                if (note <= 127) {
                    Key(
                        note, false,
                        Modifier.offset(x = kw * (i + 1) - kw * 0.3f).width(kw * 0.6f).height(76.dp)
                    )
                }
            }
        }
    }
}
