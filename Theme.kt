package com.waveforge.synth

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Accent = Color(0xFF8B6CFF)
val Accent2 = Color(0xFF3DDCFF)
val PanelBg = Color(0xFF1B1B28)
val AppBg = Color(0xFF0E0E16)
val TrackColor = Color(0xFF30303F)

@Composable
fun WaveForgeTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Accent,
            secondary = Accent2,
            background = AppBg,
            surface = PanelBg,
            onPrimary = Color.White,
            onSurface = Color(0xFFE6E6F2),
            onBackground = Color(0xFFE6E6F2),
        ),
        content = content
    )
}
