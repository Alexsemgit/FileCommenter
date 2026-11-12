# 🏷️ FileCommenter

> **File & folder notes in Windows Explorer infotips and instant full-text search (NTFS Alternate Data Streams)**
>
> *Всплывающие заметки к файлам и папкам в Проводнике Windows и мгновенный полнотекстовый поиск (NTFS ADS).*

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?logo=windows)](https://github.com)
[![Architecture](https://img.shields.io/badge/Architecture-x64-blue)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Installer](https://img.shields.io/badge/Installer-Single--File%20(setup__comment.exe)-brightgreen)]()

🌐 **Language:** **English** | [🇷🇺 Русская версия](#-русская-версия)

---

## 🌟 Features

* **💬 Native Explorer Infotips for FILES & FOLDERS:**  
  Hover over any commented file or **folder** on NTFS drives, on the Desktop, or inside Windows Explorer folders — Windows Explorer displays your note right inside the native tooltip (via the 64-bit COM In-Process Shell Extension `IQueryInfo`).
* **📁 Full Directory Commenting Support:**  
  NTFS directories natively support Alternate Data Streams (`Folder:comment`). By opening handles with `FILE_FLAG_BACKUP_SEMANTICS`, FileCommenter reads, writes, and edits folder comments just like regular files.
* **⚡ Instant Full-Text Search Engine (NTFS ADS):**  
  * Searches comments across both files and folders.
  * Supports word matching and exact-phrase queries.
  * Start scanning via the **Search** button or by pressing **Enter**.
  * Case-insensitive Unicode matching for both Latin and Cyrillic text (Win32 `CharLowerBuffW`).
  * Multi-line preview tooltip when hovering over search results.
  * **Double-click** or right-click &rarr; **Show in Explorer** opens Windows Explorer and highlights the target file or folder (`explorer.exe /select,"<path>"`).
* **🖱️ Windows Explorer Context Menu Integration:**  
  * **Comment...** on files (`*`), folders (`Directory`, `Folder`), shortcuts (`lnkfile`), and drives (`Drive`).
  * **Search in comments...** on folders, drives, and folder backgrounds (`Directory\Background`).
  * Clean Unicode UTF-16 menu labels with automatic `MuiCache` cleanup and instant shell refresh.
* **📦 Single-File Installer (`setup_comment.exe`):**  
  * Self-contained standalone executable (~1.6 MB) with embedded payloads (`FileCommentEditor.exe`, `FileCommentTip.dll`, icons, and uninstaller).
  * Built-in UAC manifest (`requireAdministrator`) for seamless installation into `C:\Program Files\FileCommenter` or any custom folder.
  * Automatically closes any running instance of `FileCommentEditor.exe` before updating files.
  * Registers with Windows **Installed Apps / Programs and Features** for clean removal.
* **🌐 Bilingual UI (EN / RU):**  
  Automatic Windows system language detection with a 1-click UI language toggle.
* **⌨️ Global Hotkeys & System Tray Agent:**  
  Customizable global shortcut (defaults to a conflict-free `Ctrl + Shift + F`, with an expanded key selector covering `A–Z`, `0–9`, `Space`, and `F1–F12`) to summon Search or the Editor from anywhere.

---

## 💾 How It Works: NTFS Alternate Data Streams

FileCommenter stores each note inside an **NTFS Alternate Data Stream** (`filename.ext:comment`):
1. **Zero modification to the original file:** File size, content, and cryptographic hashes (MD5/SHA-256) remain 100% untouched.
2. **No external database required:** Notes stay attached to the file or folder itself when you rename or move items within NTFS volumes.
3. **Safe fallback:** Copying a file to FAT32/exFAT or uploading it over a network strips only the `:comment` stream while keeping the original file completely intact.

---

## 📥 Quick Installation

### Option 1. Standalone Installer (Recommended)
1. Download **`setup_comment.exe`**.
2. Run it (the built-in UAC manifest will prompt for administrator privileges so it can write to `Program Files` and register Explorer integration).
3. Choose the installation directory and options, then click **Install**.

### Option 2. Portable / Manual Setup
1. Place `FileCommentEditor.exe`, `FileCommentTip.dll`, and `app_icon.ico` into a folder of your choice.
2. Run `install.bat` as Administrator.

---

## 🛠️ Building from Source

The project has **zero third-party dependencies** — pure Win32 API and C++17.

### Build with Microsoft Visual Studio (MSVC)
1. Open **Developer Command Prompt for Visual Studio** (x64 Native Tools).
2. Navigate to the repository directory and run:
   ```cmd
   build_msvc.bat
   ```
This compiles:
* `setup_comment.exe` — standalone all-in-one GUI installer with embedded resources.
* `FileCommentEditor.exe` — GUI editor, NTFS search engine, and system tray agent.
* `FileCommentTip.dll` — 64-bit COM In-Process Shell Extension (`IQueryInfo`).
* `TestHarness.exe` — console test harness for debugging `IQueryInfo`.

### Build with MinGW-w64 (GCC)
If you have MinGW-w64 (`g++.exe`, `windres.exe`) in your `PATH`:
```cmd
build_mingw.bat
```

---

## 🖥️ Command-Line Interface (CLI)

```cmd
:: Recursive full-text search across comments in a directory
FileCommentEditor.exe --search "contract" --dir "C:\Work" --recursive

:: Open the GUI search window scoped to a specific folder
FileCommentEditor.exe --search-gui "C:\Projects"

:: Read a file or folder comment
FileCommentEditor.exe "C:\Docs\report.docx" --get

:: Set or update a comment
FileCommentEditor.exe "C:\Docs\report.docx" --set "Final approved version"

:: Remove a comment
FileCommentEditor.exe "C:\Docs\report.docx" --remove

:: Start the background tray & hotkey agent
FileCommentEditor.exe --tray
```

---

## 🗑️ Uninstall

* Via Windows Settings: **Settings &rarr; Apps &rarr; Installed apps &rarr; FileCommenter &rarr; Uninstall**.
* Or run **`uninstall.bat`** as Administrator.

---

## 📁 Repository Structure

```text
FileCommenter/
├── .github/
│   └── workflows/
│       └── build.yml           # GitHub Actions CI/CD workflow (MSVC x64)
├── .gitignore                  # Build artifacts & IDE exclusions
├── LICENSE                     # MIT License
├── README.md                   # Project documentation (EN / RU)
├── CONTRIBUTING.md             # Contribution guidelines
├── app_icon.ico                # Application icon
├── resource.rc                 # Editor resources (application icon)
├── setup_resource.rc           # Installer resources (embedded binaries & manifest)
├── setup.manifest              # UAC manifest (requireAdministrator)
├── FileCommentEditor.cpp       # GUI editor, search engine & tray agent source
├── FileCommentTip.cpp          # COM In-Process Shell Extension (IQueryInfo)
├── FileCommentTip.def          # COM DLL exports (DllGetClassObject, DllRegisterServer)
├── setup_comment.cpp           # Standalone GUI installer source
├── TestHarness.cpp             # Console COM test harness
├── build_msvc.bat              # Visual Studio 2019/2022 build script
├── build_mingw.bat             # MinGW-w64 GCC build script
├── install.bat                 # Manual Explorer registration script
└── uninstall.bat               # Explorer unregistration & cleanup script
```

---

## 📄 License

Distributed under the [MIT License](LICENSE). Free for personal and commercial use.

---
---

## 🇷🇺 Русская версия

> **Всплывающие заметки к файлам и папкам в Проводнике Windows и мгновенный полнотекстовый поиск (NTFS Alternate Data Streams)**

[⬆️ Back to English](#-filecommenter)

### 🌟 Основные возможности

* **💬 Нативные всплывающие подсказки (Infotips) для ФАЙЛОВ и ПАПОК:**  
  Наведите курсор мыши на любой файл или **папку** на дисках NTFS, на Рабочем столе или внутри открытых папок Проводника — Windows Explorer покажет текст вашей заметки прямо во всплывающем окне (COM In-Process Shell Extension `IQueryInfo`).
* **📁 Полная поддержка комментирования папок (Каталогов):**  
  Папки в файловой системе NTFS полноценно поддерживают альтернативные потоки данных (`Folder:comment`). Благодаря использованию флага `FILE_FLAG_BACKUP_SEMANTICS` программа позволяет читать, сохранять и редактировать заметки к каталогам так же легко, как и к обычным файлам. Кнопка «Обзор...» позволяет выбирать как отдельные файлы, так и целые папки.
* **⚡ Мгновенный полнотекстовый поиск (NTFS Search Engine):**  
  * Поиск заметок как среди файлов, так и среди папок.
  * Поиск по отдельным словам или точным фразам.
  * Запуск сканирования кнопкой «Найти» или нажатием клавиши **Enter**.
  * Поддержка кириллицы и латиницы без учета регистра (Win32 `CharLowerBuffW`).
  * Многострочный тултип с полным текстом заметки при наведении на строку в результатах.
  * **Двойной клик** или правый клик &rarr; **«Расположение файла»** открывает Проводник и подсвечивает найденный файл или папку (`explorer.exe /select,"<path>"`).
* **🖱️ Контекстное меню Проводника (Right-Click Context Menu):**  
  * Пункт **«Комментировать...»** для файлов (`*`), папок (`Directory`, `Folder`), ярлыков (`lnkfile`) и дисков (`Drive`).
  * Пункт **«Искать в заметках...»** для папок, дисков и свободного места открытой папки (`Background`).
  * Чистая запись строк в Unicode UTF-16 с автоматической очисткой кэша `MuiCache` без кракозябр.
* **📦 Установка в 1 клик (`setup_comment.exe`):**  
  * Единый автономный исполняемый файл (~1.6 МБ) без внешних зависимостей.
  * Встроенный UAC-манифест (`requireAdministrator`) для беспроблемной записи в `C:\Program Files\FileCommenter`.
  * Автоматическое завершение работающего процесса `FileCommentEditor.exe` перед обновлением файлов.
  * Кнопка выбора любой пользовательской папки («Обзор...»), настройка ярлыков и автозагрузки.
  * Регистрация в панели «Установленные приложения / Программы и компоненты» Windows для удобного удаления.
* **🌐 Полная мультиязычность (RU / EN):**  
  Автоматическое определение языка Windows с возможностью ручного переключения в 1 клик.
* **⌨️ Глобальные горячие клавиши & Системный трей:**  
  Настраиваемая комбинация (по умолчанию свободная и безопасная `Ctrl + Shift + F`, расширенный список клавиш `A–Z`, `0–9`, `Space`, `F1–F12`) для моментального вызова поиска или редактора из любого приложения.

---

### 💾 Как это работает: Технология NTFS Alternate Data Streams

FileCommenter сохраняет заметки в **альтернативный поток данных NTFS** (`filename.ext:comment`):
1. **Основной файл не изменяется:** Контрольная сумма (MD5/SHA256) и размер основного файла остаются строго исходными. Никакие байты исходного файла не модифицируются.
2. **Без внешних баз данных:** Заметка «привязана» к самому файлу. При переименовании файла или перемещении между папками на NTFS-диске заметка сохраняется вместе с файлом.
3. **Безопасность:** Если файл скопировать на FAT32/exFAT или отправить по сети без поддержки потоков, основной файл останется в первозданном виде.

---

### 📥 Быстрая установка

#### Вариант 1. Автономный установщик (Рекомендуется)
1. Скачайте **`setup_comment.exe`**.
2. Запустите файл (встроенный UAC-манифест автоматически запросит права администратора для записи в `Program Files`).
3. При необходимости измените папку установки через «Обзор...» и нажмите **«Установить»**.
4. Готово! Все компоненты зарегистрированы в системе.

#### Вариант 2. Портативный режим (Portable)
1. Распакуйте файлы `FileCommentEditor.exe`, `FileCommentTip.dll`, `app_icon.ico` в удобную папку.
2. Запустите `install.bat` от имени администратора.

---

### 🛠️ Сборка из исходного кода

В проекте нет сторонних зависимостей — используется только чистый Win32 API и C++17.

* **Microsoft Visual Studio (MSVC):** откройте *Developer Command Prompt for Visual Studio (x64)* и выполните `build_msvc.bat`.
* **MinGW-w64 (GCC):** убедитесь, что `g++.exe` и `windres.exe` есть в `PATH`, и выполните `build_mingw.bat`.

---

### 🖥️ Использование из командной строки (CLI)

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

### 🗑️ Удаление

* Через стандартную панель Windows: **Параметры &rarr; Приложения &rarr; Установленные приложения &rarr; FileCommenter &rarr; Удалить**.
* Или запустите файл **`uninstall.bat`** от имени администратора.

---

### 📄 Лицензия

Проект распространяется под открытой лицензией [MIT License](LICENSE). Свободно для личного и коммерческого использования.
