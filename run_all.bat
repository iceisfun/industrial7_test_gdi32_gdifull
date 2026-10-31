@echo off
rem Runs every test exe in the build folder; output goes to results\<area>.txt.
rem Run from an interactive desktop session on Windows 10 x64. Optional arg: test-name filter.
setlocal
cd /d "%~dp0"
if not exist results mkdir results
for %%F in (build\*.exe) do (
  echo == %%~nF
  "%%F" %1 > "results\%%~nF.txt" 2>&1
  findstr /B "SUMMARY" "results\%%~nF.txt"
)
echo Done. Run: python tools\summarize.py results
endlocal
