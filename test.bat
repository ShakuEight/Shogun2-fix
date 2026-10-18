@echo off
REM test.bat -- run every fix's offline harness against build\dinput8.dll (run build.bat first).
REM Needs the game's empire.retail.dll; set GAME_DIR if the game is not in the default Steam path.
setlocal
cd /d "%~dp0"
if "%GAME_DIR%"=="" set "GAME_DIR=E:\SteamLibrary\steamapps\common\Total War SHOGUN 2"
set "FAILED=0"
REM one line per fix
python -I fixes\pathfix\tests\run_tests.py "%GAME_DIR%" || set "FAILED=1"
python -I fixes\artillery-targeting-fix\tests\run_tests.py "%GAME_DIR%" || set "FAILED=1"
REM dev tools, when built (build.bat -DTOOL_AUTORUN=ON)
if exist build\autorun_forward_test.exe (
    build\autorun_forward_test.exe "%~dp0build\version.dll" || set "FAILED=1"
)
if "%FAILED%"=="1" (
    echo SOME FIX TESTS FAILED
    exit /b 1
)
echo ALL FIX TESTS PASSED
endlocal
