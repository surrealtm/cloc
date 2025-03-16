@echo off

:: --- Setup the build environment
IF NOT DEFINED DevEnvDir call vcvarsall amd64

:: --- Don't set all variables declared below into the permament environment
setlocal enabledelayedexpansion

:: --- Make sure we are in the correct directory
cd /D "%~dp0"

:: --- Unpack arguments
for %%a in (%*) do set "%%a=1"

if "%debug%"=="1"   set release=0 && echo [Debug Mode]
if "%release%"=="1" set debug=0   && echo [Release Mode]

:: --- Prepare output directories
if not exist bin mkdir bin

:: --- Compile Line Definitions
set CL_COMMON=  ..\\src\\cloc.c /nologo /FC /Z7 /DWIN32 /link /OUT:cloc.exe
set CL_DEBUG=   cl /Od /Ob1 %CL_COMMON%
set CL_RELEASE= cl /O2 %CL_COMMON%

:: --- Build everything
pushd bin
if "%debug%"=="1"   set didbuild=1 && %CL_DEBUG%
if "%release%"=="1" set didbuild=1 && %CL_RELEASE%
popd

:: --- Warn on No Builds
if "%didbuild%"=="" (
   echo [WARNING] No valid build target specified; specify a target as argument to this script!
)
