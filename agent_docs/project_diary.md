# Project Diary

## Decisions and Lessons

- `ClassMngrEngine` is intentionally Qt-free and configured before Qt discovery; keep portable engine changes independent of Qt where possible.
- Product selection happens before Qt package discovery so the Windows WinUI lane can configure without a Qt installation.
- Windows CMake presets use Visual Studio 18 2026 with the `v145` toolset; existing build directories created with another generator require a fresh reconfigure.
