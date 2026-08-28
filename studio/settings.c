#include "settings.h"

#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static bool studio_settings_resolve_path(char *out_path, size_t out_capacity) {
    char appdata[STUDIO_MAX_PATH];
    char folder[STUDIO_MAX_PATH];

    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata) != S_OK) {
        return false;
    }
    studio_join_path(appdata, "Erire Studio", folder, sizeof(folder));
    if (!studio_ensure_directory(folder)) {
        return false;
    }
    studio_join_path(folder, "erire_studio.ini", out_path, out_capacity);
    return true;
}

void studio_settings_defaults(StudioSettings *settings) {
    size_t i;

    memset(settings, 0, sizeof(*settings));
    snprintf(settings->erire_exe, sizeof(settings->erire_exe), "erire.exe");
    snprintf(settings->python_exe, sizeof(settings->python_exe), "python.exe");
    snprintf(settings->font_name, sizeof(settings->font_name), "Cascadia Code");
    snprintf(settings->theme_name, sizeof(settings->theme_name), "dark-2026");
    settings->font_size = 14;
    settings->autosave = false;
    settings->software_rendering = true;
    settings->show_explorer = true;
    settings->show_outline = true;
    settings->show_bottom = true;
    for (i = 0; i < STUDIO_RECENT_PROJECTS; ++i) {
        settings->recent_projects[i][0] = '\0';
    }
    studio_settings_resolve_path(settings->config_path, sizeof(settings->config_path));
}

bool studio_settings_load(StudioSettings *settings) {
    size_t i;
    char key[32];

    studio_settings_defaults(settings);
    if (settings->config_path[0] == '\0') {
        return false;
    }
    GetPrivateProfileStringA("tools", "erire_exe", settings->erire_exe, settings->erire_exe, sizeof(settings->erire_exe), settings->config_path);
    GetPrivateProfileStringA("tools", "python_exe", settings->python_exe, settings->python_exe, sizeof(settings->python_exe), settings->config_path);
    GetPrivateProfileStringA("editor", "font_name", settings->font_name, settings->font_name, sizeof(settings->font_name), settings->config_path);
    GetPrivateProfileStringA("editor", "theme_name", settings->theme_name, settings->theme_name, sizeof(settings->theme_name), settings->config_path);
    settings->font_size = (int) GetPrivateProfileIntA("editor", "font_size", settings->font_size, settings->config_path);
    settings->autosave = GetPrivateProfileIntA("editor", "autosave", settings->autosave ? 1 : 0, settings->config_path) != 0;
    settings->software_rendering = GetPrivateProfileIntA("editor", "software_rendering", settings->software_rendering ? 1 : 0, settings->config_path) != 0;
    settings->show_explorer = GetPrivateProfileIntA("workspace", "show_explorer", settings->show_explorer ? 1 : 0, settings->config_path) != 0;
    settings->show_outline = GetPrivateProfileIntA("workspace", "show_outline", settings->show_outline ? 1 : 0, settings->config_path) != 0;
    settings->show_bottom = GetPrivateProfileIntA("workspace", "show_bottom", settings->show_bottom ? 1 : 0, settings->config_path) != 0;
    GetPrivateProfileStringA("workspace", "last_project", "", settings->last_project, sizeof(settings->last_project), settings->config_path);
    for (i = 0; i < STUDIO_RECENT_PROJECTS; ++i) {
        snprintf(key, sizeof(key), "project%u", (unsigned) i);
        GetPrivateProfileStringA("recent", key, "", settings->recent_projects[i], sizeof(settings->recent_projects[i]), settings->config_path);
    }
    return true;
}

bool studio_settings_save(const StudioSettings *settings) {
    size_t i;
    char key[32];
    char number[32];

    if (!settings || settings->config_path[0] == '\0') {
        return false;
    }
    WritePrivateProfileStringA("tools", "erire_exe", settings->erire_exe, settings->config_path);
    WritePrivateProfileStringA("tools", "python_exe", settings->python_exe, settings->config_path);
    WritePrivateProfileStringA("editor", "font_name", settings->font_name, settings->config_path);
    WritePrivateProfileStringA("editor", "theme_name", settings->theme_name, settings->config_path);
    snprintf(number, sizeof(number), "%d", settings->font_size);
    WritePrivateProfileStringA("editor", "font_size", number, settings->config_path);
    WritePrivateProfileStringA("editor", "autosave", settings->autosave ? "1" : "0", settings->config_path);
    WritePrivateProfileStringA("editor", "software_rendering", settings->software_rendering ? "1" : "0", settings->config_path);
    WritePrivateProfileStringA("workspace", "show_explorer", settings->show_explorer ? "1" : "0", settings->config_path);
    WritePrivateProfileStringA("workspace", "show_outline", settings->show_outline ? "1" : "0", settings->config_path);
    WritePrivateProfileStringA("workspace", "show_bottom", settings->show_bottom ? "1" : "0", settings->config_path);
    WritePrivateProfileStringA("workspace", "last_project", settings->last_project, settings->config_path);
    for (i = 0; i < STUDIO_RECENT_PROJECTS; ++i) {
        snprintf(key, sizeof(key), "project%u", (unsigned) i);
        WritePrivateProfileStringA("recent", key, settings->recent_projects[i], settings->config_path);
    }
    return true;
}

void studio_settings_push_recent(StudioSettings *settings, const char *project_path) {
    size_t i;

    if (!project_path || project_path[0] == '\0') {
        return;
    }
    for (i = 0; i < STUDIO_RECENT_PROJECTS; ++i) {
        if (_stricmp(settings->recent_projects[i], project_path) == 0) {
            size_t j;
            for (j = i; j > 0; --j) {
                strcpy(settings->recent_projects[j], settings->recent_projects[j - 1]);
            }
            strncpy(settings->recent_projects[0], project_path, sizeof(settings->recent_projects[0]) - 1);
            settings->recent_projects[0][sizeof(settings->recent_projects[0]) - 1] = '\0';
            return;
        }
    }
    for (i = STUDIO_RECENT_PROJECTS - 1; i > 0; --i) {
        strcpy(settings->recent_projects[i], settings->recent_projects[i - 1]);
    }
    strncpy(settings->recent_projects[0], project_path, sizeof(settings->recent_projects[0]) - 1);
    settings->recent_projects[0][sizeof(settings->recent_projects[0]) - 1] = '\0';
}
