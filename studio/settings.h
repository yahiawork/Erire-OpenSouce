#ifndef ERIRE_STUDIO_SETTINGS_H
#define ERIRE_STUDIO_SETTINGS_H

#include <stdbool.h>

#include "util.h"

typedef struct StudioSettings {
    char erire_exe[STUDIO_MAX_PATH];
    char python_exe[STUDIO_MAX_PATH];
    char font_name[64];
    char theme_name[64];
    int font_size;
    bool autosave;
    bool software_rendering;
    bool show_explorer;
    bool show_outline;
    bool show_bottom;
    char last_project[STUDIO_MAX_PATH];
    char recent_projects[STUDIO_RECENT_PROJECTS][STUDIO_MAX_PATH];
    char config_path[STUDIO_MAX_PATH];
} StudioSettings;

void studio_settings_defaults(StudioSettings *settings);
bool studio_settings_load(StudioSettings *settings);
bool studio_settings_save(const StudioSettings *settings);
void studio_settings_push_recent(StudioSettings *settings, const char *project_path);

#endif
