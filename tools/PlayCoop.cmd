@echo off
setlocal
rem This file is copied beside the disposable runtime, not into the Steam install.
> "%~dp0coop-control.txt" (
  echo counter=1
  echo enabled=1
  echo qa_focus=0
  echo qa_mutate_movement=0
)
"%~dp0novacoop_launch.exe" "%~dp0NovaDrift.exe" "%~dp0novacoop_sandbox.dll" "%~dp0." 0
if errorlevel 1 pause
endlocal
