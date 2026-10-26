// ============================================================================
// FileCommentEditor.cpp — GUI и CLI редактор заметок к файлам для Проводника
// 
// Версия 1.2:
//   1. Исправлено отображение названий столбцов в окне поиска (устранен висячий указатель).
//   2. Исправлен поиск без учета регистра для кириллицы (переход с std::towlower на Win32 CharLowerBuffW).
//   3. Свободное перемещение основного окна и окна поиска мышью за любые свободные места (WM_NCHITTEST / HTCAPTION).
//   4. Встроена иконка приложения (ID 1) для окна, панели задач, системного трея и .exe файла.
//   5. Поддержка многоязычного MUIVerb в контекстном меню Проводника:
//      - Привязаны системные строковые ресурсы (@FileCommentEditor.exe,-101, -102).
//      - Пункты меню автоматически отображаются на языке текущего пользователя системы (RU/EN).
//   6. Контекстное меню во всех сценариях: файлы (*), папки (Folder, Directory), фон (Background), диски (Drive).
//   7. Настраиваемые глобальные горячие клавиши (RegisterHotKey) и фоновый режим (--tray).
//   8. Автозагрузка вместе с Windows (HKCU\...\Run).
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
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

// ----------------------------------------------------------------------------
// Идентификаторы ресурсов и элементов управления
// ----------------------------------------------------------------------------
#define IDI_APPICON               1

#define IDC_EDT_FILEPATH          1001
#define IDC_BTN_BROWSE            1002
#define IDC_EDT_COMMENT           1003
#define IDC_BTN_SAVE              1004
#define IDC_BTN_DELETE            1005
#define IDC_BTN_INSTALL_CTX       1006
#define IDC_BTN_UNINSTALL_CTX     1007
#define IDC_BTN_REGISTER_DLL      1008
#define IDC_BTN_UNREG_DLL         1009
#define IDC_ST_STATUS             1010
#define IDC_ST_FILEINFO           1011
#define IDC_BTN_CLOSE             1012
#define IDC_BTN_OPEN_SEARCH       1013
#define IDC_BTN_LANG_TOGGLE       1014
#define IDC_CHK_AUTOSTART         1015
#define IDC_CHK_HOTKEY_WIN        1016
#define IDC_CHK_HOTKEY_CTRL       1017
#define IDC_CHK_HOTKEY_ALT        1018
#define IDC_CHK_HOTKEY_SHIFT      1019
#define IDC_CMB_HOTKEY_KEY        1020
#define IDC_CMB_HOTKEY_ACTION     1021
#define IDC_BTN_APPLY_HOTKEY      1022
#define IDC_ST_HOTKEY_STATUS      1023
#define IDC_BTN_MINIMIZE_TRAY     1024
#define IDC_GRP_HOTKEY            1025
#define IDC_GRP_EXPLORER          1026
#define IDC_LBL_TARGETFILE        1027
#define IDC_LBL_TOOLTIP           1028
#define IDC_LBL_MODIFIERS         1029
#define IDC_LBL_KEY               1030
#define IDC_CHK_ENABLE_TRAY       1031
#define IDC_BTN_BACKUP_MENU       1032

// Идентификаторы элементов окна поиска
#define IDC_SRCH_LBL_QUERY        2000
#define IDC_SRCH_QUERY            2001
#define IDC_SRCH_LBL_DIR          2002
#define IDC_SRCH_DIR              2003
#define IDC_SRCH_BTN_BROWSE_DIR   2004
#define IDC_SRCH_CHK_RECURSIVE    2005
#define IDC_SRCH_CHK_MATCHCASE    2006
#define IDC_SRCH_CHK_EXACTPHRASE  2007
#define IDC_SRCH_BTN_START        2008
#define IDC_SRCH_BTN_STOP         2009
#define IDC_SRCH_LISTVIEW         2010
#define IDC_SRCH_ST_STATUS        2011
#define IDC_SRCH_BTN_OPEN_FILE    2012
#define IDC_SRCH_BTN_EXPLORER     2013

// Системный трей и горячие клавиши
#define ID_HOTKEY_GLOBAL          9001
#define WM_TRAYICON               (WM_USER + 201)
#define ID_TRAY_ICON              101
#define ID_TRAY_OPEN_EDITOR       3001
#define ID_TRAY_OPEN_SEARCH       3002
#define ID_TRAY_TOGGLE_LANG       3003
#define ID_TRAY_EXIT              3004

// Меню резервного копирования (JSON)
#define ID_BACKUP_EXPORT          4001
#define ID_BACKUP_IMPORT          4002

// Пользовательские сообщения поиска
#define WM_SEARCH_RESULT_FOUND    (WM_USER + 101)
#define WM_SEARCH_PROGRESS        (WM_USER + 102)
#define WM_SEARCH_FINISHED        (WM_USER + 103)

// ----------------------------------------------------------------------------
// Мультиязычность (RU / EN)
// ----------------------------------------------------------------------------
enum AppLang {
    LANG_RU = 0,
    LANG_EN = 1
};

enum StringKey {
    STR_APP_TITLE,
    STR_SEARCH_TITLE,
    STR_TARGET_FILE_LABEL,
    STR_BROWSE,
    STR_NO_FILE_SELECTED,
    STR_FILE_NOT_EXISTS,
    STR_FILE_INFO_FORMAT,
    STR_COMMENT_LABEL,
    STR_BTN_SAVE,
    STR_BTN_DELETE,
    STR_BTN_SEARCH,
    STR_BTN_MINIMIZE_TRAY,
    STR_STATUS_READY,
    STR_STATUS_SAVED,
    STR_STATUS_DELETED,
    STR_STATUS_LOADED,
    STR_GRP_HOTKEY_AUTOSTART,
    STR_CHK_AUTOSTART,
    STR_LBL_MODIFIERS,
    STR_LBL_KEY,
    STR_LBL_ACTION,
    STR_ACT_OPEN_SEARCH,
    STR_ACT_OPEN_EDITOR,
    STR_BTN_APPLY_HOTKEY,
    STR_HOTKEY_STATUS_OK,
    STR_HOTKEY_STATUS_FAIL,
    STR_GRP_EXPLORER,
    STR_BTN_INSTALL_CTX,
    STR_BTN_UNINSTALL_CTX,
    STR_BTN_REG_DLL,
    STR_BTN_UNREG_DLL,
    STR_MSG_CTX_INSTALLED,
    STR_MSG_CTX_REMOVED,
    STR_MSG_DLL_REGISTERED,
    STR_MSG_DLL_UNREGISTERED,
    STR_MSG_SAVE_CONFIRM,
    STR_MSG_SAVE_ERROR,
    STR_MSG_DELETE_CONFIRM,
    STR_MSG_SELECT_FILE_FIRST,
    STR_TRAY_TIP,
    STR_TRAY_BALLOON_TITLE,
    STR_TRAY_BALLOON_TEXT,
    STR_MENU_OPEN_EDITOR,
    STR_MENU_OPEN_SEARCH,
    STR_MENU_SWITCH_LANG,
    STR_MENU_EXIT,
    STR_SRCH_QUERY_LBL,
    STR_SRCH_BTN_START,
    STR_SRCH_BTN_STOP,
    STR_SRCH_WHERE_LBL,
    STR_SRCH_CHK_RECURSIVE,
    STR_SRCH_CHK_MATCHCASE,
    STR_SRCH_CHK_EXACTPHRASE,
    STR_SRCH_COL_FILENAME,
    STR_SRCH_COL_SNIPPET,
    STR_SRCH_COL_FOLDER,
    STR_SRCH_STATUS_PROMPT,
    STR_SRCH_STATUS_RUNNING,
    STR_SRCH_STATUS_STOPPED,
    STR_SRCH_STATUS_DONE,
    STR_SRCH_STATUS_PROGRESS,
    STR_SRCH_BTN_EDIT,
    STR_SRCH_BTN_EXPLORER,
    STR_SRCH_BROWSE_TITLE,
    STR_CTX_COMMENT_VERB,
    STR_CTX_SEARCH_VERB,
    STR_LANG_TOGGLE_BTN,
    STR_CHK_ENABLE_TRAY,
    STR_BTN_BACKUP,
    STR_MENU_BACKUP_EXPORT,
    STR_MENU_BACKUP_IMPORT,
    STR_BACKUP_EXPORT_TITLE,
    STR_BACKUP_IMPORT_TITLE,
    STR_BACKUP_EXPORT_SUCCESS,
    STR_BACKUP_IMPORT_SUCCESS,
    STR_BACKUP_IMPORT_CONFIRM,
    STR_KEY_COUNT
};

static const wchar_t* g_dict[STR_KEY_COUNT][2] = {
    // STR_APP_TITLE
    { L"Менеджер заметок к файлам для Проводника Windows", L"File Comment Manager for Windows Explorer" },
    // STR_SEARCH_TITLE
    { L"Поиск файлов по содержимому заметок (NTFS Alternate Streams)", L"Search Files by Comments (NTFS Alternate Streams)" },
    // STR_TARGET_FILE_LABEL
    { L"Целевой файл или папка:", L"Target file or folder:" },
    // STR_BROWSE
    { L"Обзор...", L"Browse..." },
    // STR_NO_FILE_SELECTED
    { L"Файл не выбран", L"No file selected" },
    // STR_FILE_NOT_EXISTS
    { L"Файл не существует или недоступен", L"File does not exist or inaccessible" },
    // STR_FILE_INFO_FORMAT
    { L"Размер: %.1f КБ | Заметка в NTFS: %s", L"Size: %.1f KB | NTFS Comment: %s" },
    // STR_COMMENT_LABEL
    { L"Текст всплывающего комментария (тултипа в Проводнике):", L"Popup tooltip comment text (in Explorer):" },
    // STR_BTN_SAVE
    { L"💾 Сохранить", L"💾 Save" },
    // STR_BTN_DELETE
    { L"🗑 Удалить", L"🗑 Delete" },
    // STR_BTN_SEARCH
    { L"🔍 Поиск...", L"🔍 Search..." },
    // STR_BTN_MINIMIZE_TRAY
    { L"📥 В трей", L"📥 To Tray" },
    // STR_STATUS_READY
    { L"Готов к работе", L"Ready" },
    // STR_STATUS_SAVED
    { L"Заметка сохранена!", L"Comment saved!" },
    // STR_STATUS_DELETED
    { L"Заметка удалена", L"Comment removed" },
    // STR_STATUS_LOADED
    { L"Заметка загружена из NTFS :comment", L"Comment loaded from NTFS :comment" },
    // STR_GRP_HOTKEY_AUTOSTART
    { L" Быстрый вызов и автозагрузка ", L" Global Hotkey & Windows Autostart " },
    // STR_CHK_AUTOSTART
    { L"Запускать вместе с Windows", L"Start with Windows" },
    // STR_LBL_MODIFIERS
    { L"Клавиши:", L"Keys:" },
    // STR_LBL_KEY
    { L"Символ:", L"Key:" },
    // STR_LBL_ACTION
    { L"Действие:", L"Action:" },
    // STR_ACT_OPEN_SEARCH
    { L"Открыть окно поиска", L"Open Search Window" },
    // STR_ACT_OPEN_EDITOR
    { L"Открыть окно редактора", L"Open Editor Window" },
    // STR_BTN_APPLY_HOTKEY
    { L"Задать хоткей", L"Apply Hotkey" },
    // STR_HOTKEY_STATUS_OK
    { L"Хоткей активен: %s", L"Hotkey active: %s" },
    // STR_HOTKEY_STATUS_FAIL
    { L"Ошибка регистрации хоткея!", L"Failed to register hotkey!" },
    // STR_GRP_EXPLORER
    { L" Интеграция с Проводником Windows ", L" Windows Explorer Integration " },
    // STR_BTN_INSTALL_CTX
    { L"➕ Меню Проводника (файлы, папки, диск)", L"➕ Explorer Menu (files, folders, drives)" },
    // STR_BTN_UNINSTALL_CTX
    { L"➖ Удалить из меню Проводника", L"➖ Remove from Explorer Menu" },
    // STR_BTN_REG_DLL
    { L"⚙️ Зарегистрировать DLL подсказок", L"⚙️ Register Tooltip DLL" },
    // STR_BTN_UNREG_DLL
    { L"🔌 Отключить DLL подсказок", L"🔌 Unregister Tooltip DLL" },
    // STR_MSG_CTX_INSTALLED
    { L"Пункты «Комментировать...» и «Искать в заметках...» успешно зарегистрированы в Проводнике!\n"
      L"Они доступны при клике по файлам, папкам, пустому месту папки и дискам.",
      L"Context menu items 'Comment...' and 'Search in comments...' successfully added!\n"
      L"Available when right-clicking files, folders, folder background, and drive roots." },
    // STR_MSG_CTX_REMOVED
    { L"Пункты успешно удалены из контекстного меню Проводника.", L"Context menu items successfully removed." },
    // STR_MSG_DLL_REGISTERED
    { L"DLL подсказок (FileCommentTip.dll) успешно зарегистрирована в системе!\n"
      L"Наведите курсор на файл в Проводнике для проверки.",
      L"Tooltip DLL (FileCommentTip.dll) successfully registered!\n"
      L"Hover over a file in Explorer to view tooltip." },
    // STR_MSG_DLL_UNREGISTERED
    { L"Регистрация DLL снята. Проводник вернется к стандартным подсказкам.",
      L"DLL unregistered. Explorer will show default tooltips." },
    // STR_MSG_SAVE_CONFIRM
    { L"Заметка сохранена в скрытый поток NTFS :comment!", L"Comment saved into NTFS hidden :comment stream!" },
    // STR_MSG_SAVE_ERROR
    { L"Ошибка сохранения! Убедитесь, что диск NTFS и есть права на запись.",
      L"Save error! Ensure the drive is NTFS and you have write permissions." },
    // STR_MSG_DELETE_CONFIRM
    { L"Удалить пользовательскую заметку к этому файлу?", L"Remove user comment from this file?" },
    // STR_MSG_SELECT_FILE_FIRST
    { L"Сначала выберите файл через кнопку «Обзор...» или контекстное меню!",
      L"Please select a file first via 'Browse...' or context menu!" },
    // STR_TRAY_TIP
    { L"FileCommenter — Заметки к файлам", L"FileCommenter — File Comments" },
    // STR_TRAY_BALLOON_TITLE
    { L"FileCommenter активен", L"FileCommenter is active" },
    // STR_TRAY_BALLOON_TEXT
    { L"Приложение работает в фоновом режиме. Нажмите горячую клавишу для вызова.",
      L"App is running in background. Press hotkey to invoke." },
    // STR_MENU_OPEN_EDITOR
    { L"✏️ Открыть редактор заметок", L"✏️ Open Comment Editor" },
    // STR_MENU_OPEN_SEARCH
    { L"🔍 Поиск по заметкам...", L"🔍 Search in Comments..." },
    // STR_MENU_SWITCH_LANG
    { L"🌐 Switch to English", L"🌐 Переключить на русский" },
    // STR_MENU_EXIT
    { L"❌ Выход", L"❌ Exit" },
    // STR_SRCH_QUERY_LBL
    { L"Ключевые слова или фраза в заметках:", L"Keywords or phrase in comments:" },
    // STR_SRCH_BTN_START
    { L"🔍 Найти", L"🔍 Find" },
    // STR_SRCH_BTN_STOP
    { L"⏹ Стоп", L"⏹ Stop" },
    // STR_SRCH_WHERE_LBL
    { L"Где искать (папка или диск):", L"Search directory or drive:" },
    // STR_SRCH_CHK_RECURSIVE
    { L"Включая все подпапки (рекурсивно)", L"Include subfolders (recursive)" },
    // STR_SRCH_CHK_MATCHCASE
    { L"Учитывать регистр (Aa)", L"Match case (Aa)" },
    // STR_SRCH_CHK_EXACTPHRASE
    { L"Точная фраза целиком", L"Exact phrase match" },
    // STR_SRCH_COL_FILENAME
    { L"Имя файла", L"File Name" },
    // STR_SRCH_COL_SNIPPET
    { L"Фрагмент заметки NTFS", L"NTFS Comment Snippet" },
    // STR_SRCH_COL_FOLDER
    { L"Папка", L"Folder" },
    // STR_SRCH_STATUS_PROMPT
    { L"Введите поисковый запрос и нажмите «Найти»", L"Enter search query and click 'Find'" },
    // STR_SRCH_STATUS_RUNNING
    { L"Поиск запущен... Сканирование файловой системы...", L"Searching... Scanning file system..." },
    // STR_SRCH_STATUS_STOPPED
    { L"Поиск остановлен пользователем. Найдено: %u.", L"Search stopped by user. Found: %u." },
    // STR_SRCH_STATUS_DONE
    { L"Готово! Проверено %u файлов, найдено %u заметок.", L"Done! Scanned %u files, found %u comments." },
    // STR_SRCH_STATUS_PROGRESS
    { L"Проверено: %u файлов | Найдено: %u заметок", L"Scanned: %u files | Found: %u comments" },
    // STR_SRCH_BTN_EDIT
    { L"✏️ Редактировать", L"✏️ Edit Comment" },
    // STR_SRCH_BTN_EXPLORER
    { L"📂 В Проводнике", L"📂 Show in Explorer" },
    // STR_SRCH_BROWSE_TITLE
    { L"Выберите папку или диск для поиска заметок:", L"Select folder or drive to search comments:" },
    // STR_CTX_COMMENT_VERB
    { L"Комментировать...", L"Comment..." },
    // STR_CTX_SEARCH_VERB
    { L"Искать в заметках...", L"Search in comments..." },
    // STR_LANG_TOGGLE_BTN
    { L"🌐 English", L"🌐 Русский" },
    // STR_CHK_ENABLE_TRAY
    { L"Отображать значок в системном трее", L"Show icon in system tray" },
    // STR_BTN_BACKUP
    { L"💾 Резервная копия...", L"💾 Backup & Restore..." },
    // STR_MENU_BACKUP_EXPORT
    { L"📤 Экспорт всех заметок в JSON (Резервная копия)...", L"📤 Export all comments to JSON (Backup)..." },
    // STR_MENU_BACKUP_IMPORT
    { L"📥 Импорт заметок из JSON (Восстановление)...", L"📥 Import comments from JSON (Restore)..." },
    // STR_BACKUP_EXPORT_TITLE
    { L"Сохранить резервную копию заметок в JSON", L"Save comments backup to JSON" },
    // STR_BACKUP_IMPORT_TITLE
    { L"Выбрать файл резервной копии заметок (JSON)", L"Select comments backup file (JSON)" },
    // STR_BACKUP_EXPORT_SUCCESS
    { L"Резервная копия успешно создана!\nПроверено объектов: %u\nЭкспортировано заметок: %u\nФайл:\n%s",
      L"Backup successfully created!\nScanned objects: %u\nExported comments: %u\nFile:\n%s" },
    // STR_BACKUP_IMPORT_SUCCESS
    { L"Восстановление завершено!\nВсего записей в бекапе: %u\nУспешно восстановлено: %u\nПропущено/Ошибок: %u",
      L"Restore completed!\nTotal backup entries: %u\nSuccessfully restored: %u\nSkipped/Errors: %u" },
    // STR_BACKUP_IMPORT_CONFIRM
    { L"Восстановить заметки из файла:\n%s\n\nСуществующие заметки для совпадающих файлов будут перезаписаны. Продолжить?",
      L"Restore comments from file:\n%s\n\nExisting comments for matching files will be overwritten. Proceed?" }
};

static AppLang g_currentLang = LANG_RU;

static inline std::wstring GetStr(StringKey key) {
    int idx = (g_currentLang == LANG_EN) ? 1 : 0;
    return g_dict[key][idx];
}

static bool IsSystemRussian() {
    LANGID lid = GetUserDefaultUILanguage();
    WORD prim = PRIMARYLANGID(lid);
    return (prim == LANG_RUSSIAN || prim == LANG_BELARUSIAN || prim == LANG_UKRAINIAN);
}

// ----------------------------------------------------------------------------
// Глобальные переменные
// ----------------------------------------------------------------------------
static HINSTANCE g_hInstance = NULL;
static HWND g_hMainWnd = NULL;
static HWND g_hSearchWnd = NULL;
static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;
static HICON g_hAppIcon = NULL;
static HICON g_hAppIconSmall = NULL;
static std::wstring g_currentFilePath;

// Настройки хоткея
static UINT g_hotkeyMods = MOD_WIN | MOD_SHIFT;
static UINT g_hotkeyVK = 'C';
static DWORD g_hotkeyAction = 0; // 0 = поиск, 1 = редактор
static bool g_hotkeyRegistered = false;
static bool g_bEnableTrayIcon = true; // Отображать ли иконку в трее

// ----------------------------------------------------------------------------
// Структуры поиска
// ----------------------------------------------------------------------------
struct SearchResultItem {
    std::wstring filePath;
    std::wstring fileName;
    std::wstring folderPath;
    std::wstring commentText;
    std::wstring snippet;
    ULONGLONG fileSize;
};

struct SearchThreadParams {
    HWND hWndNotify;
    std::wstring searchDir;
    std::wstring query;
    bool recursive;
    bool matchCase;
    bool exactPhrase;
};

static std::atomic<bool> g_bCancelSearch(false);
static std::vector<SearchResultItem> g_searchResults;
static CRITICAL_SECTION g_resultsCS;
static bool g_csInitialized = false;

static void EnsureCS() {
    if (!g_csInitialized) {
        InitializeCriticalSection(&g_resultsCS);
        g_csInitialized = true;
    }
}

struct AutoLockCS {
    CRITICAL_SECTION* m_pCS;
    AutoLockCS(CRITICAL_SECTION* p) : m_pCS(p) { EnterCriticalSection(m_pCS); }
    ~AutoLockCS() { LeaveCriticalSection(m_pCS); }
};

// ----------------------------------------------------------------------------
// Помощники реестра
// ----------------------------------------------------------------------------
static bool RegSetStringValue(HKEY hRoot, const std::wstring& subkey, const wchar_t* valName, const std::wstring& valData) {
    HKEY hKey;
    if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD cbData = (DWORD)((valData.length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, valName, 0, REG_SZ, (const BYTE*)valData.c_str(), cbData);
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static bool RegSetDwordValue(HKEY hRoot, const std::wstring& subkey, const wchar_t* valName, DWORD valData) {
    HKEY hKey;
    if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, valName, 0, REG_DWORD, (const BYTE*)&valData, sizeof(DWORD));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static bool RegGetDwordValue(HKEY hRoot, const std::wstring& subkey, const wchar_t* valName, DWORD& outVal) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwType = 0;
        DWORD dwVal = 0;
        DWORD cbData = sizeof(dwVal);
        LONG lRes = RegQueryValueExW(hKey, valName, NULL, &dwType, (LPBYTE)&dwVal, &cbData);
        RegCloseKey(hKey);
        if (lRes == ERROR_SUCCESS && dwType == REG_DWORD) {
            outVal = dwVal;
            return true;
        }
    }
    return false;
}

static bool RegGetStringValue(HKEY hRoot, const std::wstring& subkey, const wchar_t* valName, std::wstring& outVal) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[512] = { 0 };
        DWORD dwType = 0;
        DWORD cbData = sizeof(buf);
        LONG lRes = RegQueryValueExW(hKey, valName, NULL, &dwType, (LPBYTE)buf, &cbData);
        RegCloseKey(hKey);
        if (lRes == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
            outVal = buf;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Автозагрузка с Windows
// ----------------------------------------------------------------------------
static bool IsAutostartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t szVal[MAX_PATH + 32] = { 0 };
        DWORD dwType = 0;
        DWORD cbData = sizeof(szVal);
        LONG lRes = RegQueryValueExW(hKey, L"FileCommenter", NULL, &dwType, (LPBYTE)szVal, &cbData);
        RegCloseKey(hKey);
        return (lRes == ERROR_SUCCESS && cbData > 0);
    }
    return false;
}

static bool SetAutostart(bool enable) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t szExe[MAX_PATH];
            GetModuleFileNameW(NULL, szExe, ARRAYSIZE(szExe));
            wchar_t szCmd[MAX_PATH + 32];
            StringCchPrintfW(szCmd, ARRAYSIZE(szCmd), L"\"%s\" --tray", szExe);
            DWORD cbData = (DWORD)((wcslen(szCmd) + 1) * sizeof(wchar_t));
            RegSetValueExW(hKey, L"FileCommenter", 0, REG_SZ, (const BYTE*)szCmd, cbData);
        } else {
            RegDeleteValueW(hKey, L"FileCommenter");
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// Форматирование описания горячей клавиши
// ----------------------------------------------------------------------------
static std::wstring FormatHotkeyString(UINT mods, UINT vk) {
    std::wstring res;
    if (mods & MOD_WIN) res += L"Win + ";
    if (mods & MOD_CONTROL) res += L"Ctrl + ";
    if (mods & MOD_ALT) res += L"Alt + ";
    if (mods & MOD_SHIFT) res += L"Shift + ";

    if (vk >= 'A' && vk <= 'Z') {
        res += (wchar_t)vk;
    } else if (vk >= '0' && vk <= '9') {
        res += (wchar_t)vk;
    } else if (vk >= VK_F1 && vk <= VK_F12) {
        res += L"F" + std::to_wstring(vk - VK_F1 + 1);
    } else if (vk == VK_SPACE) {
        res += L"Space";
    } else {
        wchar_t szKey[32];
        StringCchPrintfW(szKey, ARRAYSIZE(szKey), L"0x%02X", vk);
        res += szKey;
    }
    return res;
}

// Сохранение и загрузка настроек приложения
static void LoadAppSettings() {
    // 1. Язык: если сохранен в реестре — берем его, иначе определяем системный
    std::wstring langStr;
    if (RegGetStringValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"Language", langStr)) {
        if (langStr == L"en") g_currentLang = LANG_EN;
        else g_currentLang = LANG_RU;
    } else {
        g_currentLang = IsSystemRussian() ? LANG_RU : LANG_EN;
    }

    // 2. Хоткей
    DWORD dwMods = 0, dwVK = 0, dwAct = 0;
    if (RegGetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyModifiers", dwMods)) {
        g_hotkeyMods = dwMods;
    }
    if (RegGetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyVK", dwVK)) {
        g_hotkeyVK = dwVK;
    }
    if (RegGetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyAction", dwAct)) {
        g_hotkeyAction = dwAct;
    }

    // 3. Отображение иконки в системном трее
    DWORD dwTray = 1;
    if (RegGetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"EnableTrayIcon", dwTray)) {
        g_bEnableTrayIcon = (dwTray != 0);
    } else {
        g_bEnableTrayIcon = true;
    }
}

static void SaveAppSettings() {
    std::wstring langStr = (g_currentLang == LANG_EN) ? L"en" : L"ru";
    RegSetStringValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"Language", langStr);
    RegSetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyModifiers", g_hotkeyMods);
    RegSetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyVK", g_hotkeyVK);
    RegSetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"HotkeyAction", g_hotkeyAction);
    RegSetDwordValue(HKEY_CURRENT_USER, L"Software\\FileCommenter", L"EnableTrayIcon", g_bEnableTrayIcon ? 1 : 0);
}

// ----------------------------------------------------------------------------
// Работа с альтернативным потоком NTFS :comment (файлы и папки)
// ----------------------------------------------------------------------------
// Нормализация пути: удаление концевого слэша для папок (C:\Folder\ -> C:\Folder)
static std::wstring NormalizePathForADS(const std::wstring& path) {
    std::wstring p = path;
    while (p.length() > 3 && (p.back() == L'\\' || p.back() == L'/')) {
        p.pop_back();
    }
    return p;
}

static bool ReadComment(const std::wstring& filePath, std::wstring& outComment) {
    outComment.clear();
    std::wstring adsPath = NormalizePathForADS(filePath) + L":comment";

    HANDLE hFile = CreateFileW(
        adsPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, // Позволяет открывать потоки как файлов, так и папок!
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwSizeHigh = 0;
    DWORD dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
    if (dwSizeLow == 0 || dwSizeLow == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<BYTE> buffer(dwSizeLow + 4, 0);
    DWORD dwRead = 0;
    ReadFile(hFile, buffer.data(), dwSizeLow, &dwRead, NULL);
    CloseHandle(hFile);

    if (dwRead == 0) return false;

    // Чтение UTF-8 с BOM (0xEF, 0xBB, 0xBF)
    if (dwRead >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data() + 3, dwRead - 3, NULL, 0);
        if (wlen > 0) {
            outComment.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data() + 3, dwRead - 3, &outComment[0], wlen);
        }
    } else if (dwRead >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
        // UTF-16LE с BOM
        int chars = (dwRead - 2) / sizeof(wchar_t);
        outComment.assign((const wchar_t*)(buffer.data() + 2), chars);
    } else {
        // Пробуем как чистый UTF-8
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)buffer.data(), dwRead, NULL, 0);
        if (wlen > 0) {
            outComment.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data(), dwRead, &outComment[0], wlen);
        } else {
            // Фолбэк на ANSI / Windows-1251
            int wlenA = MultiByteToWideChar(CP_ACP, 0, (const char*)buffer.data(), dwRead, NULL, 0);
            if (wlenA > 0) {
                outComment.resize(wlenA);
                MultiByteToWideChar(CP_ACP, 0, (const char*)buffer.data(), dwRead, &outComment[0], wlenA);
            }
        }
    }
    return true;
}

static bool WriteComment(const std::wstring& filePath, const std::wstring& comment) {
    std::wstring adsPath = NormalizePathForADS(filePath) + L":comment";

    if (comment.empty()) {
        DeleteFileW(adsPath.c_str());
        return true;
    }

    HANDLE hFile = CreateFileW(
        adsPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_BACKUP_SEMANTICS, // Позволяет записывать поток в том числе для папок!
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Сохраняем в UTF-8 с BOM (0xEF, 0xBB, 0xBF)
    BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    DWORD dwWritten = 0;
    WriteFile(hFile, bom, sizeof(bom), &dwWritten, NULL);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, comment.c_str(), (int)comment.length(), NULL, 0, NULL, NULL);
    if (utf8Len > 0) {
        std::vector<char> utf8Buf(utf8Len);
        WideCharToMultiByte(CP_UTF8, 0, comment.c_str(), (int)comment.length(), utf8Buf.data(), utf8Len, NULL, NULL);
        WriteFile(hFile, utf8Buf.data(), utf8Len, &dwWritten, NULL);
    }

    CloseHandle(hFile);
    return true;
}

static bool DeleteComment(const std::wstring& filePath) {
    std::wstring adsPath = NormalizePathForADS(filePath) + L":comment";
    return DeleteFileW(adsPath.c_str()) != FALSE;
}

// ----------------------------------------------------------------------------
// Поисковые утилиты (с полной поддержкой кириллицы в CharLowerBuffW)
// ----------------------------------------------------------------------------
static std::wstring ToLowerString(const std::wstring& str) {
    if (str.empty()) return L"";
    std::wstring res = str;
    // Win32 CharLowerBuffW корректно переводит в нижний регистр все символы Unicode (кириллицу, латиницу и др.)
    CharLowerBuffW(&res[0], (DWORD)res.length());
    return res;
}

static bool MatchQuery(const std::wstring& comment, const std::wstring& query, bool matchCase, bool exactPhrase) {
    if (query.empty()) return true;

    if (exactPhrase) {
        if (matchCase) {
            return comment.find(query) != std::wstring::npos;
        } else {
            std::wstring cLower = ToLowerString(comment);
            std::wstring qLower = ToLowerString(query);
            return cLower.find(qLower) != std::wstring::npos;
        }
    } else {
        // Поиск всех слов (логическое AND) с учетом регистра или без
        std::wstring target = matchCase ? comment : ToLowerString(comment);
        std::wstring q = matchCase ? query : ToLowerString(query);
        std::wistringstream ss(q);
        std::wstring token;
        bool allFound = true;
        while (ss >> token) {
            if (target.find(token) == std::wstring::npos) {
                allFound = false;
                break;
            }
        }
        return allFound;
    }
}

static std::wstring ExtractSnippet(const std::wstring& comment, const std::wstring& query) {
    if (comment.empty()) return L"";
    std::wstring clean = comment;
    for (wchar_t& ch : clean) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    }
    while (clean.find(L"  ") != std::wstring::npos) {
        clean.replace(clean.find(L"  "), 2, L" ");
    }
    if (clean.length() > 120) {
        return clean.substr(0, 117) + L"...";
    }
    return clean;
}

// Рекурсивный поиск файлов с потоком :comment
static void SearchDirectoryRecursive(
    const std::wstring& directory,
    const SearchThreadParams& params,
    DWORD& scannedFiles,
    DWORD& foundFiles
) {
    if (g_bCancelSearch.load()) return;

    std::wstring searchPattern = directory;
    if (searchPattern.back() != L'\\') searchPattern += L'\\';
    searchPattern += L"*";

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subdirs;

    do {
        if (g_bCancelSearch.load()) break;

        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
            continue;
        }

        std::wstring fullPath = directory;
        if (fullPath.back() != L'\\') fullPath += L'\\';
        fullPath += ffd.cFileName;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Проверяем наличие комментария у самой найденной папки!
            std::wstring dirComment;
            if (ReadComment(fullPath, dirComment) && !dirComment.empty()) {
                if (MatchQuery(dirComment, params.query, params.matchCase, params.exactPhrase)) {
                    foundFiles++;
                    SearchResultItem item;
                    item.filePath = fullPath;
                    item.fileName = L"📁 " + std::wstring(ffd.cFileName);
                    item.folderPath = directory;
                    item.commentText = dirComment;
                    item.snippet = ExtractSnippet(dirComment, params.query);
                    item.fileSize = 0; // Для папок

                    {
                        EnsureCS();
                        AutoLockCS lock(&g_resultsCS);
                        g_searchResults.push_back(item);
                    }

                    PostMessageW(params.hWndNotify, WM_SEARCH_RESULT_FOUND, 0, (LPARAM)(g_searchResults.size() - 1));
                    PostMessageW(params.hWndNotify, WM_SEARCH_PROGRESS, scannedFiles, foundFiles);
                }
            }

            if (params.recursive) {
                // Пропускаем символические ссылки, точки монтирования и корзины
                if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                    _wcsicmp(ffd.cFileName, L"$Recycle.Bin") != 0 &&
                    _wcsicmp(ffd.cFileName, L"System Volume Information") != 0) {
                    subdirs.push_back(fullPath);
                }
            }
        } else {
            scannedFiles++;
            if ((scannedFiles % 50) == 0) {
                PostMessageW(params.hWndNotify, WM_SEARCH_PROGRESS, scannedFiles, foundFiles);
            }

            std::wstring comment;
            if (ReadComment(fullPath, comment) && !comment.empty()) {
                if (MatchQuery(comment, params.query, params.matchCase, params.exactPhrase)) {
                    foundFiles++;
                    SearchResultItem item;
                    item.filePath = fullPath;
                    item.fileName = ffd.cFileName;
                    item.folderPath = directory;
                    item.commentText = comment;
                    item.snippet = ExtractSnippet(comment, params.query);
                    item.fileSize = ((ULONGLONG)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow;

                    {
                        EnsureCS();
                        AutoLockCS lock(&g_resultsCS);
                        g_searchResults.push_back(item);
                    }

                    PostMessageW(params.hWndNotify, WM_SEARCH_RESULT_FOUND, 0, (LPARAM)(g_searchResults.size() - 1));
                    PostMessageW(params.hWndNotify, WM_SEARCH_PROGRESS, scannedFiles, foundFiles);
                }
            }
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);

    if (params.recursive && !g_bCancelSearch.load()) {
        for (const auto& sub : subdirs) {
            if (g_bCancelSearch.load()) break;
            SearchDirectoryRecursive(sub, params, scannedFiles, foundFiles);
        }
    }
}

static DWORD WINAPI SearchWorkerThread(LPVOID lpParam) {
    SearchThreadParams* pParams = (SearchThreadParams*)lpParam;
    DWORD scanned = 0;
    DWORD found = 0;

    SearchDirectoryRecursive(pParams->searchDir, *pParams, scanned, found);

    PostMessageW(pParams->hWndNotify, WM_SEARCH_PROGRESS, scanned, found);
    PostMessageW(pParams->hWndNotify, WM_SEARCH_FINISHED, scanned, found);

    delete pParams;
    return 0;
}

// ----------------------------------------------------------------------------
// Диалог выбора папки
// ----------------------------------------------------------------------------
static bool BrowseForFolder(HWND hWndParent, std::wstring& outFolder) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWndParent;
    std::wstring title = GetStr(STR_SRCH_BROWSE_TITLE);
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

// ----------------------------------------------------------------------------
// Интеграция с контекстным меню Проводника (Right-Click Context Menu)
// ----------------------------------------------------------------------------
// Поддерживает многоязычный MUIVerb со ссылкой на строковые ресурсы EXE файла (@FileCommentEditor.exe,-101, -102).
// Windows Explorer автоматически отображает пункты на языке Windows текущего пользователя!
// Также задает значение по умолчанию (Default) для программ без поддержки MUIVerb.
static bool RegisterContextVerbInHive(
    HKEY hRoot,
    const std::wstring& targetClass,
    const std::wstring& verbKey,
    const std::wstring& displayText,
    int muiStringResId,
    const std::wstring& exePath,
    const std::wstring& commandParam
) {
    std::wstring basePath = L"Software\\Classes\\" + targetClass + L"\\shell\\" + verbKey;
    std::wstring cmdPath = basePath + L"\\command";

    // 1. Отображаемое имя по умолчанию
    RegSetStringValue(hRoot, basePath, NULL, displayText);

    // 2. Многоязычный MUIVerb: Windows Explorer загружает текст на языке интерфейса пользователя
    wchar_t szMuiVerb[MAX_PATH + 32];
    StringCchPrintfW(szMuiVerb, ARRAYSIZE(szMuiVerb), L"@\"%s\",-%d", exePath.c_str(), muiStringResId);
    RegSetStringValue(hRoot, basePath, L"MUIVerb", szMuiVerb);

    // 3. Иконка из EXE файла
    RegSetStringValue(hRoot, basePath, L"Icon", exePath);

    // 4. Командная строка вызова
    wchar_t szFullCmd[MAX_PATH + 64];
    StringCchPrintfW(szFullCmd, ARRAYSIZE(szFullCmd), L"\"%s\" %s", exePath.c_str(), commandParam.c_str());
    RegSetStringValue(hRoot, cmdPath, NULL, szFullCmd);

    return true;
}

static bool InstallContextMenu() {
    wchar_t szExePath[MAX_PATH];
    GetModuleFileNameW(NULL, szExePath, ARRAYSIZE(szExePath));

    // Выбираем строку по умолчанию с учетом текущего языка
    std::wstring verbComment = GetStr(STR_CTX_COMMENT_VERB);
    std::wstring verbSearch = GetStr(STR_CTX_SEARCH_VERB);

    HKEY hives[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };

    for (int h = 0; h < 2; h++) {
        HKEY root = hives[h];

        // 1. Для всех файлов (*) -> "Комментировать..." (MUI ID 101)
        RegisterContextVerbInHive(root, L"*", L"FileCommentEditor", verbComment, 101, szExePath, L"\"%1\"");

        // 2. Для папок (Folder и Directory) -> "Комментировать..." (MUI ID 101)
        RegisterContextVerbInHive(root, L"Folder", L"FileCommentEditor", verbComment, 101, szExePath, L"\"%1\"");
        RegisterContextVerbInHive(root, L"Directory", L"FileCommentEditor", verbComment, 101, szExePath, L"\"%1\"");

        // 3. Для папок (Directory и Folder) -> "Искать в заметках..." (MUI ID 102)
        RegisterContextVerbInHive(root, L"Directory", L"FileCommentSearch", verbSearch, 102, szExePath, L"--search-gui \"%1\"");
        RegisterContextVerbInHive(root, L"Folder", L"FileCommentSearch", verbSearch, 102, szExePath, L"--search-gui \"%1\"");

        // 4. Для пустого места внутри открытой папки (Background) -> "Искать в заметках..." (MUI ID 102)
        RegisterContextVerbInHive(root, L"Directory\\Background", L"FileCommentSearch", verbSearch, 102, szExePath, L"--search-gui \"%V\"");
        RegisterContextVerbInHive(root, L"LibraryFolder\\Background", L"FileCommentSearch", verbSearch, 102, szExePath, L"--search-gui \"%V\"");

        // 5. Для дисков (Drive) -> "Искать в заметках..." и "Комментировать..."
        RegisterContextVerbInHive(root, L"Drive", L"FileCommentSearch", verbSearch, 102, szExePath, L"--search-gui \"%1\"");
        RegisterContextVerbInHive(root, L"Drive", L"FileCommentEditor", verbComment, 101, szExePath, L"\"%1\"");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

static bool UninstallContextMenu() {
    HKEY hives[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (int h = 0; h < 2; h++) {
        HKEY root = hives[h];
        RegDeleteTreeW(root, L"Software\\Classes\\*\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Folder\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\shell\\FileCommentEditor");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Folder\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Directory\\Background\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\LibraryFolder\\Background\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Drive\\shell\\FileCommentSearch");
        RegDeleteTreeW(root, L"Software\\Classes\\Drive\\shell\\FileCommentEditor");
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

// ----------------------------------------------------------------------------
// Резервное копирование и экспорт / импорт заметок в JSON
// ----------------------------------------------------------------------------
static std::string EscapeJsonString(const std::wstring& wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string utf8(len, 0);
    if (len > 0) {
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &utf8[0], len, NULL, NULL);
    }
    std::string out;
    out.reserve(utf8.size() + 16);
    for (size_t i = 0; i < utf8.size(); i++) {
        unsigned char c = (unsigned char)utf8[i];
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char hex[8];
                    StringCchPrintfA(hex, ARRAYSIZE(hex), "\\u%04x", (int)c);
                    out += hex;
                } else {
                    out += (char)c;
                }
                break;
        }
    }
    return out;
}

static std::wstring UnescapeJsonString(const std::string& input) {
    std::string unescaped;
    unescaped.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            switch (next) {
                case '\"': unescaped += '\"'; i++; break;
                case '\\': unescaped += '\\'; i++; break;
                case '/':  unescaped += '/';  i++; break;
                case 'b':  unescaped += '\b'; i++; break;
                case 'f':  unescaped += '\f'; i++; break;
                case 'n':  unescaped += '\n'; i++; break;
                case 'r':  unescaped += '\r'; i++; break;
                case 't':  unescaped += '\t'; i++; break;
                case 'u': {
                    if (i + 5 < input.size()) {
                        std::string hexStr = input.substr(i + 2, 4);
                        wchar_t code = (wchar_t)strtol(hexStr.c_str(), NULL, 16);
                        wchar_t wbuf[2] = { code, 0 };
                        char u8[8] = { 0 };
                        int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, 1, u8, sizeof(u8), NULL, NULL);
                        if (u8len > 0) {
                            unescaped.append(u8, u8len);
                        }
                        i += 5;
                    } else {
                        unescaped += input[i];
                    }
                    break;
                }
                default:
                    unescaped += input[i];
                    break;
            }
        } else {
            unescaped += input[i];
        }
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), (int)unescaped.size(), NULL, 0);
    if (wlen > 0) {
        std::wstring wout(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), (int)unescaped.size(), &wout[0], wlen);
        return wout;
    }
    return L"";
}

struct BackupItem {
    std::wstring path;
    bool isDirectory;
    std::wstring comment;
};

static void CollectCommentsRecursive(const std::wstring& rootDir, std::vector<BackupItem>& outItems, DWORD& scannedCount) {
    std::wstring pattern = rootDir;
    if (pattern.back() != L'\\') pattern += L'\\';
    pattern += L"*";

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subdirs;

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;

        std::wstring fullPath = rootDir;
        if (fullPath.back() != L'\\') fullPath += L'\\';
        fullPath += ffd.cFileName;

        scannedCount++;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring dirComment;
            if (ReadComment(fullPath, dirComment) && !dirComment.empty()) {
                BackupItem bi;
                bi.path = fullPath;
                bi.isDirectory = true;
                bi.comment = dirComment;
                outItems.push_back(bi);
            }

            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                _wcsicmp(ffd.cFileName, L"$Recycle.Bin") != 0 &&
                _wcsicmp(ffd.cFileName, L"System Volume Information") != 0) {
                subdirs.push_back(fullPath);
            }
        } else {
            std::wstring fileComment;
            if (ReadComment(fullPath, fileComment) && !fileComment.empty()) {
                BackupItem bi;
                bi.path = fullPath;
                bi.isDirectory = false;
                bi.comment = fileComment;
                outItems.push_back(bi);
            }
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);

    for (const auto& sub : subdirs) {
        CollectCommentsRecursive(sub, outItems, scannedCount);
    }
}

static bool ExportCommentsToJson(const std::wstring& scanDir, const std::wstring& jsonFilePath, DWORD& outScanned, DWORD& outExported) {
    outScanned = 0;
    outExported = 0;

    std::vector<BackupItem> items;
    std::wstring rootComment;
    if (ReadComment(scanDir, rootComment) && !rootComment.empty()) {
        BackupItem bi;
        bi.path = scanDir;
        bi.isDirectory = true;
        bi.comment = rootComment;
        items.push_back(bi);
    }

    CollectCommentsRecursive(scanDir, items, outScanned);
    outExported = (DWORD)items.size();

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"application\": \"FileCommenter\",\n";
    oss << "  \"version\": \"1.2\",\n";
    oss << "  \"scanDirectory\": \"" << EscapeJsonString(scanDir) << "\",\n";
    oss << "  \"totalComments\": " << items.size() << ",\n";
    oss << "  \"items\": [\n";

    for (size_t i = 0; i < items.size(); i++) {
        const auto& it = items[i];
        oss << "    {\n";
        oss << "      \"path\": \"" << EscapeJsonString(it.path) << "\",\n";
        oss << "      \"isDirectory\": " << (it.isDirectory ? "true" : "false") << ",\n";
        oss << "      \"comment\": \"" << EscapeJsonString(it.comment) << "\"\n";
        oss << "    }" << (i + 1 < items.size() ? "," : "") << "\n";
    }

    oss << "  ]\n";
    oss << "}\n";

    std::string jsonStr = oss.str();

    HANDLE hFile = CreateFileW(jsonFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    WriteFile(hFile, bom, sizeof(bom), &written, NULL);
    WriteFile(hFile, jsonStr.c_str(), (DWORD)jsonStr.length(), &written, NULL);
    CloseHandle(hFile);

    return true;
}

static bool ImportCommentsFromJson(const std::wstring& jsonFilePath, DWORD& outTotal, DWORD& outRestored, DWORD& outErrors) {
    outTotal = 0;
    outRestored = 0;
    outErrors = 0;

    HANDLE hFile = CreateFileW(jsonFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD sizeLow = GetFileSize(hFile, NULL);
    if (sizeLow == 0 || sizeLow == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<char> buf(sizeLow + 1, 0);
    DWORD read = 0;
    ReadFile(hFile, buf.data(), sizeLow, &read, NULL);
    CloseHandle(hFile);

    const char* start = buf.data();
    if (read >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
    }

    std::string content(start);

    size_t pos = 0;
    while ((pos = content.find("\"path\"", pos)) != std::string::npos) {
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) break;
        size_t quote1 = content.find('\"', colon);
        if (quote1 == std::string::npos) break;

        size_t quote2 = quote1 + 1;
        while (quote2 < content.size()) {
            if (content[quote2] == '\"' && content[quote2 - 1] != '\\') break;
            quote2++;
        }
        if (quote2 >= content.size()) break;

        std::string rawPath = content.substr(quote1 + 1, quote2 - quote1 - 1);
        std::wstring targetPath = UnescapeJsonString(rawPath);

        size_t commentKey = content.find("\"comment\"", quote2);
        if (commentKey == std::string::npos) break;

        size_t cColon = content.find(':', commentKey);
        if (cColon == std::string::npos) break;
        size_t cQuote1 = content.find('\"', cColon);
        if (cQuote1 == std::string::npos) break;

        size_t cQuote2 = cQuote1 + 1;
        while (cQuote2 < content.size()) {
            if (content[cQuote2] == '\"' && content[cQuote2 - 1] != '\\') break;
            cQuote2++;
        }
        if (cQuote2 >= content.size()) break;

        std::string rawComment = content.substr(cQuote1 + 1, cQuote2 - cQuote1 - 1);
        std::wstring targetComment = UnescapeJsonString(rawComment);

        outTotal++;
        if (!targetPath.empty()) {
            if (WriteComment(targetPath, targetComment)) {
                outRestored++;
            } else {
                outErrors++;
            }
        }

        pos = cQuote2 + 1;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

// ----------------------------------------------------------------------------
// Регистрация DLL расширения через regsvr32
// ----------------------------------------------------------------------------
static bool ExecuteRegsvr(bool unregister) {
    wchar_t szDllPath[MAX_PATH];
    GetModuleFileNameW(NULL, szDllPath, ARRAYSIZE(szDllPath));
    PathRemoveFileSpecW(szDllPath);
    PathAppendW(szDllPath, L"FileCommentTip.dll");

    if (!PathFileExistsW(szDllPath)) {
        std::wstring err = (g_currentLang == LANG_RU) ?
            L"Файл FileCommentTip.dll не найден рядом с этой программой!" :
            L"FileCommentTip.dll not found in the application directory!";
        MessageBoxW(g_hMainWnd, err.c_str(), L"Error", MB_ICONWARNING);
        return false;
    }

    std::wstring params = unregister ? L"/u /s \"" : L"/s \"";
    params += szDllPath;
    params += L"\"";

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // Запрос прав администратора для регистрации в реестре
    sei.lpFile = L"regsvr32.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 10000);
            DWORD dwExitCode = 0;
            GetExitCodeProcess(sei.hProcess, &dwExitCode);
            CloseHandle(sei.hProcess);
            return (dwExitCode == 0);
        }
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// Работа с системным треем и горячими клавишами
// ----------------------------------------------------------------------------
static void SetupTrayIcon(HWND hWnd, bool add) {
    if (!g_bEnableTrayIcon) {
        NOTIFYICONDATAW nid = { sizeof(nid) };
        nid.hWnd = hWnd;
        nid.uID = ID_TRAY_ICON;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        return;
    }

    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hAppIconSmall ? g_hAppIconSmall : g_hAppIcon;
    std::wstring tip = GetStr(STR_TRAY_TIP) + L" (" + FormatHotkeyString(g_hotkeyMods, g_hotkeyVK) + L")";
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), tip.c_str());

    if (add) {
        Shell_NotifyIconW(NIM_ADD, &nid);
    } else {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
}

static void ShowTrayBalloon(HWND hWnd, const std::wstring& title, const std::wstring& text) {
    if (!g_bEnableTrayIcon) return;

    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title.c_str());
    StringCchCopyW(nid.szInfo, ARRAYSIZE(nid.szInfo), text.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static bool ApplyGlobalHotkey(HWND hWnd) {
    if (g_hotkeyRegistered) {
        UnregisterHotKey(hWnd, ID_HOTKEY_GLOBAL);
        g_hotkeyRegistered = false;
    }

    if (RegisterHotKey(hWnd, ID_HOTKEY_GLOBAL, g_hotkeyMods, g_hotkeyVK)) {
        g_hotkeyRegistered = true;
        std::wstring str = FormatHotkeyString(g_hotkeyMods, g_hotkeyVK);
        wchar_t buf[256];
        StringCchPrintfW(buf, ARRAYSIZE(buf), GetStr(STR_HOTKEY_STATUS_OK).c_str(), str.c_str());
        SetDlgItemTextW(hWnd, IDC_ST_HOTKEY_STATUS, buf);
        SetupTrayIcon(hWnd, true); // обновить тултип в трее
        return true;
    } else {
        SetDlgItemTextW(hWnd, IDC_ST_HOTKEY_STATUS, GetStr(STR_HOTKEY_STATUS_FAIL).c_str());
        return false;
    }
}

// ----------------------------------------------------------------------------
// Прототипы функций интерфейса
// ----------------------------------------------------------------------------
static void OpenSearchWindow(HWND hParent, const std::wstring& initialDir);
static void UpdateMainUiLanguage(HWND hWnd);
static void UpdateSearchUiLanguage(HWND hWnd);

// Обновление интерфейса окна для выбранного файла
static void LoadFileToUI(HWND hWnd, const std::wstring& filePath) {
    g_currentFilePath = filePath;
    SetDlgItemTextW(hWnd, IDC_EDT_FILEPATH, filePath.c_str());

    std::wstring comment;
    bool hasComment = ReadComment(filePath, comment);
    SetDlgItemTextW(hWnd, IDC_EDT_COMMENT, comment.c_str());

    // Информация о файле или папке
    WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };
    if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad)) {
        wchar_t szInfo[256];
        const wchar_t* yesNo = hasComment ?
            ((g_currentLang == LANG_RU) ? L"ЕСТЬ (активен)" : L"YES (active)") :
            ((g_currentLang == LANG_RU) ? L"ОТСУТСТВУЕТ" : L"NONE");

        if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            StringCchPrintfW(szInfo, ARRAYSIZE(szInfo),
                (g_currentLang == LANG_RU) ?
                L"📁 Папка с файлами (Каталог) | Заметка NTFS: %s" :
                L"📁 File Folder (Directory) | NTFS Note: %s", yesNo);
        } else {
            ULARGE_INTEGER uli;
            uli.LowPart = fad.nFileSizeLow;
            uli.HighPart = fad.nFileSizeHigh;
            double kb = (double)uli.QuadPart / 1024.0;
            StringCchPrintfW(szInfo, ARRAYSIZE(szInfo), GetStr(STR_FILE_INFO_FORMAT).c_str(), kb, yesNo);
        }
        SetDlgItemTextW(hWnd, IDC_ST_FILEINFO, szInfo);
    } else {
        SetDlgItemTextW(hWnd, IDC_ST_FILEINFO, GetStr(STR_FILE_NOT_EXISTS).c_str());
    }

    SetDlgItemTextW(hWnd, IDC_ST_STATUS, hasComment ? GetStr(STR_STATUS_LOADED).c_str() : GetStr(STR_STATUS_READY).c_str());
}

// ----------------------------------------------------------------------------
// Сабклассинг полей ввода для запуска поиска по клавише Enter
// ----------------------------------------------------------------------------
static WNDPROC g_oldEditProc = NULL;
static LRESULT CALLBACK SearchEditEnterSubclass(HWND hEdit, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND hParent = GetParent(hEdit);
        if (hParent) {
            SendMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_SRCH_BTN_START, 0), 0);
            return 0;
        }
    }
    return CallWindowProcW(g_oldEditProc, hEdit, uMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// Оконная процедура Поиска по заметкам
// ----------------------------------------------------------------------------
LRESULT CALLBACK SearchWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    // Свободное перемещение окна мышью за любое свободное место
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hWnd, uMsg, wParam, lParam);
        if (hit == HTCLIENT) {
            return HTCAPTION;
        }
        return hit;
    }

    case WM_LBUTTONDOWN: {
        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_CREATE: {
        // Установка иконки окна поиска
        if (g_hAppIcon) SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hAppIcon);
        if (g_hAppIconSmall) SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hAppIconSmall);

        // Поле ввода запроса (с поддержкой Enter)
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 15, 320, 20, hWnd, (HMENU)IDC_SRCH_LBL_QUERY, g_hInstance, NULL);

        HWND hEdtQuery = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 36, 420, 26, hWnd, (HMENU)IDC_SRCH_QUERY, g_hInstance, NULL);
        if (!g_oldEditProc) {
            g_oldEditProc = (WNDPROC)GetWindowLongPtrW(hEdtQuery, GWLP_WNDPROC);
        }
        SetWindowLongPtrW(hEdtQuery, GWLP_WNDPROC, (LONG_PTR)SearchEditEnterSubclass);

        // Кнопки запуска и остановки поиска
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            450, 36, 110, 26, hWnd, (HMENU)IDC_SRCH_BTN_START, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            570, 36, 90, 26, hWnd, (HMENU)IDC_SRCH_BTN_STOP, g_hInstance, NULL);

        // Папка поиска (с поддержкой Enter)
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 72, 250, 20, hWnd, (HMENU)IDC_SRCH_LBL_DIR, g_hInstance, NULL);

        HWND hEdtDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 93, 530, 26, hWnd, (HMENU)IDC_SRCH_DIR, g_hInstance, NULL);
        SetWindowLongPtrW(hEdtDir, GWLP_WNDPROC, (LONG_PTR)SearchEditEnterSubclass);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            560, 93, 100, 26, hWnd, (HMENU)IDC_SRCH_BTN_BROWSE_DIR, g_hInstance, NULL);

        // Опции поиска (чекбоксы)
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 128, 260, 24, hWnd, (HMENU)IDC_SRCH_CHK_RECURSIVE, g_hInstance, NULL);
        CheckDlgButton(hWnd, IDC_SRCH_CHK_RECURSIVE, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            290, 128, 170, 24, hWnd, (HMENU)IDC_SRCH_CHK_MATCHCASE, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            470, 128, 190, 24, hWnd, (HMENU)IDC_SRCH_CHK_EXACTPHRASE, g_hInstance, NULL);

        // Таблица результатов (SysListView32) с поддержкой тултипов LVS_EX_INFOTIP
        HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            20, 160, 640, 260, hWnd, (HMENU)IDC_SRCH_LISTVIEW, g_hInstance, NULL);

        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

        HWND hToolTip = ListView_GetToolTips(hList);
        if (hToolTip) {
            SendMessageW(hToolTip, TTM_SETMAXTIPWIDTH, 0, 600);
            SendMessageW(hToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 250);
            SendMessageW(hToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
        }

        // Добавляем столбцы (строки сохраняются в локальных переменных, исключая кракозябры)
        std::wstring col0 = GetStr(STR_SRCH_COL_FILENAME);
        std::wstring col1 = GetStr(STR_SRCH_COL_SNIPPET);
        std::wstring col2 = GetStr(STR_SRCH_COL_FOLDER);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        lvc.iSubItem = 0;
        lvc.cx = 170;
        lvc.pszText = &col0[0];
        ListView_InsertColumn(hList, 0, &lvc);

        lvc.iSubItem = 1;
        lvc.cx = 260;
        lvc.pszText = &col1[0];
        ListView_InsertColumn(hList, 1, &lvc);

        lvc.iSubItem = 2;
        lvc.cx = 210;
        lvc.pszText = &col2[0];
        ListView_InsertColumn(hList, 2, &lvc);

        // Статус
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 430, 360, 22, hWnd, (HMENU)IDC_SRCH_ST_STATUS, g_hInstance, NULL);

        // Кнопки действий
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 428, 130, 30, hWnd, (HMENU)IDC_SRCH_BTN_OPEN_FILE, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            520, 428, 140, 30, hWnd, (HMENU)IDC_SRCH_BTN_EXPLORER, g_hInstance, NULL);

        // Применяем шрифты
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)g_hFontUI);

        UpdateSearchUiLanguage(hWnd);
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        switch (id) {
        case IDC_SRCH_BTN_BROWSE_DIR: {
            std::wstring folder;
            if (BrowseForFolder(hWnd, folder)) {
                SetDlgItemTextW(hWnd, IDC_SRCH_DIR, folder.c_str());
            }
            break;
        }

        case IDC_SRCH_BTN_START: {
            wchar_t szDir[MAX_PATH];
            GetDlgItemTextW(hWnd, IDC_SRCH_DIR, szDir, ARRAYSIZE(szDir));
            if (wcslen(szDir) == 0 || !PathFileExistsW(szDir)) {
                std::wstring warn = (g_currentLang == LANG_RU) ?
                    L"Укажите существующую папку или диск для поиска!" :
                    L"Please specify an existing folder or drive to search!";
                MessageBoxW(hWnd, warn.c_str(), L"Warning", MB_ICONWARNING);
                return 0;
            }

            wchar_t szQuery[512];
            GetDlgItemTextW(hWnd, IDC_SRCH_QUERY, szQuery, ARRAYSIZE(szQuery));

            HWND hList = GetDlgItem(hWnd, IDC_SRCH_LISTVIEW);
            ListView_DeleteAllItems(hList);
            {
                EnsureCS();
                AutoLockCS lock(&g_resultsCS);
                g_searchResults.clear();
            }

            g_bCancelSearch.store(false);
            EnableWindow(GetDlgItem(hWnd, IDC_SRCH_BTN_START), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_SRCH_BTN_STOP), TRUE);
            SetDlgItemTextW(hWnd, IDC_SRCH_ST_STATUS, GetStr(STR_SRCH_STATUS_RUNNING).c_str());

            SearchThreadParams* pParams = new SearchThreadParams();
            pParams->hWndNotify = hWnd;
            pParams->searchDir = szDir;
            pParams->query = szQuery;
            pParams->recursive = (IsDlgButtonChecked(hWnd, IDC_SRCH_CHK_RECURSIVE) == BST_CHECKED);
            pParams->matchCase = (IsDlgButtonChecked(hWnd, IDC_SRCH_CHK_MATCHCASE) == BST_CHECKED);
            pParams->exactPhrase = (IsDlgButtonChecked(hWnd, IDC_SRCH_CHK_EXACTPHRASE) == BST_CHECKED);

            HANDLE hThread = CreateThread(NULL, 0, SearchWorkerThread, pParams, 0, NULL);
            if (hThread) CloseHandle(hThread);
            break;
        }

        case IDC_SRCH_BTN_STOP: {
            g_bCancelSearch.store(true);
            SetDlgItemTextW(hWnd, IDC_SRCH_ST_STATUS, L"...");
            break;
        }

        case IDC_SRCH_BTN_OPEN_FILE: {
            HWND hList = GetDlgItem(hWnd, IDC_SRCH_LISTVIEW);
            int idx = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (idx >= 0 && idx < (int)g_searchResults.size()) {
                std::wstring targetPath = g_searchResults[idx].filePath;
                if (g_hMainWnd) {
                    LoadFileToUI(g_hMainWnd, targetPath);
                    ShowWindow(g_hMainWnd, SW_RESTORE);
                    SetForegroundWindow(g_hMainWnd);
                }
            } else {
                std::wstring info = (g_currentLang == LANG_RU) ?
                    L"Выберите файл из списка для редактирования." :
                    L"Please select a file from the list to edit.";
                MessageBoxW(hWnd, info.c_str(), L"Info", MB_ICONINFORMATION);
            }
            break;
        }

        case IDC_SRCH_BTN_EXPLORER: {
            HWND hList = GetDlgItem(hWnd, IDC_SRCH_LISTVIEW);
            int idx = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (idx >= 0 && idx < (int)g_searchResults.size()) {
                std::wstring targetPath = g_searchResults[idx].filePath;
                std::wstring params = L"/select,\"" + targetPath + L"\"";
                ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOWNORMAL);
            }
            break;
        }
        }
        return 0;
    }

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == IDC_SRCH_LISTVIEW) {
            // Двойной клик по найденному файлу: открывает его расположение в Проводнике
            if (pnmh->code == NM_DBLCLK) {
                NMITEMACTIVATE* pnmia = (NMITEMACTIVATE*)lParam;
                if (pnmia && pnmia->iItem >= 0) {
                    ListView_SetItemState(pnmh->hwndFrom, pnmia->iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
                SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_SRCH_BTN_EXPLORER, 0), 0);
                return 0;
            }

            // Правый клик по найденному файлу: меню "Расположение файла" и "Редактировать заметку..."
            if (pnmh->code == NM_RCLICK) {
                NMITEMACTIVATE* pnmia = (NMITEMACTIVATE*)lParam;
                int idx = (pnmia ? pnmia->iItem : -1);
                if (idx < 0) {
                    idx = ListView_GetNextItem(pnmh->hwndFrom, -1, LVNI_SELECTED);
                }
                if (idx >= 0 && idx < (int)g_searchResults.size()) {
                    ListView_SetItemState(pnmh->hwndFrom, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = CreatePopupMenu();
                    std::wstring strExplorer = (g_currentLang == LANG_RU) ? L"📂 Расположение файла" : L"📂 Open file location";
                    std::wstring strEdit = (g_currentLang == LANG_RU) ? L"✏️ Редактировать заметку..." : L"✏️ Edit comment...";

                    AppendMenuW(hMenu, MF_STRING, 1, strExplorer.c_str());
                    AppendMenuW(hMenu, MF_STRING, 2, strEdit.c_str());

                    SetForegroundWindow(hWnd);
                    int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);
                    DestroyMenu(hMenu);

                    if (cmd == 1) {
                        SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_SRCH_BTN_EXPLORER, 0), 0);
                    } else if (cmd == 2) {
                        SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_SRCH_BTN_OPEN_FILE, 0), 0);
                    }
                }
                return 0;
            }

            // Всплывающая подсказка (тултип) с полным комментарием при наведении курсора
            if (pnmh->code == LVN_GETINFOTIPW) {
                NMLVGETINFOTIPW* pTip = (NMLVGETINFOTIPW*)lParam;
                int itemIdx = pTip->iItem;
                if (itemIdx >= 0 && itemIdx < (int)g_searchResults.size()) {
                    const auto& item = g_searchResults[itemIdx];
                    std::wstring fullTip = item.fileName + L"\n" +
                        (g_currentLang == LANG_RU ? L"Папка: " : L"Folder: ") + item.folderPath + L"\n" +
                        (g_currentLang == LANG_RU ? L"Размер: " : L"Size: ") + std::to_wstring(item.fileSize / 1024) + L" KB\n" +
                        L"────────────────────────────────────────\n" +
                        (g_currentLang == LANG_RU ? L"📝 Заметка NTFS :comment:\n" : L"📝 NTFS :comment note:\n") +
                        item.commentText;

                    StringCchCopyNW(pTip->pszText, pTip->cchTextMax, fullTip.c_str(), pTip->cchTextMax);
                }
                return 0;
            }
        }
        return 0;
    }

    case WM_SEARCH_RESULT_FOUND: {
        int index = (int)lParam;
        HWND hList = GetDlgItem(hWnd, IDC_SRCH_LISTVIEW);
        if (index >= 0 && index < (int)g_searchResults.size()) {
            SearchResultItem item = g_searchResults[index];

            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = (LPWSTR)item.fileName.c_str();
            lvi.lParam = index;
            int row = ListView_InsertItem(hList, &lvi);

            ListView_SetItemText(hList, row, 1, (LPWSTR)item.snippet.c_str());
            ListView_SetItemText(hList, row, 2, (LPWSTR)item.folderPath.c_str());
        }
        return 0;
    }

    case WM_SEARCH_PROGRESS: {
        DWORD scanned = (DWORD)wParam;
        DWORD found = (DWORD)lParam;
        wchar_t szStatus[128];
        StringCchPrintfW(szStatus, ARRAYSIZE(szStatus), GetStr(STR_SRCH_STATUS_PROGRESS).c_str(), scanned, found);
        SetDlgItemTextW(hWnd, IDC_SRCH_ST_STATUS, szStatus);
        return 0;
    }

    case WM_SEARCH_FINISHED: {
        DWORD scanned = (DWORD)wParam;
        DWORD found = (DWORD)lParam;
        EnableWindow(GetDlgItem(hWnd, IDC_SRCH_BTN_START), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_SRCH_BTN_STOP), FALSE);

        wchar_t szStatus[128];
        if (g_bCancelSearch.load()) {
            StringCchPrintfW(szStatus, ARRAYSIZE(szStatus), GetStr(STR_SRCH_STATUS_STOPPED).c_str(), found);
        } else {
            StringCchPrintfW(szStatus, ARRAYSIZE(szStatus), GetStr(STR_SRCH_STATUS_DONE).c_str(), scanned, found);
        }
        SetDlgItemTextW(hWnd, IDC_SRCH_ST_STATUS, szStatus);
        return 0;
    }

    case WM_CLOSE: {
        g_bCancelSearch.store(true);
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void UpdateSearchUiLanguage(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;

    SetWindowTextW(hWnd, GetStr(STR_SEARCH_TITLE).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_LBL_QUERY, GetStr(STR_SRCH_QUERY_LBL).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_BTN_START, GetStr(STR_SRCH_BTN_START).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_BTN_STOP, GetStr(STR_SRCH_BTN_STOP).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_LBL_DIR, GetStr(STR_SRCH_WHERE_LBL).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_BTN_BROWSE_DIR, GetStr(STR_BROWSE).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_CHK_RECURSIVE, GetStr(STR_SRCH_CHK_RECURSIVE).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_CHK_MATCHCASE, GetStr(STR_SRCH_CHK_MATCHCASE).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_CHK_EXACTPHRASE, GetStr(STR_SRCH_CHK_EXACTPHRASE).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_ST_STATUS, GetStr(STR_SRCH_STATUS_PROMPT).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_BTN_OPEN_FILE, GetStr(STR_SRCH_BTN_EDIT).c_str());
    SetDlgItemTextW(hWnd, IDC_SRCH_BTN_EXPLORER, GetStr(STR_SRCH_BTN_EXPLORER).c_str());

    HWND hList = GetDlgItem(hWnd, IDC_SRCH_LISTVIEW);
    if (hList) {
        std::wstring col0 = GetStr(STR_SRCH_COL_FILENAME);
        std::wstring col1 = GetStr(STR_SRCH_COL_SNIPPET);
        std::wstring col2 = GetStr(STR_SRCH_COL_FOLDER);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT;
        lvc.pszText = &col0[0];
        ListView_SetColumn(hList, 0, &lvc);
        lvc.pszText = &col1[0];
        ListView_SetColumn(hList, 1, &lvc);
        lvc.pszText = &col2[0];
        ListView_SetColumn(hList, 2, &lvc);
    }
}

static void OpenSearchWindow(HWND hParent, const std::wstring& initialDir) {
    if (!g_hSearchWnd) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = SearchWndProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = L"FileCommentSearchWndClass";
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = g_hAppIcon;
        wc.hIconSm = g_hAppIconSmall;
        RegisterClassExW(&wc);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int winW = 696;
        int winH = 510;
        int winX = (screenW - winW) / 2;
        int winY = (screenH - winH) / 2;

        g_hSearchWnd = CreateWindowExW(
            0,
            L"FileCommentSearchWndClass",
            GetStr(STR_SEARCH_TITLE).c_str(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            winX, winY, winW, winH,
            hParent, NULL, g_hInstance, NULL
        );
    }

    if (!initialDir.empty()) {
        SetDlgItemTextW(g_hSearchWnd, IDC_SRCH_DIR, initialDir.c_str());
    } else {
        wchar_t szCurr[MAX_PATH];
        GetCurrentDirectoryW(ARRAYSIZE(szCurr), szCurr);
        SetDlgItemTextW(g_hSearchWnd, IDC_SRCH_DIR, szCurr);
    }

    UpdateSearchUiLanguage(g_hSearchWnd);
    ShowWindow(g_hSearchWnd, SW_SHOW);
    SetForegroundWindow(g_hSearchWnd);
}

// ----------------------------------------------------------------------------
// Оконная процедура GUI редактора
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    // Свободное перемещение окна мышью за любое свободное место
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hWnd, uMsg, wParam, lParam);
        if (hit == HTCLIENT) {
            return HTCAPTION;
        }
        return hit;
    }

    case WM_LBUTTONDOWN: {
        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_CREATE: {
        // Установка иконки главного окна
        if (g_hAppIcon) SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hAppIcon);
        if (g_hAppIconSmall) SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hAppIconSmall);

        g_hFontUI = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // 1. Поле выбора файла + Кнопка языка
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 15, 360, 20, hWnd, (HMENU)IDC_LBL_TARGETFILE, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            550, 10, 125, 26, hWnd, (HMENU)IDC_BTN_LANG_TOGGLE, g_hInstance, NULL);

        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 38, 520, 26, hWnd, (HMENU)IDC_EDT_FILEPATH, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            550, 38, 125, 26, hWnd, (HMENU)IDC_BTN_BROWSE, g_hInstance, NULL);

        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            20, 68, 655, 20, hWnd, (HMENU)IDC_ST_FILEINFO, g_hInstance, NULL);

        // 2. Блок редактирования комментария
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 92, 450, 20, hWnd, (HMENU)IDC_LBL_TOOLTIP, g_hInstance, NULL);

        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            20, 114, 655, 95, hWnd, (HMENU)IDC_EDT_COMMENT, g_hInstance, NULL);

        // Кнопки действий над комментарием
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            20, 216, 110, 32, hWnd, (HMENU)IDC_BTN_SAVE, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            138, 216, 105, 32, hWnd, (HMENU)IDC_BTN_DELETE, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            251, 216, 125, 32, hWnd, (HMENU)IDC_BTN_OPEN_SEARCH, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            384, 216, 170, 32, hWnd, (HMENU)IDC_BTN_BACKUP_MENU, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            562, 216, 113, 32, hWnd, (HMENU)IDC_BTN_MINIMIZE_TRAY, g_hInstance, NULL);

        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 254, 655, 20, hWnd, (HMENU)IDC_ST_STATUS, g_hInstance, NULL);

        // 3. Блок настроек горячих клавиш и автозапуска
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            20, 278, 655, 128, hWnd, (HMENU)IDC_GRP_HOTKEY, g_hInstance, NULL);

        // Чекбокс автозапуска и трея
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            35, 300, 280, 22, hWnd, (HMENU)IDC_CHK_AUTOSTART, g_hInstance, NULL);
        if (IsAutostartEnabled()) {
            CheckDlgButton(hWnd, IDC_CHK_AUTOSTART, BST_CHECKED);
        }

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            330, 300, 330, 22, hWnd, (HMENU)IDC_CHK_ENABLE_TRAY, g_hInstance, NULL);
        if (g_bEnableTrayIcon) {
            CheckDlgButton(hWnd, IDC_CHK_ENABLE_TRAY, BST_CHECKED);
        }

        // Модификаторы хоткея
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            35, 332, 80, 20, hWnd, (HMENU)IDC_LBL_MODIFIERS, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"Win", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            120, 332, 50, 20, hWnd, (HMENU)IDC_CHK_HOTKEY_WIN, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"Ctrl", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            175, 332, 52, 20, hWnd, (HMENU)IDC_CHK_HOTKEY_CTRL, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"Alt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            232, 332, 48, 20, hWnd, (HMENU)IDC_CHK_HOTKEY_ALT, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"Shift", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            285, 332, 55, 20, hWnd, (HMENU)IDC_CHK_HOTKEY_SHIFT, g_hInstance, NULL);

        // Символ клавиши
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            350, 332, 60, 20, hWnd, (HMENU)IDC_LBL_KEY, g_hInstance, NULL);

        HWND hCmbKey = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            415, 329, 75, 150, hWnd, (HMENU)IDC_CMB_HOTKEY_KEY, g_hInstance, NULL);

        // Заполнение списка клавиш
        const wchar_t* keys[] = { L"C", L"F", L"K", L"S", L"O", L"Space", L"F1", L"F2", L"F3", L"F4", L"F8", L"F9", L"F12" };
        for (const auto& k : keys) {
            SendMessageW(hCmbKey, CB_ADDSTRING, 0, (LPARAM)k);
        }
        SendMessageW(hCmbKey, CB_SETCURSEL, 0, 0); // По умолчанию "C"

        // Выбор действия по хоткею
        HWND hCmbAct = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            35, 364, 220, 100, hWnd, (HMENU)IDC_CMB_HOTKEY_ACTION, g_hInstance, NULL);
        SendMessageW(hCmbAct, CB_ADDSTRING, 0, (LPARAM)GetStr(STR_ACT_OPEN_SEARCH).c_str());
        SendMessageW(hCmbAct, CB_ADDSTRING, 0, (LPARAM)GetStr(STR_ACT_OPEN_EDITOR).c_str());
        SendMessageW(hCmbAct, CB_SETCURSEL, g_hotkeyAction, 0);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            265, 362, 130, 26, hWnd, (HMENU)IDC_BTN_APPLY_HOTKEY, g_hInstance, NULL);

        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            405, 366, 260, 20, hWnd, (HMENU)IDC_ST_HOTKEY_STATUS, g_hInstance, NULL);

        // Инициализация чекбоксов хоткея по текущему состоянию
        if (g_hotkeyMods & MOD_WIN) CheckDlgButton(hWnd, IDC_CHK_HOTKEY_WIN, BST_CHECKED);
        if (g_hotkeyMods & MOD_CONTROL) CheckDlgButton(hWnd, IDC_CHK_HOTKEY_CTRL, BST_CHECKED);
        if (g_hotkeyMods & MOD_ALT) CheckDlgButton(hWnd, IDC_CHK_HOTKEY_ALT, BST_CHECKED);
        if (g_hotkeyMods & MOD_SHIFT) CheckDlgButton(hWnd, IDC_CHK_HOTKEY_SHIFT, BST_CHECKED);

        // 4. Блок интеграции с Проводником Windows
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            20, 418, 655, 115, hWnd, (HMENU)IDC_GRP_EXPLORER, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            35, 444, 305, 32, hWnd, (HMENU)IDC_BTN_INSTALL_CTX, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            350, 444, 310, 32, hWnd, (HMENU)IDC_BTN_UNINSTALL_CTX, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            35, 484, 305, 32, hWnd, (HMENU)IDC_BTN_REGISTER_DLL, g_hInstance, NULL);

        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            350, 484, 310, 32, hWnd, (HMENU)IDC_BTN_UNREG_DLL, g_hInstance, NULL);

        // Применяем шрифты
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)g_hFontUI);

        // Обновляем тексты интерфейса
        UpdateMainUiLanguage(hWnd);

        // Регистрируем глобальный хоткей и системный трей
        ApplyGlobalHotkey(hWnd);

        // Загружаем начальный файл, если задан
        if (!g_currentFilePath.empty()) {
            LoadFileToUI(hWnd, g_currentFilePath);
        } else {
            SetDlgItemTextW(hWnd, IDC_ST_FILEINFO, GetStr(STR_NO_FILE_SELECTED).c_str());
            SetDlgItemTextW(hWnd, IDC_ST_STATUS, GetStr(STR_STATUS_READY).c_str());
        }

        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        switch (id) {
        case IDC_BTN_LANG_TOGGLE: {
            g_currentLang = (g_currentLang == LANG_RU) ? LANG_EN : LANG_RU;
            SaveAppSettings();
            UpdateMainUiLanguage(hWnd);
            if (g_hSearchWnd && IsWindow(g_hSearchWnd)) {
                UpdateSearchUiLanguage(g_hSearchWnd);
            }
            break;
        }

        case IDC_CHK_AUTOSTART: {
            bool check = (IsDlgButtonChecked(hWnd, IDC_CHK_AUTOSTART) == BST_CHECKED);
            SetAutostart(check);
            break;
        }

        case IDC_BTN_APPLY_HOTKEY: {
            UINT mods = 0;
            if (IsDlgButtonChecked(hWnd, IDC_CHK_HOTKEY_WIN) == BST_CHECKED) mods |= MOD_WIN;
            if (IsDlgButtonChecked(hWnd, IDC_CHK_HOTKEY_CTRL) == BST_CHECKED) mods |= MOD_CONTROL;
            if (IsDlgButtonChecked(hWnd, IDC_CHK_HOTKEY_ALT) == BST_CHECKED) mods |= MOD_ALT;
            if (IsDlgButtonChecked(hWnd, IDC_CHK_HOTKEY_SHIFT) == BST_CHECKED) mods |= MOD_SHIFT;

            HWND hCmbKey = GetDlgItem(hWnd, IDC_CMB_HOTKEY_KEY);
            int cur = (int)SendMessageW(hCmbKey, CB_GETCURSEL, 0, 0);
            UINT vk = 'C';
            switch (cur) {
            case 0: vk = 'C'; break;
            case 1: vk = 'F'; break;
            case 2: vk = 'K'; break;
            case 3: vk = 'S'; break;
            case 4: vk = 'O'; break;
            case 5: vk = VK_SPACE; break;
            case 6: vk = VK_F1; break;
            case 7: vk = VK_F2; break;
            case 8: vk = VK_F3; break;
            case 9: vk = VK_F4; break;
            case 10: vk = VK_F8; break;
            case 11: vk = VK_F9; break;
            case 12: vk = VK_F12; break;
            default: vk = 'C'; break;
            }

            HWND hCmbAct = GetDlgItem(hWnd, IDC_CMB_HOTKEY_ACTION);
            DWORD act = (DWORD)SendMessageW(hCmbAct, CB_GETCURSEL, 0, 0);

            g_hotkeyMods = mods;
            g_hotkeyVK = vk;
            g_hotkeyAction = act;

            SaveAppSettings();
            ApplyGlobalHotkey(hWnd);
            break;
        }

        case IDC_BTN_BROWSE: {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            std::wstring strFile = (g_currentLang == LANG_RU) ? L"📄 Выбрать файл..." : L"📄 Select file...";
            std::wstring strFolder = (g_currentLang == LANG_RU) ? L"📁 Выбрать папку..." : L"📁 Select folder...";
            AppendMenuW(hMenu, MF_STRING, 1, strFile.c_str());
            AppendMenuW(hMenu, MF_STRING, 2, strFolder.c_str());

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 1) {
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn = { sizeof(ofn) };
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = ARRAYSIZE(szFile);
                ofn.lpstrFilter = (g_currentLang == LANG_RU) ?
                    L"Все файлы (*.*)\0*.*\0" :
                    L"All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

                if (GetOpenFileNameW(&ofn)) {
                    LoadFileToUI(hWnd, szFile);
                }
            } else if (cmd == 2) {
                std::wstring folder;
                if (BrowseForFolder(hWnd, folder)) {
                    LoadFileToUI(hWnd, folder);
                }
            }
            break;
        }

        case IDC_BTN_OPEN_SEARCH: {
            std::wstring startDir;
            if (!g_currentFilePath.empty()) {
                wchar_t dir[MAX_PATH];
                StringCchCopyW(dir, ARRAYSIZE(dir), g_currentFilePath.c_str());
                PathRemoveFileSpecW(dir);
                startDir = dir;
            }
            OpenSearchWindow(hWnd, startDir);
            break;
        }

        case IDC_CHK_ENABLE_TRAY: {
            g_bEnableTrayIcon = (IsDlgButtonChecked(hWnd, IDC_CHK_ENABLE_TRAY) == BST_CHECKED);
            SaveAppSettings();
            SetupTrayIcon(hWnd, g_bEnableTrayIcon);
            break;
        }

        case IDC_BTN_BACKUP_MENU: {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            std::wstring strExport = GetStr(STR_MENU_BACKUP_EXPORT);
            std::wstring strImport = GetStr(STR_MENU_BACKUP_IMPORT);
            AppendMenuW(hMenu, MF_STRING, ID_BACKUP_EXPORT, strExport.c_str());
            AppendMenuW(hMenu, MF_STRING, ID_BACKUP_IMPORT, strImport.c_str());

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == ID_BACKUP_EXPORT) {
                std::wstring scanDir;
                if (!g_currentFilePath.empty()) {
                    wchar_t dir[MAX_PATH];
                    StringCchCopyW(dir, ARRAYSIZE(dir), g_currentFilePath.c_str());
                    if (!(GetFileAttributesW(dir) & FILE_ATTRIBUTE_DIRECTORY)) {
                        PathRemoveFileSpecW(dir);
                    }
                    scanDir = dir;
                }
                if (scanDir.empty()) {
                    wchar_t cur[MAX_PATH];
                    GetCurrentDirectoryW(ARRAYSIZE(cur), cur);
                    scanDir = cur;
                }

                if (!BrowseForFolder(hWnd, scanDir)) {
                    break;
                }

                wchar_t szFile[MAX_PATH] = L"FileComments_Backup.json";
                OPENFILENAMEW ofn = { sizeof(ofn) };
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = ARRAYSIZE(szFile);
                ofn.lpstrFilter = (g_currentLang == LANG_RU) ?
                    L"JSON файлы резервных копий (*.json)\0*.json\0Все файлы (*.*)\0*.*\0" :
                    L"JSON Backup Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrDefExt = L"json";
                std::wstring titleExp = GetStr(STR_BACKUP_EXPORT_TITLE);
                ofn.lpstrTitle = titleExp.c_str();
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

                if (GetSaveFileNameW(&ofn)) {
                    DWORD scanned = 0, exported = 0;
                    if (ExportCommentsToJson(scanDir, szFile, scanned, exported)) {
                        wchar_t msg[512];
                        StringCchPrintfW(msg, ARRAYSIZE(msg), GetStr(STR_BACKUP_EXPORT_SUCCESS).c_str(), scanned, exported, szFile);
                        MessageBoxW(hWnd, msg, L"FileCommenter Backup", MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hWnd, (g_currentLang == LANG_RU) ? L"Ошибка при записи файла резервной копии!" : L"Failed to write backup file!", L"Error", MB_ICONERROR);
                    }
                }
            } else if (cmd == ID_BACKUP_IMPORT) {
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn = { sizeof(ofn) };
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = ARRAYSIZE(szFile);
                ofn.lpstrFilter = (g_currentLang == LANG_RU) ?
                    L"JSON файлы резервных копий (*.json)\0*.json\0Все файлы (*.*)\0*.*\0" :
                    L"JSON Backup Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                std::wstring titleImp = GetStr(STR_BACKUP_IMPORT_TITLE);
                ofn.lpstrTitle = titleImp.c_str();
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

                if (GetOpenFileNameW(&ofn)) {
                    wchar_t prompt[512];
                    StringCchPrintfW(prompt, ARRAYSIZE(prompt), GetStr(STR_BACKUP_IMPORT_CONFIRM).c_str(), szFile);
                    if (MessageBoxW(hWnd, prompt, L"FileCommenter Restore", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        DWORD total = 0, restored = 0, errors = 0;
                        if (ImportCommentsFromJson(szFile, total, restored, errors)) {
                            wchar_t msg[512];
                            StringCchPrintfW(msg, ARRAYSIZE(msg), GetStr(STR_BACKUP_IMPORT_SUCCESS).c_str(), total, restored, errors);
                            MessageBoxW(hWnd, msg, L"FileCommenter Restore", MB_ICONINFORMATION);

                            if (!g_currentFilePath.empty()) {
                                LoadFileToUI(hWnd, g_currentFilePath);
                            }
                        } else {
                            MessageBoxW(hWnd, (g_currentLang == LANG_RU) ? L"Ошибка при чтении файла резервной копии!" : L"Failed to read backup file!", L"Error", MB_ICONERROR);
                        }
                    }
                }
            }
            break;
        }

        case IDC_BTN_MINIMIZE_TRAY: {
            ShowWindow(hWnd, SW_HIDE);
            ShowTrayBalloon(hWnd, GetStr(STR_TRAY_BALLOON_TITLE), GetStr(STR_TRAY_BALLOON_TEXT));
            break;
        }

        case IDC_BTN_SAVE: {
            if (g_currentFilePath.empty()) {
                MessageBoxW(hWnd, GetStr(STR_MSG_SELECT_FILE_FIRST).c_str(), L"Warning", MB_ICONWARNING);
                break;
            }

            int len = GetWindowTextLengthW(GetDlgItem(hWnd, IDC_EDT_COMMENT));
            std::wstring comment(len, L'\0');
            GetDlgItemTextW(hWnd, IDC_EDT_COMMENT, &comment[0], len + 1);

            if (WriteComment(g_currentFilePath, comment)) {
                SetDlgItemTextW(hWnd, IDC_ST_STATUS, GetStr(STR_STATUS_SAVED).c_str());
                LoadFileToUI(hWnd, g_currentFilePath);
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, g_currentFilePath.c_str(), NULL);
                MessageBoxW(hWnd, GetStr(STR_MSG_SAVE_CONFIRM).c_str(), L"Success", MB_ICONINFORMATION);
            } else {
                MessageBoxW(hWnd, GetStr(STR_MSG_SAVE_ERROR).c_str(), L"Error", MB_ICONERROR);
            }
            break;
        }

        case IDC_BTN_DELETE: {
            if (g_currentFilePath.empty()) {
                MessageBoxW(hWnd, GetStr(STR_MSG_SELECT_FILE_FIRST).c_str(), L"Warning", MB_ICONWARNING);
                break;
            }

            if (MessageBoxW(hWnd, GetStr(STR_MSG_DELETE_CONFIRM).c_str(), L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                DeleteComment(g_currentFilePath);
                SetDlgItemTextW(hWnd, IDC_ST_STATUS, GetStr(STR_STATUS_DELETED).c_str());
                LoadFileToUI(hWnd, g_currentFilePath);
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, g_currentFilePath.c_str(), NULL);
            }
            break;
        }

        case IDC_BTN_INSTALL_CTX: {
            if (InstallContextMenu()) {
                MessageBoxW(hWnd, GetStr(STR_MSG_CTX_INSTALLED).c_str(), L"Success", MB_ICONINFORMATION);
            } else {
                MessageBoxW(hWnd, L"Registry error", L"Error", MB_ICONERROR);
            }
            break;
        }

        case IDC_BTN_UNINSTALL_CTX: {
            UninstallContextMenu();
            MessageBoxW(hWnd, GetStr(STR_MSG_CTX_REMOVED).c_str(), L"Success", MB_ICONINFORMATION);
            break;
        }

        case IDC_BTN_REGISTER_DLL: {
            if (ExecuteRegsvr(false)) {
                MessageBoxW(hWnd, GetStr(STR_MSG_DLL_REGISTERED).c_str(), L"Success", MB_ICONINFORMATION);
            }
            break;
        }

        case IDC_BTN_UNREG_DLL: {
            if (ExecuteRegsvr(true)) {
                MessageBoxW(hWnd, GetStr(STR_MSG_DLL_UNREGISTERED).c_str(), L"Success", MB_ICONINFORMATION);
            }
            break;
        }
        }
        return 0;
    }

    case WM_HOTKEY: {
        if (wParam == ID_HOTKEY_GLOBAL) {
            if (g_hotkeyAction == 0) {
                // Открыть окно поиска
                OpenSearchWindow(NULL, L"");
                ShowWindow(g_hSearchWnd, SW_RESTORE);
                SetForegroundWindow(g_hSearchWnd);
            } else {
                // Открыть главное окно редактора
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
        }
        return 0;
    }

    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            if (g_hotkeyAction == 0) {
                OpenSearchWindow(NULL, L"");
                ShowWindow(g_hSearchWnd, SW_RESTORE);
                SetForegroundWindow(g_hSearchWnd);
            } else {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
        } else if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_EDITOR, GetStr(STR_MENU_OPEN_EDITOR).c_str());
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_SEARCH, GetStr(STR_MENU_OPEN_SEARCH).c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE_LANG, GetStr(STR_MENU_SWITCH_LANG).c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, GetStr(STR_MENU_EXIT).c_str());

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == ID_TRAY_OPEN_EDITOR) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (cmd == ID_TRAY_OPEN_SEARCH) {
                OpenSearchWindow(NULL, L"");
                ShowWindow(g_hSearchWnd, SW_RESTORE);
                SetForegroundWindow(g_hSearchWnd);
            } else if (cmd == ID_TRAY_TOGGLE_LANG) {
                SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_LANG_TOGGLE, 0), 0);
            } else if (cmd == ID_TRAY_EXIT) {
                DestroyWindow(hWnd);
            }
        }
        return 0;
    }

    case WM_CLOSE: {
        // Если настроен автозапуск или хоткей, сворачиваем в трей при закрытии
        if (IsAutostartEnabled()) {
            ShowWindow(hWnd, SW_HIDE);
            ShowTrayBalloon(hWnd, GetStr(STR_TRAY_BALLOON_TITLE), GetStr(STR_TRAY_BALLOON_TEXT));
            return 0;
        }
        break;
    }

    case WM_DESTROY: {
        SetupTrayIcon(hWnd, false);
        if (g_hotkeyRegistered) {
            UnregisterHotKey(hWnd, ID_HOTKEY_GLOBAL);
            g_hotkeyRegistered = false;
        }
        if (g_hFontUI) DeleteObject(g_hFontUI);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void UpdateMainUiLanguage(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;

    SetWindowTextW(hWnd, GetStr(STR_APP_TITLE).c_str());
    SetDlgItemTextW(hWnd, IDC_LBL_TARGETFILE, GetStr(STR_TARGET_FILE_LABEL).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_LANG_TOGGLE, GetStr(STR_LANG_TOGGLE_BTN).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_BROWSE, GetStr(STR_BROWSE).c_str());
    SetDlgItemTextW(hWnd, IDC_LBL_TOOLTIP, GetStr(STR_COMMENT_LABEL).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_SAVE, GetStr(STR_BTN_SAVE).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_DELETE, GetStr(STR_BTN_DELETE).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_OPEN_SEARCH, GetStr(STR_BTN_SEARCH).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_BACKUP_MENU, GetStr(STR_BTN_BACKUP).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_MINIMIZE_TRAY, GetStr(STR_BTN_MINIMIZE_TRAY).c_str());
    SetDlgItemTextW(hWnd, IDC_ST_STATUS, GetStr(STR_STATUS_READY).c_str());

    SetDlgItemTextW(hWnd, IDC_GRP_HOTKEY, GetStr(STR_GRP_HOTKEY_AUTOSTART).c_str());
    SetDlgItemTextW(hWnd, IDC_CHK_AUTOSTART, GetStr(STR_CHK_AUTOSTART).c_str());
    SetDlgItemTextW(hWnd, IDC_CHK_ENABLE_TRAY, GetStr(STR_CHK_ENABLE_TRAY).c_str());
    CheckDlgButton(hWnd, IDC_CHK_ENABLE_TRAY, g_bEnableTrayIcon ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(hWnd, IDC_LBL_MODIFIERS, GetStr(STR_LBL_MODIFIERS).c_str());
    SetDlgItemTextW(hWnd, IDC_LBL_KEY, GetStr(STR_LBL_KEY).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_APPLY_HOTKEY, GetStr(STR_BTN_APPLY_HOTKEY).c_str());

    HWND hCmbAct = GetDlgItem(hWnd, IDC_CMB_HOTKEY_ACTION);
    if (hCmbAct) {
        int cur = (int)SendMessageW(hCmbAct, CB_GETCURSEL, 0, 0);
        SendMessageW(hCmbAct, CB_RESETCONTENT, 0, 0);
        SendMessageW(hCmbAct, CB_ADDSTRING, 0, (LPARAM)GetStr(STR_ACT_OPEN_SEARCH).c_str());
        SendMessageW(hCmbAct, CB_ADDSTRING, 0, (LPARAM)GetStr(STR_ACT_OPEN_EDITOR).c_str());
        SendMessageW(hCmbAct, CB_SETCURSEL, (cur >= 0) ? cur : g_hotkeyAction, 0);
    }

    SetDlgItemTextW(hWnd, IDC_GRP_EXPLORER, GetStr(STR_GRP_EXPLORER).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_INSTALL_CTX, GetStr(STR_BTN_INSTALL_CTX).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_UNINSTALL_CTX, GetStr(STR_BTN_UNINSTALL_CTX).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_REGISTER_DLL, GetStr(STR_BTN_REG_DLL).c_str());
    SetDlgItemTextW(hWnd, IDC_BTN_UNREG_DLL, GetStr(STR_BTN_UNREG_DLL).c_str());

    std::wstring hotkeyStr = FormatHotkeyString(g_hotkeyMods, g_hotkeyVK);
    wchar_t buf[256];
    StringCchPrintfW(buf, ARRAYSIZE(buf), GetStr(STR_HOTKEY_STATUS_OK).c_str(), hotkeyStr.c_str());
    SetDlgItemTextW(hWnd, IDC_ST_HOTKEY_STATUS, buf);

    if (!g_currentFilePath.empty()) {
        LoadFileToUI(hWnd, g_currentFilePath);
    } else {
        SetDlgItemTextW(hWnd, IDC_ST_FILEINFO, GetStr(STR_NO_FILE_SELECTED).c_str());
    }

    SetupTrayIcon(hWnd, true);
}

// ----------------------------------------------------------------------------
// Консольный поиск (CLI)
// ----------------------------------------------------------------------------
static int RunCliSearch(const std::wstring& query, const std::wstring& dir, bool recursive, bool matchCase) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hOut || hOut == INVALID_HANDLE_VALUE) {
        AllocConsole();
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    std::wstring header = L"\n--- Search query: \"" + query + L"\" in \"" + dir + L"\" ---\n\n";
    DWORD dw;
    WriteConsoleW(hOut, header.c_str(), (DWORD)header.length(), &dw, NULL);

    SearchThreadParams params;
    params.hWndNotify = NULL;
    params.searchDir = dir;
    params.query = query;
    params.recursive = recursive;
    params.matchCase = matchCase;
    params.exactPhrase = false;

    DWORD scanned = 0;
    DWORD found = 0;
    g_bCancelSearch.store(false);
    g_searchResults.clear();

    SearchDirectoryRecursive(dir, params, scanned, found);

    for (const auto& item : g_searchResults) {
        std::wstring line = L"[FOUND] " + item.filePath + L"\n   Comment: " + item.snippet + L"\n\n";
        WriteConsoleW(hOut, line.c_str(), (DWORD)line.length(), &dw, NULL);
    }

    std::wstring summary = L"Scanned: " + std::to_wstring(scanned) + 
                           L", Found: " + std::to_wstring(found) + L"\n\n";
    WriteConsoleW(hOut, summary.c_str(), (DWORD)summary.length(), &dw, NULL);
    return 0;
}

// ----------------------------------------------------------------------------
// Точка входа WinMain
// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Загрузка кастомной иконки (ID 1 из resource.rc)
    g_hAppIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!g_hAppIcon) g_hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!g_hAppIcon) g_hAppIcon = LoadIconW(NULL, IDI_APPLICATION);

    g_hAppIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!g_hAppIconSmall) g_hAppIconSmall = g_hAppIcon;

    // Загрузка сохраненных настроек (язык, хоткей)
    LoadAppSettings();

    // Парсинг аргументов командной строки
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startInTray = false;

    if (argc >= 2) {
        std::wstring arg1 = argv[1];

        if (arg1 == L"--install-context") {
            bool ok = InstallContextMenu();
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--uninstall-context") {
            bool ok = UninstallContextMenu();
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--tray") {
            startInTray = true;
        } else if (arg1 == L"--no-tray") {
            g_bEnableTrayIcon = false;
        } else if (arg1 == L"--export" && argc >= 3) {
            std::wstring jsonPath = argv[2];
            std::wstring scanDir = L".";
            for (int i = 3; i < argc; i++) {
                if (std::wstring(argv[i]) == L"--dir" && i + 1 < argc) {
                    scanDir = argv[++i];
                }
            }
            DWORD scanned = 0, exported = 0;
            bool ok = ExportCommentsToJson(scanDir, jsonPath, scanned, exported);
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--import" && argc >= 3) {
            std::wstring jsonPath = argv[2];
            DWORD total = 0, restored = 0, errors = 0;
            bool ok = ImportCommentsFromJson(jsonPath, total, restored, errors);
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--search-gui") {
            std::wstring searchDir;
            if (argc >= 3) searchDir = argv[2];
            LocalFree(argv);

            INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
            InitCommonControlsEx(&icex);

            OpenSearchWindow(NULL, searchDir);

            MSG msg;
            while (GetMessageW(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            return (int)msg.wParam;
        } else if (arg1 == L"--search" && argc >= 3) {
            std::wstring query = argv[2];
            std::wstring searchDir = L".";
            bool recursive = false;
            bool matchCase = false;

            for (int i = 3; i < argc; i++) {
                std::wstring opt = argv[i];
                if (opt == L"--dir" && i + 1 < argc) {
                    searchDir = argv[++i];
                } else if (opt == L"--recursive" || opt == L"-r") {
                    recursive = true;
                } else if (opt == L"--match-case" || opt == L"-c") {
                    matchCase = true;
                }
            }

            int code = RunCliSearch(query, searchDir, recursive, matchCase);
            LocalFree(argv);
            return code;
        } else if (arg1 == L"--get" && argc >= 3) {
            std::wstring fpath = argv[2];
            std::wstring comment;
            bool ok = ReadComment(fpath, comment);
            if (ok) {
                HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD dwWritten = 0;
                WriteConsoleW(hStdOut, comment.c_str(), (DWORD)comment.length(), &dwWritten, NULL);
            }
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--set" && argc >= 4) {
            std::wstring fpath = argv[2];
            std::wstring comment = argv[3];
            bool ok = WriteComment(fpath, comment);
            LocalFree(argv);
            return ok ? 0 : 1;
        } else if (arg1 == L"--remove" && argc >= 3) {
            std::wstring fpath = argv[2];
            bool ok = DeleteComment(fpath);
            LocalFree(argv);
            return ok ? 0 : 1;
        } else {
            // Запуск из контекстного меню с путём к файлу: FileCommentEditor.exe "C:\test.txt"
            g_currentFilePath = arg1;
        }
    }

    if (argv) LocalFree(argv);

    // Инициализация общих элементов управления
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // Регистрация класса окна
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FileCommentEditorWndClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = g_hAppIcon;
    wc.hIconSm = g_hAppIconSmall;
    RegisterClassExW(&wc);

    // Создаем окно по центру экрана
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 710;
    int winH = 595;
    int winX = (screenW - winW) / 2;
    int winY = (screenH - winH) / 2;

    HWND hWnd = CreateWindowExW(
        0,
        L"FileCommentEditorWndClass",
        GetStr(STR_APP_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    g_hMainWnd = hWnd;

    if (!startInTray) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    } else {
        // Запущен с автостартом — работаем скрытно в трее
        ShowTrayBalloon(hWnd, GetStr(STR_TRAY_BALLOON_TITLE), GetStr(STR_TRAY_BALLOON_TEXT));
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
