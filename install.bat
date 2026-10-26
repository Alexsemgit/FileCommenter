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
echo  Ustanovka FileCommenter v sistemu
echo ========================================================

echo.
echo [1/2] Registratsiya Shell Extension DLL v sisteme...
regsvr32.exe /s "%~dp0FileCommentTip.dll"
if %errorlevel% equ 0 (
    echo   [OK] FileCommentTip.dll uspeshno zaregistrirovana!
) else (
    echo   [OSHIBKA] Ne udalos zaregistrirovat DLL.
)

echo.
echo [2/2] Dobavlenie punktov v kontekstnoe menyu Provodnika...
"%~dp0FileCommentEditor.exe" --install-context
echo   [OK] Punkty kontekstnogo menyu uspeshno zaregistrirovany:
echo        * Lyubye fayly (*)              -^> "Kommentirovat..."
echo        * Papki (Directory, Folder)     -^> "Kommentirovat..."
echo        * Papki (Directory, Folder)     -^> "Iskat v zametkakh..."
echo        * Fon papki (Background)        -^> "Iskat v zametkakh..."
echo        * Diski (C:, D:, Drive)         -^> "Iskat v zametkakh..."

echo.
echo ========================================================
echo  USTANOVKA ZAVERSHENA!
echo ========================================================
pause
