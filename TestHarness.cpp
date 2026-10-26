// ============================================================================
// TestHarness.cpp — Автономный стенд тестирования Shell-расширения подсказок
// 
// Назначение:
//   Позволяет разработчику тестировать и отлаживать DLL тултипов (FileCommentTip.dll)
//   нажатием одной кнопки F5 в Visual Studio БЕЗ запуска и перезапуска explorer.exe!
// 
// Использование:
//   TestHarness.exe "C:\Path\To\TestFile.txt"
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>

// CLSID расширения: {B4C83F12-88E4-48FE-9509-E83BE3222F09}
static const GUID CLSID_FileCommentTip = 
    { 0xb4c83f12, 0x88e4, 0x48fe, { 0x95, 0x09, 0xe8, 0x3b, 0xe3, 0x22, 0x2f, 0x09 } };

int wmain(int argc, wchar_t* argv[]) {
    // Включаем UTF-8 вывод в консоли
    SetConsoleOutputCP(CP_UTF8);

    std::wcout << L"=====================================================" << std::endl;
    std::wcout << L"  Тестовый стенд FileCommentTip Shell Extension" << std::endl;
    std::wcout << L"=====================================================" << std::endl;

    if (argc < 2) {
        std::wcout << L"Использование: TestHarness.exe <путь_к_файлу>" << std::endl;
        std::wcout << L"Пример: TestHarness.exe \"C:\\Users\\User\\Desktop\\document.docx\"" << std::endl;
        return 1;
    }

    std::wstring testFile = argv[1];
    std::wcout << L"Проверяемый файл: " << testFile << std::endl;

    // 1. Инициализация COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcout << L"[ОШИБКА] Не удалось инициализировать COM: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    // 2. Создание экземпляра нашего COM-объекта
    IPersistFile* pPersistFile = NULL;
    hr = CoCreateInstance(
        CLSID_FileCommentTip,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IPersistFile,
        (void**)&pPersistFile
    );

    if (FAILED(hr)) {
        std::wcout << L"[ОШИБКА] CoCreateInstance вернул ошибку 0x" << std::hex << hr << std::endl;
        std::wcout << L"Убедитесь, что FileCommentTip.dll зарегистрирована в системе через regsvr32!" << std::endl;
        CoUninitialize();
        return 1;
    }

    std::wcout << L"[OK] COM-объект успешно создан." << std::endl;

    // 3. Загружаем путь к файлу через IPersistFile
    hr = pPersistFile->Load(testFile.c_str(), STGM_READ);
    if (FAILED(hr)) {
        std::wcout << L"[ОШИБКА] IPersistFile::Load вернул ошибку 0x" << std::hex << hr << std::endl;
        pPersistFile->Release();
        CoUninitialize();
        return 1;
    }

    // 4. Запрашиваем интерфейс IQueryInfo
    IQueryInfo* pQueryInfo = NULL;
    hr = pPersistFile->QueryInterface(IID_IQueryInfo, (void**)&pQueryInfo);
    pPersistFile->Release();

    if (FAILED(hr)) {
        std::wcout << L"[ОШИБКА] QueryInterface(IID_IQueryInfo) не удался: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 5. Вызываем GetInfoTip
    WCHAR* pszTip = NULL;
    hr = pQueryInfo->GetInfoTip(0, &pszTip);
    pQueryInfo->Release();

    if (hr == S_OK && pszTip != NULL) {
        std::wcout << L"\n[РЕЗУЛЬТАТ] Сформирован тултип для Проводника:\n" << std::endl;
        std::wcout << pszTip << std::endl;
        std::wcout << L"\n=====================================================" << std::endl;
        CoTaskMemFree(pszTip);
    } else if (hr == S_FALSE) {
        std::wcout << L"\n[S_FALSE] У файла нет комментария :comment. Проводник покажет стандартный тултип." << std::endl;
    } else {
        std::wcout << L"[ОШИБКА] GetInfoTip вернул hr = 0x" << std::hex << hr << std::endl;
    }

    CoUninitialize();
    return 0;
}
