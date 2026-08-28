#include "util.h"

#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

#include "fileio.h"

bool studio_utf8_to_wide(const char *input, wchar_t *output, size_t output_capacity) {
    int count;

    if (!input || !output || output_capacity == 0) {
        return false;
    }
    count = MultiByteToWideChar(CP_UTF8, 0, input, -1, output, (int) output_capacity);
    return count > 0;
}

bool studio_wide_to_utf8(const wchar_t *input, char *output, size_t output_capacity) {
    int count;

    if (!input || !output || output_capacity == 0) {
        return false;
    }
    count = WideCharToMultiByte(CP_UTF8, 0, input, -1, output, (int) output_capacity, NULL, NULL);
    return count > 0;
}

bool studio_module_directory(char *out, size_t out_capacity) {
    ErError error;
    char path[STUDIO_MAX_PATH];

    er_error_clear(&error);
    if (!er_get_current_module_path(path, sizeof(path), &error)) {
        return false;
    }
    er_path_dirname(path, out, out_capacity);
    return true;
}

void studio_join_path(const char *left, const char *right, char *out, size_t out_capacity) {
    er_path_join(left, right, out, out_capacity);
}

void studio_normalize_slashes(char *path) {
    size_t i;

    if (!path) {
        return;
    }
    for (i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
}

bool studio_path_file_url(const char *path, char *out, size_t out_capacity) {
    size_t index = 0;
    size_t i;

    if (!path || !out || out_capacity < 10) {
        return false;
    }

    index += (size_t) snprintf(out + index, out_capacity - index, "file:///");
    for (i = 0; path[i] != '\0' && index + 4 < out_capacity; ++i) {
        char ch = path[i];
        if (ch == '\\') {
            out[index++] = '/';
        } else if (ch == ' ') {
            memcpy(out + index, "%20", 3);
            index += 3;
        } else {
            out[index++] = ch;
        }
    }
    out[index] = '\0';
    return true;
}

bool studio_ensure_directory(const char *path) {
    ErError error;

    er_error_clear(&error);
    return er_directory_create_recursive(path, &error);
}

bool studio_pick_folder(HWND owner, char *out_path, size_t out_capacity) {
    BROWSEINFOA info;
    LPITEMIDLIST id_list;
    bool ok = false;

    memset(&info, 0, sizeof(info));
    info.hwndOwner = owner;
    info.lpszTitle = "Select project folder";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    id_list = SHBrowseForFolderA(&info);
    if (!id_list) {
        return false;
    }
    ok = SHGetPathFromIDListA(id_list, out_path) == TRUE;
    CoTaskMemFree(id_list);
    if (!ok && out_capacity > 0) {
        out_path[0] = '\0';
    }
    return ok;
}

bool studio_open_file_dialog(HWND owner, char *out_path, size_t out_capacity) {
    OPENFILENAMEA dialog;

    if (!out_path || out_capacity == 0) {
        return false;
    }
    memset(&dialog, 0, sizeof(dialog));
    out_path[0] = '\0';

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = out_path;
    dialog.nMaxFile = (DWORD) out_capacity;
    dialog.lpstrFilter = "Erire Files\0*.er;*.py;*.json;*.txt;*.ini\0All Files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    return GetOpenFileNameA(&dialog) == TRUE;
}

bool studio_save_file_dialog(HWND owner, const char *suggested_name, char *out_path, size_t out_capacity) {
    OPENFILENAMEA dialog;

    if (!out_path || out_capacity == 0) {
        return false;
    }
    memset(&dialog, 0, sizeof(dialog));
    out_path[0] = '\0';
    if (suggested_name) {
        strncpy(out_path, suggested_name, out_capacity - 1);
        out_path[out_capacity - 1] = '\0';
    }

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = out_path;
    dialog.nMaxFile = (DWORD) out_capacity;
    dialog.lpstrFilter = "Erire Files\0*.er;*.py;*.json;*.txt;*.ini\0All Files\0*.*\0";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    return GetSaveFileNameA(&dialog) == TRUE;
}

const char *studio_language_from_path(const char *path) {
    if (!path) {
        return "text";
    }
    if (er_path_has_extension(path, ".er")) {
        return "erire";
    }
    if (er_path_has_extension(path, ".py")) {
        return "python";
    }
    if (er_path_has_extension(path, ".json")) {
        return "json";
    }
    return "text";
}
