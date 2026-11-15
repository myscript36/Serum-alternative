package com.waveforge.synth

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ScrollableTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        Synth.init(this)
        MidiInput.start(this)
        if (Build.VERSION.SDK_INT >= 33 && savedInstanceState == null) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 1)
        }
        setContent { WaveForgeTheme { App() } }
    }
}

private val TABS = listOf("OSC", "FILTER / ENV", "MOD", "FX", "PLAY", "PRESETS")

@Composable
fun App() {
    var tab by remember { mutableIntStateOf(0) }
    var octave by remember { mutableIntStateOf(3) }
    Column(Modifier.fillMaxSize().background(AppBg).systemBarsPadding()) {
        TopBar()
        if (!Synth.ready) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    CircularProgressIndicator()
                    Text("Building wavetables…", modifier = Modifier.padding(top = 12.dp))
                }
            }
        } else {
            ScrollableTabRow(selectedTabIndex = tab, edgePadding = 0.dp, containerColor = AppBg) {
                TABS.forEachIndexed { i, t ->
                    Tab(selected = tab == i, onClick = { tab = i }, text = { Text(t, fontSize = 12.sp, maxLines = 1) })
                }
            }
            Box(Modifier.weight(1f).fillMaxWidth()) {
                when (tab) {
                    0 -> OscScreen()
                    1 -> FilterScreen()
                    2 -> ModScreen()
                    3 -> FxScreen()
                    4 -> PlayScreen()
                    else -> PresetScreen()
                }
            }
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                TextButton(onClick = { if (octave > 0) octave-- }) { Text("Oct −") }
                Text("C${octave - 1}", fontSize = 12.sp)
                TextButton(onClick = { if (octave < 8) octave++ }) { Text("Oct +") }
                Box(Modifier.weight(1f))
                TextButton(onClick = { Synth.panic() }) { Text("Panic") }
            }
            PianoKeyboard(baseNote = octave * 12)
        }
    }
}

@Composable
private fun TopBar() {
    var scope by remember { mutableStateOf(FloatArray(256)) }
    var peak by remember { mutableFloatStateOf(0f) }
    var voices by remember { mutableIntStateOf(0) }
    LaunchedEffect(Synth.ready) {
        if (Synth.ready) {
            val buf = FloatArray(256)
            while (true) {
                NativeSynth.scope(buf)
                scope = buf.copyOf()
                peak = NativeSynth.peak()
                voices = NativeSynth.voices()
                delay(40)
            }
        }
    }
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        Column(Modifier.weight(1f)) {
            Text("WaveForge", style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.primary)
            Text(Synth.presetName, fontSize = 12.sp, maxLines = 1)
            if (!Synth.audioOk) {
                TextButton(onClick = { Synth.restartAudio() }) { Text("Audio failed - retry", color = Color(0xFFFF6B6B)) }
            }
        }
        Text("$voices v", fontSize = 11.sp)
        Canvas(Modifier.width(110.dp).height(36.dp).background(Color(0xFF101018))) {
            val w = size.width
            val h = size.height
            val path = Path()
            for (i in scope.indices) {
                val x = i.toFloat() / (scope.size - 1) * w
                val y = h / 2f - scope[i].coerceIn(-1f, 1f) * h * 0.45f
                if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            drawPath(path, Accent2, style = Stroke(1.5.dp.toPx()))
        }
        Canvas(Modifier.width(10.dp).height(36.dp).background(Color(0xFF101018))) {
            val p = peak.coerceIn(0f, 1f)
            drawLine(
                if (p > 0.95f) Color(0xFFFF4D4D) else Accent,
                Offset(size.width / 2f, size.height),
                Offset(size.width / 2f, size.height * (1f - p)),
                strokeWidth = size.width
            )
        }
    }
}
