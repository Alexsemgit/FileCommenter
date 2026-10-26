# 🏷️ FileCommenter

> **Native file notes in Windows Explorer infotips & instant full-text search engine (NTFS Alternate Data Streams)**
> 
> *Всплывающие заметки к файлам и мгновенный полнотекстовый поиск в Проводнике Windows 10 & 11.*

[![Platform](https://shields.io)](https://github.com)
[![Architecture](https://shields.io)]()
[![C++](https://shields.io)](https://cppreference.com)
[![License](https://shields.io)](LICENSE)
[![Installer](https://shields.io)]()

---

## 🌟 Features

* **💬 Native Infotips for FILES and FOLDERS:**  
  Hover your mouse cursor over any file or **folder** — Windows Explorer will display your custom note directly in the tooltip window via a COM In-Process Shell Extension (`IQueryInfo`).
* **📁 Full Folder (Directory) Comment Support:**  
  Folders in the NTFS file system fully support alternate data streams (`Folder:comment`). By utilizing the `FILE_FLAG_BACKUP_SEMANTICS` flag, the application allows reading, saving, and editing directory notes just as easily as regular files. The "Browse..." button supports selecting both individual files and entire folders.
* **⚡ Instant Full-Text Search (NTFS Search Engine):**  
  * Search notes across both files and folders.
  * Search by individual words or exact phrases.
  * Trigger scans using the "Find" button or by pressing **Enter**.
  * Case-insensitive support for Cyrillic and Latin characters via Win32 `CharLowerBuffW`.
  * Multi-line tooltip showing the complete note text when hovering over search results.
  * **Double-click** or right-click &rarr; **"Open file location"** opens Explorer and highlights the found file or folder (`explorer.exe /select,"<path>"`).
* **🖱️ Explorer Context Menu integration:**  
  * **"Comment..."** item for files (`*`), folders (`Directory`, `Folder`), and drives (`Drive`).
  * **"Search in notes..."** item for folders, drives, and empty directory backgrounds (`Background`).
  * Multilingual `MUIVerb` — menu items automatically adapt to English or Russian based on system language settings!
* **📦 1-Click Installer (`setup_comment.exe`):**  
  * Single standalone executable with zero external dependencies.
  * Suggests default folder (`C:\Program Files\FileCommenter` / `%LOCALAPPDATA%`).
  * "Browse..." button to select any custom installation path.
  * Optional toggles: context menu integration, DLL registration, Desktop/Start Menu shortcuts, and startup launch.
  * Registers in Windows Add/Remove Programs for seamless uninstallation.
* **🌐 Multilingual UI (EN / RU):**  
  Automatic Windows language detection with a 1-click manual switch shortcut.
* **⌨️ Global Hotkeys & System Tray Agent:**  
  Customizable shortcut (default: `Win + Shift + C`) to instantly call the search interface or editor from any active application.

---

## 💾 How It Works: NTFS Alternate Data Streams Technology

FileCommenter stores notes inside the **NTFS Alternate Data Stream** (`filename.ext:comment`):
1. **Original File Remains Intact:** The checksum (MD5/SHA256) and size of the primary file remain completely unchanged. No bytes of the original file are ever modified.
2. **No External Databases:** The note is permanently bound to the file itself. Renaming the file or moving it across folders on the same or any other NTFS drive preserves the attached note.
3. **Safety & Fallbacks:** If the file is copied to FAT32/exFAT partitions or transmitted over a network that drops streams, the primary file remains completely untouched in its pristine form.

---

## 📥 Quick Start / Installation

### Option 1. Standalone Installer (Recommended)
1. Download **`setup_comment.exe`**.
2. Run the executable.
3. Modify the installation path via "Browse..." if necessary and click **"Install"**.
4. Done! All components are automatically registered in the system.

### Option 2. Portable Mode
1. Extract `FileCommentEditor.exe`, `FileCommentTip.dll`, and `app_icon.ico` to a preferred directory.
2. Run `install.bat` as Administrator.

---

## 🛠️ Building from Source

The project has zero external dependencies, relying entirely on pure Win32 API and C++17.

### Building via Microsoft Visual Studio (MSVC)
1. Open the **Developer Command Prompt for Visual Studio** (x64 Native Tools).
2. Navigate to the project directory and run:
   ```cmd
   build_msvc.bat
   ```
The script compiles:
* `setup_comment.exe` — All-in-one standalone graphical installer with embedded resources.
* `FileCommentEditor.exe` — Graphical editor, search engine, and system tray agent.
* `FileCommentTip.dll` — 64-bit COM In-Process Explorer tooltip shell extension.
* `TestHarness.exe` — Console environment for debugging the `IQueryInfo` interface.

### Building via MinGW-w64 (GCC)
If you have the GCC compiler for Windows (`g++.exe`) installed, run:
```cmd
build_mingw.bat
```

---

## 🖥️ Command Line Interface (CLI) Usage

```cmd
:: Full-text search for notes within a directory and its subdirectories
FileCommentEditor.exe --search "contract" --dir "C:\Work" --recursive

:: Open the search GUI targeted at a specific directory
FileCommentEditor.exe --search-gui "C:\Projects"

:: Read a file comment
FileCommentEditor.exe "C:\Docs\report.docx" --get

:: Set or update a file comment
FileCommentEditor.exe "C:\Docs\report.docx" --set "Final version, approved by Director"

:: Remove a comment
FileCommentEditor.exe "C:\Docs\report.docx" --remove

:: Launch the system tray agent
FileCommentEditor.exe --tray
```

---

## 🗑️ Uninstall

* Via standard Windows panel: **Settings &rarr; Apps &rarr; Installed Apps &rarr; FileCommenter &rarr; Uninstall**.
* Or run the **`uninstall.bat`** file as Administrator.

---

## 📁 GitHub Repository Structure

Используйте код с осторожностью.
FileCommenter/
├── .github/
│   └── workflows/
│       └── build.yml           # CI/CD automated build for GitHub Actions (MSVC x64)
├── .gitignore                  # Excludes compiler and IDE temporary files
├── LICENSE                     # MIT License
├── README.md                   # Project documentation
├── CONTRIBUTING.md             # Contribution guidelines
├── app_icon.ico                # Vector brand application icon
├── resource.rc                 # Editor resources (icon + EN/RU MUIVerb strings)
├── setup_resource.rc           # Installer resources (embedded components)
├── FileCommentEditor.cpp       # Source code for GUI editor, search, and tray agent
├── FileCommentTip.cpp          # COM In-Process Shell Extension (IQueryInfo)
├── FileCommentTip.def          # COM DLL exports (DllGetClassObject, DllRegisterServer)
├── setup_comment.cpp           # Source code for the standalone graphical installer
├── TestHarness.cpp             # DLL test harness bench for debugging
├── build_msvc.bat              # Build script for Visual Studio 2019/2022
├── build_mingw.bat             # Build script for MinGW GCC
├── install.bat                 # Manual Windows component registration script
└── uninstall.bat               # Deregistration and cleanup script

---

## 📄 License

This project is licensed under the [MIT License](LICENSE). Free for both personal and commercial use.

---
---

## 🌟 Основные возможности (Russian version)

* **💬 Нативные всплывающие подсказки (Infotips) для ФАЙЛОВ и ПАПОК:**  
  Наведите курсор мыши на любой файл или **папку** — Windows Explorer покажет текст вашей заметки прямо во всплывающем окне (COM In-Process Shell Extension `IQueryInfo`).
* **📁 Полная поддержка комментирования папок (Каталогов):**  
  Папки в файловой системе NTFS также полноценно поддерживают альтернативные потоки данных (`Folder:comment`). Благодаря использованию флага `FILE_FLAG_BACKUP_SEMANTICS` программа позволяет читать, сохранять и редактировать заметки к каталогам так же легко, как и к обычным файлам. Кнопка «Обзор...» позволяет выбирать как отдельные файлы, так и целые папки.
* **⚡ Мгновенный полнотекстовый поиск (NTFS Search Engine):**  
  * Поиск заметок как среди файлов, так и среди папок.
  * Поиск по отдельным словам или точным фразам.
  * Запуск сканирования кнопкой «Найти» или нажатием клавиши **Enter**.
  * Поддержка кириллицы и латиницы без учета регистра (Win32 `CharLowerBuffW`).
  * Многострочный тултип с полным текстом заметки при наведении на строку в результатах.
  * **Двойной клик** или правый клик &rarr; **«Расположение файла»** открывает Проводник и подсвечивает найденный файл или папку (`explorer.exe /select,"<path>"`).
* **🖱️ Контекстное меню Проводника (Right-Click Context Menu):**  
  * Пункт **«Комментировать...»** для файлов (`*`), папок (`Directory`, `Folder`) и дисков (`Drive`).
  * Пункт **«Искать в заметках...»** для папок, дисков и свободного места открытой папки (`Background`).
  * Мультиязычный `MUIVerb` — пункты автоматически отображаются на русском или английском в зависимости от языка системы!
* **📦 Установка в 1 клик (`setup_comment.exe`):**  
  * Единый автономный исполняемый файл без внешних зависимостей.
  * Предложение папки по умолчанию (`C:\Program Files\FileCommenter` / `%LOCALAPPDATA%`).
  * Кнопка выбора любой пользовательской папки («Обзор...»).
  * Выбор опций: добавление в контекстное меню, регистрация DLL, ярлыки на рабочем столе и в меню «Пуск», автозагрузка.
  * Регистрация в панели «Программы и компоненты» (Add/Remove Programs) Windows для удобного удаления.
* **🌐 Полная мультиязычность (RU / EN):**  
  Автоматическое определение языка Windows с возможностью ручного переключения в 1 клик.
* **⌨️ Глобальные горячие клавиши & Системный трей:**  
  Настраиваемая комбинация (по умолчанию `Win + Shift + C`) для моментального вызова поиска или редактора из любого приложения.

---

## 💾 Как это работает: Технология NTFS Alternate Data Streams

FileCommenter сохраняет заметки в **альтернативный поток данных NTFS** (`filename.ext:comment`):
1. **Основной файл не изменяется:** Контрольная сумма (MD5/SHA256) and размер основного файла остаются строго исходными. Никакие байты исходного файла не модифицируются.
2. **Без внешних баз данных:** Заметка «привязана» к самому файлу. При переименовании файла или перемещении между папками на том же или любом другом NTFS-диске заметка сохраняется вместе с файлом.
3. **Безопасность:** Если файл скопировать на FAT32/exFAT или отправить по сети без поддержки потоков, основной файл останется в первозданном виде.

---

## 📥 Быстрая установка

### Вариант 1. Автономный установщик (Рекомендуется)
1. Скачайте **`setup_comment.exe`**.
2. Запустите файл.
3. При необходимости измените папку установки через «Обзор...» и нажмите **«Установить»**.
4. Готово! Все компоненты зарегистрированы в системе.

### Вариант 2. Портативный режим (Portable)
1. Распакуйте файлы `FileCommentEditor.exe`, `FileCommentTip.dll`, `app_icon.ico` в удобную папку.
2. Запустите `install.bat` от имени администратора.

---

## 🛠️ Сборка из исходного кода (Building from Source)

В проекте нет сторонних зависимостей. Используется только чистый Win32 API и C++17.

### Сборка через Microsoft Visual Studio (MSVC)
1. Откройте **Developer Command Prompt for Visual Studio** (x64 Native Tools).
2. Перейдите в папку проекта и запустите:
   ```cmd
   build_msvc.bat
   ```
Скрипт скомпилирует:
* `setup_comment.exe` — автономный установщик «всё-в-одном» со встроенными ресурсами.
* `FileCommentEditor.exe` — графический редактор, движок поиска и агент системного трея.
* `FileCommentTip.dll` — 64-битное COM In-Process расширение тултипов Проводника.
* `TestHarness.exe` — консольный стенд отладки интерфейса `IQueryInfo`.

### Сборка через MinGW-w64 (GCC)
Если у вас установлен компилятор GCC для Windows (`g++.exe`):
```cmd
build_mingw.bat
```

---

## 🖥️ Использование из командной строки (CLI)

```cmd
:: Полнотекстовый поиск по заметкам в папке и её подпапках
FileCommentEditor.exe --search "договор" --dir "C:\Work" --recursive

:: Открыть графическое окно поиска для конкретной папки
FileCommentEditor.exe --search-gui "C:\Projects"

:: Прочитать комментарий файла
FileCommentEditor.exe "C:\Docs\report.docx" --get

:: Задать или обновить комментарий
FileCommentEditor.exe "C:\Docs\report.docx" --set "Финальная версия, согласовано с директором"

:: Удалить комментарий
FileCommentEditor.exe "C:\Docs\report.docx" --remove

:: Запустить агент в системном трее
FileCommentEditor.exe --tray
```

---

## 🗑️ Удаление (Uninstall)

* Через стандартную панель Windows: **Параметры &rarr; Приложения &rarr; Установленные приложения &rarr; FileCommenter &rarr; Удалить**.
* Или запустите файл **`uninstall.bat`** от имени администратора.

---

## 📁 Структура репозитория для GitHub

FileCommenter/
├── .github/
│   └── workflows/
│       └── build.yml           # Автосборка CI/CD для GitHub Actions (MSVC x64)
├── .gitignore                  # Исключение временных файлов компиляторов и IDE
├── LICENSE                     # Лицензия MIT
├── README.md                   # Документация проекта
├── CONTRIBUTING.md             # Руководство для участников
├── app_icon.ico                # Векторная фирменная иконка приложения
├── resource.rc                 # Ресурсы редактора (иконка + MUIVerb строки RU/EN)
├── setup_resource.rc           # Ресурсы установщика (встраивание компонентов)
├── FileCommentEditor.cpp       # Исходный код GUI редактора, поиска и трея
├── FileCommentTip.cpp          # COM In-Process Shell Extension (IQueryInfo)
├── FileCommentTip.def          # Экспорты COM DLL (DllGetClassObject, DllRegisterServer)
├── setup_comment.cpp           # Исходный код единого графического инсталлятора
├── TestHarness.cpp             # Тестовый пускатель DLL для отладки
├── build_msvc.bat              # Скрипт сборки под Visual Studio 2019/2022
├── build_mingw.bat             # Скрипт сборки под MinGW GCC
├── install.bat                 # Ручная регистрация компонентов в Windows
└── uninstall.bat               # Снятие регистрации и удаление
