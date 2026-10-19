@echo off
setlocal
set "NOVACOOP_PACKAGE=%~dp0"
powershell.exe -NoProfile -Command "$ErrorActionPreference='Stop'; & ([scriptblock]::Create([IO.File]::ReadAllText((Join-Path $env:NOVACOOP_PACKAGE 'InstallCoop.ps1'))))"
set "installResult=%errorlevel%"
if not "%installResult%"=="0" echo Installation failed. See the message above.
pause
exit /b %installResult%
