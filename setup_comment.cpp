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
#include <tlhelp32.h>
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

// Проверка: запущен ли процесс с правами администратора
static bool IsProcessElevated() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated != 0;
        }
        CloseHandle(hToken);
    }
    return fRet != FALSE;
}

// Перезапуск текущего установщика с запросом прав администратора (UAC Elevation)
static bool RelaunchElevated(HWND hWndParent, const wchar_t* lpCmdLine = L"") {
    wchar_t szExePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szExePath, ARRAYSIZE(szExePath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas"; // Вызов окна подтверждения UAC
        sei.lpFile = szExePath;
        sei.lpParameters = (lpCmdLine && wcslen(lpCmdLine) > 0) ? lpCmdLine : NULL;
        sei.hwnd = hWndParent;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            return true;
        }
    }
    return false;
}

// Принудительное завершение всех запущенных копий приложения перед установкой/обновлением
static void TerminateRunningEditor() {
    // 1. Пытаемся корректно закрыть окна FileCommenter
    HWND hWnd = FindWindowW(L"FileCommentEditorWndClass", NULL);
    for (int attempts = 0; attempts < 5 && hWnd != NULL; ++attempts) {
        SendMessageW(hWnd, WM_CLOSE, 0, 0);
        Sleep(100);
        hWnd = FindWindowW(L"FileCommentEditorWndClass", NULL);
    }

    // 2. Поиск и принудительное завершение всех процессов FileCommentEditor.exe и TestHarness.exe
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"FileCommentEditor.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"TestHarness.exe") == 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        WaitForSingleObject(hProc, 2000);
                        CloseHandle(hProc);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }

    // 3. Дополнительная страховка через taskkill
    WinExec("taskkill /f /im FileCommentEditor.exe /im TestHarness.exe >nul 2>&1", SW_HIDE);
    Sleep(250);
}

// Извлечение встроенного бинарного ресурса в файл на диске с поддержкой горячей замены занятых файлов
static bool ExtractResourceToFile(int resId, const std::wstring& targetFilePath) {
    HRSRC hRes = FindResourceW(g_hInstance, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hLoaded = LoadResource(g_hInstance, hRes);
    if (!hLoaded) return false;

    DWORD resSize = SizeofResource(g_hInstance, hRes);
    if (resSize == 0) return false;

    void* pData = LockResource(hLoaded);
    if (!pData) return false;

    // Снимаем атрибуты "Только для чтения" или "Системный", если они были выставлены
    DWORD dwOldAttr = GetFileAttributesW(targetFilePath.c_str());
    if (dwOldAttr != INVALID_FILE_ATTRIBUTES && (dwOldAttr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM))) {
        SetFileAttributesW(targetFilePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    }

    // Попытка 1: открыть и перезаписать напрямую с расширенными флагами общего доступа
    HANDLE hFile = CreateFileW(targetFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        // Убедимся, что процесс выгружен
        TerminateRunningEditor();
        Sleep(200);

        // В Windows заблокированный DLL/EXE нельзя перезаписать напрямую, но МОЖНО переименовать!
        // Переименовываем старый занятый файл во временное имя:
        for (int moveTry = 0; moveTry < 5; ++moveTry) {
            std::wstring oldPath = targetFilePath + L".old_" + std::to_wstring(GetTickCount()) + L"_" + std::to_wstring(moveTry);
            DeleteFileW(oldPath.c_str());
            if (MoveFileExW(targetFilePath.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                // Помечаем переименованный старый файл на удаление при следующей перезагрузке системы
                MoveFileExW(oldPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                break;
            }
            Sleep(150);
        }

        // Исходное имя файла освободилось! Создаем новый файл
        hFile = CreateFileW(targetFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    // Если всё ещё занят, делаем дополнительные попытки с паузой
    for (int retry = 0; retry < 5 && hFile == INVALID_HANDLE_VALUE; ++retry) {
        Sleep(250);
        hFile = CreateFileW(targetFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

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

// Очистка кэша MUI Windows Explorer от поврежденных/устаревших строк
static void PurgeMuiCacheKey(HKEY hRoot, const wchar_t* subKey) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey, 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        wchar_t valName[512];
        DWORD valLen = ARRAYSIZE(valName);
        DWORD index = 0;
        std::vector<std::wstring> toDelete;
        while (RegEnumValueW(hKey, index, valName, &valLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            if (wcsstr(valName, L"FileCommentEditor") != NULL || wcsstr(valName, L"FileCommentTip") != NULL) {
                toDelete.push_back(valName);
            }
            index++;
            valLen = ARRAYSIZE(valName);
        }
        for (const auto& v : toDelete) {
            RegDeleteValueW(hKey, v.c_str());
        }
        RegCloseKey(hKey);
    }
}

// Получение списка всех целевых разделов реестра (включая всех пользователей в HKEY_USERS для UAC)
static std::vector<HKEY> GetAllTargetClassRoots() {
    std::vector<HKEY> roots;
    roots.push_back(HKEY_CURRENT_USER);
    roots.push_back(HKEY_LOCAL_MACHINE);

    HKEY hUsers;
    if (RegOpenKeyExW(HKEY_USERS, NULL, 0, KEY_READ, &hUsers) == ERROR_SUCCESS) {
        wchar_t subName[256];
        DWORD subLen = ARRAYSIZE(subName);
        DWORD index = 0;
        while (RegEnumKeyExW(hUsers, index, subName, &subLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            if (wcsncmp(subName, L"S-1-5-21-", 9) == 0 && wcsstr(subName, L"_Classes") == NULL) {
                HKEY hUser;
                if (RegOpenKeyExW(HKEY_USERS, subName, 0, KEY_READ | KEY_WRITE, &hUser) == ERROR_SUCCESS) {
                    roots.push_back(hUser);
                }
            }
            index++;
            subLen = ARRAYSIZE(subName);
        }
        RegCloseKey(hUsers);
    }
    return roots;
}

// Прямая синхронная регистрация пунктов контекстного меню без использования MUIVerb (чистый UTF-16 Default)
static void PurgeAndRegisterContextMenuDirect(const std::wstring& editorExe, SetupLang lang) {
    std::wstring verbComment = (lang == SETUP_LANG_RU) ? L"Комментировать..." : L"Comment...";
    std::wstring verbSearch  = (lang == SETUP_LANG_RU) ? L"Искать в заметках..." : L"Search in comments...";

    std::wstring cmdComment = L"\"" + editorExe + L"\" \"%1\"";
    std::wstring cmdSearch  = L"\"" + editorExe + L"\" --search-gui \"%1\"";
    std::wstring cmdSearchBg = L"\"" + editorExe + L"\" --search-gui \"%V\"";

    std::vector<HKEY> hives = GetAllTargetClassRoots();

    for (HKEY root : hives) {
        PurgeMuiCacheKey(root, L"Software\\Classes\\Local Settings\\MuiCache");
        PurgeMuiCacheKey(root, L"Software\\Microsoft\\Windows\\Shell\\MuiCache");

        // Удаляем устаревшие ключи
        RegDeleteTreeW(root, L"Software\\Classes\\*\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\AllFilesystemObjects\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\lnkfile\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Folder\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Folder\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\Background\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\LibraryFolder\\Background\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Drive\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Drive\\shell\\FileCommentEditor");

        // Записываем чистые Unicode-значения (Default) и Icon
        const wchar_t* commentTargets[] = { L"*", L"AllFilesystemObjects", L"lnkfile", L"Folder", L"Directory", L"Drive" };
        for (const auto& t : commentTargets) {
            std::wstring basePath = L"Software\\Classes\\" + std::wstring(t) + L"\\shell\\FileCommentEditor";
            RegSetString(root, basePath, NULL, verbComment);
            RegSetString(root, basePath, L"Icon", editorExe);
            RegSetString(root, basePath + L"\\command", NULL, cmdComment);
        }

        const wchar_t* searchTargets[] = { L"Directory", L"Folder", L"Drive" };
        for (const auto& t : searchTargets) {
            std::wstring basePath = L"Software\\Classes\\" + std::wstring(t) + L"\\shell\\FileCommentSearch";
            RegSetString(root, basePath, NULL, verbSearch);
            RegSetString(root, basePath, L"Icon", editorExe);
            RegSetString(root, basePath + L"\\command", NULL, cmdSearch);
        }

        const wchar_t* bgTargets[] = { L"Directory\\Background", L"LibraryFolder\\Background" };
        for (const auto& t : bgTargets) {
            std::wstring basePath = L"Software\\Classes\\" + std::wstring(t) + L"\\shell\\FileCommentSearch";
            RegSetString(root, basePath, NULL, verbSearch);
            RegSetString(root, basePath, L"Icon", editorExe);
            RegSetString(root, basePath + L"\\command", NULL, cmdSearchBg);
        }

        if (root != HKEY_CURRENT_USER && root != HKEY_LOCAL_MACHINE) {
            RegCloseKey(root);
        }
    }

    // Запись в HKEY_CLASSES_ROOT напрямую
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"*\\shell\\FileCommentEditor");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shell\\FileCommentEditor");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"lnkfile\\shell\\FileCommentEditor");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Folder\\shell\\FileCommentEditor");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Directory\\shell\\FileCommentEditor");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Directory\\shell\\FileCommentSearch");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Folder\\shell\\FileCommentSearch");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\FileCommentSearch");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"LibraryFolder\\Background\\shell\\FileCommentSearch");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Drive\\shell\\FileCommentSearch");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"Drive\\shell\\FileCommentEditor");

    const wchar_t* commentTargetsCR[] = { L"*", L"AllFilesystemObjects", L"lnkfile", L"Folder", L"Directory", L"Drive" };
    for (const auto& t : commentTargetsCR) {
        std::wstring basePath = std::wstring(t) + L"\\shell\\FileCommentEditor";
        RegSetString(HKEY_CLASSES_ROOT, basePath, NULL, verbComment);
        RegSetString(HKEY_CLASSES_ROOT, basePath, L"Icon", editorExe);
        RegSetString(HKEY_CLASSES_ROOT, basePath + L"\\command", NULL, cmdComment);
    }

    const wchar_t* searchTargetsCR[] = { L"Directory", L"Folder", L"Drive" };
    for (const auto& t : searchTargetsCR) {
        std::wstring basePath = std::wstring(t) + L"\\shell\\FileCommentSearch";
        RegSetString(HKEY_CLASSES_ROOT, basePath, NULL, verbSearch);
        RegSetString(HKEY_CLASSES_ROOT, basePath, L"Icon", editorExe);
        RegSetString(HKEY_CLASSES_ROOT, basePath + L"\\command", NULL, cmdSearch);
    }

    const wchar_t* bgTargetsCR[] = { L"Directory\\Background", L"LibraryFolder\\Background" };
    for (const auto& t : bgTargetsCR) {
        std::wstring basePath = std::wstring(t) + L"\\shell\\FileCommentSearch";
        RegSetString(HKEY_CLASSES_ROOT, basePath, NULL, verbSearch);
        RegSetString(HKEY_CLASSES_ROOT, basePath, L"Icon", editorExe);
        RegSetString(HKEY_CLASSES_ROOT, basePath + L"\\command", NULL, cmdSearchBg);
    }
}

// Синхронная прямая регистрация Shell Extension DLL (FileCommentTip.dll)
static bool RegisterShellExtensionDirect(const std::wstring& tipDll) {
    HMODULE hTip = LoadLibraryExW(tipDll.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (hTip) {
        typedef HRESULT (STDAPICALLTYPE *PFNDLLREG)();
        PFNDLLREG pfn = (PFNDLLREG)GetProcAddress(hTip, "DllRegisterServer");
        if (pfn) {
            HRESULT hr = pfn();
            FreeLibrary(hTip);
            if (SUCCEEDED(hr)) return true;
        } else {
            FreeLibrary(hTip);
        }
    }

    // Fallback: синхронный запуск regsvr32 с ожиданием
    std::wstring regCmd = L"/s \"" + tipDll + L"\"";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = L"regsvr32.exe";
    sei.lpParameters = regCmd.c_str();
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei) && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 5000);
        CloseHandle(sei.hProcess);
        return true;
    }
    return false;
}

// Вспомогательное чтение комментария из файла или папки для установщика
static bool ReadExistingComment(const std::wstring& targetPath, std::wstring& outComment) {
    outComment.clear();
    std::wstring norm = targetPath;
    while (norm.length() > 3 && (norm.back() == L'\\' || norm.back() == L'/')) norm.pop_back();

    std::wstring adsPath = norm + L":comment";
    HANDLE hFile = CreateFileW(adsPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwSize = GetFileSize(hFile, NULL);
        if (dwSize > 0 && dwSize < 65536) {
            std::vector<BYTE> buf(dwSize);
            DWORD dwRead = 0;
            if (ReadFile(hFile, buf.data(), dwSize, &dwRead, NULL) && dwRead > 0) {
                if (dwRead >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
                    outComment.assign((wchar_t*)(buf.data() + 2), (dwRead - 2) / sizeof(wchar_t));
                } else if (dwRead >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
                    int wLen = MultiByteToWideChar(CP_UTF8, 0, (char*)(buf.data() + 3), dwRead - 3, NULL, 0);
                    if (wLen > 0) {
                        outComment.resize(wLen);
                        MultiByteToWideChar(CP_UTF8, 0, (char*)(buf.data() + 3), dwRead - 3, &outComment[0], wLen);
                    }
                } else {
                    int wLen = MultiByteToWideChar(CP_UTF8, 0, (char*)buf.data(), dwRead, NULL, 0);
                    if (wLen > 0) {
                        outComment.resize(wLen);
                        MultiByteToWideChar(CP_UTF8, 0, (char*)buf.data(), dwRead, &outComment[0], wLen);
                    }
                }
            }
        }
        CloseHandle(hFile);
    }

    if (!outComment.empty()) return true;

    DWORD dwAttr = GetFileAttributesW(norm.c_str());
    if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring ini = norm + L"\\desktop.ini";
        wchar_t buf[1024] = { 0 };
        if (GetPrivateProfileStringW(L".ShellClassInfo", L"InfoTip", L"", buf, ARRAYSIZE(buf), ini.c_str()) > 0) {
            outComment = buf;
            return true;
        }
        if (GetPrivateProfileStringW(L"SummaryInformation", L"Comment", L"", buf, ARRAYSIZE(buf), ini.c_str()) > 0) {
            outComment = buf;
            return true;
        }
    }
    return false;
}

// Синхронизация ярлыков в папке Recent
static void SyncRecentDirectoryShortcuts(const std::wstring& recentDir) {
    std::wstring pattern = recentDir + L"\\*.lnk";
    WIN32_FIND_DATAW wfd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &wfd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    IShellLinkW* psl = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
        IPersistFile* ppf = NULL;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            do {
                if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::wstring lnkFile = recentDir + L"\\" + wfd.cFileName;
                if (SUCCEEDED(ppf->Load(lnkFile.c_str(), STGM_READWRITE))) {
                    wchar_t szTarget[MAX_PATH] = { 0 };
                    WIN32_FIND_DATAW fd = { 0 };
                    if (SUCCEEDED(psl->GetPath(szTarget, ARRAYSIZE(szTarget), &fd, SLGP_RAWPATH)) && wcslen(szTarget) > 0) {
                        std::wstring comment;
                        if (ReadExistingComment(szTarget, comment) && !comment.empty()) {
                            std::wstring desc = L"📝 Заметка: " + comment;
                            psl->SetDescription(desc.c_str());
                            ppf->Save(lnkFile.c_str(), TRUE);
                            SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, lnkFile.c_str(), NULL);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &wfd));
            ppf->Release();
        }
        psl->Release();
    }
    FindClose(hFind);
}

// Полная синхронизация ярлыков Recent для всех пользователей
static void SyncAllExistingShortcutsAndRecent() {
    wchar_t szRecent[MAX_PATH] = { 0 };
    if (SHGetSpecialFolderPathW(NULL, szRecent, CSIDL_RECENT, FALSE)) {
        SyncRecentDirectoryShortcuts(szRecent);
    }

    wchar_t szSysDrive[MAX_PATH] = L"C:";
    GetEnvironmentVariableW(L"SystemDrive", szSysDrive, ARRAYSIZE(szSysDrive));
    std::wstring usersPattern = std::wstring(szSysDrive) + L"\\Users\\*";
    WIN32_FIND_DATAW ufd;
    HANDLE hUsers = FindFirstFileW(usersPattern.c_str(), &ufd);
    if (hUsers != INVALID_HANDLE_VALUE) {
        do {
            if ((ufd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
                wcscmp(ufd.cFileName, L".") != 0 && wcscmp(ufd.cFileName, L"..") != 0 &&
                _wcsicmp(ufd.cFileName, L"Public") != 0 && _wcsicmp(ufd.cFileName, L"Default") != 0 && 
                _wcsicmp(ufd.cFileName, L"Default User") != 0 && _wcsicmp(ufd.cFileName, L"All Users") != 0) {
                std::wstring uRecent = std::wstring(szSysDrive) + L"\\Users\\" + ufd.cFileName + L"\\AppData\\Roaming\\Microsoft\\Windows\\Recent";
                if (PathFileExistsW(uRecent.c_str())) {
                    SyncRecentDirectoryShortcuts(uRecent);
                }
            }
        } while (FindNextFileW(hUsers, &ufd));
        FindClose(hUsers);
    }
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

    HWND hInstallBtn = GetDlgItem(hWnd, IDC_BTN_INSTALL);
    if (hInstallBtn && !g_bInstalledSuccessfully) {
        #ifndef BCM_SETSHIELD
        #define BCM_SETSHIELD 0x0000160C
        #endif
        SendMessageW(hInstallBtn, BCM_SETSHIELD, 0, !IsProcessElevated());
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

    // 0. Обязательная проверка прав администратора для записи в Program Files
    if (!IsProcessElevated()) {
        std::wstring msg = (g_lang == SETUP_LANG_RU) ?
            L"Для установки FileCommenter в папку «" + installDir + L"» требуются права администратора.\n\n"
            L"Перезапустить установщик от имени администратора прямо сейчас?" :
            L"Administrator privileges are required to install FileCommenter into '" + installDir + L"'.\n\n"
            L"Relaunch installer with administrator privileges now?";

        int ans = MessageBoxW(hWnd, msg.c_str(), (g_lang == SETUP_LANG_RU) ? L"Требуются права администратора" : L"Administrator Privileges Required", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1);
        if (ans == IDYES) {
            if (RelaunchElevated(hWnd)) {
                PostQuitMessage(0);
                return;
            }
        }
        std::wstring cancelMsg = (g_lang == SETUP_LANG_RU) ?
            L"Установка отменена. Запись в системную папку «Program Files» заблокирована Windows без прав администратора." :
            L"Setup cancelled. Writing to 'Program Files' is blocked by Windows without administrator privileges.";
        MessageBoxW(hWnd, cancelMsg.c_str(), L"FileCommenter", MB_ICONWARNING);
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
            L"Не удалось создать папку:\n" + installDir + L"\n\nОтказано в доступе. Запустите установку от имени администратора." :
            L"Could not create directory:\n" + installDir + L"\n\nAccess denied. Please run setup as administrator.";
        MessageBoxW(hWnd, errMsg.c_str(), L"Error", MB_ICONERROR);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_EDT_PATH), TRUE);
        return;
    }

    SendMessageW(hProgress, PBM_SETPOS, 10, 0);
    SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Выгрузка запущенных процессов FileCommenter..." : L"Stopping running FileCommenter processes...");
    UpdateWindow(hWnd);

    // Принудительно выгружаем все запущенные экземпляры приложения
    TerminateRunningEditor();

    // Если в папке уже есть FileCommentTip.dll, временно отзываем регистрацию
    std::wstring oldTipDll = installDir + L"\\FileCommentTip.dll";
    if (PathFileExistsW(oldTipDll.c_str())) {
        std::wstring unregCmd = L"/u /s \"" + oldTipDll + L"\"";
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpFile = L"regsvr32.exe";
        sei.lpParameters = unregCmd.c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 1500);
            CloseHandle(sei.hProcess);
        }
    }
    Sleep(200);

    SendMessageW(hProgress, PBM_SETPOS, 25, 0);
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
        std::wstring err;
        if (!IsProcessElevated()) {
            err = (g_lang == SETUP_LANG_RU) ?
                L"Ошибка записи файлов программы в «" + installDir + L"»!\n\n"
                L"Причина: Отсутствуют права администратора. Windows заблокировала запись в системную папку Program Files.\n\n"
                L"Решение: Запустите установочный файл правой кнопкой мыши -> «Запуск от имени администратора»." :
                L"Error writing application files to '" + installDir + L"'!\n\n"
                L"Reason: Missing administrator privileges. Windows blocked write access to Program Files.\n\n"
                L"Solution: Run installer via right click -> 'Run as administrator'.";
        } else {
            err = (g_lang == SETUP_LANG_RU) ?
                L"Ошибка при записи файлов программы. Убедитесь, что FileCommenter не заблокирован антивирусом или другими процессами." :
                L"Error writing application files. Make sure FileCommenter is not locked by antivirus or another process.";
        }
        MessageBoxW(hWnd, err.c_str(), L"Error", MB_ICONERROR);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_EDT_PATH), TRUE);
        return;
    }

    // 2. Синхронная прямая регистрация Shell Extension DLL (FileCommentTip.dll)
    if (IsDlgButtonChecked(hWnd, IDC_CHK_SHELL_TIP) == BST_CHECKED) {
        SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Регистрация подсказок в Проводнике..." : L"Registering Explorer infotip shell extension...");
        UpdateWindow(hWnd);

        RegisterShellExtensionDirect(tipDll);
    }
    SendMessageW(hProgress, PBM_SETPOS, 70, 0);

    // 3. Синхронная прямая настройка контекстного меню (чистый UTF-16, без MUIVerb и кракозябр)
    if (IsDlgButtonChecked(hWnd, IDC_CHK_CTX_MENU) == BST_CHECKED) {
        SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Настройка контекстного меню Проводника..." : L"Configuring Explorer context menu...");
        UpdateWindow(hWnd);

        PurgeAndRegisterContextMenuDirect(editorExe, g_lang);
    }
    SendMessageW(hProgress, PBM_SETPOS, 80, 0);

    // 3.1 Синхронизация ярлыков в «Недавние» и «Главная» для отображения заметок
    SetDlgItemTextW(hWnd, IDC_ST_STATUS, (g_lang == SETUP_LANG_RU) ? L"Синхронизация подсказок в «Недавние» и «Главная»..." : L"Synchronizing tooltips in Recent & Home...");
    UpdateWindow(hWnd);
    SyncAllExistingShortcutsAndRecent();
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    g_hInstance = hInstance;
    g_lang = IsSystemRussian() ? SETUP_LANG_RU : SETUP_LANG_EN;

    // Автоматическая проверка и повышение прав администратора при запуске
    if (!IsProcessElevated()) {
        // Запрашиваем повышение прав через системный диалог UAC
        if (RelaunchElevated(NULL, lpCmdLine)) {
            return 0; // Завершаем текущий непривилегированный процесс
        }

        // Если пользователь отказался в окне UAC
        int choice = MessageBoxW(
            NULL,
            (g_lang == SETUP_LANG_RU)
                ? L"Программе установки требуются права администратора для записи в системную папку «C:\\Program Files» и интеграции в Проводник Windows.\n\nЗапустить установку от имени администратора?"
                : L"Setup requires administrator privileges to write into 'C:\\Program Files' and configure Windows Explorer.\n\nRun setup with administrator privileges?",
            (g_lang == SETUP_LANG_RU) ? L"Требуются права администратора" : L"Administrator Privileges Required",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1
        );

        if (choice == IDYES) {
            if (RelaunchElevated(NULL, lpCmdLine)) {
                return 0;
            }
        }
    }

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
