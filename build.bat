@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "BUILD=%ROOT%\build"
set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set "CONFIG=Release"

echo.
echo ============================================================
echo  Configure CMake
echo ============================================================
echo.

cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FAILED] CMake configure failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  Build  [%CONFIG% ^| x64]
echo ============================================================
echo.

"%MSBUILD%" "%BUILD%\SearchAssetsImGui.sln" /p:Configuration=%CONFIG% /p:Platform=x64 /v:minimal /m

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FAILED] Build failed with error %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Build succeeded.
echo Output: %BUILD%\%CONFIG%\SearchAssetsV2.exe
echo.
pause
