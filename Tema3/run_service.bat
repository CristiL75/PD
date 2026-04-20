@echo off
REM Script pentru instalarea si pornirea serviciului Windows cu admin
REM Trebuie rulat cu "Run as Administrator"

setlocal enabledelayedexpansion

echo.
echo ========================================
echo Serviciu Windows: Hello World Service
echo ========================================
echo.

REM Obtine calea completa a executabilului
cd /d "%~dp0"
set "EXE_PATH=%cd%\hello_world_service.exe"

echo Executabil: %EXE_PATH%

REM Verifica daca executabilul exista
if not exist "%EXE_PATH%" (
    echo EROARE: Executabilul nu exista!
    pause
    exit /b 1
)

REM Verifica daca serviciul exista deja si il sterge
echo.
echo Verificare serviciu existent...
sc query Tema3HelloWorldService >nul 2>&1
if !errorlevel! equ 0 (
    echo Serviciu existent detectat. Se sterge...
    sc stop Tema3HelloWorldService >nul 2>&1
    timeout /t 1 /nobreak >nul
    sc delete Tema3HelloWorldService >nul 2>&1
    timeout /t 1 /nobreak >nul
)

REM Creeaza serviciul
echo.
echo Se creeaza serviciul...
sc create Tema3HelloWorldService binPath= "%EXE_PATH%" start= demand
if !errorlevel! neq 0 (
    echo EROARE la crearea serviciului!
    pause
    exit /b 2
)
echo OK.

REM Porneste serviciul
echo.
echo Se porneste serviciul...
sc start Tema3HelloWorldService

REM Asteapta putin pentru ca serviciul sa se initializeze
timeout /t 2 /nobreak >nul

REM Verifica statusul
echo.
echo Status serviciu:
sc query Tema3HelloWorldService

REM Verifica log file in ProgramData
echo.
echo Verificare mesaj Hello World:
set "LOGFILE=C:\ProgramData\Tema3_HelloWorld.log"
if exist "%LOGFILE%" (
    echo Fisier log gasit: %LOGFILE%
    type "%LOGFILE%"
) else (
    echo Fisier log nu exista la %LOGFILE%
)

echo.
echo ========================================
echo Comenzi utile:
echo   sc stop Tema3HelloWorldService
echo   sc delete Tema3HelloWorldService  
echo   type "%TEMP%\Tema3_HelloWorld.log"
echo ========================================
echo.

pause
