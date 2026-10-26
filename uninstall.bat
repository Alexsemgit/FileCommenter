@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [TREBUYUTSYA PRAVA ADMINISTRATORA]
    echo Zapustite etot fayl pravoy knopkoy myshi -^> "Zapusk ot imeni administratora".
    pause
    exit /b 1
)

echo ========================================================
echo  Udalenie FileCommenter iz sistemy
echo ========================================================

echo.
echo [1/2] Udalenie punkta iz kontekstnogo menyu Provodnika...
"%~dp0FileCommentEditor.exe" --uninstall-context
echo   [OK] Punkt menyu udalen.

echo.
echo [2/2] Snyatie registratsii Shell Extension DLL...
regsvr32.exe /u /s "%~dp0FileCommentTip.dll"
echo   [OK] Registratsiya DLL snyata.

echo.
echo ========================================================
echo  FileCommenter uspeshno udalen iz sistemy.
echo ========================================================
pause
