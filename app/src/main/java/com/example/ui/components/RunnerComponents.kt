package com.example.ui.components

import android.text.format.Formatter
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.KeyboardArrowRight
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.window.Dialog
import com.example.R
import com.example.ui.theme.RunnerColorTokens
import com.example.ui.theme.RunnerDimens
import com.example.ui.theme.RunnerMotion
import com.example.ui.theme.RunnerShapes

@Composable
fun rememberFormattedFileSize(bytes: Long): String {
    val context = LocalContext.current
    return remember(bytes, context) {
        Formatter.formatFileSize(context, bytes)
    }
}

@Composable
fun SectionGroupCard(
    title: String,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            text = title,
            style = MaterialTheme.typography.labelMedium,
            color = RunnerColorTokens.TextSecondary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(
                start = RunnerDimens.SpacingSmall,
                bottom = RunnerDimens.SpacingSmall
            )
        )
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RunnerShapes.CardShape)
                .background(RunnerColorTokens.SurfaceBase)
                .background(RunnerColorTokens.CardOverlay)
                .border(
                    width = RunnerDimens.BorderThin,
                    color = RunnerColorTokens.BorderSubtle,
                    shape = RunnerShapes.CardShape
                ),
            content = content
        )
    }
}

@Composable
fun GroupRowDivider() {
    HorizontalDivider(
        modifier = Modifier.padding(start = RunnerDimens.DividerInset),
        thickness = RunnerDimens.BorderThin,
        color = RunnerColorTokens.DividerSubtle
    )
}

@Composable
fun InfoStatusRow(
    icon: ImageVector,
    label: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .heightIn(min = RunnerDimens.RowMinHeightSingle)
            .padding(
                horizontal = RunnerDimens.SpacingLarge,
                vertical = RunnerDimens.SpacingMedium
            )
            .semantics(mergeDescendants = true) {},
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = RunnerColorTokens.TextSecondary,
            modifier = Modifier.size(RunnerDimens.IconSizeStandard)
        )
        Spacer(modifier = Modifier.width(RunnerDimens.SpacingMedium))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = RunnerColorTokens.TextPrimary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = value,
                style = MaterialTheme.typography.bodyMedium,
                color = RunnerColorTokens.TextSecondary,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
fun NavigationSelectorRow(
    icon: ImageVector,
    label: String,
    currentValue: String,
    onClick: () -> Unit,
    testTag: String,
    modifier: Modifier = Modifier
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()
    val scale by animateFloatAsState(
        targetValue = if (isPressed) RunnerMotion.PRESS_SCALE_TARGET else RunnerMotion.PRESS_SCALE_DEFAULT,
        animationSpec = tween(
            durationMillis = RunnerMotion.DURATION_FAST_MS,
            easing = RunnerMotion.StandardEasing
        ),
        label = "nav_row_scale"
    )

    Row(
        modifier = modifier
            .fillMaxWidth()
            .heightIn(min = RunnerDimens.RowMinHeightSingle)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .clickable(
                interactionSource = interactionSource,
                indication = androidx.compose.material3.ripple(),
                role = Role.Button,
                onClick = onClick
            )
            .padding(
                horizontal = RunnerDimens.SpacingLarge,
                vertical = RunnerDimens.SpacingMedium
            )
            .semantics(mergeDescendants = true) {}
            .testTag(testTag),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = RunnerColorTokens.TextSecondary,
            modifier = Modifier.size(RunnerDimens.IconSizeStandard)
        )
        Spacer(modifier = Modifier.width(RunnerDimens.SpacingMedium))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = RunnerColorTokens.TextPrimary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = currentValue,
                style = MaterialTheme.typography.bodyMedium,
                color = RunnerColorTokens.TextSecondary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        Spacer(modifier = Modifier.width(RunnerDimens.SpacingSmall))
        Icon(
            imageVector = Icons.AutoMirrored.Outlined.KeyboardArrowRight,
            contentDescription = null,
            tint = RunnerColorTokens.TextSecondary,
            modifier = Modifier.size(RunnerDimens.IconSizeStandard)
        )
    }
}

@Composable
fun ToggleSettingRow(
    label: String,
    description: String,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
    testTag: String,
    modifier: Modifier = Modifier
) {
    val haptics = LocalHapticFeedback.current
    Row(
        modifier = modifier
            .fillMaxWidth()
            .heightIn(min = RunnerDimens.RowMinHeightWithDesc)
            .clickable(
                enabled = enabled,
                role = Role.Switch,
                onClick = {
                    haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onCheckedChange(!checked)
                }
            )
            .padding(
                horizontal = RunnerDimens.SpacingLarge,
                vertical = RunnerDimens.SpacingMedium
            )
            .semantics(mergeDescendants = true) {}
            .testTag(testTag),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = if (enabled) RunnerColorTokens.TextPrimary else RunnerColorTokens.TextDisabled,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Spacer(modifier = Modifier.height(RunnerDimens.SpacingXSmall))
            Text(
                text = description,
                style = MaterialTheme.typography.bodyMedium,
                color = RunnerColorTokens.TextSecondary,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
        Spacer(modifier = Modifier.width(RunnerDimens.SpacingMedium))
        Switch(
            checked = checked,
            onCheckedChange = null,
            enabled = enabled,
            colors = SwitchDefaults.colors(
                checkedThumbColor = Color.White,
                checkedTrackColor = RunnerColorTokens.AccentBlue,
                uncheckedThumbColor = RunnerColorTokens.TextSecondary,
                uncheckedTrackColor = RunnerColorTokens.InteractiveSecondary,
                uncheckedBorderColor = RunnerColorTokens.BorderInteractive
            )
        )
    }
}

@Composable
fun PrimaryAccentButton(
    text: String,
    icon: ImageVector? = null,
    enabled: Boolean = true,
    onClick: () -> Unit,
    testTag: String,
    modifier: Modifier = Modifier
) {
    val haptics = LocalHapticFeedback.current
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()
    val scale by animateFloatAsState(
        targetValue = if (isPressed) RunnerMotion.PRESS_SCALE_TARGET else RunnerMotion.PRESS_SCALE_DEFAULT,
        animationSpec = tween(
            durationMillis = RunnerMotion.DURATION_FAST_MS,
            easing = RunnerMotion.StandardEasing
        ),
        label = "primary_btn_scale"
    )

    Button(
        onClick = {
            haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
            onClick()
        },
        enabled = enabled,
        interactionSource = interactionSource,
        shape = RunnerShapes.ButtonShape,
        border = BorderStroke(RunnerDimens.BorderThin, RunnerColorTokens.AccentBlueBorder),
        colors = ButtonDefaults.buttonColors(
            containerColor = RunnerColorTokens.AccentBlue,
            contentColor = RunnerColorTokens.TextPrimary,
            disabledContainerColor = RunnerColorTokens.InteractiveSecondary,
            disabledContentColor = RunnerColorTokens.TextDisabled
        ),
        modifier = modifier
            .heightIn(min = RunnerDimens.TouchTargetMin)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .testTag(testTag)
    ) {
        if (icon != null) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                modifier = Modifier.size(RunnerDimens.IconSizeSmall)
            )
            Spacer(modifier = Modifier.width(RunnerDimens.SpacingSmall))
        }
        Text(
            text = text,
            style = MaterialTheme.typography.bodyLarge,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
fun SecondarySurfaceButton(
    text: String,
    icon: ImageVector? = null,
    enabled: Boolean = true,
    onClick: () -> Unit,
    testTag: String,
    modifier: Modifier = Modifier
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()
    val scale by animateFloatAsState(
        targetValue = if (isPressed) RunnerMotion.PRESS_SCALE_TARGET else RunnerMotion.PRESS_SCALE_DEFAULT,
        animationSpec = tween(
            durationMillis = RunnerMotion.DURATION_FAST_MS,
            easing = RunnerMotion.StandardEasing
        ),
        label = "secondary_btn_scale"
    )

    Button(
        onClick = onClick,
        enabled = enabled,
        interactionSource = interactionSource,
        shape = RunnerShapes.ButtonShape,
        border = BorderStroke(RunnerDimens.BorderThin, RunnerColorTokens.BorderInteractive),
        colors = ButtonDefaults.buttonColors(
            containerColor = RunnerColorTokens.InteractiveSecondary,
            contentColor = RunnerColorTokens.TextPrimary,
            disabledContainerColor = RunnerColorTokens.SurfaceBase,
            disabledContentColor = RunnerColorTokens.TextDisabled
        ),
        modifier = modifier
            .heightIn(min = RunnerDimens.TouchTargetMin)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .testTag(testTag)
    ) {
        if (icon != null) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                modifier = Modifier.size(RunnerDimens.IconSizeSmall)
            )
            Spacer(modifier = Modifier.width(RunnerDimens.SpacingSmall))
        }
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = if (enabled) RunnerColorTokens.TextPrimary else RunnerColorTokens.TextDisabled,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

data class SheetChoiceItem<T>(
    val value: T,
    val label: String,
    val testTag: String
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun <T> SingleChoiceBottomSheet(
    title: String,
    items: List<SheetChoiceItem<T>>,
    selectedValue: T,
    onSelect: (T) -> Unit,
    onDismiss: () -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        shape = RunnerShapes.BottomSheetTopShape,
        containerColor = RunnerColorTokens.ModalSurface,
        scrimColor = RunnerColorTokens.ScrimDark,
        dragHandle = null
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .drawBehind {
                    drawRect(
                        brush = Brush.radialGradient(
                            colors = listOf(
                                RunnerColorTokens.RadialGlowTop,
                                RunnerColorTokens.RadialGlowTransparent
                            ),
                            center = Offset(size.width * 0.5f, 0f),
                            radius = RunnerDimens.RadialGlowRadius
                        )
                    )
                }
                .padding(bottom = RunnerDimens.SpacingXLarge)
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        horizontal = RunnerDimens.SpacingLarge,
                        vertical = RunnerDimens.SpacingMedium
                    )
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    color = RunnerColorTokens.TextPrimary,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.align(Alignment.Center)
                )
                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .align(Alignment.CenterEnd)
                        .size(RunnerDimens.TouchTargetMin)
                        .testTag("sheet_close_button")
                ) {
                    Icon(
                        imageVector = Icons.Outlined.Close,
                        contentDescription = stringResource(R.string.action_close),
                        tint = RunnerColorTokens.TextSecondary
                    )
                }
            }

            HorizontalDivider(
                thickness = RunnerDimens.BorderThin,
                color = RunnerColorTokens.DividerSubtle
            )

            items.forEachIndexed { index, item ->
                val isSelected = item.value == selectedValue
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = RunnerDimens.RowMinHeightSingle)
                        .clickable(
                            role = Role.RadioButton,
                            onClick = { onSelect(item.value) }
                        )
                        .padding(
                            horizontal = RunnerDimens.SpacingLarge,
                            vertical = RunnerDimens.SpacingMedium
                        )
                        .semantics(mergeDescendants = true) {}
                        .testTag(item.testTag),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = item.label,
                        style = MaterialTheme.typography.bodyLarge,
                        color = if (isSelected) RunnerColorTokens.TextPrimary else RunnerColorTokens.TextSecondary,
                        modifier = Modifier.weight(1f)
                    )
                    if (isSelected) {
                        Icon(
                            imageVector = Icons.Filled.Check,
                            contentDescription = null,
                            tint = RunnerColorTokens.TextPrimary,
                            modifier = Modifier.size(RunnerDimens.IconSizeStandard)
                        )
                    }
                }
                if (index < items.lastIndex) {
                    GroupRowDivider()
                }
            }
        }
    }
}

@Composable
fun ConfirmationModalDialog(
    title: String,
    description: String,
    confirmText: String,
    cancelText: String,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RunnerShapes.DialogShape)
                .background(RunnerColorTokens.ModalSurface)
                .border(
                    width = RunnerDimens.BorderThin,
                    color = RunnerColorTokens.BorderInteractive,
                    shape = RunnerShapes.DialogShape
                )
                .drawBehind {
                    drawRect(
                        brush = Brush.radialGradient(
                            colors = listOf(
                                RunnerColorTokens.RadialGlowTop,
                                RunnerColorTokens.RadialGlowTransparent
                            ),
                            center = Offset(size.width * 0.5f, 0f),
                            radius = RunnerDimens.RadialGlowRadius
                        )
                    )
                }
                .padding(RunnerDimens.SpacingLarge)
        ) {
            Box(modifier = Modifier.fillMaxWidth()) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    color = RunnerColorTokens.TextPrimary,
                    textAlign = TextAlign.Center,
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(horizontal = RunnerDimens.TouchTargetMin)
                )
                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .size(RunnerDimens.TouchTargetMin)
                        .testTag("dialog_close_button")
                ) {
                    Icon(
                        imageVector = Icons.Outlined.Close,
                        contentDescription = stringResource(R.string.action_close),
                        tint = RunnerColorTokens.TextSecondary
                    )
                }
            }
            Spacer(modifier = Modifier.height(RunnerDimens.SpacingSmall))
            Text(
                text = description,
                style = MaterialTheme.typography.bodyMedium,
                color = RunnerColorTokens.TextSecondary,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(modifier = Modifier.height(RunnerDimens.SpacingLarge))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingMedium)
            ) {
                SecondarySurfaceButton(
                    text = cancelText,
                    onClick = onDismiss,
                    testTag = "dialog_cancel_button",
                    modifier = Modifier.weight(1f)
                )
                PrimaryAccentButton(
                    text = confirmText,
                    onClick = onConfirm,
                    testTag = "dialog_confirm_button",
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}
