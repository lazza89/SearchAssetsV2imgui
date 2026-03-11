@echo off
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set SLN="%~dp0build\SearchAssetsImGui.sln"
set CONFIG=Release
set PLATFORM=x64

echo.
echo ============================================================
echo  Building SearchAssetsImGui  [%CONFIG% ^| %PLATFORM%]
echo ============================================================
echo.

if not exist %SLN% (
    echo [ERROR] Solution not found: %SLN%
    echo Run CMake first to generate the build files.
    pause
    exit /b 1
)

%MSBUILD% %SLN% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /v:minimal /m

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FAILED] Build exited with error %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Build succeeded.
echo Output: %~dp0build\Release\SearchAssetsImGui.exe
echo.
pause
