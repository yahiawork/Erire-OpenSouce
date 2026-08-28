#include "console.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "fileio.h"

#ifdef _WIN32
#include <windows.h>
#define ER_CONSOLE_STRICMP _stricmp
#else
#define ER_CONSOLE_STRICMP strcmp
#endif

typedef struct ErConsoleBuiltinProfile {
    const char *name;
    const char *exe_path;
    const char *args;
    const char *description;
} ErConsoleBuiltinProfile;

static const ErConsoleBuiltinProfile ER_CONSOLE_BUILTINS[] = {
    { "powershell", "C:\\Program Files\\PowerShell\\7\\pwsh.exe", "-NoLogo", "PowerShell 7" },
    { "cmd", "C:\\Windows\\System32\\cmd.exe", "", "Windows command prompt" },
    { "wsl", "C:\\Windows\\System32\\wsl.exe", "", "Default WSL shell" },
    { "ubuntu", "C:\\Windows\\System32\\wsl.exe", "-d Ubuntu", "Ubuntu through WSL" },
    { "debian", "C:\\Windows\\System32\\wsl.exe", "-d Debian", "Debian through WSL" },
    { "kali", "C:\\Windows\\System32\\wsl.exe", "-d kali-linux", "Kali Linux through WSL" },
    { "devshell", "C:\\Program Files\\PowerShell\\7\\pwsh.exe", "-NoLogo", "Developer shell alias" }

};

static bool er_console_profile_name_is_safe(const char *profile) {
    size_t i;

    if (!profile || profile[0] == '\0') {
        return false;
    }

    for (i = 0; profile[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char) profile[i];
        if (!(isalnum(ch) || ch == '-' || ch == '_' || ch == '.')) {
            return false;
        }
    }

    return true;
}

static char *er_console_dup(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *) malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void er_console_trim_in_place(char *text) {
    char *start;
    char *end;

    if (!text) {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char) *start)) {
        ++start;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1])) {
        --end;
    }
    *end = '\0';
}

static bool er_console_read_profile_file(
    const char *path,
    const char *profile,
    char **out_exe,
    char **out_args,
    ErError *error
) {
    char *data = NULL;
    size_t size = 0;
    char *cursor;

    *out_exe = NULL;
    *out_args = NULL;

    if (!er_file_exists(path)) {
        return false;
    }

    if (!er_file_read_all(path, &data, &size, error)) {
        return false;
    }

    cursor = data;
    while (cursor && *cursor != '\0') {
        char *line_end = strpbrk(cursor, "\r\n");
        char *separator;
        char *pipe;
        char *name;
        char *exe_text;
        char *args_text;

        if (line_end) {
            *line_end = '\0';
        }

        er_console_trim_in_place(cursor);
        if (cursor[0] == '\0' || cursor[0] == '#' || cursor[0] == ';') {
            goto next_line;
        }

        separator = strchr(cursor, '=');
        if (!separator) {
            goto next_line;
        }

        *separator = '\0';
        name = cursor;
        exe_text = separator + 1;
        er_console_trim_in_place(name);
        er_console_trim_in_place(exe_text);

        if (ER_CONSOLE_STRICMP(name, profile) != 0) {
            goto next_line;
        }

        pipe = strchr(exe_text, '|');
        if (pipe) {
            *pipe = '\0';
            args_text = pipe + 1;
            er_console_trim_in_place(exe_text);
            er_console_trim_in_place(args_text);
        } else {
            args_text = "";
        }

        if (exe_text[0] == '\0') {
            er_error_set(error, 0, 0, "Console profile '%s' has an empty executable path", profile);
            free(data);
            return false;
        }

        *out_exe = er_console_dup(exe_text);
        *out_args = er_console_dup(args_text);
        if (!*out_exe || !*out_args) {
            free(*out_exe);
            free(*out_args);
            *out_exe = NULL;
            *out_args = NULL;
            er_error_set(error, 0, 0, "Out of memory while reading console profile '%s'", profile);
            free(data);
            return false;
        }

        free(data);
        return true;

next_line:
        if (!line_end) {
            break;
        }
        cursor = line_end + 1;
        if (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }
    }

    free(data);
    return false;
}

static bool er_console_try_config_profile(
    const char *profile,
    char **out_exe,
    char **out_args,
    ErError *error
) {
    char module_path[1024];
    char module_dir[1024];
    char profile_path[1024];

    *out_exe = NULL;
    *out_args = NULL;

    if (er_console_read_profile_file("tools\\console_profiles.txt", profile, out_exe, out_args, error)) {
        return true;
    }

    if (er_get_current_module_path(module_path, sizeof(module_path), error)) {
        er_path_dirname(module_path, module_dir, sizeof(module_dir));
        er_path_join(module_dir, "tools\\console_profiles.txt", profile_path, sizeof(profile_path));
        if (strcmp(profile_path, "tools\\console_profiles.txt") != 0 &&
            er_console_read_profile_file(profile_path, profile, out_exe, out_args, error)) {
            return true;
        }
    } else {
        er_error_clear(error);
    }

    return false;
}

static bool er_console_find_builtin_profile(
    const char *profile,
    const char **out_exe,
    const char **out_args
) {
    size_t i;

    *out_exe = NULL;
    *out_args = NULL;

    for (i = 0; i < sizeof(ER_CONSOLE_BUILTINS) / sizeof(ER_CONSOLE_BUILTINS[0]); ++i) {
        if (ER_CONSOLE_STRICMP(ER_CONSOLE_BUILTINS[i].name, profile) == 0) {
            *out_exe = ER_CONSOLE_BUILTINS[i].exe_path;
            *out_args = ER_CONSOLE_BUILTINS[i].args;
            return true;
        }
    }

    return false;
}

#ifdef _WIN32
static bool er_console_launch_process(const char *exe_path, const char *args, ErError *error) {
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    char command_line[2048];
    int written;

    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);

    written = snprintf(
        command_line,
        sizeof(command_line),
        "\"%s\"%s%s",
        exe_path,
        (args && args[0] != '\0') ? " " : "",
        (args && args[0] != '\0') ? args : ""
    );
    if (written < 0 || (size_t) written >= sizeof(command_line)) {
        er_error_set(error, 0, 0, "Console launch command is too long");
        return false;
    }

    if (!CreateProcessA(
            exe_path,
            command_line,
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &startup_info,
            &process_info
        )) {
        er_error_set(error, 0, 0, "Could not launch console profile '%s' (Win32 error %lu)", exe_path, (unsigned long) GetLastError());
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}
#endif

bool er_console_launch_profile(const char *profile, ErError *error) {
    char *configured_exe = NULL;
    char *configured_args = NULL;
    const char *builtin_exe = NULL;
    const char *builtin_args = NULL;
    bool ok = false;

    er_error_clear(error);

    if (!er_console_profile_name_is_safe(profile)) {
        er_error_set(error, 0, 0, "Console profile name is invalid");
        return false;
    }

    if (er_console_try_config_profile(profile, &configured_exe, &configured_args, error)) {
#ifdef _WIN32
        ok = er_console_launch_process(configured_exe, configured_args, error);
#else
        er_error_set(error, 0, 0, "Console profiles are currently supported on Windows only");
#endif
        free(configured_exe);
        free(configured_args);
        return ok;
    }
    if (er_error_has(error)) {
        return false;
    }

    if (!er_console_find_builtin_profile(profile, &builtin_exe, &builtin_args)) {
        er_error_set(error, 0, 0, "Unknown console profile '%s'. Use --console-help or tools\\console_profiles.txt", profile);
        return false;
    }

#ifdef _WIN32
    return er_console_launch_process(builtin_exe, builtin_args, error);
#else
    er_error_set(error, 0, 0, "Console profiles are currently supported on Windows only");
    return false;
#endif
}

void er_console_print_profiles(FILE *out) {
    size_t i;

    if (!out) {
        return;
    }

    fputs("Built-in console profiles:\n", out);
    for (i = 0; i < sizeof(ER_CONSOLE_BUILTINS) / sizeof(ER_CONSOLE_BUILTINS[0]); ++i) {
        fprintf(
            out,
            "  %s: %s\n",
            ER_CONSOLE_BUILTINS[i].name,
            ER_CONSOLE_BUILTINS[i].description
        );
    }
    fputs("\nCustom profiles:\n", out);
    fputs("  Define them in tools\\console_profiles.txt using:\n", out);
    fputs("  name = C:\\\\Path\\\\To\\\\Executable.exe | optional arguments\n", out);
}
