package com.waveforge.synth

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlin.math.sin

/** Value formatters that mirror the engine's parameter mappings. */
object Fmt {
    fun time(x: Float): String {
        val t = 0.001f * 10000f.pow(x)
        return if (t < 1f) "${(t * 1000f).roundToInt()} ms" else String.format("%.2f s", t)
    }
    fun lfoRate(x: Float): String = String.format("%.2f Hz", 0.05f * 600f.pow(x))
    fun cutoff(x: Float): String {
        val f = 20f * 1000f.pow(x)
        return if (f < 1000f) "${f.roundToInt()} Hz" else String.format("%.1f kHz", f / 1000f)
    }
    fun percent(x: Float): String = "${(x * 100f).roundToInt()}%"
    fun signed(x: Float): String = String.format("%+.2f", x)
    fun cents(x: Float): String = "${(x * 100f).roundToInt()} ct"
    fun glide(x: Float): String = if (x < 0.001f) "off" else String.format("%.2f s", 0.003f + x * x * 1.5f)
    fun delayTime(x: Float): String = "${((0.02f + x * x * 1.5f) * 1000f).roundToInt()} ms"
}

@Composable
fun Knob(
    id: Int,
    label: String,
    modifier: Modifier = Modifier,
    fmt: ((Float) -> String)? = null,
) {
    val d = P.defs[id]
    val v = Synth.values[id].floatValue
    val norm = ((v - d.lo) / (d.hi - d.lo)).coerceIn(0f, 1f)
    val accent = MaterialTheme.colorScheme.primary
    Column(
        modifier
            .width(66.dp)
            .pointerInput(id) {
                var acc = 0f
                detectDragGestures(
                    onDragStart = { acc = (Synth.get(id) - d.lo) / (d.hi - d.lo) },
                    onDrag = { change, drag ->
                        change.consume()
                        acc = (acc - drag.y / 240f + drag.x / 480f).coerceIn(0f, 1f)
                        Synth.set(id, d.lo + acc * (d.hi - d.lo))
                    }
                )
            }
            .pointerInput(id) { detectTapGestures(onDoubleTap = { Synth.set(id, d.def) }) },
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(Modifier.size(46.dp)) {
            val stroke = 5.dp.toPx()
            val arcSize = Size(size.width - stroke, size.height - stroke)
            val tl = Offset(stroke / 2f, stroke / 2f)
            drawArc(TrackColor, 135f, 270f, false, tl, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
            val bipolar = d.lo < 0f
            val start = if (bipolar) 270f else 135f
            val sweep = if (bipolar) (norm - 0.5f) * 270f else 270f * norm
            drawArc(accent, start, sweep, false, tl, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
            val ang = Math.toRadians((135f + 270f * norm).toDouble())
            val r = size.width / 2f
            val c = Offset(size.width / 2f, size.height / 2f)
            drawLine(
                Color.White,
                Offset(c.x + (r * 0.25f * cos(ang)).toFloat(), c.y + (r * 0.25f * sin(ang)).toFloat()),
                Offset(c.x + (r * 0.62f * cos(ang)).toFloat(), c.y + (r * 0.62f * sin(ang)).toFloat()),
                strokeWidth = 2.dp.toPx(), cap = StrokeCap.Round
            )
        }
        val text = fmt?.invoke(v) ?: if (d.kind == 'f') String.format("%.2f", v) else v.roundToInt().toString()
        Text(text, fontSize = 10.sp, color = Accent2, maxLines = 1, textAlign = TextAlign.Center)
        Text(label, fontSize = 10.sp, color = Color(0xFFA0A0B8), maxLines = 1, textAlign = TextAlign.Center)
    }
}

@Composable
fun Knob(key: String, label: String, modifier: Modifier = Modifier, fmt: ((Float) -> String)? = null) =
    Knob(P.id(key), label, modifier, fmt)

@Composable
fun Choice(key: String, labels: List<String>, modifier: Modifier = Modifier) {
    val id = P.id(key)
    var open by remember { mutableStateOf(false) }
    val idx = Synth.values[id].floatValue.roundToInt().coerceIn(0, labels.size - 1)
    Box(modifier) {
        OutlinedButton(
            onClick = { open = true },
            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 2.dp)
        ) { Text(labels[idx], fontSize = 12.sp, maxLines = 1) }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            labels.forEachIndexed { i, l ->
                DropdownMenuItem(text = { Text(l) }, onClick = { Synth.set(id, i.toFloat()); open = false })
            }
        }
    }
}

@Composable
fun Toggle(key: String, label: String, modifier: Modifier = Modifier) {
    val id = P.id(key)
    val on = Synth.values[id].floatValue > 0.5f
    FilterChip(
        selected = on,
        onClick = { Synth.set(id, if (on) 0f else 1f) },
        label = { Text(label, fontSize = 12.sp) },
        modifier = modifier
    )
}

@Composable
fun Section(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Card(
        modifier.fillMaxWidth().padding(vertical = 4.dp),
        colors = CardDefaults.cardColors(containerColor = PanelBg)
    ) {
        Column(Modifier.padding(10.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.height(6.dp))
            content()
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun KnobRow(content: @Composable () -> Unit) {
    FlowRow(
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
        modifier = Modifier.fillMaxWidth()
    ) { content() }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ChipRow(content: @Composable () -> Unit) {
    FlowRow(
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp),
        modifier = Modifier.fillMaxWidth()
    ) { content() }
}

/** Layered wavetable display. Drag horizontally to scan through the table. */
@Composable
fun WavetableView(o: String) {
    val table = Synth.getKey("${o}_table").roundToInt()
    val pos = Synth.getKey("${o}_pos")
    val ready = Synth.ready
    val ghosts = remember(table, ready) {
        if (!ready) emptyList() else List(14) { i -> FloatArray(96).also { NativeSynth.tableFrame(table, i / 13f, it) } }
    }
    val cur = remember(table, pos, ready) {
        FloatArray(256).also { if (ready) NativeSynth.tableFrame(table, pos, it) }
    }
    val accent = MaterialTheme.colorScheme.primary
    Canvas(
        Modifier
            .fillMaxWidth()
            .height(120.dp)
            .background(Color(0xFF101018))
            .pointerInput(o) {
                val id = P.id("${o}_pos")
                detectHorizontalDragGestures { change, dx ->
                    change.consume()
                    Synth.set(id, Synth.get(id) + dx / size.width)
                }
            }
    ) {
        val w = size.width
        val h = size.height
        ghosts.forEachIndexed { gi, a ->
            val k = gi / 13f
            val ox = k * w * 0.18f
            val oy = (1f - k) * h * 0.28f + h * 0.08f
            val path = Path()
            for (j in a.indices) {
                val x = ox + j.toFloat() / (a.size - 1) * w * 0.78f
                val y = oy + h * 0.3f - a[j] * h * 0.13f
                if (j == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            drawPath(path, Color(0x40FFFFFF), style = Stroke(1.dp.toPx()))
        }
        val path = Path()
        for (j in cur.indices) {
            val x = j.toFloat() / (cur.size - 1) * w
            val y = h / 2f - cur[j] * h * 0.4f
            if (j == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        drawPath(path, accent, style = Stroke(2.dp.toPx()))
        drawLine(Color(0x33FFFFFF), Offset(0f, h / 2f), Offset(w, h / 2f), 1f)
    }
}

/** Small ADSR shape preview. */
@Composable
fun EnvView(n: Int) {
    val a = Synth.getKey("e${n}_a")
    val d = Synth.getKey("e${n}_d")
    val s = Synth.getKey("e${n}_s")
    val r = Synth.getKey("e${n}_r")
    val accent = MaterialTheme.colorScheme.secondary
    Canvas(Modifier.fillMaxWidth().height(56.dp).background(Color(0xFF101018))) {
        val aw = 0.04f + 0.26f * a
        val dw = 0.04f + 0.26f * d
        val sw = 0.18f
        val rw = 0.04f + 0.26f * r
        val total = aw + dw + sw + rw
        val w = size.width
        val h = size.height
        val pad = 4.dp.toPx()
        fun x(t: Float) = pad + t / total * (w - 2 * pad)
        fun y(v: Float) = h - pad - v * (h - 2 * pad)
        val path = Path()
        path.moveTo(x(0f), y(0f))
        path.lineTo(x(aw), y(1f))
        path.lineTo(x(aw + dw), y(s))
        path.lineTo(x(aw + dw + sw), y(s))
        path.lineTo(x(total), y(0f))
        drawPath(path, accent, style = Stroke(2.dp.toPx()))
    }
}
