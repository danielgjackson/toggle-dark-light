@ECHO OFF
SETLOCAL EnableDelayedExpansion
CD /D %~dp0

rem If launched from anything other than cmd.exe, will have "%WINDIR%\system32\cmd.exe" in the command line
set INTERACTIVE_IBUILD=
IF "%1"=="/NONINTERACTIVE" GOTO NONINTERACTIVE
ECHO.%CMDCMDLINE% | FINDSTR /C:"%COMSPEC% /c" >NUL
IF ERRORLEVEL 1 GOTO NONINTERACTIVE
rem Preserve this as it seems to be corrupted below otherwise?!
SET CMDLINE=%CMDCMDLINE%
rem If launched from anything other than cmd.exe, last character of command line will always be a double quote
IF NOT ^!CMDCMDLINE:~-1!==^" GOTO NONINTERACTIVE
rem If run from Explorer, last-but-one character of command line will be a space
IF NOT "!CMDLINE:~-2,1!"==" " GOTO NONINTERACTIVE
SET INTERACTIVE_IBUILD=1
:NONINTERACTIVE

::: Build first
CALL ./build.cmd
IF ERRORLEVEL 1 GOTO ERROR

::: Extract version
set VER=
for /f "tokens=3 usebackq" %%f in (`type toggle.rc ^| findstr /R /C:"#define[ ]VER_MAJOR"`) do set VER=%%f
for /f "tokens=3 usebackq" %%f in (`type toggle.rc ^| findstr /R /C:"#define[ ]VER_MINOR"`) do set VER=%VER%.%%f
for /f "tokens=3 usebackq" %%f in (`type toggle.rc ^| findstr /R /C:"#define[ ]VER_BUILD"`) do set VER=%VER%.%%f
rem for /f "tokens=3 usebackq" %%f in (`type toggle.rc ^| findstr /R /C:"#define[ ]VER_REVISION"`) do set VER=%VER%.%%f

::: Using .wixproj for WiX V4
dotnet build -c Release toggle.wixproj
IF ERRORLEVEL 1 (
  ECHO If WiX Toolset not installed:
  ECHO   dotnet tool install --global wix
  REM ECHO   dotnet add package WixToolset.UI.wixext
  REM ECHO   dotnet add package WixToolset.Util.wixext
  ECHO.
  GOTO ERROR
)
copy /Y bin\Release\toggle.msi toggle.msi

:PATH_WINDOWSKIT
SET WINDOWSKIT_PATH=
SET FIND_WINDOWSKIT=
FOR %%p IN (signtool.exe) DO SET "FIND_WINDOWSKIT=%%~$PATH:p"
IF NOT DEFINED FIND_WINDOWSKIT GOTO LOCATE_WINDOWSKIT
SET WIX_PATH=%FIND_WINDOWSKIT:~0,-12%
ECHO Windows Kit located on path: %WINDOWSKIT_PATH%
GOTO SIGN

:LOCATE_WINDOWSKIT
ECHO Searching for Windows Kit...
FOR /F "usebackq tokens=*" %%f IN (`DIR /B /ON "%ProgramFiles(x86)%\Windows Kits\10\bin\*"`) DO IF EXIST "%ProgramFiles(x86)%\Windows Kits\10\bin\%%f\x64\signtool.exe" SET "WINDOWSKIT_PATH=%ProgramFiles(x86)%\Windows Kits\10\bin\%%f\x64\"
IF "%WINDOWSKIT_PATH%"=="" ECHO Cannot find Windows Kit & GOTO ERROR
ECHO Windows Kit found at: %WINDOWSKIT_PATH%

:SIGN
::: "%WINDOWSKIT_PATH%MakeCert.exe" -$ individual -pe -n "CN=danielgjackson" -sv D:\Certificates\mycert.pvk D:\Certificates\mycert.cer
::: "%WINDOWSKIT_PATH%Pvk2Pfx.exe" -pvk D:\Certificates\mycert.pvk -spc D:\Certificates\mycert.cer -pfx D:\Certificates\mycert.pfx

SET TIMESTAMP_SERVER=
::: SET TIMESTAMP_SERVER=/t http://timestamp.verisign.com/scripts/timstamp.dll
::: SET TIMESTAMP_SERVER=/t http://timestamp.globalsign.com/scripts/timstamp.dll
::: SET TIMESTAMP_SERVER=/t http://timestamp.comodoca.com/authenticode
::: SET TIMESTAMP_SERVER=/t http://www.startssl.com/timestamp
::: SET TIMESTAMP_SERVER=/t http://timestamp.sectigo.com
::: SET TIMESTAMP_SERVER=/t http://tsa.starfieldtech.com
::: SET TIMESTAMP_SERVER=/t http://timestamp.digicert.com?alg=sha1


rem "%WIX_PATH%insignia.exe" -ib "toggle.msi" -o "engine.exe"
rem IF ERRORLEVEL 1 GOTO ERROR
rem "%WINDOWSKIT_PATH%signtool.exe" sign /f "D:\Certificates\mycert.pfx" /d "Toggle Dark/Light Setup" %TIMESTAMP_SERVER% /v "engine.exe"
rem IF ERRORLEVEL 1 GOTO ERROR
rem "%WIX_PATH%insignia.exe" -ab "engine.exe" "toggle.msi" -o "toggle.msi"
rem IF ERRORLEVEL 1 GOTO ERROR
"%WINDOWSKIT_PATH%signtool.exe" sign /debug /v /a /f "D:\Certificates\mycert.pfx" /d "Toggle Light/Dark" %TIMESTAMP_SERVER% "toggle.msi"
IF ERRORLEVEL 1 GOTO ERROR

ECHO.
ECHO Done: V%VER%
IF DEFINED INTERACTIVE_IBUILD COLOR 2F & PAUSE & COLOR
GOTO :EOF

:ERROR
ECHO ERROR: An error occured.
IF DEFINED INTERACTIVE_IBUILD COLOR 4F & PAUSE & COLOR
EXIT /B 1
GOTO :EOF
