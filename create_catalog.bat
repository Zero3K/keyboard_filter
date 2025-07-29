@echo off
REM Script to create catalog file for keyboard filter driver
REM This script should be run after building the driver but before package creation

setlocal enabledelayedexpansion

REM Check if required parameters are provided
if "%~1"=="" (
    echo Usage: create_catalog.bat ^<architecture^> [os_version]
    echo.
    echo Examples:
    echo   create_catalog.bat x64 Win8.1
    echo   create_catalog.bat x86 Win7
    echo   create_catalog.bat x64 Win8
    echo.
    echo Default OS version is Win8.1 if not specified
    exit /b 1
)

set ARCH=%1
set OS_VER=%2
if "%OS_VER%"=="" set OS_VER=Win8.1

REM Set build output directory based on architecture and OS version
set BUILD_DIR=build\%ARCH%-%OS_VER%Release
set INTERMEDIATE_DIR=build\intermediate\%ARCH%-%OS_VER%Release

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo Error: Build directory %BUILD_DIR% does not exist.
    echo Please build the driver first using:
    echo   msbuild kbfiltr.vcxproj /p:Configuration="%OS_VER% Release" /property:Platform=%ARCH%
    exit /b 1
)

REM Check if driver file exists
if not exist "%BUILD_DIR%\kbfiltr.sys" (
    echo Error: Driver file %BUILD_DIR%\kbfiltr.sys does not exist.
    echo Please build the driver first.
    exit /b 1
)

REM Check if INF file exists
if not exist "%INTERMEDIATE_DIR%\kbfiltr.inf" (
    echo Error: INF file %INTERMEDIATE_DIR%\kbfiltr.inf does not exist.
    echo Please build the driver first to generate the INF from the INX template.
    exit /b 1
)

echo Creating catalog file for %ARCH% %OS_VER%...

REM Set OS version parameters for inf2cat
if "%OS_VER%"=="Win7" (
    set OS_LIST=7_X86,7_X64
) else if "%OS_VER%"=="Win8" (
    set OS_LIST=8_X86,8_X64
) else if "%OS_VER%"=="Win8.1" (
    set OS_LIST=6_3_X86,6_3_X64
) else (
    echo Error: Unsupported OS version %OS_VER%
    echo Supported versions: Win7, Win8, Win8.1
    exit /b 1
)

REM Create final INF file with CatalogFile directive enabled
echo Creating final INF file with catalog reference...
powershell -Command "(Get-Content '%INTERMEDIATE_DIR%\kbfiltr.inf') -replace ';CatalogFile=kbfiltr.cat', 'CatalogFile=kbfiltr.cat' | Set-Content '%BUILD_DIR%\kbfiltr.inf'"

REM Create catalog file using inf2cat
echo Running inf2cat to create catalog file...
inf2cat /driver:"%BUILD_DIR%" /os:%OS_LIST% /verbose

if errorlevel 1 (
    echo Error: inf2cat failed to create catalog file
    exit /b 1
)

REM Verify catalog file was created
if exist "%BUILD_DIR%\kbfiltr.cat" (
    echo Success: Catalog file created at %BUILD_DIR%\kbfiltr.cat
    echo Success: Final INF file created at %BUILD_DIR%\kbfiltr.inf
    echo.
    echo Complete driver package contents:
    echo - Driver: %BUILD_DIR%\kbfiltr.sys
    echo - INF:    %BUILD_DIR%\kbfiltr.inf (with catalog reference)
    echo - Catalog: %BUILD_DIR%\kbfiltr.cat
    echo.
    echo Next steps:
    echo 1. Sign the catalog file with your code signing certificate
    echo 2. Test install the driver package
) else (
    echo Error: Catalog file was not created
    exit /b 1
)

endlocal