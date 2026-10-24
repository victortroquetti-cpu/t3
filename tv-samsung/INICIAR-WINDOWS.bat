@echo off
rem Liga o servidor da Minha TV no Windows. Deixe a janela aberta enquanto usa a TV.
chcp 65001 >nul
cd /d "%~dp0"

where py >nul 2>nul
if %errorlevel%==0 (
    py -3 servidor\servidor.py %*
    goto fim
)

python --version >nul 2>nul
if %errorlevel%==0 (
    python servidor\servidor.py %*
    goto fim
)

echo.
echo Nao encontrei o Python 3 neste PC.
echo Instale pelo site https://www.python.org/downloads/ (marque "Add python.exe to PATH")
echo ou rode no PowerShell:  winget install Python.Python.3.12
echo Depois abra este arquivo de novo.

:fim
echo.
pause
