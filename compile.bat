@echo off
setlocal enabledelayedexpansion

set "PF64=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"
for %%I in ("!PF64!") do set "PF64=%%~sI"
for %%I in ("!PF86!") do set "PF86=%%~sI"
set "VSWHERE=!PF86!\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSBASE=%%i"
)

if not defined VSBASE (
    echo ERROR: Visual Studio with C++ tools not found.
    exit /b 1
)

for /f "delims=" %%i in ('dir /b /ad /o-n "!VSBASE!\VC\Tools\MSVC" 2^>nul') do (
    set "MSVCVER=%%i"
    goto :msvc_found
)

:msvc_found
if not defined MSVCVER (
    echo ERROR: MSVC tools not found under !VSBASE!.
    exit /b 1
)

for /f "delims=" %%i in ('dir /b /ad /o-n "!PF86!\Windows Kits\10\Include" 2^>nul') do (
    if exist "!PF86!\Windows Kits\10\Include\%%i\um\windows.h" (
        if exist "!PF86!\Windows Kits\10\Include\%%i\ucrt\string.h" (
            set "WINSDKVER=%%i"
            goto :winsdk_found
        )
    )
)

:winsdk_found
if not defined WINSDKVER (
    echo ERROR: No usable Windows 10 SDK found.
    exit /b 1
)

set "CLPATH=!VSBASE!\VC\Tools\MSVC\!MSVCVER!\bin\Hostx64\x86\cl.exe"
set "LINKPATH=!VSBASE!\VC\Tools\MSVC\!MSVCVER!\bin\Hostx64\x86\link.exe"

set "INC1=!PF86!\Windows Kits\10\Include\!WINSDKVER!\um"
set "INC2=!PF86!\Windows Kits\10\Include\!WINSDKVER!\shared"
set "INC3=!PF86!\Windows Kits\10\Include\!WINSDKVER!\ucrt"
set "INC4=!VSBASE!\VC\Tools\MSVC\!MSVCVER!\include"

set "LIB1=!PF86!\Windows Kits\10\Lib\!WINSDKVER!\um\x86"
set "LIB2=!PF86!\Windows Kits\10\Lib\!WINSDKVER!\ucrt\x86"
set "LIB3=!VSBASE!\VC\Tools\MSVC\!MSVCVER!\lib\x86"

cd /d "%~dp0"

echo Building BullyAchievements.asi...

"%CLPATH%" /c /EHsc /O2 /MT /I"%INC1%" /I"%INC2%" /I"%INC3%" /I"%INC4%" AchievementsASI.cpp /Fo:AchievementsASI.obj || exit /b 1
"%LINKPATH%" /DLL /LIBPATH:"%LIB1%" /LIBPATH:"%LIB2%" /LIBPATH:"%LIB3%" AchievementsASI.obj kernel32.lib user32.lib /OUT:BullyAchievements.asi || exit /b 1

echo Build SUCCESS: BullyAchievements.asi

endlocal
