package com.example.ui.theme

import androidx.compose.animation.core.Easing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.SpringSpec
import androidx.compose.animation.core.spring

object RunnerMotion {
    const val DURATION_FAST_MS = 130
    const val DURATION_STANDARD_MS = 220
    const val DURATION_LONG_MS = 320

    const val PRESS_SCALE_TARGET = 0.97f
    const val PRESS_SCALE_DEFAULT = 1.0f

    val StandardEasing: Easing = FastOutSlowInEasing

    fun <T> standardSpring(): SpringSpec<T> = spring(
        dampingRatio = Spring.DampingRatioNoBouncy,
        stiffness = Spring.StiffnessMedium
    )
}
