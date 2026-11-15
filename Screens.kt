package com.waveforge.synth

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
private fun Page(content: @Composable () -> Unit) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 8.dp, vertical = 4.dp)
    ) { content() }
}

// ------------------------------------------------------------------ OSC
@Composable
private fun OscSection(o: String, title: String) {
    Section(title) {
        ChipRow {
            Toggle("${o}_on", "ON")
            Choice("${o}_table", Labels.tables)
            Choice("${o}_wmode", Labels.warp)
        }
        WavetableView(o)
        KnobRow {
            Knob("${o}_pos", "WT Pos", fmt = Fmt::percent)
            Knob("${o}_warp", "Warp", fmt = Fmt::percent)
            Knob("${o}_level", "Level", fmt = Fmt::percent)
            Knob("${o}_pan", "Pan", fmt = Fmt::signed)
            Knob("${o}_oct", "Octave")
            Knob("${o}_semi", "Semi")
            Knob("${o}_fine", "Fine", fmt = Fmt::cents)
            Knob("${o}_phase", "Phase", fmt = Fmt::percent)
            Knob("${o}_rand", "Rand Ph", fmt = Fmt::percent)
        }
        Text("Unison", fontSize = 11.sp, color = Accent2)
        KnobRow {
            Knob("${o}_uni", "Voices")
            Knob("${o}_det", "Detune", fmt = Fmt::percent)
            Knob("${o}_blend", "Blend", fmt = Fmt::percent)
            Knob("${o}_spread", "Stereo", fmt = Fmt::percent)
        }
    }
}

@Composable
fun OscScreen() = Page {
    OscSection("a", "Oscillator A")
    OscSection("b", "Oscillator B")
    Section("Sub & Noise") {
        ChipRow {
            Toggle("sub_on", "SUB")
            Choice("sub_shape", Labels.sub)
            Toggle("noise_on", "NOISE")
            Choice("noise_type", Labels.noise)
        }
        KnobRow {
            Knob("sub_oct", "Sub Oct")
            Knob("sub_level", "Sub Lvl", fmt = Fmt::percent)
            Knob("noise_level", "Noise Lvl", fmt = Fmt::percent)
        }
    }
}

// ------------------------------------------------------------------ FILTER / ENV
@Composable
private fun EnvSection(n: Int, title: String) {
    Section(title) {
        EnvView(n)
        KnobRow {
            Knob("e${n}_a", "Attack", fmt = Fmt::time)
            Knob("e${n}_d", "Decay", fmt = Fmt::time)
            Knob("e${n}_s", "Sustain", fmt = Fmt::percent)
            Knob("e${n}_r", "Release", fmt = Fmt::time)
        }
    }
}

@Composable
fun FilterScreen() = Page {
    Section("Filter") {
        ChipRow {
            Toggle("f_on", "ON")
            Choice("f_type", Labels.filter)
        }
        Text("Routing", fontSize = 11.sp, color = Accent2)
        ChipRow {
            Toggle("f_ra", "Osc A")
            Toggle("f_rb", "Osc B")
            Toggle("f_rs", "Sub")
            Toggle("f_rn", "Noise")
        }
        KnobRow {
            Knob("f_cutoff", "Cutoff", fmt = Fmt::cutoff)
            Knob("f_res", "Reso", fmt = Fmt::percent)
            Knob("f_drive", "Drive", fmt = Fmt::percent)
            Knob("f_mix", "Mix", fmt = Fmt::percent)
            Knob("f_key", "Key Trk", fmt = Fmt::percent)
        }
    }
    EnvSection(1, "Env 1 (Amp)")
    EnvSection(2, "Env 2 (mod source)")
    EnvSection(3, "Env 3 (mod source)")
}

// ------------------------------------------------------------------ MOD
@Composable
private fun LfoSection(n: Int) {
    Section("LFO $n") {
        ChipRow {
            Choice("l${n}_shape", Labels.lfoShapes)
            Toggle("l${n}_sync", "Sync")
            Toggle("l${n}_retrig", "Trig")
            if (Synth.getKey("l${n}_sync") > 0.5f) Choice("l${n}_div", Labels.divs)
        }
        KnobRow {
            if (Synth.getKey("l${n}_sync") <= 0.5f) Knob("l${n}_rate", "Rate", fmt = Fmt::lfoRate)
            Knob("l${n}_phase", "Phase", fmt = Fmt::percent)
        }
    }
}

@Composable
fun ModScreen() = Page {
    for (n in 1..4) LfoSection(n)
    Section("Mod Matrix") {
        for (n in 1..8) {
            Column(Modifier.padding(vertical = 3.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text("$n", fontSize = 12.sp, color = Accent2)
                    Choice("m${n}_src", Labels.modSrc)
                    Text("→", fontSize = 12.sp)
                    Choice("m${n}_dst", Labels.modDst)
                }
                val id = P.id("m${n}_amt")
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Slider(
                        value = Synth.get(id),
                        onValueChange = { Synth.set(id, it) },
                        valueRange = -1f..1f,
                        modifier = Modifier.weight(1f)
                    )
                    Text(Fmt.signed(Synth.get(id)), fontSize = 11.sp, modifier = Modifier.padding(start = 6.dp))
                }
            }
        }
    }
}

// ------------------------------------------------------------------ FX
@Composable
fun FxScreen() = Page {
    Section("Distortion") {
        ChipRow { Toggle("dist_on", "ON"); Choice("dist_type", Labels.dist) }
        KnobRow {
            Knob("dist_amt", "Drive", fmt = Fmt::percent)
            Knob("dist_mix", "Mix", fmt = Fmt::percent)
        }
    }
    Section("Chorus") {
        ChipRow { Toggle("ch_on", "ON") }
        KnobRow {
            Knob("ch_rate", "Rate", fmt = Fmt::percent)
            Knob("ch_depth", "Depth", fmt = Fmt::percent)
            Knob("ch_mix", "Mix", fmt = Fmt::percent)
        }
    }
    Section("Delay (ping-pong)") {
        ChipRow {
            Toggle("dl_on", "ON")
            Toggle("dl_sync", "Sync")
            if (Synth.getKey("dl_sync") > 0.5f) Choice("dl_div", Labels.divs)
        }
        KnobRow {
            if (Synth.getKey("dl_sync") <= 0.5f) Knob("dl_time", "Time", fmt = Fmt::delayTime)
            Knob("dl_fb", "Feedback", fmt = Fmt::percent)
            Knob("dl_tone", "Tone", fmt = Fmt::percent)
            Knob("dl_mix", "Mix", fmt = Fmt::percent)
        }
    }
    Section("Reverb") {
        ChipRow { Toggle("rv_on", "ON") }
        KnobRow {
            Knob("rv_size", "Size", fmt = Fmt::percent)
            Knob("rv_damp", "Damp", fmt = Fmt::percent)
            Knob("rv_mix", "Mix", fmt = Fmt::percent)
        }
    }
}

// ------------------------------------------------------------------ PLAY / GLOBAL
@Composable
fun PlayScreen() = Page {
    Section("Macros") {
        KnobRow {
            for (i in 1..4) Knob("macro$i", "Macro $i", fmt = Fmt::percent)
        }
        Text("Assign macros in the Mod Matrix. MIDI CC 20-23 control Macro 1-4.", fontSize = 11.sp)
    }
    Section("Voice") {
        ChipRow { Toggle("mono", "Mono"); Toggle("legato", "Legato") }
        KnobRow {
            Knob("voices", "Poly")
            Knob("glide", "Glide", fmt = Fmt::glide)
            Knob("bend", "Bend Rng")
            Knob("master", "Master", fmt = Fmt::percent)
            Knob("bpm", "BPM")
        }
    }
    Section("Arpeggiator") {
        ChipRow {
            Toggle("arp_on", "ON")
            Choice("arp_rate", Labels.arpRate)
            Choice("arp_pat", Labels.arpPat)
        }
        KnobRow {
            Knob("arp_oct", "Octaves")
            Knob("arp_gate", "Gate", fmt = Fmt::percent)
        }
    }
    Section("MIDI") {
        val devs = MidiInput.devices
        Text(
            if (devs.isEmpty()) "No external MIDI controller connected. Other apps can send MIDI to the virtual device \"WaveForge Synth\"."
            else "Connected: " + devs.joinToString(", "),
            fontSize = 12.sp
        )
        Button(onClick = { Synth.panic() }) { Text("All notes off") }
    }
}

// ------------------------------------------------------------------ PRESETS
@Composable
private fun PresetRow(name: String, onLoad: () -> Unit, onDelete: (() -> Unit)? = null) {
    Row(
        Modifier.fillMaxWidth().clickable { onLoad() }.padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(name, modifier = Modifier.weight(1f), color = if (name == Synth.presetName) Accent2 else androidx.compose.ui.graphics.Color.Unspecified)
        if (onDelete != null) TextButton(onClick = onDelete) { Text("Delete") }
    }
}

@Composable
fun PresetScreen() {
    val ctx = LocalContext.current
    val factory = remember { Presets.factory(ctx) }
    var user by remember { mutableStateOf(Presets.user(ctx)) }
    var name by remember { mutableStateOf("") }
    Page {
        Section("Save current sound") {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = name, onValueChange = { name = it }, singleLine = true,
                    label = { Text("Preset name") }, modifier = Modifier.weight(1f)
                )
                Button(onClick = {
                    if (name.isNotBlank()) { Presets.save(ctx, name.trim()); user = Presets.user(ctx); name = "" }
                }) { Text("Save") }
            }
        }
        if (user.isNotEmpty()) {
            Section("User presets") {
                user.forEach { p ->
                    PresetRow(p.name, { Synth.loadPreset(p.name, p.params) }) {
                        Presets.delete(ctx, p.name); user = Presets.user(ctx)
                    }
                }
            }
        }
        Section("Factory presets") {
            factory.forEach { p -> PresetRow(p.name, { Synth.loadPreset(p.name, p.params) }) }
        }
    }
}
