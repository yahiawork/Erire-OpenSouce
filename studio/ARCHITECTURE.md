# Erire Studio Architecture

## 1. Full architecture overview

Erire Studio is split into three layers:

- `native host`: Win32 window, menu bar, WebView2 bootstrapping, dialogs, settings, process control
- `frontend UI`: project explorer, tabs, editor shell, outline, output, problems, status bar
- `IDE services`: project scanning, document persistence, template generation, run/check/build runner, outline extraction

The host owns the shell and OS integration. The frontend owns interaction-heavy UI state. Services stay boring and explicit so the message bridge remains easy to debug.

## 2. Exact source/project folder structure

```text
studio/
  ARCHITECTURE.md
  README.md
  main.c
  app.c
  app.h
  bridge.c
  bridge.h
  json.c
  json.h
  outline.c
  outline.h
  project.c
  project.h
  runner.c
  runner.h
  settings.c
  settings.h
  templates.c
  templates.h
  util.c
  util.h
  webview2_host.c
  webview2_host.h
  frontend/
    index.html
    app.css
    app.js
```

## 3. Full subsystem/module breakdown

- `main.c`: process bootstrap and COM lifetime
- `app.c`: main window procedure, native menu creation, startup, resize, OS command routing
- `bridge.c`: frontend command dispatch and outbound event formatting
- `webview2_host.c`: create environment/controller, load frontend, handle web messages
- `project.c`: locate project root, parse `app.json`, scan folders, open/save files
- `outline.c`: line-based Erire/Python symbol extraction
- `runner.c`: one active child process with stdout/stderr capture and stop support
- `settings.c`: `%APPDATA%\Erire Studio\erire_studio.ini`
- `templates.c`: create starter Erire projects with Python bridge scaffolding
- `frontend/app.js`: workspace model, tabs, explorer, editor, outline, logs, settings dialogs

## 4. Native host design

- native `HMENU` for a real desktop IDE menu bar
- single root Win32 window with WebView2 filling the client area
- native file/folder picker dialogs exposed through bridge commands
- `WM_APP` messages used to marshal runner output back to the UI thread
- WebView2 bridge uses explicit stringified JSON only

## 5. Frontend UI design

- toolbar row inside WebView2 below the native menu
- three-column main split: explorer, workspace, outline
- bottom tabbed panel: Output / Terminal / Problems
- status bar at the bottom
- modal overlays for New Project and Settings

## 6. Editor strategy and document model

The v1 editor is a lightweight layered editor:

- one authoritative `textarea` for input, IME, caret, selection, clipboard, and keyboard behavior
- one synchronized highlighted `<pre>` behind it for syntax color
- one synchronized gutter for line numbers
- one shared editor view bound to the active document for fast tab switching

Document model fields:

- `path`
- `name`
- `language`
- `content`
- `dirty`
- `scrollTop`
- `scrollLeft`
- `selectionStart`
- `selectionEnd`
- `line`
- `column`

Tradeoff: this is lighter than Monaco and far more practical than a fully custom DOM editor, but it still leaves room for diagnostics, completions, symbol navigation, and incremental tokenization later.

## 7. Syntax highlighting strategy

- Erire: sequential tokenizer with states for strings and comments; keywords, commands, identifiers, numbers, operators, punctuation, imports
- Python: lightweight tokenizer for comments, strings, numbers, keywords, decorators, identifiers
- rendering path: tokenize plain text into HTML spans for the mirror layer
- large files: optional fallback to plain escaped text to stay responsive

## 8. Backend services design

- `project service`: open/close/refresh project, scan tree, resolve entry file
- `document persistence`: open file, save file, save as
- `runner service`: run/check/build/run-python, stop, stream output
- `settings service`: load/save config, recent projects, last project, layout toggles
- `template service`: create folder tree and default files
- `outline service`: symbol extraction for initial load and future navigation hooks

## 9. Message bridge protocol

Top-level shape:

```json
{ "type": "app:openFile", "payload": { "path": "C:\\demo\\src\\main.er" } }
```

The protocol uses one `type` string plus a `payload` object. No hidden side effects. All cross-layer messages are logged as plain JSON strings during development.

## 10. New project generation flow

1. Frontend opens modal and collects name, location, template.
2. Native creates directories and default files.
3. Native opens the new project root immediately.
4. Frontend receives `project:loaded`, `project:tree`, and initial `file:content`.

## 11. Default Erire project template

Generated structure:

```text
ProjectName/
  app.json
  src/
    main.er
    backend.er
  pages/
  components/
  assets/
  python/
    app_bridge.py
  build/
  tests/
```

The template is UI-centric and includes Python bridge scaffolding from day one.

## 12. Run / Check / Build / Python execution flow

- `run project`: `erire.exe --run <entry.er>`
- `check project`: `erire.exe --check <entry.er>`
- `build project`: `erire.exe --build <entry.er> <project>\build\<name>.exe`
- `run python`: `python.exe <active.py>`

All executions stream output incrementally to the bottom panels and update process state in the status bar.

## 13. Settings/config design

INI sections:

- `[tools]`: `erire_exe`, `python_exe`
- `[editor]`: `font_name`, `font_size`, `autosave`
- `[workspace]`: `last_project`, `show_explorer`, `show_outline`, `show_bottom`
- `[recent]`: `project0..project7`

## 14. UI styling rules and layout metrics

- background: `#11161d`
- primary panel: `#161c24`
- elevated panel: `#1b2330`
- editor surface: `#0f172a`
- border: `#283241`
- text: `#e6edf3`
- muted text: `#94a3b8`
- accent: `#4f8cff`

Core metrics:

- toolbar: `58px`
- left panel: `280px`
- right panel: `240px`
- bottom panel: `220px`
- panel gap: `8px`
- common padding: `12px`

## 15. Implementation plan in realistic build order

1. Boot the Win32 window, menu bar, and WebView2 host.
2. Load the frontend and prove bridge round-trips.
3. Add settings load/save and startup restoration.
4. Add project open/new/refresh and explorer rendering.
5. Add editor tabs, save flow, and outline extraction.
6. Add runner service and output streaming.
7. Add bottom problems parsing, status updates, and layout persistence.
8. Harden editor performance, diagnostics plumbing, and symbol navigation hooks.

## 16. Starter code skeletons or detailed pseudo-code

The source files in this folder are the starter skeletons:

- `webview2_host.c`: dynamic loader + plain C COM callbacks
- `app.c`: window proc and command routing
- `bridge.c`: JSON dispatch table for host commands
- `frontend/app.js`: full UI state and editor shell

They are implementation-oriented rather than mockup-only and are meant to be extended in place.
