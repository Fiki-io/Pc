package com.example.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Shapes

object RunnerShapes {
    val CardShape = RoundedCornerShape(RunnerDimens.CardRadius)
    val DialogShape = RoundedCornerShape(RunnerDimens.SheetRadius)
    val BottomSheetTopShape = RoundedCornerShape(
        topStart = RunnerDimens.SheetRadius,
        topEnd = RunnerDimens.SheetRadius
    )
    val ButtonShape = RoundedCornerShape(RunnerDimens.ButtonRadius)
    val PillShape = RoundedCornerShape(RunnerDimens.PillRadius)
    val ControlPadShape = RoundedCornerShape(RunnerDimens.ControlPadRadius)

    val MaterialShapes = Shapes(
        small = ControlPadShape,
        medium = ButtonShape,
        large = CardShape,
        extraLarge = DialogShape
    )
}
