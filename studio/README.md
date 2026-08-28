# Erire Studio

`Erire Studio` is the dedicated lightweight desktop IDE for Erire projects on Windows.

It is built as:

- native host in C
- Win32 shell
- WebView2 frontend container
- HTML/CSS/JavaScript UI
- native services for files, projects, settings, and process execution

## Build Notes

`ErireStudio.exe` is wired through the top-level `Makefile`.

Expected local dependencies:

- Windows SDK
- a C compiler such as `gcc` or `clang`
- WebView2 SDK headers available to the compiler (`WebView2.h`)
- WebView2 Runtime installed on the target machine

The implementation loads `WebView2Loader.dll` dynamically at runtime instead of linking it directly.
At startup, Erire Studio now prefers the local project runtime at `./webview2/` as the WebView2 browser folder.
If `./webview2/WebView2Loader.dll` is present, it is loaded first; otherwise the host falls back to the system loader.

## Source Layout

- `main.c`: Win32 entry point
- `app.c`: main window, menu commands, bridge integration
- `bridge.c`: JSON command routing between frontend and native services
- `webview2_host.c`: WebView2 initialization and message bridge plumbing
- `project.c`: project detection, tree scanning, file I/O helpers
- `outline.c`: lightweight symbol extraction for Erire and Python
- `runner.c`: external process spawning and log streaming
- `settings.c`: INI-backed persistent settings
- `templates.c`: starter project generation
- `util.c`: UTF/path helpers
- `json.c`: tiny JSON helpers used by the bridge
- `frontend/`: HTML/CSS/JS application loaded inside WebView2
- `ARCHITECTURE.md`: implementation blueprint matching the source tree

## Current Scope

This rebuild focuses on a practical v1 foundation:

- native menu + WebView2 workspace shell
- left explorer, center tabs/editor, right outline, bottom logs/problems
- Erire and Python project awareness
- new project generation
- run/check/build using `erire.exe`
- run current Python file using `python.exe`
- simple layered editor with syntax highlighting hooks
- explicit JSON bridge for future IDE services
