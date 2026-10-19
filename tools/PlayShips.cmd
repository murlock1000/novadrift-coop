@echo off
setlocal
rem Runs only the isolated two-ship copy and its separate profile.
set "diagnostics=0"
set "displayMode=fullscreen"
:options
if "%~1"=="" goto launch
if /I "%~1"=="diagnostics" (
  set "diagnostics=1"
) else if /I "%~1"=="windowed" (
  set "displayMode=windowed"
) else if /I "%~1"=="fullscreen" (
  set "displayMode=fullscreen"
) else (
  echo Usage: PlayShips.cmd [fullscreen^|windowed] [diagnostics]
  exit /b 2
)
shift /1
goto options
:launch
> "%~dp0ship-control.txt" (
  echo counter=1
  echo qa_focus=0
  echo mutate=0
  echo diagnostics=%diagnostics%
)
"%~dp0novacoop_launch.exe" "%~dp0NovaDrift.exe" "%~dp0novacoop_sandbox.dll" "%~dp0." 0 %displayMode%
if errorlevel 1 pause
endlocal
