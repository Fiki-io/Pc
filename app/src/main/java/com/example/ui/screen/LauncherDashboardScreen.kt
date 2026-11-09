package com.example.ui.screen

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.outlined.Archive
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.CloudDownload
import androidx.compose.material.icons.outlined.DeveloperBoard
import androidx.compose.material.icons.outlined.FolderOpen
import androidx.compose.material.icons.outlined.Memory
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.SettingsSuggest
import androidx.compose.material.icons.outlined.Storage
import androidx.compose.material.icons.outlined.Terminal
import androidx.compose.material.icons.outlined.Tv
import androidx.compose.material.icons.outlined.Verified
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import com.example.R
import com.example.data.CpuTranslatorOption
import com.example.data.MaliGraphicsBackend
import com.example.data.RenderResolutionOption
import com.example.ui.components.ConfirmationModalDialog
import com.example.ui.components.GroupRowDivider
import com.example.ui.components.InfoStatusRow
import com.example.ui.components.NavigationSelectorRow
import com.example.ui.components.PrimaryAccentButton
import com.example.ui.components.SecondarySurfaceButton
import com.example.ui.components.SectionGroupCard
import com.example.ui.components.SheetChoiceItem
import com.example.ui.components.SingleChoiceBottomSheet
import com.example.ui.components.ToggleSettingRow
import com.example.ui.components.rememberFormattedFileSize
import com.example.ui.theme.RunnerColorTokens
import com.example.ui.theme.RunnerDimens
import com.example.ui.theme.RunnerMotion
import com.example.ui.theme.RunnerShapes
import com.example.viewmodel.ActiveSheetType
import com.example.viewmodel.RunnerUiState
import java.util.Locale

@Composable
fun LauncherDashboardScreen(
    uiState: RunnerUiState,
    onPickRuntimeArchive: () -> Unit,
    onDownloadGithubRuntime: () -> Unit,
    onCancelDownloadRuntime: () -> Unit,
    onRequestRemoveRuntime: () -> Unit,
    onConfirmRemoveRuntime: () -> Unit,
    onDismissRemoveRuntimeDialog: () -> Unit,
    onPickGtaSaExe: () -> Unit,
    onToggleLaaFlag: (Boolean) -> Unit,
    onPickGta3Img: () -> Unit,
    onReprobeGpu: () -> Unit,
    onOpenSheet: (ActiveSheetType) -> Unit,
    onDismissSheet: () -> Unit,
    onSelectTranslator: (CpuTranslatorOption) -> Unit,
    onSelectMaliBackend: (MaliGraphicsBackend) -> Unit,
    onSelectResolution: (RenderResolutionOption) -> Unit,
    onToggleDxvk: (Boolean) -> Unit,
    onToggleStrongMem: (Boolean) -> Unit,
    onToggleSafeFlags: (Boolean) -> Unit,
    onToggleCallRet: (Boolean) -> Unit,
    onToggleEsync: (Boolean) -> Unit,
    onLaunchGame: () -> Unit,
    onOpenActiveDisplay: () -> Unit,
    onStopActiveSession: () -> Unit,
    onDismissBanner: () -> Unit,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    Box(
        modifier = modifier
            .fillMaxSize()
            .background(
                brush = Brush.verticalGradient(
                    colors = listOf(
                        RunnerColorTokens.BackgroundTop,
                        RunnerColorTokens.BackgroundBottom
                    )
                )
            )
            .windowInsetsPadding(WindowInsets.safeDrawing)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(
                    horizontal = RunnerDimens.ScreenHorizontalMargin,
                    vertical = RunnerDimens.ScreenVerticalPadding
                ),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .widthIn(max = RunnerDimens.MaxContentWidth)
                    .animateContentSize(
                        animationSpec = tween(
                            durationMillis = RunnerMotion.DURATION_STANDARD_MS,
                            easing = RunnerMotion.StandardEasing
                        )
                    ),
                verticalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingLarge)
            ) {
                // Top Header & Primary Launch Action
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = stringResource(R.string.header_title_runner),
                            style = MaterialTheme.typography.headlineSmall,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                        Text(
                            text = stringResource(R.string.header_subtitle_architecture),
                            style = MaterialTheme.typography.bodyMedium,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                    Spacer(modifier = Modifier.width(RunnerDimens.SpacingMedium))
                    if (uiState.sessionState.isRunning) {
                        Row(horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingSmall)) {
                            SecondarySurfaceButton(
                                text = stringResource(R.string.action_stop_session),
                                onClick = onStopActiveSession,
                                testTag = "stop_session_top_button"
                            )
                            PrimaryAccentButton(
                                text = stringResource(R.string.action_open_display),
                                icon = Icons.Outlined.Tv,
                                onClick = onOpenActiveDisplay,
                                testTag = "open_display_top_button"
                            )
                        }
                    } else {
                        PrimaryAccentButton(
                            text = stringResource(R.string.action_launch_game),
                            icon = Icons.Filled.PlayArrow,
                            onClick = onLaunchGame,
                            testTag = "launch_gtasa_button"
                        )
                    }
                }

                // Status / Error Banner (Only shown when real message exists)
                AnimatedVisibility(
                    visible = uiState.statusBannerMessage != null,
                    enter = fadeIn(tween(RunnerMotion.DURATION_STANDARD_MS)),
                    exit = fadeOut(tween(RunnerMotion.DURATION_FAST_MS))
                ) {
                    val msg = uiState.statusBannerMessage.orEmpty()
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RunnerShapes.CardShape)
                            .background(RunnerColorTokens.InteractiveSecondary)
                            .border(
                                width = RunnerDimens.BorderThin,
                                color = RunnerColorTokens.BorderInteractive,
                                shape = RunnerShapes.CardShape
                            )
                            .padding(
                                horizontal = RunnerDimens.SpacingLarge,
                                vertical = RunnerDimens.SpacingMedium
                            )
                            .testTag("status_feedback_banner"),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = msg,
                            style = MaterialTheme.typography.bodyMedium,
                            color = RunnerColorTokens.TextPrimary,
                            modifier = Modifier.weight(1f)
                        )
                        IconButton(
                            onClick = onDismissBanner,
                            modifier = Modifier
                                .size(RunnerDimens.TouchTargetMin)
                                .testTag("dismiss_banner_button")
                        ) {
                            Icon(
                                imageVector = Icons.Outlined.Close,
                                contentDescription = stringResource(R.string.action_close),
                                tint = RunnerColorTokens.TextSecondary
                            )
                        }
                    }
                }

                // Section 1: Paket Runtime Bionic ARM64
                RuntimeSectionCard(
                    uiState = uiState,
                    onPickRuntimeArchive = onPickRuntimeArchive,
                    onDownloadGithubRuntime = onDownloadGithubRuntime,
                    onCancelDownloadRuntime = onCancelDownloadRuntime
                )

                // Section 2: Instalasi GTA San Andreas PC
                GameInstallationSectionCard(
                    uiState = uiState,
                    onPickGtaSaExe = onPickGtaSaExe,
                    onToggleLaaFlag = onToggleLaaFlag,
                    onPickGta3Img = onPickGta3Img
                )

                // Section 3: Profil Eksekusi & Grafis Mali
                ExecutionProfileSectionCard(
                    uiState = uiState,
                    onOpenSheet = onOpenSheet,
                    onToggleDxvk = onToggleDxvk,
                    onToggleStrongMem = onToggleStrongMem,
                    onToggleSafeFlags = onToggleSafeFlags,
                    onToggleCallRet = onToggleCallRet,
                    onToggleEsync = onToggleEsync
                )

                // Section 4: Deteksi Perangkat Keras GPU
                GpuHardwareSectionCard(
                    uiState = uiState,
                    onReprobeGpu = onReprobeGpu
                )

                // Separate Neutral Action Card for Removing Installed Runtime
                if (uiState.runtimeState.isVerified) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(min = RunnerDimens.RowMinHeightSingle)
                            .clip(RunnerShapes.CardShape)
                            .background(RunnerColorTokens.SurfaceBase)
                            .background(RunnerColorTokens.CardOverlay)
                            .border(
                                width = RunnerDimens.BorderThin,
                                color = RunnerColorTokens.BorderSubtle,
                                shape = RunnerShapes.CardShape
                            )
                            .clickable(
                                role = Role.Button,
                                onClick = onRequestRemoveRuntime
                            )
                            .padding(RunnerDimens.SpacingMedium)
                            .testTag("remove_runtime_card_button"),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = stringResource(R.string.action_remove_runtime),
                            style = MaterialTheme.typography.bodyLarge,
                            color = RunnerColorTokens.TextSecondary,
                            textAlign = TextAlign.Center
                        )
                    }
                }
            }
        }
    }

    // Bottom Sheets for Single-Choice Settings
    when (uiState.activeSheet) {
        ActiveSheetType.CPU_TRANSLATOR -> {
            SingleChoiceBottomSheet(
                title = stringResource(R.string.sheet_title_select_translator),
                items = listOf(
                    SheetChoiceItem(
                        value = CpuTranslatorOption.BOX64_WOW64,
                        label = stringResource(R.string.value_translator_box64),
                        testTag = "option_translator_box64"
                    ),
                    SheetChoiceItem(
                        value = CpuTranslatorOption.FEX_WOW64,
                        label = stringResource(R.string.value_translator_fex),
                        testTag = "option_translator_fex"
                    )
                ),
                selectedValue = uiState.profile.cpuTranslator,
                onSelect = onSelectTranslator,
                onDismiss = onDismissSheet
            )
        }

        ActiveSheetType.MALI_BACKEND -> {
            SingleChoiceBottomSheet(
                title = stringResource(R.string.sheet_title_select_backend),
                items = listOf(
                    SheetChoiceItem(
                        value = MaliGraphicsBackend.VIRGL_GLES32,
                        label = stringResource(R.string.value_backend_virgl),
                        testTag = "option_backend_virgl"
                    ),
                    SheetChoiceItem(
                        value = MaliGraphicsBackend.ZINK_VULKAN,
                        label = stringResource(R.string.value_backend_zink),
                        testTag = "option_backend_zink"
                    )
                ),
                selectedValue = uiState.profile.maliBackend,
                onSelect = onSelectMaliBackend,
                onDismiss = onDismissSheet
            )
        }

        ActiveSheetType.RENDER_RESOLUTION -> {
            SingleChoiceBottomSheet(
                title = stringResource(R.string.sheet_title_select_resolution),
                items = listOf(
                    SheetChoiceItem(
                        value = RenderResolutionOption.RES_540P,
                        label = stringResource(R.string.value_res_960_540),
                        testTag = "option_res_540p"
                    ),
                    SheetChoiceItem(
                        value = RenderResolutionOption.RES_720P,
                        label = stringResource(R.string.value_res_1280_720),
                        testTag = "option_res_720p"
                    ),
                    SheetChoiceItem(
                        value = RenderResolutionOption.RES_900P,
                        label = stringResource(R.string.value_res_1600_900),
                        testTag = "option_res_900p"
                    ),
                    SheetChoiceItem(
                        value = RenderResolutionOption.RES_1080P,
                        label = stringResource(R.string.value_res_1920_1080),
                        testTag = "option_res_1080p"
                    )
                ),
                selectedValue = uiState.profile.resolution,
                onSelect = onSelectResolution,
                onDismiss = onDismissSheet
            )
        }

        ActiveSheetType.NONE -> Unit
    }

    if (uiState.showRemoveRuntimeDialog) {
        ConfirmationModalDialog(
            title = stringResource(R.string.dialog_remove_runtime_title),
            description = stringResource(R.string.dialog_remove_runtime_desc),
            confirmText = stringResource(R.string.action_confirm_delete),
            cancelText = stringResource(R.string.action_cancel),
            onConfirm = onConfirmRemoveRuntime,
            onDismiss = onDismissRemoveRuntimeDialog
        )
    }
}

@Composable
private fun RuntimeSectionCard(
    uiState: RunnerUiState,
    onPickRuntimeArchive: () -> Unit,
    onDownloadGithubRuntime: () -> Unit,
    onCancelDownloadRuntime: () -> Unit
) {
    val rt = uiState.runtimeState
    val downloadedFormatted = rememberFormattedFileSize(uiState.downloadProgressBytes)
    val totalFormatted = rememberFormattedFileSize(
        if (uiState.downloadTotalBytes > 0L) uiState.downloadTotalBytes else uiState.downloadProgressBytes
    )

    val statusText = when {
        uiState.isExtractingRuntime -> stringResource(R.string.status_runtime_extracting)
        uiState.isDownloadingRuntime -> {
            val pct = if (uiState.downloadTotalBytes > 0L) {
                ((uiState.downloadProgressBytes * 100L) / uiState.downloadTotalBytes).toInt()
            } else {
                0
            }
            stringResource(R.string.status_runtime_downloading, pct, downloadedFormatted, totalFormatted)
        }
        rt.isVerified -> stringResource(R.string.status_runtime_verified, rt.verifiedModuleCount)
        rt.errorMessage.isNotBlank() -> rt.errorMessage
        else -> stringResource(R.string.status_runtime_not_installed)
    }

    SectionGroupCard(title = stringResource(R.string.section_bionic_runtime)) {
        InfoStatusRow(
            icon = Icons.Outlined.Verified,
            label = stringResource(R.string.label_runtime_status),
            value = statusText
        )

        if (uiState.isDownloadingRuntime || uiState.isExtractingRuntime) {
            GroupRowDivider()
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        horizontal = RunnerDimens.SpacingLarge,
                        vertical = RunnerDimens.SpacingMedium
                    )
            ) {
                if (uiState.isDownloadingRuntime && uiState.downloadTotalBytes > 0L) {
                    val rawFraction = (uiState.downloadProgressBytes.toFloat() /
                        uiState.downloadTotalBytes.toFloat()).coerceIn(0f, 1f)
                    val animatedProgress by animateFloatAsState(
                        targetValue = rawFraction,
                        animationSpec = tween(
                            durationMillis = RunnerMotion.DURATION_FAST_MS,
                            easing = RunnerMotion.StandardEasing
                        ),
                        label = "download_progress"
                    )
                    LinearProgressIndicator(
                        progress = { animatedProgress },
                        color = RunnerColorTokens.AccentBlue,
                        trackColor = RunnerColorTokens.InteractiveSecondary,
                        modifier = Modifier
                            .fillMaxWidth()
                            .testTag("runtime_download_progress")
                    )
                } else {
                    LinearProgressIndicator(
                        color = RunnerColorTokens.AccentBlue,
                        trackColor = RunnerColorTokens.InteractiveSecondary,
                        modifier = Modifier
                            .fillMaxWidth()
                            .testTag("runtime_indeterminate_progress")
                    )
                }
            }
        }

        if (rt.isVerified) {
            GroupRowDivider()
            InfoStatusRow(
                icon = Icons.Outlined.Terminal,
                label = stringResource(R.string.label_runtime_wine_elf),
                value = if (rt.wineBinaryValidElf64) {
                    rt.wineBinaryPath
                } else {
                    stringResource(R.string.value_status_missing)
                }
            )
            GroupRowDivider()
            val translatorsValue = when {
                rt.wowBox64Present && rt.wow64FexPresent -> stringResource(R.string.value_translators_both)
                rt.wowBox64Present -> stringResource(R.string.value_translators_box64_only)
                rt.wow64FexPresent -> stringResource(R.string.value_translators_fex_only)
                else -> stringResource(R.string.value_status_missing)
            }
            InfoStatusRow(
                icon = Icons.Outlined.Memory,
                label = stringResource(R.string.label_runtime_translators),
                value = translatorsValue
            )
            if (rt.lastArchiveSha256.isNotBlank()) {
                GroupRowDivider()
                InfoStatusRow(
                    icon = Icons.Outlined.Archive,
                    label = stringResource(R.string.label_runtime_sha256),
                    value = rt.lastArchiveSha256
                )
            }
        }

        GroupRowDivider()
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(RunnerDimens.SpacingLarge),
            verticalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingSmall)
        ) {
            SecondarySurfaceButton(
                text = stringResource(R.string.action_select_runtime_archive),
                icon = Icons.Outlined.FolderOpen,
                enabled = !uiState.isDownloadingRuntime && !uiState.isExtractingRuntime,
                onClick = onPickRuntimeArchive,
                testTag = "select_runtime_archive_button",
                modifier = Modifier.fillMaxWidth()
            )
            if (uiState.isDownloadingRuntime) {
                SecondarySurfaceButton(
                    text = stringResource(R.string.action_cancel_download),
                    icon = Icons.Outlined.Close,
                    onClick = onCancelDownloadRuntime,
                    testTag = "cancel_download_runtime_button",
                    modifier = Modifier.fillMaxWidth()
                )
            } else {
                SecondarySurfaceButton(
                    text = stringResource(R.string.action_download_github_runtime),
                    icon = Icons.Outlined.CloudDownload,
                    enabled = !uiState.isExtractingRuntime,
                    onClick = onDownloadGithubRuntime,
                    testTag = "download_github_runtime_button",
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

@Composable
private fun GameInstallationSectionCard(
    uiState: RunnerUiState,
    onPickGtaSaExe: () -> Unit,
    onToggleLaaFlag: (Boolean) -> Unit,
    onPickGta3Img: () -> Unit
) {
    val exe = uiState.exeState
    val img = uiState.imgState
    val exeSizeFormatted = rememberFormattedFileSize(exe.fileSizeBytes)

    val exeStatusText = when {
        !exe.isSelected -> stringResource(R.string.status_exe_not_selected)
        exe.isValidPe32 -> stringResource(R.string.status_exe_valid_pe32, exeSizeFormatted)
        exe.errorMessage.isNotBlank() -> exe.errorMessage
        else -> stringResource(R.string.status_exe_invalid)
    }

    SectionGroupCard(title = stringResource(R.string.section_game_installation)) {
        InfoStatusRow(
            icon = Icons.Outlined.Storage,
            label = stringResource(R.string.label_game_executable),
            value = exeStatusText
        )

        if (exe.isValidPe32) {
            GroupRowDivider()
            val entryHex = String.format(Locale.US, "%08X", exe.entryPointRva)
            val baseHex = String.format(Locale.US, "%08X", exe.imageBase)
            val versionLabel = when (exe.detectedVersionCode) {
                "1.0_US_HOODLUM_COMPACT" -> stringResource(R.string.version_10_us_hoodlum)
                "1.0_EU_NO_CD" -> stringResource(R.string.version_10_eu)
                "1.01_PATCHED" -> stringResource(R.string.version_101_patched)
                "3.0_STEAM_ENC" -> stringResource(R.string.version_30_steam)
                else -> stringResource(R.string.version_custom_pe32, entryHex)
            }
            InfoStatusRow(
                icon = Icons.Outlined.Verified,
                label = stringResource(R.string.label_exe_version),
                value = versionLabel
            )
            GroupRowDivider()
            InfoStatusRow(
                icon = Icons.Outlined.Memory,
                label = stringResource(R.string.label_exe_entry_point),
                value = stringResource(
                    R.string.value_exe_entry_format,
                    entryHex,
                    baseHex,
                    exeSizeFormatted
                )
            )
            if (exe.importedDlls.isNotEmpty()) {
                GroupRowDivider()
                val dllPreview = exe.importedDlls.joinToString(", ")
                InfoStatusRow(
                    icon = Icons.Outlined.DeveloperBoard,
                    label = stringResource(R.string.label_exe_imported_dlls),
                    value = pluralStringResource(
                        R.plurals.plural_imported_dlls,
                        exe.importedDlls.size,
                        exe.importedDlls.size,
                        dllPreview
                    )
                )
            }
            GroupRowDivider()
            ToggleSettingRow(
                label = stringResource(R.string.label_exe_laa_flag),
                description = stringResource(R.string.desc_exe_laa_flag),
                checked = exe.isLargeAddressAware,
                onCheckedChange = onToggleLaaFlag,
                testTag = "toggle_laa_patch_row"
            )
        }

        GroupRowDivider()
        val imgStatusValue = when {
            !img.isSelected -> stringResource(R.string.status_img_not_selected)
            img.isValidVer2 -> stringResource(
                R.string.status_img_valid_ver2,
                img.dffModelCount,
                img.txdTextureCount,
                img.colCollisionCount
            )
            else -> img.errorMessage
        }
        InfoStatusRow(
            icon = Icons.Outlined.Archive,
            label = stringResource(R.string.label_img_archive),
            value = imgStatusValue
        )

        GroupRowDivider()
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(RunnerDimens.SpacingLarge),
            horizontalArrangement = Arrangement.spacedBy(RunnerDimens.SpacingSmall)
        ) {
            SecondarySurfaceButton(
                text = stringResource(R.string.action_select_gtasa_exe),
                icon = Icons.Outlined.FolderOpen,
                onClick = onPickGtaSaExe,
                testTag = "select_gtasa_exe_button",
                modifier = Modifier.weight(1f)
            )
            SecondarySurfaceButton(
                text = stringResource(R.string.action_select_gta3_img),
                icon = Icons.Outlined.Archive,
                onClick = onPickGta3Img,
                testTag = "select_gta3_img_button",
                modifier = Modifier.weight(1f)
            )
        }
    }
}

@Composable
private fun ExecutionProfileSectionCard(
    uiState: RunnerUiState,
    onOpenSheet: (ActiveSheetType) -> Unit,
    onToggleDxvk: (Boolean) -> Unit,
    onToggleStrongMem: (Boolean) -> Unit,
    onToggleSafeFlags: (Boolean) -> Unit,
    onToggleCallRet: (Boolean) -> Unit,
    onToggleEsync: (Boolean) -> Unit
) {
    val profile = uiState.profile
    val translatorLabel = when (profile.cpuTranslator) {
        CpuTranslatorOption.BOX64_WOW64 -> stringResource(R.string.value_translator_box64)
        CpuTranslatorOption.FEX_WOW64 -> stringResource(R.string.value_translator_fex)
    }
    val backendLabel = when (profile.maliBackend) {
        MaliGraphicsBackend.VIRGL_GLES32 -> stringResource(R.string.value_backend_virgl)
        MaliGraphicsBackend.ZINK_VULKAN -> stringResource(R.string.value_backend_zink)
    }
    val resolutionLabel = when (profile.resolution) {
        RenderResolutionOption.RES_540P -> stringResource(R.string.value_res_960_540)
        RenderResolutionOption.RES_720P -> stringResource(R.string.value_res_1280_720)
        RenderResolutionOption.RES_900P -> stringResource(R.string.value_res_1600_900)
        RenderResolutionOption.RES_1080P -> stringResource(R.string.value_res_1920_1080)
    }

    SectionGroupCard(title = stringResource(R.string.section_execution_profile)) {
        NavigationSelectorRow(
            icon = Icons.Outlined.Memory,
            label = stringResource(R.string.label_cpu_translator),
            currentValue = translatorLabel,
            onClick = { onOpenSheet(ActiveSheetType.CPU_TRANSLATOR) },
            testTag = "row_select_cpu_translator"
        )
        GroupRowDivider()
        NavigationSelectorRow(
            icon = Icons.Outlined.DeveloperBoard,
            label = stringResource(R.string.label_mali_graphics_backend),
            currentValue = backendLabel,
            onClick = { onOpenSheet(ActiveSheetType.MALI_BACKEND) },
            testTag = "row_select_mali_backend"
        )
        GroupRowDivider()
        NavigationSelectorRow(
            icon = Icons.Outlined.SettingsSuggest,
            label = stringResource(R.string.label_render_resolution),
            currentValue = resolutionLabel,
            onClick = { onOpenSheet(ActiveSheetType.RENDER_RESOLUTION) },
            testTag = "row_select_render_resolution"
        )
        GroupRowDivider()
        ToggleSettingRow(
            label = stringResource(R.string.label_use_dxvk_d3d9),
            description = stringResource(R.string.desc_use_dxvk_d3d9),
            checked = profile.useDxvkD3d9,
            onCheckedChange = onToggleDxvk,
            testTag = "toggle_use_dxvk_row"
        )
        GroupRowDivider()
        ToggleSettingRow(
            label = stringResource(R.string.label_box64_strongmem),
            description = stringResource(R.string.desc_box64_strongmem),
            checked = profile.box64StrongMem,
            onCheckedChange = onToggleStrongMem,
            testTag = "toggle_box64_strongmem_row"
        )
        GroupRowDivider()
        ToggleSettingRow(
            label = stringResource(R.string.label_box64_safeflags),
            description = stringResource(R.string.desc_box64_safeflags),
            checked = profile.box64SafeFlags,
            onCheckedChange = onToggleSafeFlags,
            testTag = "toggle_box64_safeflags_row"
        )
        GroupRowDivider()
        ToggleSettingRow(
            label = stringResource(R.string.label_box64_callret),
            description = stringResource(R.string.desc_box64_callret),
            checked = profile.box64CallRet,
            onCheckedChange = onToggleCallRet,
            testTag = "toggle_box64_callret_row"
        )
        GroupRowDivider()
        ToggleSettingRow(
            label = stringResource(R.string.label_enable_esync),
            description = stringResource(R.string.desc_enable_esync),
            checked = profile.enableEsync,
            onCheckedChange = onToggleEsync,
            testTag = "toggle_enable_esync_row"
        )
    }
}

@Composable
private fun GpuHardwareSectionCard(
    uiState: RunnerUiState,
    onReprobeGpu: () -> Unit
) {
    val gpu = uiState.gpuProbe
    SectionGroupCard(title = stringResource(R.string.section_gpu_probe)) {
        val familyStatus = if (gpu.isMaliFamily) {
            stringResource(R.string.value_gpu_mali_detected)
        } else {
            stringResource(R.string.value_gpu_standard_detected)
        }
        InfoStatusRow(
            icon = Icons.Outlined.DeveloperBoard,
            label = stringResource(R.string.label_gpu_renderer),
            value = if (gpu.querySucceeded) {
                "${gpu.renderer} • $familyStatus"
            } else {
                stringResource(R.string.value_status_missing)
            }
        )
        if (gpu.querySucceeded) {
            GroupRowDivider()
            InfoStatusRow(
                icon = Icons.Outlined.Verified,
                label = stringResource(R.string.label_gpu_vendor),
                value = "${gpu.vendor} • ${gpu.glVersion}"
            )
            GroupRowDivider()
            InfoStatusRow(
                icon = Icons.Outlined.Memory,
                label = stringResource(R.string.label_gpu_max_texture),
                value = stringResource(R.string.value_gpu_max_texture_px, gpu.maxTextureSize)
            )
        }
        GroupRowDivider()
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(RunnerDimens.SpacingLarge)
        ) {
            SecondarySurfaceButton(
                text = stringResource(R.string.action_reprobe_gpu),
                icon = Icons.Outlined.Refresh,
                onClick = onReprobeGpu,
                testTag = "reprobe_gpu_button",
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}
