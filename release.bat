@echo off
REM release.bat -- build, test and package the PUBLIC release DLL (players: copy in, delete to uninstall).
REM   1. build.bat: the dev DLL and the offline harnesses (build\)
REM   2. the public DLL in its own tree (build_release\, CMake option PUBLIC_RELEASE): every fix whose
REM      FIX_<NAME> option is ON, release sources only (no log, no capture, no keys)
REM   3. tests: every dev run, then each fix's release equivalence runs
REM   4. package (tools\release\package.py): release\<name>-<version>\ (dinput8.dll, README.txt, SHA256.txt) and .zip
REM Run from PowerShell as:  cmd /c "E:\_SHOGUN\Shogun2_fixes\release.bat"
REM Needs the game's empire.retail.dll for the tests; set GAME_DIR if not in the default Steam path.
setlocal
cd /d "%~dp0"
REM Today the public DLL holds the path fix only, so the package is the path fix's.
set "VERSION=1.0.1"
set "PKGNAME=Shogun2-PathFix"
set "PKGREADME=fixes\pathfix\release\README.txt"
call "%~dp0build.bat"
if errorlevel 1 exit /b 1
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" set "CMAKE_EXE=cmake"
"%CMAKE_EXE%" -S . -B build_release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPUBLIC_RELEASE=ON
if errorlevel 1 exit /b 1
"%CMAKE_EXE%" --build build_release
if errorlevel 1 exit /b 1
if "%GAME_DIR%"=="" set "GAME_DIR=E:\SteamLibrary\steamapps\common\Total War SHOGUN 2"
REM one line per fix: its runner with the release equivalence runs
python -I fixes\pathfix\tests\run_tests.py "%GAME_DIR%" --with-release
if errorlevel 1 (
    echo RELEASE NOT PACKAGED: tests failed
    exit /b 1
)
python -I tools\release\package.py %VERSION% %PKGNAME% %PKGREADME%
if errorlevel 1 exit /b 1
echo RELEASE OK: release\%PKGNAME%-%VERSION%
endlocal
