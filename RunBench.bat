@echo off
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" ^
 "%~dp0RetainedDebugDraw.uproject" ^
 -game -fullscreen -ResX=1920 -ResY=1080 ^
 -ExecCmds="t.MaxFPS 10000, r.VSync 0, r.PostProcessing.DisableMaterials 1, r.BloomQuality 0, r.Tonemapper.Quality 0, r.DefaultFeature.AutoExposure 0, r.MotionBlurQuality 0, r.AntiAliasingMethod 0, r.SSR.Quality 0, r.AmbientOcclusionLevels 0, r.DynamicRes.OperationMode 0, r.ScreenPercentage 100, stat unit"