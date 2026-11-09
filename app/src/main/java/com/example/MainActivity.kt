package com.example

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.data.RunnerConstants
import com.example.data.RunnerRepository
import com.example.ui.screen.LauncherDashboardScreen
import com.example.ui.screen.SessionDisplayScreen
import com.example.ui.theme.MyApplicationTheme
import com.example.ui.theme.RunnerMotion
import com.example.viewmodel.ActiveSheetType
import com.example.viewmodel.RunnerViewModel

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val repository = RunnerRepository(applicationContext)
        setContent {
            MyApplicationTheme {
                val viewModel: RunnerViewModel = viewModel(
                    factory = remember {
                        RunnerViewModel.Factory(application, repository)
                    }
                )
                RunnerAppContent(viewModel = viewModel)
            }
        }
    }
}

@Composable
fun RunnerAppContent(viewModel: RunnerViewModel) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()

    val pickRuntimeLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        viewModel.onSelectRuntimeArchiveUri(uri)
    }

    val pickExeLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        viewModel.onSelectGtaSaExeUri(uri)
    }

    val pickImgLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        viewModel.onSelectGta3ImgUri(uri)
    }

    AnimatedContent(
        targetState = uiState.isDisplayScreenOpen,
        transitionSpec = {
            val enterSlide = slideInHorizontally(
                animationSpec = tween(
                    durationMillis = RunnerMotion.DURATION_STANDARD_MS,
                    easing = RunnerMotion.StandardEasing
                ),
                initialOffsetX = { fullWidth -> if (targetState) fullWidth / 5 else -fullWidth / 5 }
            ) + fadeIn(
                animationSpec = tween(
                    durationMillis = RunnerMotion.DURATION_STANDARD_MS,
                    easing = RunnerMotion.StandardEasing
                )
            )
            val exitSlide = slideOutHorizontally(
                animationSpec = tween(
                    durationMillis = RunnerMotion.DURATION_FAST_MS,
                    easing = RunnerMotion.StandardEasing
                ),
                targetOffsetX = { fullWidth -> if (targetState) -fullWidth / 5 else fullWidth / 5 }
            ) + fadeOut(
                animationSpec = tween(
                    durationMillis = RunnerMotion.DURATION_FAST_MS,
                    easing = RunnerMotion.StandardEasing
                )
            )
            enterSlide togetherWith exitSlide
        },
        label = "screen_transition"
    ) { showDisplay ->
        if (showDisplay) {
            SessionDisplayScreen(
                uiState = uiState,
                onBackToConfig = { viewModel.setDisplayScreenOpen(false) },
                onStopSession = { viewModel.stopGtaSaSession() },
                onToggleTouchOverlay = { viewModel.toggleTouchOverlay() },
                onToggleLogPanel = { viewModel.toggleSessionLogPanel() },
                onAttachSurface = { surface, w, h -> viewModel.attachSurface(surface, w, h) },
                onDetachSurface = { viewModel.detachSurface() },
                onSendKeyInput = { vk, pressed -> viewModel.sendKeyInput(vk, pressed) },
                onSendMouseDelta = { dx, dy, mask -> viewModel.sendMouseDelta(dx, dy, mask) }
            )
        } else {
            LauncherDashboardScreen(
                uiState = uiState,
                onPickRuntimeArchive = {
                    pickRuntimeLauncher.launch(arrayOf(RunnerConstants.MIME_ANY_BINARY))
                },
                onDownloadGithubRuntime = { viewModel.startGithubRuntimeDownload() },
                onCancelDownloadRuntime = { viewModel.cancelGithubRuntimeDownload() },
                onRequestRemoveRuntime = { viewModel.requestRemoveRuntimeConfirmation(true) },
                onConfirmRemoveRuntime = { viewModel.confirmRemoveInstalledRuntime() },
                onDismissRemoveRuntimeDialog = { viewModel.requestRemoveRuntimeConfirmation(false) },
                onPickGtaSaExe = {
                    pickExeLauncher.launch(arrayOf(RunnerConstants.MIME_ANY_BINARY))
                },
                onToggleLaaFlag = { enable -> viewModel.onToggleLargeAddressAware(enable) },
                onPickGta3Img = {
                    pickImgLauncher.launch(arrayOf(RunnerConstants.MIME_ANY_BINARY))
                },
                onReprobeGpu = { viewModel.refreshEnvironmentState() },
                onOpenSheet = { sheet -> viewModel.openBottomSheet(sheet) },
                onDismissSheet = { viewModel.openBottomSheet(ActiveSheetType.NONE) },
                onSelectTranslator = { opt -> viewModel.selectCpuTranslator(opt) },
                onSelectMaliBackend = { backend -> viewModel.selectMaliBackend(backend) },
                onSelectResolution = { res -> viewModel.selectRenderResolution(res) },
                onToggleDxvk = { enabled -> viewModel.setUseDxvkD3d9(enabled) },
                onToggleStrongMem = { enabled -> viewModel.setBox64StrongMem(enabled) },
                onToggleSafeFlags = { enabled -> viewModel.setBox64SafeFlags(enabled) },
                onToggleCallRet = { enabled -> viewModel.setBox64CallRet(enabled) },
                onToggleEsync = { enabled -> viewModel.setEnableEsync(enabled) },
                onLaunchGame = { viewModel.launchGtaSaSession() },
                onOpenActiveDisplay = { viewModel.setDisplayScreenOpen(true) },
                onStopActiveSession = { viewModel.stopGtaSaSession() },
                onDismissBanner = { viewModel.clearStatusBanner() }
            )
        }
    }
}
