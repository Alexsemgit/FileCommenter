// ============================================================================
// setup_comment.cpp — Автономный единый установщик (Single-File Installer)
// 
// Возможности:
//   1. Все компоненты (FileCommentEditor.exe, FileCommentTip.dll, app_icon.ico, 
//      деинсталлятор) встроены в ресурсы самого setup_comment.exe.
//   2. Предложение папки установки по умолчанию (Program Files / Local AppData).
//   3. Удобный выбор любой пользовательской папки («Обзор...»).
//   4. Настраиваемые опции:
//      - Контекстное меню Проводника («Комментировать...», «Искать в заметках...»)
//      - Регистрация всплывающих подсказок Windows Explorer (FileCommentTip.dll)
//      - Создание ярлыков на Рабочем столе и в меню Пуск
//      - Автозагрузка вместе с Windows
//      - Немедленный запуск программы в трей после установки
//   5. Регистрация в панели «Программы и компоненты» Windows для легкого удаления.
//   6. Поддержка русского и английского языков с автоопределением по системе.
//   7. Свободное перемещение окна мышью за любую свободную область.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Ресурсы
#define IDI_SETUP_ICON           1
#define IDR_BIN_EDITOR           1001
#define IDR_BIN_TIP_DLL          1002
#define IDR_BIN_ICON             1003
#define IDR_BIN_UNINSTALL_BAT    1004

// Элементы управления
#define IDC_ST_HEADER_TITLE      101
#define IDC_ST_HEADER_SUB        102
#define IDC_GRP_PATH             103
#define IDC_EDT_PATH             104
#define IDC_BTN_BROWSE           105
#define IDC_GRP_OPTIONS          106
#define IDC_CHK_CTX_MENU         107
#define IDC_CHK_SHELL_TIP        108
#define IDC_CHK_SHORTCUTS        109
#define IDC_CHK_AUTOSTART        110
#define IDC_CHK_LAUNCH_AFTER     111
#define IDC_PROGRESS             112
#define IDC_ST_STATUS            113
#define IDC_BTN_INSTALL          114
#define IDC_BTN_CANCEL           115
#define IDC_BTN_LANG             116

// Языки
enum SetupLang {
    SETUP_LANG_RU = 0,
    SETUP_LANG_EN = 1
};

static SetupLang g_lang = SETUP_LANG_RU;
static HINSTANCE g_hInstance = NULL;
static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontHeader = NULL;
static HICON g_hIcon = NULL;
static bool g_bInstalledSuccessfully = false;

// Проверка системного языка (русский)
static bool IsSystemRussian() {
    LANGID langId = GetUserDefaultUILanguage();
    return (PRIMARYLANGID(langId) == LANG_RUSSIAN);
}

// Помощник создания каталогов рекурсивно
static bool CreateDirRecursive(const std::wstring& path) {
    if (path.empty()) return false;
    if (PathFileExistsW(path.c_str())) return true;

    wchar_t folder[MAX_PATH];
    StringCchCopyW(folder, ARRAYSIZE(folder), path.c_str());

    for (wchar_t* p = folder; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t orig = *p;
            *p = 0;
            if (wcslen(folder) > 2 && folder[wcslen(folder) - 1] != L':') {
                CreateDirectoryW(folder, NULL);
            }
            *p = orig;
        }
    }
    return CreateDirectoryW(folder, NULL) != FALSE || PathFileExistsW(path.c_str());
}

// Извлечение встроенного бинарного ресурса в файл на диске
static bool ExtractResourceToFile(int resId, const std::wstring& targetFilePath) {
    HRSRC hRes = FindResourceW(g_hInstance, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hLoaded = LoadResource(g_hInstance, hRes);
    if (!hLoaded) return false;

    DWORD resSize = SizeofResource(g_hInstance, hRes);
    if (resSize == 0) return false;

    void* pData = LockResource(hLoaded);
    if (!pData) return false;

    HANDLE hFile = CreateFileW(targetFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL bSuccess = WriteFile(hFile, pData, resSize, &written, NULL);
    CloseHandle(hFile);

    return (bSuccess && written == resSize);
}

// Запись строкового значения реестра
static bool RegSetString(HKEY hRoot, const std::wstring& subkey, const wchar_t* valName, const std::wstring& valData) {
    HKEY hKey;
    if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD cbData = (DWORD)((valData.length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, valName, 0, REG_SZ, (const BYTE*)valData.c_str(), cbData);
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// Создание ярлыка Windows (.lnk)
static bool CreateShellShortcut(
    const std::wstring& shortcutPath,
    const std::wstring& targetPath,
    const std::wstring& arguments,
    const std::wstring& iconPath,
    const std::wstring& description
) {
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        if (!arguments.empty()) psl->SetArguments(arguments.c_str());
        if (!description.empty()) psl->SetDescription(description.c_str());
        if (!iconPath.empty()) psl->SetIconLocation(iconPath.c_str(), 0);

        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
        return SUCCEEDED(hr);
    }
    return false;
}

// Диалог выбора папки
static bool BrowseForFolder(HWND hWndParent, std::wstring& outFolder) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWndParent;
    std::wstring title = (g_lang == SETUP_LANG_RU) ? L"Выберите папку для установки FileCommenter:" : L"Select destination folder for FileCommenter:";
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            outFolder = path;
            CoTaskMemFree(pidl);
            return true;
        }
        CoTaskMemFree(pidl);
    }
    return false;
}

// Определение папки по умолчанию
static std::wstring GetDefaultInstallPath() {
    wchar_t szProgFiles[MAX_PATH] = { 0 };
    if (SHGetSpecialFolderPathW(NULL, szProgFiles, CSIDL_PROGRAM_FILES, FALSE) && wcslen(szProgFiles) > 0) {
        std::wstring p = szProgFiles;
        if (p.back() != L'\\') p += L'\\';
        p += L"FileCommenter";
        return p;
    }
    
    // Резервный путь в LocalAppData
    wchar_t szLocalApp[MAX_PATH] = { 0 };
    if (SHGetSpecialFolderPathW(NULL, szLocalApp, CSIDL_LOCAL_APPDATA, FALSE) && wcslen(szLocalApp) > 0) {
        std::wstring p = szLocalApp;
        if (p.back() != L'\\') p += L'\\';
        p += L"Programs\\FileCommenter";
        return p;
    }

    return L"C:\\Program Files\\FileCommenter";
}

// Создание чистого скрипта деинсталляции
static void WriteUninstallScript(const std::wstring& installDir) {
    std::wstring batPath = installDir;
    if (batPath.back() != L'\\') batPath += L'\\';
    batPath += L"uninstall.bat";

    std::string script = 
        "@echo off\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0\"\r\n"
        "net session >nul 2>&1\r\n"
        "if %errorlevel% neq 0 (\r\n"
        "    echo [ADMIN RIGHTS REQUIRED / TREBUYUTSYA PRAVA ADMINISTRATORA]\r\n"
        "    echo Run this file via right click -^> Run as administrator.\r\n"
        "    pause\r\n"
        "    exit /b 1\r\n"
        ")\r\n"
        "echo ========================================================\r\n"
        "echo  Uninstalling FileCommenter...\r\n"
        "echo ========================================================\r\n"
        "echo [1/5] Terminating active processes...\r\n"
        "taskkill /f /im FileCommentEditor.exe >nul 2>&1\r\n"
        "echo [2/5] Unregistering Shell Extension DLL...\r\n"
        "regsvr32.exe /u /s \"%~dp0FileCommentTip.dll\"\r\n"
        "echo [3/5] Removing Explorer context menu entries...\r\n"
        "\"%~dp0FileCommentEditor.exe\" --uninstall-context >nul 2>&1\r\n"
        "echo [4/5] Removing autostart and registry entries...\r\n"
        "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"FileCommenter\" /f >nul 2>&1\r\n"
        "reg delete \"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FileCommenter\" /f >nul 2>&1\r\n"
        "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FileCommenter\" /f >nul 2>&1\r\n"
        "reg delete \"HKCU\\Software\\FileCommenter\" /f >nul 2>&1\r\n"
        "del /f /q \"%USERPROFILE%\\Desktop\\FileCommenter.lnk\" >nul 2>&1\r\n"
        "del /f /q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\FileCommenter.lnk\" >nul 2>&1\r\n"
        "echo [5/5] Deleting files...\r\n"
        "echo Done! FileCommenter has been uninstalled successfully.\r\n"
        "start \"\" cmd /c \"ping 127.0.0.1 -n 2 >nul & del /f /q \\\"%~dp0*.*\\\" >nul 2>&1 & rmdir /s /q \\\"%~dp0\\\" >nul 2>&1\"\r\n";

    HANDLE hFile = CreateFileW(batPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, script.c_str(), (DWORD)script.length(), &written, NULL);
        CloseHandle(hFile);
    }
}

// Обновление локализованных текстов
static void UpdateTexts(HWND hWnd) {
    if (g_lang == SETUP_LANG_RU) {
        SetWindowTextW(hWnd, L"Установка FileCommenter v1.2");
        SetDlgItemTextW(hWnd, IDC_ST_HEADER_TITLE, L"Установка FileCommenter");
        SetDlgItemTextW(hWnd, IDC_ST_HEADER_SUB, L"Заметки и мгновенный поиск по файлам Windows (NTFS Streams)");
        SetDlgItemTextW(hWnd, IDC_GRP_PATH, L" Папка установки: ");
        SetDlgItemTextW(hWnd, IDC_BTN_BROWSE, L"Обзор...");
        SetDlgItemTextW(hWnd, IDC_GRP_OPTIONS, L" Дополнительные действия: ");
        SetDlgItemTextW(hWnd, IDC_CHK_CTX_MENU, L"Интегрировать в контекстное меню Проводника («Комментировать...» / «Искать...»)");
        SetDlgItemTextW(hWnd, IDC_CHK_SHELL_TIP, L"Включить всплывающие подсказки с заметками при наведении (FileCommentTip.dll)");
        SetDlgItemTextW(hWnd, IDC_CHK_SHORTCUTS, L"Создать ярлыки на Рабочем столе и в меню «Пуск»");
        SetDlgItemTextW(hWnd, IDC_CHK_AUTOSTART, L"Запускать FileCommenter вместе с Windows");
        SetDlgItemTextW(hWnd, IDC_CHK_LAUNCH_AFTER, L"Запустить приложение в системном трее сразу после установки");
        SetDlgItemTextW(hWnd, IDC_BTN_LANG, L"Language: EN");
        if (g_bInstalledSuccessfully) {
            SetDlgItemTextW(hWnd, IDC_BTN_INSTALL, L"Готово");
            SetDlgItemTextW(hWnd, IDC_BTN_CANCEL, L"Закрыть");
            SetDlgItemTextW(hWnd, IDC_ST_STATUS, L"Установка успешно завершена! Программа готова к использованию.");
        } else {
            SetDlgItemTextW(hWnd, IDC_BTN_INSTALL, L"Установить");
            SetDlgItemTextW(hWnd, IDC_BTN_CANCEL, L"Отмена");
            SetDlgItemTextW(hWnd, IDC_ST_STATUS, L"Нажмите «Установить» для начала установки.");
        }
    } else {
        SetWindowTextW(hWnd, L"FileCommenter Setup v1.2");
        SetDlgItemTextW(hWnd, IDC_ST_HEADER_TITLE, L"FileCommenter Setup");
        SetDlgItemTextW(hWnd, IDC_ST_HEADER_SUB, L"File notes & instant search for Windows Explorer (NTFS Streams)");
        SetDlgItemTextW(hWnd, IDC_GRP_PATH, L" Destination Folder: ");
        SetDlgItemTextW(hWnd, IDC_BTN_BROWSE, L"Browse...");
        SetDlgItemTextW(hWnd, IDC_GRP_OPTIONS, L" Setup Options: ");
        SetDlgItemTextW(hWnd, IDC_CHK_CTX_MENU, L"Add to Explorer right-click context menu ('Comment...' / 'Search...')");
        SetDlgItemTextW(hWnd, IDC_CHK_SHELL_TIP, L"Enable mouse hover infotip notes in Explorer (FileCommentTip.dll)");
        SetDlgItemTextW(hWnd, IDC_CHK_SHORTCUTS, L"Create shortcuts on Desktop and Start Menu");
        SetDlgItemTextW(hWnd, IDC_CHK_AUTOSTART, L"Start FileCommenter automatically with Windows");
        SetDlgItemTextW(hWnd, IDC_CHK_LAUNCH_AFTER, L"Launch application in system tray after setup finishes");
        SetDlgItemTextW(hWnd, IDC_BTN_LANG, L"Язык: RU");
        if (g_bInstalledSuccessfully) {
            SetDlgItemTextW(hWnd, IDC_BTN_INSTALL, L"Finish");
            SetDlgItemTextW(hWnd, IDC_BTN_CANCEL, L"Close");
            SetDlgItemTextW(hWnd, IDC_ST_STATUS, L"Installation completed successfully! Ready to use.");
        } else {
            SetDlgItemTextW(hWnd, IDC_BTN_INSTALL, L"Install");
            SetDlgItemTextW(hWnd, IDC_BTN_CANCEL, L"Cancel");
            SetDlgItemTextW(hWnd, IDC_ST_STATUS, L"Click 'Install' to start installation.");
        }
    }
}

// Процесс установки
static void DoInstallation(HWND hWnd) {
    wchar_t szInstallPath[MAX_PATH] = { 0 };
    GetDlgItemTextW(hWnd, IDC_EDT_PATH, szInstallPath, ARRAYSIZE(szInstallPath));

    std::wstring installDir = szInstallPath;
    while (!installDir.empty() && (installDir.back() == L' ' || installDir.back() == L'\\')) {
        installDir.pop_back();
    }

    if (installDir.empty()) {
        MessageBoxW(hWnd, (g_lang == SETUP_LANG_RU) ? L"Укажите папку для установки!" : L"Please specify a destination folder!", L"Error", MB_ICONWARNING);
        return;
    }

    // Блокируем элементы управления на время процесса
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_EDT_PATH), FALSE);

    HWND hProgress = GetDlgItem(hWnd, IDC_PROGRESS);
    SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(hProgress, PBM_SETPOS, 5, 0);

    SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Создание папки установки..." : L"Creating destination folder...");
    UpdateWindow(hWnd);

    if (!CreateDirRecursive(installDir)) {
        std::wstring errMsg = (g_lang == SETUP_LANG_RU) ?
            L"Не удалось создать папку:\n" + installDir + L"\n\nВозможно требуются права администратора или путь недоступен." :
            L"Could not create directory:\n" + installDir + L"\n\nAdministrator privileges may be required.";
        MessageBoxW(hWnd, errMsg.c_str(), L"Error", MB_ICONERROR);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_EDT_PATH), TRUE);
        return;
    }

    SendMessageW(hProgress, PBM_SETPOS, 20, 0);
    SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Распаковка компонентов..." : L"Extracting components...");
    UpdateWindow(hWnd);

    // 1. Распаковка бинарных файлов
    std::wstring editorExe = installDir + L"\\FileCommentEditor.exe";
    std::wstring tipDll = installDir + L"\\FileCommentTip.dll";
    std::wstring appIcon = installDir + L"\\app_icon.ico";

    bool b1 = ExtractResourceToFile(IDR_BIN_EDITOR, editorExe);
    SendMessageW(hProgress, PBM_SETPOS, 40, 0);

    bool b2 = ExtractResourceToFile(IDR_BIN_TIP_DLL, tipDll);
    SendMessageW(hProgress, PBM_SETPOS, 55, 0);

    bool b3 = ExtractResourceToFile(IDR_BIN_ICON, appIcon);
    SendMessageW(hProgress, PBM_SETPOS, 65, 0);

    WriteUninstallScript(installDir);

    if (!b1 || !b2) {
        std::wstring err = (g_lang == SETUP_LANG_RU) ?
            L"Ошибка при записи файлов программы. Убедитесь, что FileCommenter не запущен." :
            L"Error writing application files. Make sure FileCommenter is not running.";
        MessageBoxW(hWnd, err.c_str(), L"Error", MB_ICONERROR);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);
        return;
    }

    // 2. Регистрация Shell Extension DLL (FileCommentTip.dll)
    if (IsDlgButtonChecked(hWnd, IDC_CHK_SHELL_TIP) == BST_CHECKED) {
        SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Регистрация подсказок в Проводнике..." : L"Registering Explorer infotip shell extension...");
        UpdateWindow(hWnd);

        std::wstring regCmd = L"/s \"" + tipDll + L"\"";
        ShellExecuteW(NULL, L"open", L"regsvr32.exe", regCmd.c_str(), NULL, SW_HIDE);
    }
    SendMessageW(hProgress, PBM_SETPOS, 75, 0);

    // 3. Добавление в контекстное меню Проводника
    if (IsDlgButtonChecked(hWnd, IDC_CHK_CTX_MENU) == BST_CHECKED) {
        SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Настройка контекстного меню Проводника..." : L"Configuring Explorer context menu...");
        UpdateWindow(hWnd);

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpFile = editorExe.c_str();
        sei.lpParameters = L"--install-context";
        sei.nShow = SW_HIDE;
        ShellExecuteExW(&sei);
    }
    SendMessageW(hProgress, PBM_SETPOS, 85, 0);

    // 4. Создание ярлыков
    if (IsDlgButtonChecked(hWnd, IDC_CHK_SHORTCUTS) == BST_CHECKED) {
        SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Создание ярлыков..." : L"Creating shortcuts...");
        UpdateWindow(hWnd);

        wchar_t szDesktop[MAX_PATH];
        if (SHGetSpecialFolderPathW(NULL, szDesktop, CSIDL_DESKTOPDIRECTORY, FALSE)) {
            std::wstring deskLnk = std::wstring(szDesktop) + L"\\FileCommenter.lnk";
            CreateShellShortcut(deskLnk, editorExe, L"--tray", appIcon, L"FileCommenter");
        }

        wchar_t szPrograms[MAX_PATH];
        if (SHGetSpecialFolderPathW(NULL, szPrograms, CSIDL_PROGRAMS, FALSE)) {
            std::wstring startLnk = std::wstring(szPrograms) + L"\\FileCommenter.lnk";
            CreateShellShortcut(startLnk, editorExe, L"--tray", appIcon, L"FileCommenter");
        }
    }
    SendMessageW(hProgress, PBM_SETPOS, 90, 0);

    // 5. Автозагрузка с Windows
    if (IsDlgButtonChecked(hWnd, IDC_CHK_AUTOSTART) == BST_CHECKED) {
        std::wstring runCmd = L"\"" + editorExe + L"\" --tray";
        RegSetString(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"FileCommenter", runCmd);
    }

    // 6. Регистрация в панели «Программы и компоненты» Windows (Add/Remove Programs)
    std::wstring uninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FileCommenter";
    std::wstring uninstBat = L"\"" + installDir + L"\\uninstall.bat\"";
    std::wstring iconVal = L"\"" + editorExe + L"\",0";

    RegSetString(HKEY_CURRENT_USER, uninstKey, L"DisplayName", L"FileCommenter");
    RegSetString(HKEY_CURRENT_USER, uninstKey, L"DisplayVersion", L"1.2.0");
    RegSetString(HKEY_CURRENT_USER, uninstKey, L"Publisher", L"FileCommenter Project");
    RegSetString(HKEY_CURRENT_USER, uninstKey, L"InstallLocation", installDir);
    RegSetString(HKEY_CURRENT_USER, uninstKey, L"DisplayIcon", iconVal);
    RegSetString(HKEY_CURRENT_USER, uninstKey, L"UninstallString", uninstBat);

    // Оповещаем оболочку об изменении связей
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    SendMessageW(hProgress, PBM_SETPOS, 100, 0);

    // 7. Запуск в трей
    if (IsDlgButtonChecked(hWnd, IDC_CHK_LAUNCH_AFTER) == BST_CHECKED) {
        ShellExecuteW(NULL, L"open", editorExe.c_str(), L"--tray", NULL, SW_SHOWNORMAL);
    }

    g_bInstalledSuccessfully = true;
    UpdateTexts(hWnd);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);

    // Звуковой сигнал об успешном завершении
    MessageBeep(MB_OK);
}

// Оконная процедура
static LRESULT CALLBACK SetupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        if (g_hIcon) {
            SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
            SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);
        }

        g_hFontUI = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontHeader = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Заголовок и подзаголовок
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 18, 420, 26, hWnd, (HMENU)IDC_ST_HEADER_TITLE, g_hInstance, NULL);

        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 46, 440, 20, hWnd, (HMENU)IDC_ST_HEADER_SUB, g_hInstance, NULL);

        // Переключатель языка в правом верхнем углу
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            475, 18, 120, 26, hWnd, (HMENU)IDC_BTN_LANG, g_hInstance, NULL);

        // Группа пути установки
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            20, 75, 575, 75, hWnd, (HMENU)IDC_GRP_PATH, g_hInstance, NULL);

        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            35, 102, 435, 26, hWnd, (HMENU)IDC_EDT_PATH, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            480, 102, 100, 26, hWnd, (HMENU)IDC_BTN_BROWSE, g_hInstance, NULL);

        // Группа параметров установки
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            20, 160, 575, 175, hWnd, (HMENU)IDC_GRP_OPTIONS, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 186, 545, 22, hWnd, (HMENU)IDC_CHK_CTX_MENU, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_CHK_CTX_MENU, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 214, 545, 22, hWnd, (HMENU)IDC_CHK_SHELL_TIP, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_CHK_SHELL_TIP, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 242, 545, 22, hWnd, (HMENU)IDC_CHK_SHORTCUTS, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_CHK_SHORTCUTS, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 270, 545, 22, hWnd, (HMENU)IDC_CHK_AUTOSTART, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_CHK_AUTOSTART, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 298, 545, 22, hWnd, (HMENU)IDC_CHK_LAUNCH_AFTER, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_CHK_LAUNCH_AFTER, BST_CHECKED);

        // Прогресс-бар и статус
        HWND hProg = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 348, 575, 14, hWnd, (HMENU)IDC_PROGRESS, g_hInstance, NULL);

        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            20, 368, 575, 20, hWnd, (HMENU)IDC_ST_STATUS, g_hInstance, NULL);

        // Кнопки Установить / Отмена
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            355, 398, 115, 32, hWnd, (HMENU)IDC_BTN_INSTALL, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            480, 398, 115, 32, hWnd, (HMENU)IDC_BTN_CANCEL, g_hInstance, NULL);

        // Шрифты
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)g_hFontUI);

        SendMessageW(GetDlgItem(hWnd, IDC_ST_HEADER_TITLE), WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);

        // Начальный путь установки
        std::wstring defPath = GetDefaultInstallPath();
        SetDlgItemTextW(hWnd, IDC_EDT_PATH, defPath.c_str());

        UpdateTexts(hWnd);
        return 0;
    }

    // Свободное перемещение окна мышью за любую свободную область
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hWnd, uMsg, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hWnd, &pt);
            HWND hChild = ChildWindowFromPoint(hWnd, pt);
            if (!hChild || hChild == hWnd) {
                return HTCAPTION;
            }
            wchar_t cls[64] = { 0 };
            GetClassNameW(hChild, cls, ARRAYSIZE(cls));
            if (wcscmp(cls, L"Static") == 0 || wcscmp(cls, L"Button") == 0) {
                LONG style = GetWindowLongW(hChild, GWL_STYLE);
                if ((style & BS_GROUPBOX) == BS_GROUPBOX || wcscmp(cls, L"Static") == 0) {
                    return HTCAPTION;
                }
            }
        }
        return hit;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case IDC_BTN_LANG: {
            g_lang = (g_lang == SETUP_LANG_RU) ? SETUP_LANG_EN : SETUP_LANG_RU;
            UpdateTexts(hWnd);
            break;
        }

        case IDC_BTN_BROWSE: {
            std::wstring folder;
            if (BrowseForFolder(hWnd, folder)) {
                if (folder.back() != L'\\') folder += L'\\';
                folder += L"FileCommenter";
                SetDlgItemTextW(hWnd, IDC_EDT_PATH, folder.c_str());
            }
            break;
        }

        case IDC_BTN_INSTALL: {
            if (g_bInstalledSuccessfully) {
                PostQuitMessage(0);
            } else {
                DoInstallation(hWnd);
            }
            break;
        }

        case IDC_BTN_CANCEL: {
            PostQuitMessage(0);
            break;
        }
        }
        return 0;
    }

    case WM_DESTROY: {
        if (g_hFontUI) DeleteObject(g_hFontUI);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontHeader) DeleteObject(g_hFontHeader);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_hInstance = hInstance;
    g_lang = IsSystemRussian() ? SETUP_LANG_RU : SETUP_LANG_EN;

    CoInitialize(NULL);
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    g_hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SETUP_ICON));

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SetupWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FileCommenterSetupClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = g_hIcon;
    wc.hIconSm = g_hIcon;
    RegisterClassExW(&wc);

    int winW = 630;
    int winH = 485;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"FileCommenter Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
