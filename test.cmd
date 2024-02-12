@echo off
call .\build.cmd
if errorlevel 1 (
    echo ERROR: Not running, build failed.
    goto :eof
)
.\toggle.exe /CONSOLE:ATTACH /TOGGLE /EXIT
