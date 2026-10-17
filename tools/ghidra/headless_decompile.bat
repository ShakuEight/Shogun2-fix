@echo off
REM headless_decompile.bat <project> <program> <out_dir> <addr>[:<stem>] ...
REM Runs tools\ghidra\decompile_funcs.py read-only on a Ghidra project in E:\_SHOGUN\GHIDRA
REM through launch.bat (8 GB heap; analyzeHeadless.bat hard-codes 2 GB).
REM Example: headless_decompile.bat s2x86 empire.retail.dll E:\_SHOGUN\Shogun2_fixes\docs\re\walk_decompiles\x86 1018b310 103cbe40
setlocal
set L=E:\_SHOGUN\ghidra_12.0.3_PUBLIC_20260210\ghidra_12.0.3_PUBLIC\support\launch.bat
set PROJ=%~1
set PROG=%~2
shift
shift
set ARGS=
:loop
if "%~1"=="" goto run
set ARGS=%ARGS% %~1
shift
goto loop
:run
call "%L%" fg jdk Ghidra-Headless 8G "" ghidra.app.util.headless.AnalyzeHeadless E:\_SHOGUN\GHIDRA %PROJ% -process %PROG% -noanalysis -readOnly -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra -postScript decompile_funcs.py %ARGS%
endlocal
