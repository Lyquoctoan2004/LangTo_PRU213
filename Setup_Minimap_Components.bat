@echo off
echo ========================================================
echo   Auto-setting up Minimap Component for BP_ThirdPersonCharacter
echo ========================================================
echo.
echo Please ensure Unreal Editor is CLOSED before running this.
echo.
pause

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0LangToLua.uproject" -ExecutePythonScript="%~dp0add_capture_v3.py" -nullrhi -unattended -nosplash -stdout

echo.
echo ========================================================
echo   Done! You can now reopen LangToLua.uproject in Unreal Editor.
echo ========================================================
pause
