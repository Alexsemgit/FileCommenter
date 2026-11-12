// ============================================================================
// FileCommentTip.cpp — In-Process COM Shell InfoTip Handler для Проводника Windows
// 
// Назначение:
//   Отображает всплывающую подсказку (тултип) при наведении курсора на файл
//   в Проводнике Windows. Читает комментарий из альтернативного потока данных
//   NTFS (<ИмяФайла>:comment) и объединяет его со стандартной системной
//   информацией (тип файла, размер, дата изменения).
// 
// Интерфейсы:
//   - IQueryInfo (GUID: {00021500-0000-0000-C000-000000000046})
//   - IPersistFile (GUID: {0000010b-0000-0000-C000-000000000046})
//   - IClassFactory
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <new>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// CLSID расширения: {B4C83F12-88E4-48FE-9509-E83BE3222F09}
static const GUID CLSID_FileCommentTip = 
    { 0xb4c83f12, 0x88e4, 0x48fe, { 0x95, 0x09, 0xe8, 0x3b, 0xe3, 0x22, 0x2f, 0x09 } };

static const wchar_t* CLSID_STRING = L"{B4C83F12-88E4-48FE-9509-E83BE3222F09}";
static const wchar_t* HANDLER_FRIENDLY_NAME = L"FileCommentTip InfoTip Shell Extension";

// Глобальный счетчик активных ссылок COM-сервера
static LONG g_cDllRef = 0;
// Дескриптор загруженного модуля DLL
static HINSTANCE g_hInst = NULL;

// Вспомогательная функция форматирования размера файла
static void FormatFileSize(ULONGLONG bytes, wchar_t* outBuf, size_t bufSize) {
    if (bytes < 1024) {
        StringCchPrintfW(outBuf, bufSize, L"%llu Б", bytes);
    } else if (bytes < 1024 * 1024) {
        double kb = (double)bytes / 1024.0;
        StringCchPrintfW(outBuf, bufSize, L"%.1f КБ (%llu байт)", kb, bytes);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        StringCchPrintfW(outBuf, bufSize, L"%.2f МБ", mb);
    } else {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        StringCchPrintfW(outBuf, bufSize, L"%.2f ГБ", gb);
    }
}

// Вспомогательная функция нормализации пути для потока NTFS
static std::wstring NormalizePathForADS(const std::wstring& path) {
    std::wstring p = path;
    while (p.length() > 3 && (p.back() == L'\\' || p.back() == L'/')) {
        p.pop_back();
    }
    return p;
}

// Вспомогательная функция чтения комментария из потока NTFS :comment
// Поддерживает как обычные файлы, так и папки (каталоги) благодаря FILE_FLAG_BACKUP_SEMANTICS
static bool ReadNTFSComment(const std::wstring& filePath, std::wstring& outComment) {
    outComment.clear();
    std::wstring adsPath = NormalizePathForADS(filePath) + L":comment";

    HANDLE hFile = CreateFileW(
        adsPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwSizeHigh = 0;
    DWORD dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
    if (dwSizeLow == 0 || dwSizeLow == INVALID_FILE_SIZE || dwSizeHigh > 0 || dwSizeLow > 65536) {
        // Ограничиваем комментарий 64 КБ ради безопасности
        CloseHandle(hFile);
        return false;
    }

    std::vector<BYTE> buffer(dwSizeLow + 4, 0);
    DWORD dwRead = 0;
    BOOL bSuccess = ReadFile(hFile, buffer.data(), dwSizeLow, &dwRead, NULL);
    CloseHandle(hFile);

    if (!bSuccess || dwRead == 0) {
        return false;
    }

    // Проверяем кодировку: UTF-16 LE с BOM (0xFF, 0xFE)
    if (dwRead >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
        const wchar_t* wstr = reinterpret_cast<const wchar_t*>(buffer.data() + 2);
        size_t wlen = (dwRead - 2) / sizeof(wchar_t);
        outComment.assign(wstr, wlen);
    }
    // Проверяем UTF-8 с BOM (0xEF, 0xBB, 0xBF)
    else if (dwRead >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data() + 3, dwRead - 3, NULL, 0);
        if (wlen > 0) {
            outComment.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data() + 3, dwRead - 3, &outComment[0], wlen);
        }
    }
    // Пробуем как стандартный UTF-8 без BOM
    else {
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)buffer.data(), dwRead, NULL, 0);
        if (wlen > 0) {
            outComment.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, (const char*)buffer.data(), dwRead, &outComment[0], wlen);
        } else {
            // Фолбэк на системную кодировку ANSI/CP1251
            wlen = MultiByteToWideChar(CP_ACP, 0, (const char*)buffer.data(), dwRead, NULL, 0);
            if (wlen > 0) {
                outComment.resize(wlen);
                MultiByteToWideChar(CP_ACP, 0, (const char*)buffer.data(), dwRead, &outComment[0], wlen);
            }
        }
    }

    // Удаляем концевые пробелы и переносы
    while (!outComment.empty() && (outComment.back() == L'\r' || outComment.back() == L'\n' || outComment.back() == L' ')) {
        outComment.pop_back();
    }

    return !outComment.empty();
}

// Вспомогательная функция чтения InfoTip из desktop.ini (для папок в панели навигации и Быстром доступе)
static bool ReadDesktopIniComment(const std::wstring& folderPath, std::wstring& outComment) {
    outComment.clear();
    std::wstring iniPath = NormalizePathForADS(folderPath) + L"\\desktop.ini";
    DWORD dwAttr = GetFileAttributesW(iniPath.c_str());
    if (dwAttr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    wchar_t buf[4096] = { 0 };
    DWORD readChars = GetPrivateProfileStringW(
        L".ShellClassInfo",
        L"InfoTip",
        L"",
        buf,
        ARRAYSIZE(buf),
        iniPath.c_str()
    );

    if (readChars > 0) {
        outComment.assign(buf, readChars);
        while (!outComment.empty() && (outComment.back() == L'\r' || outComment.back() == L'\n' || outComment.back() == L' ')) {
            outComment.pop_back();
        }
        return !outComment.empty();
    }
    return false;
}

// Вспомогательная функция разрешения ярлыков .lnk (для секций «Недавние», «Быстрый доступ» и Рабочего стола)
static bool ResolveShortcut(const std::wstring& lnkPath, std::wstring& outTargetPath) {
    outTargetPath.clear();

    bool bNeedUninit = false;
    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hrInit)) {
        bNeedUninit = true;
    }

    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Load(lnkPath.c_str(), STGM_READ);
            if (SUCCEEDED(hr)) {
                // Разрешаем ярлык без UI и без поиска по сети
                psl->Resolve(NULL, SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK | SLR_NOLINKINFO);

                wchar_t szTarget[MAX_PATH] = { 0 };
                WIN32_FIND_DATAW wfd = { 0 };
                hr = psl->GetPath(szTarget, MAX_PATH, &wfd, SLGP_RAWPATH);
                if (SUCCEEDED(hr) && wcslen(szTarget) > 0) {
                    outTargetPath = szTarget;
                }
            }
            ppf->Release();
        }
        psl->Release();
    }

    if (bNeedUninit) {
        CoUninitialize();
    }

    return !outTargetPath.empty();
}

// ============================================================================
// Класс CFileCommentTip: реализует IQueryInfo, IPersistFile и IShellExtInit
// ============================================================================
class CFileCommentTip : public IQueryInfo, public IPersistFile, public IShellExtInit {
private:
    LONG m_cRef;
    std::wstring m_filePath;

public:
    CFileCommentTip() : m_cRef(1) {
        InterlockedIncrement(&g_cDllRef);
    }

    virtual ~CFileCommentTip() {
        InterlockedDecrement(&g_cDllRef);
    }

    // --- Методы IUnknown ---
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;

        if (IsEqualIID(riid, IID_IUnknown)) {
            *ppv = static_cast<IUnknown*>(static_cast<IQueryInfo*>(this));
        } else if (IsEqualIID(riid, IID_IQueryInfo)) {
            *ppv = static_cast<IQueryInfo*>(this);
        } else if (IsEqualIID(riid, IID_IPersistFile)) {
            *ppv = static_cast<IPersistFile*>(this);
        } else if (IsEqualIID(riid, IID_IPersist)) {
            *ppv = static_cast<IPersist*>(this);
        } else if (IsEqualIID(riid, IID_IShellExtInit)) {
            *ppv = static_cast<IShellExtInit*>(this);
        } else {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // --- Методы IShellExtInit (КРИТИЧНО ДЛЯ «БЫСТРОГО ДОСТУПА», «НЕДАВНИХ» И НАВИГАЦИОННОЙ ПАНЕЛИ) ---
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE /*pidlFolder*/, IDataObject* pdtobj, HKEY /*hkeyProgID*/) override {
        if (!pdtobj) return E_INVALIDARG;

        // 1. Пробуем извлечь путь через CF_HDROP
        FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium = { 0 };
        if (SUCCEEDED(pdtobj->GetData(&fe, &medium))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
            if (hDrop) {
                wchar_t szPath[MAX_PATH] = { 0 };
                if (DragQueryFileW(hDrop, 0, szPath, ARRAYSIZE(szPath)) > 0) {
                    m_filePath = szPath;
                }
                GlobalUnlock(medium.hGlobal);
            }
            ReleaseStgMedium(&medium);
            if (!m_filePath.empty()) return S_OK;
        }

        // 2. Если CF_HDROP нет (виртуальные представления Windows 11 Home / Quick Access),
        // пробуем получить через IShellItemArray
        IShellItemArray* pItemArray = NULL;
        if (SUCCEEDED(SHCreateShellItemArrayFromDataObject(pdtobj, IID_IShellItemArray, (void**)&pItemArray))) {
            DWORD count = 0;
            pItemArray->GetCount(&count);
            if (count > 0) {
                IShellItem* pItem = NULL;
                if (SUCCEEDED(pItemArray->GetItemAt(0, &pItem))) {
                    LPWSTR pszPath = NULL;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                        m_filePath = pszPath;
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pItemArray->Release();
        }

        return m_filePath.empty() ? E_FAIL : S_OK;
    }

    // --- Методы IPersistFile ---
    STDMETHODIMP GetClassID(CLSID* pClassID) override {
        if (!pClassID) return E_POINTER;
        *pClassID = CLSID_FileCommentTip;
        return S_OK;
    }

    STDMETHODIMP IsDirty() override {
        return S_FALSE;
    }

    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD /*dwMode*/) override {
        if (!pszFileName) return E_INVALIDARG;
        m_filePath = pszFileName;
        return S_OK;
    }

    STDMETHODIMP Save(LPCOLESTR /*pszFileName*/, BOOL /*fRemember*/) override {
        return E_NOTIMPL;
    }

    STDMETHODIMP SaveCompleted(LPCOLESTR /*pszFileName*/) override {
        return E_NOTIMPL;
    }

    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName) override {
        if (!ppszFileName) return E_POINTER;
        if (m_filePath.empty()) return E_FAIL;

        size_t cch = m_filePath.length() + 1;
        *ppszFileName = static_cast<LPOLESTR>(CoTaskMemAlloc(cch * sizeof(wchar_t)));
        if (!*ppszFileName) return E_OUTOFMEMORY;

        StringCchCopyW(*ppszFileName, cch, m_filePath.c_str());
        return S_OK;
    }

    // --- Методы IQueryInfo ---
    STDMETHODIMP GetInfoFlags(DWORD* pdwFlags) override {
        if (!pdwFlags) return E_POINTER;
        *pdwFlags = 0;
        return S_OK;
    }

    STDMETHODIMP GetInfoTip(DWORD /*dwFlags*/, WCHAR** ppwszTip) override {
        if (!ppwszTip) return E_POINTER;
        *ppwszTip = NULL;

        if (m_filePath.empty()) {
            return S_FALSE; // Передаем управление стандартному обработчику Проводника
        }

        std::wstring effectivePath = m_filePath;
        std::wstring shortcutTarget;
        bool isShortcut = false;

        // В секциях «Недавние» (Recent) и «Быстрый доступ» элементы представлены ярлыками .lnk
        // Если это файл .lnk, разрешаем его к реальному целевому объекту:
        if (m_filePath.length() >= 4 && 
            _wcsicmp(m_filePath.c_str() + m_filePath.length() - 4, L".lnk") == 0) {
            if (ResolveShortcut(m_filePath, shortcutTarget)) {
                effectivePath = shortcutTarget;
                isShortcut = true;
            }
        }

        // 1. Проверяем комментарий в скрытом потоке NTFS (:comment)
        std::wstring commentText;
        bool hasComment = ReadNTFSComment(effectivePath, commentText);

        // 2. Если в потоке пусто, а объект является папкой — проверяем desktop.ini InfoTip
        if (!hasComment || commentText.empty()) {
            DWORD dwAttr = GetFileAttributesW(effectivePath.c_str());
            if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
                hasComment = ReadDesktopIniComment(effectivePath, commentText);
            }
        }

        // Если комментария нет, возвращаем S_FALSE:
        // Проводник покажет свой родной тултип (например, путь к файлу)
        if (!hasComment || commentText.empty()) {
            return S_FALSE;
        }

        // Формируем объединенный тултип:
        std::wstring tipResult;

        // Если это ярлык — указываем это и отображаем путь к цели
        if (isShortcut) {
            tipResult += L"Тип: Ярлык\n";
            if (!shortcutTarget.empty()) {
                tipResult += L"Расположение: ";
                tipResult += shortcutTarget;
                tipResult += L"\n";
            }
        } else {
            // Тип исходного файла
            SHFILEINFOW sfi = { 0 };
            if (SHGetFileInfoW(effectivePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_TYPENAME)) {
                if (wcslen(sfi.szTypeName) > 0) {
                    tipResult += L"Тип: ";
                    tipResult += sfi.szTypeName;
                    tipResult += L"\n";
                }
            }
        }

        // Получаем размер и дату изменения файла
        WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };
        if (GetFileAttributesExW(effectivePath.c_str(), GetFileExInfoStandard, &fad)) {
            // Размер (только для обычных файлов, не для папок)
            if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ULARGE_INTEGER uliSize;
                uliSize.LowPart = fad.nFileSizeLow;
                uliSize.HighPart = fad.nFileSizeHigh;
                wchar_t szSize[64];
                FormatFileSize(uliSize.QuadPart, szSize, ARRAYSIZE(szSize));
                tipResult += L"Размер: ";
                tipResult += szSize;
                tipResult += L"\n";
            }

            // Дата и время изменения
            FILETIME ftLocal;
            if (FileTimeToLocalFileTime(&fad.ftLastWriteTime, &ftLocal)) {
                SYSTEMTIME st;
                if (FileTimeToSystemTime(&ftLocal, &st)) {
                    wchar_t szDate[64];
                    StringCchPrintfW(szDate, ARRAYSIZE(szDate), L"Изменен: %02d.%02d.%04d %02d:%02d\n",
                        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
                    tipResult += szDate;
                }
            }
        }

        // Добавляем разделитель и сам комментарий
        tipResult += L"──────────────────────────────\n";
        tipResult += L"📝 Заметка: ";
        tipResult += commentText;

        // Выделяем память через CoTaskMemAlloc, как требует контракт Shell
        size_t cch = tipResult.length() + 1;
        *ppwszTip = static_cast<WCHAR*>(CoTaskMemAlloc(cch * sizeof(WCHAR)));
        if (!*ppwszTip) {
            return E_OUTOFMEMORY;
        }

        StringCchCopyW(*ppwszTip, cch, tipResult.c_str());
        return S_OK;
    }
};

// ============================================================================
// Фабрика классов COM (IClassFactory)
// ============================================================================
class CClassFactory : public IClassFactory {
private:
    LONG m_cRef;

public:
    CClassFactory() : m_cRef(1) {
        InterlockedIncrement(&g_cDllRef);
    }

    virtual ~CClassFactory() {
        InterlockedDecrement(&g_cDllRef);
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override {
        if (pUnkOuter != NULL) {
            return CLASS_E_NOAGGREGATION;
        }
        if (!ppvObject) {
            return E_POINTER;
        }
        *ppvObject = NULL;

        CFileCommentTip* pNewObj = new (std::nothrow) CFileCommentTip();
        if (!pNewObj) {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = pNewObj->QueryInterface(riid, ppvObject);
        pNewObj->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) {
            InterlockedIncrement(&g_cDllRef);
        } else {
            InterlockedDecrement(&g_cDllRef);
        }
        return S_OK;
    }
};

// ============================================================================
// Стандартные точки входа COM In-Process DLL
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow(void) {
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;

    if (!IsEqualCLSID(rclsid, CLSID_FileCommentTip)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    CClassFactory* pFactory = new (std::nothrow) CClassFactory();
    if (!pFactory) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

// ============================================================================
// Саморегистрация в реестре Windows (DllRegisterServer / DllUnregisterServer)
// ============================================================================

// Вспомогательная функция записи строкового ключа
static LONG SetRegistryValue(HKEY hKeyRoot, LPCWSTR subKey, LPCWSTR valueName, LPCWSTR data) {
    HKEY hKey;
    LONG lRes = RegCreateKeyExW(hKeyRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lRes == ERROR_SUCCESS) {
        lRes = RegSetValueExW(hKey, valueName, 0, REG_SZ, (const BYTE*)data, (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
    return lRes;
}

STDAPI DllRegisterServer(void) {
    wchar_t szModulePath[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szModulePath, ARRAYSIZE(szModulePath)) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t szClsidKey[MAX_PATH];
    StringCchPrintfW(szClsidKey, ARRAYSIZE(szClsidKey), L"Software\\Classes\\CLSID\\%s", CLSID_STRING);

    // 1. Регистрация CLSID
    SetRegistryValue(HKEY_LOCAL_MACHINE, szClsidKey, NULL, HANDLER_FRIENDLY_NAME);

    // InprocServer32
    wchar_t szInprocKey[MAX_PATH];
    StringCchPrintfW(szInprocKey, ARRAYSIZE(szInprocKey), L"%s\\InprocServer32", szClsidKey);
    SetRegistryValue(HKEY_LOCAL_MACHINE, szInprocKey, NULL, szModulePath);
    SetRegistryValue(HKEY_LOCAL_MACHINE, szInprocKey, L"ThreadingModel", L"Apartment");

    // Регистрация в HKLM и HKCU (для гарантии работы без повышенных прав)
    HKEY hives[2] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int h = 0; h < 2; h++) {
        HKEY root = hives[h];
        SetRegistryValue(root, szClsidKey, NULL, HANDLER_FRIENDLY_NAME);
        SetRegistryValue(root, szInprocKey, NULL, szModulePath);
        SetRegistryValue(root, szInprocKey, L"ThreadingModel", L"Apartment");

        // Регистрация для всех файлов (*)
        SetRegistryValue(root, 
            L"Software\\Classes\\*\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);

        // Регистрация для физических папок (Directory)
        SetRegistryValue(root, 
            L"Software\\Classes\\Directory\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);

        // Регистрация для логических папок Shell (Folder)
        SetRegistryValue(root, 
            L"Software\\Classes\\Folder\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);

        // Регистрация для дисков (Drive: C:, D:, E: и т.д.)
        SetRegistryValue(root, 
            L"Software\\Classes\\Drive\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);

        // Регистрация для всех объектов файловой системы (AllFilesystemObjects) - для плиток Быстрого доступа
        SetRegistryValue(root, 
            L"Software\\Classes\\AllFilesystemObjects\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);

        // Регистрация для ярлыков (.lnk / lnkfile) - КРИТИЧЕСКИ ВАЖНО ДЛЯ СЕКЦИИ «НЕДАВНИЕ» И «БЫСТРЫЙ ДОСТУП»!
        SetRegistryValue(root, 
            L"Software\\Classes\\lnkfile\\shellex\\{00021500-0000-0000-C000-000000000046}", 
            NULL, CLSID_STRING);
    }

    // Одобрение расширения в системе (Approved Shell Extensions)
    SetRegistryValue(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        CLSID_STRING, HANDLER_FRIENDLY_NAME);

    // Оповещаем Проводник об изменении системных ассоциаций
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    wchar_t szClsidKey[MAX_PATH];
    StringCchPrintfW(szClsidKey, ARRAYSIZE(szClsidKey), L"Software\\Classes\\CLSID\\%s", CLSID_STRING);

    // Удаляем ветки реестра в HKLM и HKCU
    HKEY hives[2] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int h = 0; h < 2; h++) {
        HKEY root = hives[h];
        RegDeleteTreeW(root, szClsidKey);
        RegDeleteKeyW(root, L"Software\\Classes\\*\\shellex\\{00021500-0000-0000-C000-000000000046}");
        RegDeleteKeyW(root, L"Software\\Classes\\Directory\\shellex\\{00021500-0000-0000-C000-000000000046}");
        RegDeleteKeyW(root, L"Software\\Classes\\Folder\\shellex\\{00021500-0000-0000-C000-000000000046}");
        RegDeleteKeyW(root, L"Software\\Classes\\Drive\\shellex\\{00021500-0000-0000-C000-000000000046}");
        RegDeleteKeyW(root, L"Software\\Classes\\AllFilesystemObjects\\shellex\\{00021500-0000-0000-C000-000000000046}");
        RegDeleteKeyW(root, L"Software\\Classes\\lnkfile\\shellex\\{00021500-0000-0000-C000-000000000046}");
    }

    // Удаляем из Approved
    HKEY hKeyApproved;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", 0, KEY_WRITE, &hKeyApproved) == ERROR_SUCCESS) {
        RegDeleteValueW(hKeyApproved, CLSID_STRING);
        RegCloseKey(hKeyApproved);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return S_OK;
}
