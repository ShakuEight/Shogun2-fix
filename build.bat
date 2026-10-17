@echo off
REM build.bat -- configure and build build\dinput8.dll (x86) and the offline harness with CMake + NMake.
REM Does NOT copy anything into the game folder (see README.md, Install).
REM Run from PowerShell as:  cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" set "CMAKE_EXE=cmake"
"%CMAKE_EXE%" -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b 1
"%CMAKE_EXE%" --build build
if errorlevel 1 exit /b 1
echo BUILD OK: build\dinput8.dll
endlocal
