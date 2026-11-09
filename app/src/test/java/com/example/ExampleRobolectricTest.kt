package com.example

import android.content.Context
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import com.example.data.CpuTranslatorOption
import com.example.data.MaliGraphicsBackend
import com.example.data.RenderResolutionOption
import com.example.ui.screen.LauncherDashboardScreen
import com.example.ui.theme.MyApplicationTheme
import com.example.viewmodel.ActiveSheetType
import com.example.viewmodel.RunnerUiState
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [36])
class ExampleRobolectricTest {

    @get:Rule
    val composeTestRule = createComposeRule()

    @Test
    fun `read string from context`() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val appName = context.getString(R.string.app_name)
        assertEquals("SA PC Runner", appName)
    }

    @Test
    fun `dashboard screen renders core actions and profile selectors`() {
        composeTestRule.setContent {
            MyApplicationTheme {
                LauncherDashboardScreen(
                    uiState = RunnerUiState(),
                    onPickRuntimeArchive = {},
                    onDownloadGithubRuntime = {},
                    onCancelDownloadRuntime = {},
                    onRequestRemoveRuntime = {},
                    onConfirmRemoveRuntime = {},
                    onDismissRemoveRuntimeDialog = {},
                    onPickGtaSaExe = {},
                    onToggleLaaFlag = {},
                    onPickGta3Img = {},
                    onReprobeGpu = {},
                    onOpenSheet = {},
                    onDismissSheet = {},
                    onSelectTranslator = {},
                    onSelectMaliBackend = {},
                    onSelectResolution = {},
                    onToggleDxvk = {},
                    onToggleStrongMem = {},
                    onToggleSafeFlags = {},
                    onToggleCallRet = {},
                    onToggleEsync = {},
                    onLaunchGame = {},
                    onOpenActiveDisplay = {},
                    onStopActiveSession = {},
                    onDismissBanner = {}
                )
            }
        }
        composeTestRule.waitForIdle()
        composeTestRule.onNodeWithTag("launch_gtasa_button").assertIsDisplayed()
        composeTestRule.onNodeWithTag("select_runtime_archive_button").performScrollTo().assertIsDisplayed()
        composeTestRule.onNodeWithTag("select_gtasa_exe_button").performScrollTo().assertIsDisplayed()
        composeTestRule.onNodeWithTag("row_select_cpu_translator").performScrollTo().assertIsDisplayed()
        composeTestRule.onNodeWithTag("row_select_mali_backend").performScrollTo().assertIsDisplayed()
    }
}
