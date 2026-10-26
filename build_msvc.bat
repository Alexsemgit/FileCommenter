@echo off
setlocal
cd /d "%~dp0"

echo ===================================================
echo   Sborka FileCommenter s pomoschyu Microsoft MSVC
echo ===================================================

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [POISK] cl.exe ne nayden v tekuschem okne.
    echo [POISK] Probuyu nayti Visual Studio avtomaticheski...
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    )
)

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [OSHIBKA] Kompilyator cl.exe ne nayden!
    echo Zapustite etot skript iz Developer Command Prompt for Visual Studio.
    pause
    exit /b 1
)

echo.
echo [1/3] Kompilyatsiya FileCommentTip.dll (COM In-Process DLL 64-bit)...
cl.exe /nologo /O2 /W3 /EHsc /D_UNICODE /DUNICODE /MD /LD FileCommentTip.cpp /link /DEF:FileCommentTip.def /OUT:FileCommentTip.dll uuid.lib shlwapi.lib shell32.lib ole32.lib user32.lib advapi32.lib
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat FileCommentTip.dll
    pause
    exit /b 1
)

echo.
echo [2/3] Kompilyatsiya FileCommentEditor.exe (GUI/CLI s ikonkami i resursami)...
rc.exe /nologo resource.rc
cl.exe /nologo /O2 /W3 /EHsc /D_UNICODE /DUNICODE /MD FileCommentEditor.cpp resource.res /link /SUBSYSTEM:WINDOWS /OUT:FileCommentEditor.exe comctl32.lib shell32.lib shlwapi.lib user32.lib gdi32.lib advapi32.lib ole32.lib
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat FileCommentEditor.exe
    pause
    exit /b 1
)

echo.
echo [3/4] Kompilyatsiya setup_comment.exe (Avtonomnyy installer)...
rc.exe /nologo /fo setup_resource.res setup_resource.rc
cl.exe /nologo /O2 /W3 /EHsc /D_UNICODE /DUNICODE /MD setup_comment.cpp setup_resource.res /link /SUBSYSTEM:WINDOWS /OUT:setup_comment.exe comctl32.lib shell32.lib shlwapi.lib user32.lib gdi32.lib advapi32.lib ole32.lib uuid.lib
if %errorlevel% neq 0 (
    echo [OSHIBKA] Ne udalos skompilirovat setup_comment.exe
    pause
    exit /b 1
)

echo.
echo [4/4] Kompilyatsiya TestHarness.exe (Stend otladki F5)...
cl.exe /nologo /O2 /W3 /EHsc /D_UNICODE /DUNICODE /MD TestHarness.cpp /link /SUBSYSTEM:CONSOLE /OUT:TestHarness.exe ole32.lib user32.lib uuid.lib
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
echo    - FileCommentTip.dll    (rasshirenie dlya Provodnika)
echo    - FileCommentEditor.exe (redaktor zametok, poisk i MUI)
echo    - TestHarness.exe       (testovyy puskatelem)
echo ===================================================
del *.obj *.exp *.lib resource.res setup_resource.res 2>nul
pause
