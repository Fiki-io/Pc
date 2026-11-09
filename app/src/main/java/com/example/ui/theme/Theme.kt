package com.example.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val RunnerDarkColorScheme = darkColorScheme(
    primary = RunnerColorTokens.AccentBlue,
    onPrimary = RunnerColorTokens.TextPrimary,
    secondary = RunnerColorTokens.InteractiveSecondary,
    onSecondary = RunnerColorTokens.TextPrimary,
    background = RunnerColorTokens.BackgroundBottom,
    onBackground = RunnerColorTokens.TextPrimary,
    surface = RunnerColorTokens.SurfaceBase,
    onSurface = RunnerColorTokens.TextPrimary,
    surfaceVariant = RunnerColorTokens.CardSolidFallback,
    onSurfaceVariant = RunnerColorTokens.TextSecondary,
    outline = RunnerColorTokens.BorderSubtle,
    scrim = Color.Black
)

@Composable
fun MyApplicationTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = RunnerDarkColorScheme,
        typography = Typography,
        shapes = RunnerShapes.MaterialShapes,
        content = content
    )
}
