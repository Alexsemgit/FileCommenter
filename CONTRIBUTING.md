# Contributing to FileCommenter

Thank you for your interest in contributing to **FileCommenter**!

## How You Can Contribute

1. **Reporting Bugs**:
   - Check existing issues to see if the bug has already been reported.
   - Provide your Windows version (e.g. Windows 11 23H2 or Windows 10 22H2), build environment (MSVC or MinGW), and clear reproduction steps.

2. **Suggesting Enhancements**:
   - Open a feature request describing the use case and proposed behavior.

3. **Submitting Pull Requests**:
   - Fork the repository and create a feature branch (`git checkout -b feature/my-feature`).
   - Keep code clean, formatted, and strictly compatible with both **MSVC** (Visual Studio 2019/2022) and **MinGW-w64 GCC**.
   - Make sure `build_msvc.bat` and `build_mingw.bat` compile without warnings.
   - Commit your changes with descriptive messages.
   - Open a Pull Request against the `main` branch.

## Coding Guidelines

- Standard C++17 with Win32 API.
- Unicode throughout: use wide strings (`std::wstring`, `wchar_t`, `L"..."`, and `*W` Win32 APIs).
- Maintain bilingual RU/EN support in all user-facing strings and resources.
- Avoid introducing heavyweight third-party runtime dependencies; stick to core Windows DLLs (`user32`, `kernel32`, `shell32`, `shlwapi`, `comctl32`, `ole32`, `advapi32`).
