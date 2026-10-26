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

// ============================================================================
// Класс CFileCommentTip: реализует IQueryInfo и IPersistFile
// ============================================================================
class CFileCommentTip : public IQueryInfo, public IPersistFile {
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

        // Читаем комментарий из потока :comment
        std::wstring commentText;
        bool hasComment = ReadNTFSComment(m_filePath, commentText);

        // Если у файла нет нашего комментария, возвращаем S_FALSE:
        // Проводник покажет свой родной, нетронутый тултип!
        if (!hasComment || commentText.empty()) {
            return S_FALSE;
        }

        // Если комментарий есть — формируем объединенный красивый тултип:
        // 1. Свойства файла (тип, размер, дата изменения)
        // 2. Визуальный разделитель
        // 3. Комментарий
        std::wstring tipResult;

        // Получаем тип файла
        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(m_filePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_TYPENAME)) {
            if (wcslen(sfi.szTypeName) > 0) {
                tipResult += L"Тип: ";
                tipResult += sfi.szTypeName;
                tipResult += L"\n";
            }
        }

        // Получаем размер и дату изменения файла
        WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };
        if (GetFileAttributesExW(m_filePath.c_str(), GetFileExInfoStandard, &fad)) {
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
                    StringCchPrintfW(szDate, ARRAYSIZE(szDate), L"Изменен: %02d.%02d.%04d %02d:%02d",
                        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
                    tipResult += szDate;
                    tipResult += L"\n";
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
