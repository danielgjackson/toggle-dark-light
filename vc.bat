@ECHO OFF
IF NOT DEFINED VCPARAMS SET VCPARAMS=x86
SETLOCAL EnableDelayedExpansion

SET FIND_CL=
FOR %%p IN (cl.exe) DO SET "FIND_CL=%%~$PATH:p"
IF DEFINED FIND_CL (
  ENDLOCAL
  GOTO RUN
)

SET VCVARSALL=
FOR %%f IN (70 71 80 90 100 110 120 130 140) DO IF EXIST "!VS%%fCOMNTOOLS!\..\..\VC\vcvarsall.bat" SET VCVARSALL=!VS%%fCOMNTOOLS!\..\..\VC\vcvarsall.bat
FOR /F "usebackq tokens=*" %%f IN (`DIR /B /ON "%ProgramFiles(x86)%\Microsoft Visual Studio\????"`) DO FOR %%g IN (Community Professional Enterprise) DO IF EXIST "%ProgramFiles(x86)%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat" SET "VCVARSALL=%ProgramFiles(x86)%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat"
FOR /F "usebackq tokens=*" %%f IN (`DIR /B /ON "%ProgramFiles%\Microsoft Visual Studio\????"`) DO FOR %%g IN (Community Professional Enterprise) DO IF EXIST "%ProgramFiles%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat" SET "VCVARSALL=%ProgramFiles%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat"
IF DEFINED VCVARSALL (
  ENDLOCAL
  SET "VCVARSALL=%VCVARSALL%"
  GOTO VCVARS
)
ECHO Error: Cannot locate C compiler environment 'vcvarsall.bat'.
EXIT /B 1
GOTO :EOF

:VCVARS
@ECHO ON
CALL "%VCVARSALL%" %VCPARAMS%
@ECHO OFF

:RUN
@ECHO ON
%*