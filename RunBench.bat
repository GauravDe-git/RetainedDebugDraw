@echo off
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" ^
 "%~dp0RetainedDebugDraw.uproject" ^
 -game -fullscreen -ResX=1920 -ResY=1080 ^
 -ExecCmds="stat unit"