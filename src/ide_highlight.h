#ifndef ERIRE_IDE_HIGHLIGHT_H
#define ERIRE_IDE_HIGHLIGHT_H

#include <stdbool.h>
#include <stdio.h>

#include "error.h"

// Installs Erire syntax highlighting for all detected Electron-based IDEs
// (VS Code, VS Code Insiders, Antigravity IDE, Cursor, VSCodium, Windsurf, Positron, etc.)
bool er_ide_highlight_install(bool verbose, FILE *out, ErError *error);

// Automatically checks and installs highlighting on first run if not already present
void er_ide_highlight_auto_install_if_needed(void);

#endif
