@echo off
setlocal
cd /d "%~dp0"

set "STUDIO_EXE=build\studio\bin\minisnn_studio.exe"

if not exist "%STUDIO_EXE%" (
    where mingw32-make >nul 2>nul
    if errorlevel 1 (
        echo ERRO: mingw32-make nao foi encontrado no PATH.
        pause
        exit /b 1
    )

    mingw32-make core-studio
    if errorlevel 1 (
        echo ERRO: falha ao compilar o miniSNN Studio.
        pause
        exit /b 1
    )
)

if not exist "%STUDIO_EXE%" (
    echo ERRO: o executavel nao foi criado em %STUDIO_EXE%.
    pause
    exit /b 1
)

if /I "%~1"=="--build-only" (
    endlocal
    exit /b 0
)

start "" "%CD%\%STUDIO_EXE%" --repository-root "%CD%"
if errorlevel 1 (
    echo ERRO: nao foi possivel abrir o miniSNN Studio.
    pause
    exit /b 1
)

endlocal
exit /b 0
