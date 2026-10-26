@echo off
setlocal
cd /d "%~dp0"

echo ===================================================
echo   Sborka FileCommenter s pomoschyu MinGW-w64 (GCC)
echo ===================================================

where g++.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [OSHIBKA] Kompilyator g++.exe ne nayden v PATH!
    pause
    exit /b 1
)

echo.
echo [1/3] Kompilyatsiya FileCommentTip.dll (COM DLL 64-bit)...
g++.exe -O2 -shared -std=c++17 -municode FileCommentTip.cpp FileCommentTip.def -o FileCommentTip.dll -luuid -lshlwapi -lshell32 -lole32 -luser32 -ladvapi32 -static
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat FileCommentTip.dll
    pause
    exit /b 1
)

echo.
echo [2/3] Kompilyatsiya FileCommentEditor.exe (GUI/CLI s ikonkami i resursami)...
windres.exe resource.rc -O coff -o resource.res
g++.exe -O2 -mwindows -std=c++17 -municode FileCommentEditor.cpp resource.res -o FileCommentEditor.exe -lcomctl32 -lshell32 -lshlwapi -lole32 -luser32 -lgdi32 -ladvapi32 -static
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat FileCommentEditor.exe
    del resource.res 2>nul
    pause
    exit /b 1
)
del resource.res 2>nul

echo.
echo [3/4] Kompilyatsiya setup_comment.exe (Avtonomnyy installer)...
windres.exe setup_resource.rc -O coff -o setup_resource.res
g++.exe -O2 -mwindows -std=c++17 -municode setup_comment.cpp setup_resource.res -o setup_comment.exe -lcomctl32 -lshell32 -lshlwapi -lole32 -luser32 -lgdi32 -ladvapi32 -luuid -static -static-libgcc -static-libstdc++
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat setup_comment.exe
    del setup_resource.res 2>nul
    pause
    exit /b 1
)
del setup_resource.res 2>nul

echo.
echo [4/4] Kompilyatsiya TestHarness.exe (F5 test)...
g++.exe -O2 -std=c++17 -municode TestHarness.cpp -o TestHarness.exe -lole32 -luser32 -luuid -static
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat TestHarness.exe
    pause
    exit /b 1
)

echo.
echo ===================================================
echo  SBORKA USPESHNO ZAVERSHENA!
echo  Polucheny fayly:
echo    - setup_comment.exe     (avtonomnyy ustanovschik "vse-v-odnom")
echo    - FileCommentTip.dll
echo    - FileCommentEditor.exe (s krasivoy ikonkoy i MUI)
echo    - TestHarness.exe
echo ===================================================
pause
