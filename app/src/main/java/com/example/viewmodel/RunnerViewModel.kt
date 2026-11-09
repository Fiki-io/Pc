package com.example.viewmodel

import android.app.Application
import android.net.Uri
import android.view.Surface
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.example.R
import com.example.data.CpuTranslatorOption
import com.example.data.ExecutionProfileConfig
import com.example.data.GpuProbeData
import com.example.data.LiveSessionState
import com.example.data.MaliGraphicsBackend
import com.example.data.Pe32ExecutableState
import com.example.data.RenderResolutionOption
import com.example.data.RenderWareImgState
import com.example.data.RunnerConstants
import com.example.data.RunnerRepository
import com.example.data.RuntimeVerificationState
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

enum class ActiveSheetType {
    NONE,
    CPU_TRANSLATOR,
    MALI_BACKEND,
    RENDER_RESOLUTION
}

data class RunnerUiState(
    val isDisplayScreenOpen: Boolean = false,
    val gpuProbe: GpuProbeData = GpuProbeData(),
    val runtimeState: RuntimeVerificationState = RuntimeVerificationState(),
    val isExtractingRuntime: Boolean = false,
    val isDownloadingRuntime: Boolean = false,
    val downloadProgressBytes: Long = 0L,
    val downloadTotalBytes: Long = 0L,
    val exeState: Pe32ExecutableState = Pe32ExecutableState(),
    val imgState: RenderWareImgState = RenderWareImgState(),
    val profile: ExecutionProfileConfig = ExecutionProfileConfig(),
    val sessionState: LiveSessionState = LiveSessionState(),
    val activeSheet: ActiveSheetType = ActiveSheetType.NONE,
    val showRemoveRuntimeDialog: Boolean = false,
    val showTouchOverlay: Boolean = true,
    val showSessionLogPanel: Boolean = false,
    val statusBannerMessage: String? = null
)

class RunnerViewModel(
    application: Application,
    private val repository: RunnerRepository
) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(RunnerUiState())
    val uiState: StateFlow<RunnerUiState> = _uiState.asStateFlow()

    private var downloadJob: Job? = null
    private var telemetryJob: Job? = null

    init {
        viewModelScope.launch {
            repository.executionProfileFlow.collect { profile ->
                _uiState.update { it.copy(profile = profile) }
            }
        }
        refreshEnvironmentState()
    }

    fun refreshEnvironmentState() {
        viewModelScope.launch {
            val gpu = repository.probeGpuHardware()
            val runtime = repository.verifyInstalledRuntime()
            val (exe, img) = repository.inspectExistingGameFiles()
            _uiState.update { state ->
                state.copy(
                    gpuProbe = gpu,
                    runtimeState = runtime,
                    exeState = exe,
                    imgState = img
                )
            }
        }
    }

    fun onSelectRuntimeArchiveUri(uri: Uri?) {
        if (uri == null) return
        viewModelScope.launch {
            _uiState.update {
                it.copy(isExtractingRuntime = true, statusBannerMessage = null)
            }
            val result = repository.importAndExtractRuntimeUri(uri)
            result.fold(
                onSuccess = { (outcome, verified) ->
                    val msg = getApplication<Application>().getString(
                        R.string.msg_runtime_extracted_verified,
                        outcome.extractedFiles,
                        outcome.extractedSymlinks
                    )
                    _uiState.update {
                        it.copy(
                            isExtractingRuntime = false,
                            runtimeState = verified,
                            statusBannerMessage = msg
                        )
                    }
                },
                onFailure = { err ->
                    val verified = repository.verifyInstalledRuntime()
                    _uiState.update {
                        it.copy(
                            isExtractingRuntime = false,
                            runtimeState = verified,
                            statusBannerMessage = err.message ?: err.javaClass.simpleName
                        )
                    }
                }
            )
        }
    }

    fun startGithubRuntimeDownload() {
        if (_uiState.value.isDownloadingRuntime || _uiState.value.isExtractingRuntime) return
        downloadJob = viewModelScope.launch {
            _uiState.update {
                it.copy(
                    isDownloadingRuntime = true,
                    downloadProgressBytes = 0L,
                    downloadTotalBytes = 0L,
                    statusBannerMessage = null
                )
            }
            try {
                val result = repository.downloadAndExtractGithubRuntime { downloaded, total ->
                    _uiState.update { state ->
                        state.copy(
                            downloadProgressBytes = downloaded,
                            downloadTotalBytes = total
                        )
                    }
                }
                result.fold(
                    onSuccess = { (outcome, verified) ->
                        val msg = getApplication<Application>().getString(
                            R.string.msg_runtime_extracted_verified,
                            outcome.extractedFiles,
                            outcome.extractedSymlinks
                        )
                        _uiState.update {
                            it.copy(
                                isDownloadingRuntime = false,
                                isExtractingRuntime = false,
                                runtimeState = verified,
                                statusBannerMessage = msg
                            )
                        }
                    },
                    onFailure = { err ->
                        val msg = getApplication<Application>().getString(
                            R.string.err_download_failed,
                            err.message ?: err.javaClass.simpleName
                        )
                        _uiState.update {
                            it.copy(
                                isDownloadingRuntime = false,
                                isExtractingRuntime = false,
                                statusBannerMessage = msg
                            )
                        }
                    }
                )
            } catch (_: CancellationException) {
                _uiState.update {
                    it.copy(
                        isDownloadingRuntime = false,
                        isExtractingRuntime = false
                    )
                }
            }
        }
    }

    fun cancelGithubRuntimeDownload() {
        downloadJob?.cancel()
        downloadJob = null
    }

    fun requestRemoveRuntimeConfirmation(show: Boolean) {
        _uiState.update { it.copy(showRemoveRuntimeDialog = show) }
    }

    fun confirmRemoveInstalledRuntime() {
        viewModelScope.launch {
            val verified = repository.removeInstalledRuntime()
            val msg = getApplication<Application>().getString(R.string.msg_runtime_removed)
            _uiState.update {
                it.copy(
                    showRemoveRuntimeDialog = false,
                    runtimeState = verified,
                    statusBannerMessage = msg
                )
            }
        }
    }

    fun onSelectGtaSaExeUri(uri: Uri?) {
        if (uri == null) return
        viewModelScope.launch {
            val res = repository.importAndInspectGtaSaExe(uri)
            res.fold(
                onSuccess = { exe ->
                    _uiState.update {
                        it.copy(exeState = exe, statusBannerMessage = null)
                    }
                },
                onFailure = { err ->
                    val msg = getApplication<Application>().getString(
                        R.string.err_copy_selected_file,
                        err.message ?: err.javaClass.simpleName
                    )
                    _uiState.update {
                        it.copy(statusBannerMessage = msg)
                    }
                }
            )
        }
    }

    fun onToggleLargeAddressAware(enable: Boolean) {
        viewModelScope.launch {
            val res = repository.patchGtaSaLargeAddressAware(enable)
            res.fold(
                onSuccess = { updatedExe ->
                    val msgRes = if (updatedExe.isLargeAddressAware) {
                        R.string.msg_laa_patched_enabled
                    } else {
                        R.string.msg_laa_patched_disabled
                    }
                    _uiState.update {
                        it.copy(
                            exeState = updatedExe,
                            statusBannerMessage = getApplication<Application>().getString(msgRes)
                        )
                    }
                },
                onFailure = { err ->
                    _uiState.update {
                        it.copy(statusBannerMessage = err.message ?: err.javaClass.simpleName)
                    }
                }
            )
        }
    }

    fun onSelectGta3ImgUri(uri: Uri?) {
        if (uri == null) return
        viewModelScope.launch {
            val res = repository.importAndInspectGta3Img(uri)
            res.fold(
                onSuccess = { img ->
                    _uiState.update {
                        it.copy(imgState = img, statusBannerMessage = null)
                    }
                },
                onFailure = { err ->
                    val msg = getApplication<Application>().getString(
                        R.string.err_copy_selected_file,
                        err.message ?: err.javaClass.simpleName
                    )
                    _uiState.update {
                        it.copy(statusBannerMessage = msg)
                    }
                }
            )
        }
    }

    fun openBottomSheet(sheet: ActiveSheetType) {
        _uiState.update { it.copy(activeSheet = sheet) }
    }

    fun selectCpuTranslator(option: CpuTranslatorOption) {
        viewModelScope.launch {
            repository.setCpuTranslator(option)
            _uiState.update { it.copy(activeSheet = ActiveSheetType.NONE) }
        }
    }

    fun selectMaliBackend(backend: MaliGraphicsBackend) {
        viewModelScope.launch {
            repository.setMaliBackend(backend)
            _uiState.update { it.copy(activeSheet = ActiveSheetType.NONE) }
        }
    }

    fun selectRenderResolution(resolution: RenderResolutionOption) {
        viewModelScope.launch {
            repository.setRenderResolution(resolution)
            _uiState.update { it.copy(activeSheet = ActiveSheetType.NONE) }
        }
    }

    fun setUseDxvkD3d9(enabled: Boolean) {
        viewModelScope.launch { repository.setUseDxvkD3d9(enabled) }
    }

    fun setBox64StrongMem(enabled: Boolean) {
        viewModelScope.launch { repository.setBox64StrongMem(enabled) }
    }

    fun setBox64SafeFlags(enabled: Boolean) {
        viewModelScope.launch { repository.setBox64SafeFlags(enabled) }
    }

    fun setBox64CallRet(enabled: Boolean) {
        viewModelScope.launch { repository.setBox64CallRet(enabled) }
    }

    fun setEnableEsync(enabled: Boolean) {
        viewModelScope.launch { repository.setEnableEsync(enabled) }
    }

    fun launchGtaSaSession() {
        val current = _uiState.value
        val app = getApplication<Application>()
        if (!current.runtimeState.isVerified) {
            _uiState.update {
                it.copy(statusBannerMessage = app.getString(R.string.err_runtime_missing_before_launch))
            }
            return
        }
        if (!current.exeState.isValidPe32) {
            _uiState.update {
                it.copy(statusBannerMessage = app.getString(R.string.err_exe_missing_before_launch))
            }
            return
        }

        viewModelScope.launch {
            val startRes = repository.startSession(current.profile)
            startRes.fold(
                onSuccess = {
                    val snap = repository.pollSessionTelemetry()
                    _uiState.update {
                        it.copy(
                            isDisplayScreenOpen = true,
                            sessionState = snap,
                            statusBannerMessage = null
                        )
                    }
                    startTelemetryLoop()
                },
                onFailure = { err ->
                    _uiState.update {
                        it.copy(statusBannerMessage = err.message ?: err.javaClass.simpleName)
                    }
                }
            )
        }
    }

    fun stopGtaSaSession() {
        viewModelScope.launch {
            repository.stopSession()
            val snap = repository.pollSessionTelemetry()
            _uiState.update {
                it.copy(sessionState = snap)
            }
            stopTelemetryLoop()
        }
    }

    fun setDisplayScreenOpen(open: Boolean) {
        _uiState.update { it.copy(isDisplayScreenOpen = open) }
        if (open) {
            startTelemetryLoop()
        } else if (!_uiState.value.sessionState.isRunning) {
            stopTelemetryLoop()
        }
    }

    fun toggleTouchOverlay() {
        _uiState.update { it.copy(showTouchOverlay = !it.showTouchOverlay) }
    }

    fun toggleSessionLogPanel() {
        _uiState.update { it.copy(showSessionLogPanel = !it.showSessionLogPanel) }
    }

    fun clearStatusBanner() {
        _uiState.update { it.copy(statusBannerMessage = null) }
    }

    fun attachSurface(surface: Surface, width: Int, height: Int) {
        repository.getNativeBridge().attachSurface(surface, width, height)
    }

    fun detachSurface() {
        repository.getNativeBridge().detachSurface()
    }

    fun sendKeyInput(win32VkCode: Int, pressed: Boolean) {
        repository.getNativeBridge().sendKeyInput(win32VkCode, pressed)
    }

    fun sendMouseDelta(dx: Float, dy: Float, buttonMask: Int) {
        repository.getNativeBridge().sendMouseDelta(dx, dy, buttonMask)
    }

    private fun startTelemetryLoop() {
        if (telemetryJob?.isActive == true) return
        telemetryJob = viewModelScope.launch {
            while (isActive) {
                val snap = repository.pollSessionTelemetry()
                _uiState.update { it.copy(sessionState = snap) }
                if (!snap.isRunning && !_uiState.value.isDisplayScreenOpen) {
                    break
                }
                delay(RunnerConstants.TELEMETRY_POLL_INTERVAL_MS)
            }
        }
    }

    private fun stopTelemetryLoop() {
        telemetryJob?.cancel()
        telemetryJob = null
    }

    override fun onCleared() {
        stopTelemetryLoop()
        repository.getNativeBridge().detachSurface()
        super.onCleared()
    }

    class Factory(
        private val application: Application,
        private val repository: RunnerRepository
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return RunnerViewModel(application, repository) as T
        }
    }
}
