@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0.."
if not exist local\build mkdir local\build
cl /nologo /LD /std:c++20 /EHsc /MD /O2 /DNDEBUG /DYYTK_DEFINE_INTERNAL /Ilocal\deps-research\YYToolkit-v5-experimental\ExamplePlugin\include src\coop_ally.cpp local\deps-research\YYToolkit-v5-experimental\ExamplePlugin\include\YYToolkit\YYTK_Shared_Types.cpp /Felocal\build\NovaDriftCoop.dll /Folocal\build\ /link /INCREMENTAL:NO user32.lib xinput9_1_0.lib
set build_result=%errorlevel%
popd
exit /b %build_result%
