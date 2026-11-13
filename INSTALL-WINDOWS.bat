@echo off
rem Smart Estate - double-click me to install everything on this Windows PC.
title Smart Estate installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows\install-windows.ps1"
if errorlevel 1 (
  echo.
  echo Something went wrong. Read the red text above, or follow docs\INSTALL.md by hand.
  pause
)
