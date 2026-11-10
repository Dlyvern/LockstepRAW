@echo off
setlocal enabledelayedexpansion

:: Configuration
set PHYSX_BUILD_TYPE=%~1
if "%PHYSX_BUILD_TYPE%"=="" set PHYSX_BUILD_TYPE=checked
set "PHYSX_PRESET_BASE=vc17win64-cpu-only"
set "PHYSX_PRESET=%PHYSX_PRESET_BASE%-lockstep-ci"

set PHYSX_MSBUILD_CONFIGURATION=
if /I "%PHYSX_BUILD_TYPE%"=="debug" set PHYSX_MSBUILD_CONFIGURATION=Debug
if /I "%PHYSX_BUILD_TYPE%"=="checked" set PHYSX_MSBUILD_CONFIGURATION=Checked
if /I "%PHYSX_BUILD_TYPE%"=="profile" set PHYSX_MSBUILD_CONFIGURATION=Profile
if /I "%PHYSX_BUILD_TYPE%"=="release" set PHYSX_MSBUILD_CONFIGURATION=Release

if "%PHYSX_MSBUILD_CONFIGURATION%"=="" (
    echo [Lockstep_PhysX_build] Error: Unsupported PhysX build type "%PHYSX_BUILD_TYPE%"
    exit /b 1
)

for %%I in ("%~dp0..") do set PHYSX_EXTERNAL_ROOT=%%~fI
set "PHYSX_ROOT=%PHYSX_EXTERNAL_ROOT%\PhysX\physx"
set "PHYSX_LIB_OUTPUT=%PHYSX_EXTERNAL_ROOT%\lib\windows\%PHYSX_BUILD_TYPE%"
set "PHYSX_PRESET_TEMPLATE=%PHYSX_ROOT%\buildtools\presets\public\%PHYSX_PRESET_BASE%.xml"
set "PHYSX_PRESET_XML=%PHYSX_ROOT%\buildtools\presets\%PHYSX_PRESET%.xml"
set "PHYSX_COMPILER_DIR=%PHYSX_ROOT%\compiler\%PHYSX_PRESET%"
set "PHYSX_SOLUTION=%PHYSX_COMPILER_DIR%\PhysXSDK.sln"
set "PHYSX_BUILD_OUTPUT="

pushd "%PHYSX_ROOT%"

echo [Lockstep_PhysX_build] Python version:
python --version 2>&1 || echo [Lockstep_PhysX_build] WARNING: python not found in PATH

if not exist "%PHYSX_PRESET_TEMPLATE%" (
    echo [Lockstep_PhysX_build] Error: PhysX preset template not found at "%PHYSX_PRESET_TEMPLATE%"
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

echo [Lockstep_PhysX_build] Preparing PhysX preset %PHYSX_PRESET%...
python -c "from pathlib import Path; template = Path(r'%PHYSX_PRESET_TEMPLATE%'); output = Path(r'%PHYSX_PRESET_XML%'); text = template.read_text(); text = text.replace('name=\"%PHYSX_PRESET_BASE%\"', 'name=\"%PHYSX_PRESET%\"', 1); text = text.replace('name=\"PX_BUILDSNIPPETS\" value=\"True\"', 'name=\"PX_BUILDSNIPPETS\" value=\"False\"', 1); text = text.replace('name=\"PX_BUILDPVDRUNTIME\" value=\"True\"', 'name=\"PX_BUILDPVDRUNTIME\" value=\"False\"', 1); output.write_text(text)"
if errorlevel 1 (
    echo [Lockstep_PhysX_build] Error: Failed to write temporary PhysX preset
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

echo [Lockstep_PhysX_build] Generating Visual Studio project files...
call generate_projects.bat %PHYSX_PRESET% 2>&1
if errorlevel 1 (
    echo [Lockstep_PhysX_build] Error: Failed to generate PhysX Visual Studio projects
    echo [Lockstep_PhysX_build] Available presets:
    dir /b compiler 2>nul
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

if not exist "%PHYSX_SOLUTION%" (
    echo [Lockstep_PhysX_build] Error: Generated PhysX solution not found at "%PHYSX_SOLUTION%"
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

echo [Lockstep_PhysX_build] Building PhysX in %PHYSX_BUILD_TYPE% mode...
msbuild "%PHYSX_SOLUTION%" /p:Configuration=%PHYSX_MSBUILD_CONFIGURATION% /maxcpucount /t:Build /v:n /nologo
if errorlevel 1 (
    echo [Lockstep_PhysX_build] Error: MSBuild returned a non-zero exit code
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)
echo [Lockstep_PhysX_build] MSBuild completed successfully

rem The vc143 preset always outputs to bin\win.x86_64.vc143.mt\<type>\
set "PHYSX_BUILD_OUTPUT=%PHYSX_ROOT%\bin\win.x86_64.vc143.mt\%PHYSX_BUILD_TYPE%"
echo [Lockstep_PhysX_build] Expecting PhysX output at %PHYSX_BUILD_OUTPUT%

if not exist "%PHYSX_BUILD_OUTPUT%\" (
    echo [Lockstep_PhysX_build] Error: PhysX build output directory not found at "%PHYSX_BUILD_OUTPUT%"
    echo [Lockstep_PhysX_build] Contents of bin\:
    dir /b "%PHYSX_ROOT%\bin" 2>nul
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

echo [Lockstep_PhysX_build] Using PhysX build output from %PHYSX_BUILD_OUTPUT%
echo [Lockstep_PhysX_build] Copying built PhysX binaries to %PHYSX_LIB_OUTPUT%...
mkdir "%PHYSX_LIB_OUTPUT%" 2>nul

robocopy "%PHYSX_BUILD_OUTPUT%" "%PHYSX_LIB_OUTPUT%" *.lib *.dll *.pdb /njh /njs /ndl /nc /ns /np
set "ROBOCOPY_EXIT=%ERRORLEVEL%"
echo [Lockstep_PhysX_build] robocopy exit code: %ROBOCOPY_EXIT%
if %ROBOCOPY_EXIT% GEQ 8 (
    echo [Lockstep_PhysX_build] Error: Failed to copy PhysX binaries, robocopy exit %ROBOCOPY_EXIT%
    set "LOCKSTEP_EXIT_CODE=1" & goto :cleanup_and_exit
)

echo [Lockstep_PhysX_build] PhysX built and installed to %PHYSX_LIB_OUTPUT%
set "LOCKSTEP_EXIT_CODE=0" & goto :cleanup_and_exit

:cleanup_and_exit
if exist "%PHYSX_PRESET_XML%" (
    del /f /q "%PHYSX_PRESET_XML%" >nul 2>&1
    if exist "%PHYSX_PRESET_XML%" (
        echo [Lockstep_PhysX_build] Warning: Failed to delete temporary PhysX preset "%PHYSX_PRESET_XML%"
    )
)
popd
endlocal & exit /b %LOCKSTEP_EXIT_CODE%
