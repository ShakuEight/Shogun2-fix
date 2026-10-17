@echo off
rem usage: ghx86.bat <logname> <script> <args...>
set G=E:\_SHOGUN\ghidra_12.0.3_PUBLIC_20260210\ghidra_12.0.3_PUBLIC\support
set LOG=%1
shift
set S=%1
shift
set A=
:loop
if "%~1"=="" goto run
set A=%A% %1
shift
goto loop
:run
call "%G%\launch.bat" fg jdk Ghidra-Headless 12G "-XX:ParallelGCThreads=2 -XX:CICompilerCount=2" ghidra.app.util.headless.AnalyzeHeadless E:\_SHOGUN\GHIDRA s2x86 -process empire.retail.dll -noanalysis -readOnly -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra -log E:\_SHOGUN\Shogun2_fixes\logs\%LOG%.log -scriptlog E:\_SHOGUN\Shogun2_fixes\logs\%LOG%_script.log -postScript %S% %A%
