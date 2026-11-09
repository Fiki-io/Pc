package com.example.data

import android.content.Context
import android.net.Uri
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.example.domain.engine.ArchiveExtractionOutcome
import com.example.domain.engine.LaaPatchOutcome
import com.example.domain.engine.NativeEngineBridge
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request

private val Context.runnerDataStore: DataStore<Preferences> by preferencesDataStore(
    name = RunnerConstants.DATASTORE_NAME
)

class RunnerRepository(
    private val appContext: Context,
    private val nativeBridge: NativeEngineBridge = NativeEngineBridge()
) {

    private val prefCpuTranslator = stringPreferencesKey(RunnerConstants.KEY_CPU_TRANSLATOR)
    private val prefMaliBackend = stringPreferencesKey(RunnerConstants.KEY_MALI_BACKEND)
    private val prefResolution = stringPreferencesKey(RunnerConstants.KEY_RENDER_RESOLUTION)
    private val prefUseDxvk = booleanPreferencesKey(RunnerConstants.KEY_USE_DXVK)
    private val prefStrongMem = booleanPreferencesKey(RunnerConstants.KEY_BOX64_STRONGMEM)
    private val prefSafeFlags = booleanPreferencesKey(RunnerConstants.KEY_BOX64_SAFEFLAGS)
    private val prefCallRet = booleanPreferencesKey(RunnerConstants.KEY_BOX64_CALLRET)
    private val prefEnableEsync = booleanPreferencesKey(RunnerConstants.KEY_ENABLE_ESYNC)
    private val prefLastSha256 = stringPreferencesKey(RunnerConstants.KEY_LAST_ARCHIVE_SHA256)

    private val httpClient: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(RunnerConstants.HTTP_CONNECT_TIMEOUT_SEC, TimeUnit.SECONDS)
            .readTimeout(RunnerConstants.HTTP_READ_TIMEOUT_SEC, TimeUnit.SECONDS)
            .followRedirects(true)
            .followSslRedirects(true)
            .build()
    }

    val executionProfileFlow: Flow<ExecutionProfileConfig> =
        appContext.runnerDataStore.data.map { prefs ->
            ExecutionProfileConfig(
                cpuTranslator = CpuTranslatorOption.fromDllName(
                    prefs[prefCpuTranslator] ?: CpuTranslatorOption.BOX64_WOW64.dllName
                ),
                maliBackend = MaliGraphicsBackend.fromId(
                    prefs[prefMaliBackend] ?: MaliGraphicsBackend.VIRGL_GLES32.backendId
                ),
                resolution = RenderResolutionOption.fromId(
                    prefs[prefResolution] ?: RenderResolutionOption.RES_720P.id
                ),
                useDxvkD3d9 = prefs[prefUseDxvk] ?: false,
                box64StrongMem = prefs[prefStrongMem] ?: true,
                box64SafeFlags = prefs[prefSafeFlags] ?: true,
                box64CallRet = prefs[prefCallRet] ?: true,
                enableEsync = prefs[prefEnableEsync] ?: true
            )
        }

    fun getRuntimeDir(): File = File(appContext.filesDir, RunnerConstants.RUNTIME_DIR_NAME)

    fun getGameDir(): File = File(appContext.filesDir, RunnerConstants.GAME_DIR_NAME).apply {
        if (!exists()) mkdirs()
    }

    private fun getStagingDir(): File = File(appContext.filesDir, RunnerConstants.STAGING_DIR_NAME).apply {
        if (!exists()) mkdirs()
    }

    suspend fun probeGpuHardware(): GpuProbeData = withContext(Dispatchers.IO) {
        nativeBridge.probeGpuHardware()
    }

    suspend fun verifyInstalledRuntime(): RuntimeVerificationState = withContext(Dispatchers.IO) {
        val lastSha = appContext.runnerDataStore.data.first()[prefLastSha256].orEmpty()
        nativeBridge.verifyInstalledRuntime(getRuntimeDir().absolutePath, lastSha)
    }

    suspend fun inspectExistingGameFiles(): Pair<Pe32ExecutableState, RenderWareImgState> =
        withContext(Dispatchers.IO) {
            val exeFile = File(getGameDir(), RunnerConstants.DEFAULT_EXE_FILENAME)
            val exeState = if (exeFile.exists() && exeFile.length() > 0L) {
                nativeBridge.inspectPe32Binary(exeFile.absolutePath)
            } else {
                Pe32ExecutableState(isSelected = false)
            }

            val modelsDir = File(getGameDir(), "models")
            val imgFile = File(modelsDir, RunnerConstants.DEFAULT_IMG_FILENAME).let {
                if (it.exists()) it else File(getGameDir(), RunnerConstants.DEFAULT_IMG_FILENAME)
            }
            val imgState = if (imgFile.exists() && imgFile.length() > 0L) {
                nativeBridge.inspectRenderWareImg(imgFile.absolutePath)
            } else {
                RenderWareImgState(isSelected = false)
            }
            exeState to imgState
        }

    suspend fun importAndExtractRuntimeUri(uri: Uri): Result<Pair<ArchiveExtractionOutcome, RuntimeVerificationState>> =
        withContext(Dispatchers.IO) {
            val stagingFile = File(getStagingDir(), RunnerConstants.STAGING_ARCHIVE_FILENAME)
            try {
                copyUriToFile(uri, stagingFile)
                getRuntimeDir().deleteRecursively()
                val outcome = nativeBridge.extractRuntimeArchive(
                    archivePath = stagingFile.absolutePath,
                    destDir = getRuntimeDir().absolutePath
                )
                stagingFile.delete()
                if (!outcome.success) {
                    return@withContext Result.failure(IllegalStateException(outcome.errorMessage))
                }
                appContext.runnerDataStore.edit { prefs ->
                    prefs[prefLastSha256] = outcome.archiveSha256
                }
                val verification = nativeBridge.verifyInstalledRuntime(
                    runtimeRootDir = getRuntimeDir().absolutePath,
                    lastSha256 = outcome.archiveSha256
                )
                if (!verification.isVerified) {
                    return@withContext Result.failure(IllegalStateException(verification.errorMessage))
                }
                Result.success(outcome to verification)
            } catch (ce: CancellationException) {
                stagingFile.delete()
                throw ce
            } catch (e: Exception) {
                stagingFile.delete()
                Result.failure(e)
            }
        }

    suspend fun downloadAndExtractGithubRuntime(
        onProgress: (downloadedBytes: Long, totalBytes: Long) -> Unit
    ): Result<Pair<ArchiveExtractionOutcome, RuntimeVerificationState>> =
        withContext(Dispatchers.IO) {
            val stagingFile = File(getStagingDir(), RunnerConstants.STAGING_ARCHIVE_FILENAME)
            try {
                val request = Request.Builder()
                    .url(RunnerConstants.GITHUB_RUNTIME_RELEASE_URL)
                    .get()
                    .build()

                httpClient.newCall(request).execute().use { response ->
                    if (!response.isSuccessful) {
                        return@withContext Result.failure(
                            IllegalStateException("HTTP ${response.code}: ${response.message}")
                        )
                    }
                    val body = response.body
                        ?: return@withContext Result.failure(IllegalStateException("Empty HTTP body"))
                    val contentLength = body.contentLength()
                    body.byteStream().use { input ->
                        FileOutputStream(stagingFile).use { output ->
                            val buffer = ByteArray(RunnerConstants.STREAM_BUFFER_SIZE)
                            var downloaded = 0L
                            while (true) {
                                coroutineContext.ensureActive()
                                val read = input.read(buffer)
                                if (read == -1) break
                                output.write(buffer, 0, read)
                                downloaded += read
                                onProgress(downloaded, contentLength)
                            }
                            output.fd.sync()
                        }
                    }
                }

                getRuntimeDir().deleteRecursively()
                val outcome = nativeBridge.extractRuntimeArchive(
                    archivePath = stagingFile.absolutePath,
                    destDir = getRuntimeDir().absolutePath
                )
                stagingFile.delete()
                if (!outcome.success) {
                    return@withContext Result.failure(IllegalStateException(outcome.errorMessage))
                }
                appContext.runnerDataStore.edit { prefs ->
                    prefs[prefLastSha256] = outcome.archiveSha256
                }
                val verification = nativeBridge.verifyInstalledRuntime(
                    runtimeRootDir = getRuntimeDir().absolutePath,
                    lastSha256 = outcome.archiveSha256
                )
                if (!verification.isVerified) {
                    return@withContext Result.failure(IllegalStateException(verification.errorMessage))
                }
                Result.success(outcome to verification)
            } catch (ce: CancellationException) {
                stagingFile.delete()
                throw ce
            } catch (e: Exception) {
                stagingFile.delete()
                Result.failure(e)
            }
        }

    suspend fun removeInstalledRuntime(): RuntimeVerificationState = withContext(Dispatchers.IO) {
        nativeBridge.stopSession()
        getRuntimeDir().deleteRecursively()
        appContext.runnerDataStore.edit { prefs ->
            prefs.remove(prefLastSha256)
        }
        nativeBridge.verifyInstalledRuntime(getRuntimeDir().absolutePath, "")
    }

    suspend fun importAndInspectGtaSaExe(uri: Uri): Result<Pe32ExecutableState> =
        withContext(Dispatchers.IO) {
            val targetExe = File(getGameDir(), RunnerConstants.DEFAULT_EXE_FILENAME)
            try {
                copyUriToFile(uri, targetExe)
                val state = nativeBridge.inspectPe32Binary(targetExe.absolutePath)
                if (!state.isValidPe32) {
                    targetExe.delete()
                    return@withContext Result.failure(IllegalArgumentException(state.errorMessage))
                }
                Result.success(state)
            } catch (e: Exception) {
                Result.failure(e)
            }
        }

    suspend fun patchGtaSaLargeAddressAware(enable: Boolean): Result<Pe32ExecutableState> =
        withContext(Dispatchers.IO) {
            val targetExe = File(getGameDir(), RunnerConstants.DEFAULT_EXE_FILENAME)
            if (!targetExe.exists()) {
                return@withContext Result.failure(IllegalStateException("gta_sa.exe not found"))
            }
            val patchOutcome: LaaPatchOutcome = nativeBridge.patchLargeAddressAware(
                path = targetExe.absolutePath,
                enable = enable
            )
            if (!patchOutcome.success || patchOutcome.verifiedEnabled != enable) {
                return@withContext Result.failure(IllegalStateException(patchOutcome.errorMessage))
            }
            val refreshed = nativeBridge.inspectPe32Binary(targetExe.absolutePath)
            Result.success(refreshed)
        }

    suspend fun importAndInspectGta3Img(uri: Uri): Result<RenderWareImgState> =
        withContext(Dispatchers.IO) {
            val modelsDir = File(getGameDir(), "models").apply { if (!exists()) mkdirs() }
            val targetImg = File(modelsDir, RunnerConstants.DEFAULT_IMG_FILENAME)
            try {
                copyUriToFile(uri, targetImg)
                val state = nativeBridge.inspectRenderWareImg(targetImg.absolutePath)
                if (!state.isValidVer2) {
                    targetImg.delete()
                    return@withContext Result.failure(IllegalArgumentException(state.errorMessage))
                }
                Result.success(state)
            } catch (e: Exception) {
                Result.failure(e)
            }
        }

    suspend fun setCpuTranslator(option: CpuTranslatorOption) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefCpuTranslator] = option.dllName
        }
    }

    suspend fun setMaliBackend(backend: MaliGraphicsBackend) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefMaliBackend] = backend.backendId
        }
    }

    suspend fun setRenderResolution(resolution: RenderResolutionOption) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefResolution] = resolution.id
        }
    }

    suspend fun setUseDxvkD3d9(enabled: Boolean) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefUseDxvk] = enabled
        }
    }

    suspend fun setBox64StrongMem(enabled: Boolean) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefStrongMem] = enabled
        }
    }

    suspend fun setBox64SafeFlags(enabled: Boolean) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefSafeFlags] = enabled
        }
    }

    suspend fun setBox64CallRet(enabled: Boolean) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefCallRet] = enabled
        }
    }

    suspend fun setEnableEsync(enabled: Boolean) {
        appContext.runnerDataStore.edit { prefs ->
            prefs[prefEnableEsync] = enabled
        }
    }

    suspend fun startSession(profile: ExecutionProfileConfig): Result<Unit> =
        withContext(Dispatchers.IO) {
            nativeBridge.startSession(
                runtimeRootDir = getRuntimeDir().absolutePath,
                gameDirPath = getGameDir().absolutePath,
                exeName = RunnerConstants.DEFAULT_EXE_FILENAME,
                profile = profile
            )
        }

    suspend fun stopSession() = withContext(Dispatchers.IO) {
        nativeBridge.stopSession()
    }

    suspend fun pollSessionTelemetry(): LiveSessionState = withContext(Dispatchers.IO) {
        nativeBridge.pollSessionTelemetry()
    }

    fun getNativeBridge(): NativeEngineBridge = nativeBridge

    private fun copyUriToFile(uri: Uri, destFile: File) {
        val inputStream = appContext.contentResolver.openInputStream(uri)
            ?: throw IllegalStateException("ContentResolver returned null InputStream")
        inputStream.use { input ->
            FileOutputStream(destFile).use { output ->
                val buffer = ByteArray(RunnerConstants.STREAM_BUFFER_SIZE)
                while (true) {
                    val read = input.read(buffer)
                    if (read == -1) break
                    output.write(buffer, 0, read)
                }
                output.fd.sync()
            }
        }
    }
}
