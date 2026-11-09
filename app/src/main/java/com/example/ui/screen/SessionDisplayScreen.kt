package com.example.ui.screen

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.outlined.Gamepad
import androidx.compose.material.icons.outlined.StopCircle
import androidx.compose.material.icons.outlined.Terminal
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.viewinterop.AndroidView
import com.example.R
import com.example.data.RunnerConstants
import com.example.ui.components.SecondarySurfaceButton
import com.example.ui.components.rememberFormattedFileSize
import com.example.ui.theme.RunnerColorTokens
import com.example.ui.theme.RunnerDimens
import com.example.ui.theme.RunnerMotion
import com.example.ui.theme.RunnerShapes
import com.example.viewmodel.RunnerUiState

@Composable
fun SessionDisplayScreen(
    uiState: RunnerUiState,
    onBackToConfig: () -> Unit,
    onStopSession: () -> Unit,
    onToggleTouchOverlay: () -> Unit,
    onToggleLogPanel: () -> Unit,
    onAttachSurface: (Surface, Int, Int) -> Unit,
    onDetachSurface: () -> Unit,
    onSendKeyInput: (Int, Boolean) -> Unit,
    onSendMouseDelta: (Float, Float, Int) -> Unit,
    modifier: Modifier = Modifier
) {
    BackHandler(onBack = onBackToConfig)

    DisposableEffect(Unit) {
        onDispose {
            onDetachSurface()
        }
    }

    val session = uiState.sessionState
    val rssFormatted = rememberFormattedFileSize(session.rssMemoryBytes)

    Box(
        modifier = modifier
            .fillMaxSize()
            .background(RunnerColorTokens.BackgroundBottom)
            .windowInsetsPadding(WindowInsets.safeDrawing)
    ) {
        // Real Native EGL/GLES3 SurfaceView connected to BionicSessionEngine
        AndroidView(
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            val frame = holder.surfaceFrame
                            onAttachSurface(holder.surface, frame.width(), frame.height())
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                            onAttachSurface(holder.surface, width, height)
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            onDetachSurface()
                        }
                    })
                }
            },
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        onSendMouseDelta(dragAmount.x, dragAmount.y, 0)
                    }
                }
                .testTag("native_session_surface_view")
        )

        // Top Bar: Back arrow + Title + Live Telemetry + Controls/Log Toggles
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
                .background(RunnerColorTokens.ModalSurface)
                .padding(
                    horizontal = RunnerDimens.SpacingMedium,
                    vertical = RunnerDimens.SpacingSmall
                )
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = onBackToConfig,
                    modifier = Modifier
                        .size(RunnerDimens.TouchTargetMin)
                        .testTag("display_back_button")
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Outlined.ArrowBack,
                        contentDescription = stringResource(R.string.action_close),
                        tint = RunnerColorTokens.TextPrimary
                    )
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = stringResource(R.string.display_title_active_session),
                        style = MaterialTheme.typography.titleMedium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    val subtitle = if (session.isRunning) {
                        stringResource(
                            R.string.display_status_running,
                            session.winePid,
                            session.surfaceFps,
                            rssFormatted,
                            session.dispatchedInputEvents
                        )
                    } else {
                        stringResource(
                            R.string.display_status_stopped,
                            session.statusText.ifBlank { "IDLE" }
                        )
                    }
                    Text(
                        text = subtitle,
                        style = MaterialTheme.typography.bodyMedium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                IconButton(
                    onClick = onToggleTouchOverlay,
                    modifier = Modifier
                        .size(RunnerDimens.TouchTargetMin)
                        .testTag("toggle_touch_overlay_button")
                ) {
                    Icon(
                        imageVector = Icons.Outlined.Gamepad,
                        contentDescription = stringResource(R.string.action_toggle_controls),
                        tint = if (uiState.showTouchOverlay) {
                            RunnerColorTokens.AccentBlue
                        } else {
                            RunnerColorTokens.TextSecondary
                        }
                    )
                }
                IconButton(
                    onClick = onToggleLogPanel,
                    modifier = Modifier
                        .size(RunnerDimens.TouchTargetMin)
                        .testTag("toggle_log_panel_button")
                ) {
                    Icon(
                        imageVector = Icons.Outlined.Terminal,
                        contentDescription = stringResource(R.string.action_toggle_logs),
                        tint = if (uiState.showSessionLogPanel) {
                            RunnerColorTokens.AccentBlue
                        } else {
                            RunnerColorTokens.TextSecondary
                        }
                    )
                }
                if (session.isRunning) {
                    Spacer(modifier = Modifier.width(RunnerDimens.SpacingXSmall))
                    SecondarySurfaceButton(
                        text = stringResource(R.string.action_stop_session),
                        icon = Icons.Outlined.StopCircle,
                        onClick = onStopSession,
                        testTag = "display_stop_session_button"
                    )
                }
            }

            // Live Process Output Log Panel (Wine / Box64 / FEX / VirGL)
            AnimatedVisibility(
                visible = uiState.showSessionLogPanel,
                enter = fadeIn(tween(RunnerMotion.DURATION_STANDARD_MS)),
                exit = fadeOut(tween(RunnerMotion.DURATION_FAST_MS))
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = RunnerDimens.SpacingSmall)
                        .height(RunnerDimens.LogPanelHeight)
                        .clip(RunnerShapes.ControlPadShape)
                        .background(RunnerColorTokens.SurfaceBase)
                        .border(
                            width = RunnerDimens.BorderThin,
                            color = RunnerColorTokens.BorderSubtle,
                            shape = RunnerShapes.ControlPadShape
                        )
                        .padding(RunnerDimens.SpacingMedium)
                        .testTag("session_log_panel")
                ) {
                    Text(
                        text = stringResource(R.string.label_log_output_header),
                        style = MaterialTheme.typography.labelMedium,
                        color = RunnerColorTokens.TextSecondary
                    )
                    Spacer(modifier = Modifier.height(RunnerDimens.SpacingXSmall))
                    if (session.recentLogs.isEmpty()) {
                        Text(
                            text = stringResource(R.string.label_log_empty),
                            style = MaterialTheme.typography.bodySmall,
                            color = RunnerColorTokens.TextSecondary
                        )
                    } else {
                        LazyColumn(modifier = Modifier.fillMaxSize()) {
                            itemsIndexed(
                                items = session.recentLogs,
                                key = { index, line -> "$index-$line" }
                            ) { _, line ->
                                Text(
                                    text = line,
                                    style = MaterialTheme.typography.bodySmall,
                                    color = RunnerColorTokens.TextPrimary
                                )
                            }
                        }
                    }
                }
            }
        }

        // GTA SA & SA-MP Touch Controller Overlay
        AnimatedVisibility(
            visible = uiState.showTouchOverlay,
            enter = fadeIn(tween(RunnerMotion.DURATION_STANDARD_MS)),
            exit = fadeOut(tween(RunnerMotion.DURATION_FAST_MS)),
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.BottomCenter)
        ) {
            GtaSaTouchControlsOverlay(
                onSendKeyInput = onSendKeyInput,
                onSendMouseDelta = onSendMouseDelta
            )
        }
    }
}

@Composable
private fun GtaSaTouchControlsOverlay(
    onSendKeyInput: (Int, Boolean) -> Unit,
    onSendMouseDelta: (Float, Float, Int) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(RunnerDimens.SpacingLarge),
        verticalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingMedium)
    ) {
        // Top Utility Keys for GTA SA Menu & SA-MP (ESC, Chat T, Scoreboard TAB)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            HoldableKeyPadButton(
                label = stringResource(R.string.control_action_esc),
                onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_ESCAPE, pressed) },
                testTag = "key_btn_esc"
            )
            Spacer(modifier = Modifier.width(RunnerDimens.SpacingSmall))
            HoldableKeyPadButton(
                label = stringResource(R.string.control_action_samp_chat),
                onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_T, pressed) },
                testTag = "key_btn_samp_chat"
            )
            Spacer(modifier = Modifier.width(RunnerDimens.SpacingSmall))
            HoldableKeyPadButton(
                label = stringResource(R.string.control_action_samp_tab),
                onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_TAB, pressed) },
                testTag = "key_btn_samp_tab"
            )
        }

        // Bottom Left WASD D-Pad + Bottom Right Action Cluster
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.Bottom
        ) {
            // WASD Movement Cluster
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingXSmall)
            ) {
                HoldableKeyPadButton(
                    label = stringResource(R.string.control_move_up),
                    onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_W, pressed) },
                    testTag = "key_btn_w"
                )
                Row(horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingXSmall)) {
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_move_left),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_A, pressed) },
                        testTag = "key_btn_a"
                    )
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_move_down),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_S, pressed) },
                        testTag = "key_btn_s"
                    )
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_move_right),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_D, pressed) },
                        testTag = "key_btn_d"
                    )
                }
            }

            // GTA SA Action Buttons (Fire, Aim, Enter Vehicle F, Jump Space, Sprint Shift)
            Column(
                horizontalAlignment = Alignment.End,
                verticalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingXSmall)
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingXSmall)) {
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_action_aim),
                        onPressChanged = { pressed ->
                            onSendMouseDelta(0f, 0f, if (pressed) RunnerConstants.MOUSE_BTN_RIGHT else 0)
                        },
                        testTag = "key_btn_aim"
                    )
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_action_fire),
                        onPressChanged = { pressed ->
                            onSendMouseDelta(0f, 0f, if (pressed) RunnerConstants.MOUSE_BTN_LEFT else 0)
                        },
                        testTag = "key_btn_fire"
                    )
                }
                Row(horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingXSmall)) {
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_action_enter_vehicle),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_KEY_F, pressed) },
                        testTag = "key_btn_f"
                    )
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_action_jump),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_SPACE, pressed) },
                        testTag = "key_btn_space"
                    )
                    HoldableKeyPadButton(
                        label = stringResource(R.string.control_action_sprint),
                        onPressChanged = { pressed -> onSendKeyInput(RunnerConstants.VK_SHIFT, pressed) },
                        testTag = "key_btn_shift"
                    )
                }
            }
        }
    }
}

@Composable
private fun HoldableKeyPadButton(
    label: String,
    onPressChanged: (Boolean) -> Unit,
    testTag: String,
    modifier: Modifier = Modifier
) {
    val haptics = LocalHapticFeedback.current
    var isHeld by remember { mutableStateOf(false) }

    Box(
        modifier = modifier
            .heightIn(min = RunnerDimens.ControlActionHeight)
            .clip(RunnerShapes.ControlPadShape)
            .background(
                if (isHeld) RunnerColorTokens.OverlayButtonActiveBg else RunnerColorTokens.OverlayButtonBg
            )
            .border(
                width = RunnerDimens.BorderThin,
                color = if (isHeld) RunnerColorTokens.AccentBlueBorder else RunnerColorTokens.BorderInteractive,
                shape = RunnerShapes.ControlPadShape
            )
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        isHeld = true
                        haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                        onPressChanged(true)
                        try {
                            awaitRelease()
                        } finally {
                            isHeld = false
                            onPressChanged(false)
                        }
                    }
                )
            }
            .padding(
                horizontal = RunnerDimens.SpacingMedium,
                vertical = RunnerDimens.SpacingSmall
            )
            .testTag(testTag),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = RunnerColorTokens.TextPrimary,
            textAlign = TextAlign.Center,
            maxLines = 1
        )
    }
}
