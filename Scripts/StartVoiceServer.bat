@echo off
rem Local speech server for the lobby talk area: reads replies with Microsoft Edge's Vietnamese male voice (NamMinh) only.
rem Add --piper-fallback to the last command to read with Piper when Edge fails. Keep this window open while playing.
rem Python packages and Piper voices live in %LOCALAPPDATA%\LangToLuaVoice.
set "VOICEDIR=%LOCALAPPDATA%\LangToLuaVoice"
if not exist "%VOICEDIR%\venv\Scripts\python.exe" (
  echo The voice server is not installed. Run:
  echo   python -m venv "%VOICEDIR%\venv"
  echo   "%VOICEDIR%\venv\Scripts\python.exe" -m pip install "piper-tts[http]" edge-tts miniaudio
  echo   "%VOICEDIR%\venv\Scripts\python.exe" -m piper.download_voices --download-dir "%VOICEDIR%\voices" vi_VN-vivos-x_low
  pause
  exit /b 1
)
"%VOICEDIR%\venv\Scripts\python.exe" "%~dp0voice_server.py" --port 5000 --piper-dir "%VOICEDIR%\voices" --piper-voice vi_VN-vivos-x_low
pause
