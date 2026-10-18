@echo off
setlocal
if exist "%~dp0local\installation.env.cmd" call "%~dp0local\installation.env.cmd"
if not defined NOVACOOP_INSTALL set "NOVACOOP_INSTALL=%USERPROFILE%\Games\NovaDriftCoop"
if not exist "%NOVACOOP_INSTALL%\ConfigureP2.cmd" (
  echo Install the co-op copy first. See README.md.
  pause
  exit /b 2
)
call "%NOVACOOP_INSTALL%\ConfigureP2.cmd" %*
