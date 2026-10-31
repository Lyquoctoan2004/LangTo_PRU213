@echo off
chcp 65001 >nul
title LangToLua - Khoi phuc File Lon (>100MB) sau khi Git Pull/Clone
echo ==========================================================
echo   DANG KHOI PHUC CAC FILE .UASSET VA PACKAGED GAME (>100MB)
echo ==========================================================
powershell -ExecutionPolicy Bypass -File "%~dp0Scripts\Restore_Large_Files.ps1"
echo.
pause
