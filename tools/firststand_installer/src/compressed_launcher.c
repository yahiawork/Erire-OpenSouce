#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SFX_MAGIC "ERIRECAB1"
#define SFX_MAGIC_SIZE 16
#define SFX_BUFFER_SIZE (1024 * 1024)

typedef struct SfxFooter {
    char magic[SFX_MAGIC_SIZE];
    uint64_t cab_offset;
    uint64_t cab_size;
} SfxFooter;

static void show_error(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Erire Installer", MB_ICONERROR | MB_OK);
}

static void join_path(wchar_t *out, size_t out_count, const wchar_t *left, const wchar_t *right) {
    size_t len;
    if (!out_count) return;
    _snwprintf(out, out_count, L"%s", left ? left : L"");
    out[out_count - 1] = L'\0';
    len = wcslen(out);
    if (len > 0 && out[len - 1] != L'\\' && out[len - 1] != L'/') {
        _snwprintf(out + len, out_count - len, L"\\");
        out[out_count - 1] = L'\0';
    }
    len = wcslen(out);
    _snwprintf(out + len, out_count - len, L"%s", right ? right : L"");
    out[out_count - 1] = L'\0';
}

static void append_quoted(wchar_t *cmd, size_t cmd_count, const wchar_t *arg) {
    size_t len = wcslen(cmd);
    _snwprintf(cmd + len, cmd_count - len, L"\"");
    cmd[cmd_count - 1] = L'\0';
    len = wcslen(cmd);
    while (*arg && len + 3 < cmd_count) {
        if (*arg == L'"') {
            cmd[len++] = L'\\';
        }
        cmd[len++] = *arg++;
    }
    if (len + 2 < cmd_count) {
        cmd[len++] = L'"';
    }
    cmd[len] = L'\0';
}

static int read_footer(const wchar_t *self_path, SfxFooter *footer) {
    FILE *file = _wfopen(self_path, L"rb");
    if (!file) return 0;
    if (_fseeki64(file, -(int64_t)sizeof(*footer), SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    if (fread(footer, 1, sizeof(*footer), file) != sizeof(*footer)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return memcmp(footer->magic, SFX_MAGIC, strlen(SFX_MAGIC)) == 0;
}

static int write_embedded_cab(const wchar_t *self_path, const wchar_t *cab_path, const SfxFooter *footer) {
    FILE *in = _wfopen(self_path, L"rb");
    FILE *out = NULL;
    unsigned char *buffer = NULL;
    uint64_t remaining = footer->cab_size;
    int ok = 0;

    if (!in) return 0;
    out = _wfopen(cab_path, L"wb");
    if (!out) {
        fclose(in);
        return 0;
    }
    buffer = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, SFX_BUFFER_SIZE);
    if (!buffer) goto cleanup;
    if (_fseeki64(in, (int64_t)footer->cab_offset, SEEK_SET) != 0) goto cleanup;

    while (remaining > 0) {
        size_t want = remaining > SFX_BUFFER_SIZE ? SFX_BUFFER_SIZE : (size_t)remaining;
        size_t got = fread(buffer, 1, want, in);
        if (!got) goto cleanup;
        if (fwrite(buffer, 1, got, out) != got) goto cleanup;
        remaining -= got;
    }
    ok = 1;

cleanup:
    if (buffer) HeapFree(GetProcessHeap(), 0, buffer);
    fclose(out);
    fclose(in);
    if (!ok) DeleteFileW(cab_path);
    return ok;
}

static int run_and_wait(wchar_t *command, const wchar_t *work_dir, DWORD show_window, DWORD creation_flags) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = (WORD)show_window;
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, creation_flags, NULL, work_dir, &si, &pi)) {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exit_code == 0;
}

static int extract_cab(const wchar_t *cab_path, const wchar_t *extract_dir) {
    wchar_t extrac32[MAX_PATH];
    wchar_t cmd[8192];
    if (!SearchPathW(NULL, L"extrac32.exe", NULL, MAX_PATH, extrac32, NULL)) {
        show_error(L"Windows extrac32.exe was not found.");
        return 0;
    }
    cmd[0] = L'\0';
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), extrac32);
    wcsncat(cmd, L" /Y /E /L ", (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1);
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), extract_dir);
    wcsncat(cmd, L" ", (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1);
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), cab_path);
    return run_and_wait(cmd, extract_dir, SW_HIDE, CREATE_NO_WINDOW);
}

static int run_installer(const wchar_t *installer_path, const wchar_t *work_dir) {
    SHELLEXECUTEINFOW info;
    DWORD exit_code = 1;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"open";
    info.lpFile = installer_path;
    info.lpDirectory = work_dir;
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        return 0;
    }
    if (info.hProcess) {
        WaitForSingleObject(info.hProcess, INFINITE);
        GetExitCodeProcess(info.hProcess, &exit_code);
        CloseHandle(info.hProcess);
    }
    return exit_code == 0;
}

static void cleanup_work_dir(const wchar_t *work_dir, const wchar_t *cab_path, const wchar_t *installer_path) {
    DeleteFileW(installer_path);
    DeleteFileW(cab_path);
    RemoveDirectoryW(work_dir);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    wchar_t self_path[MAX_PATH];
    wchar_t temp_root[MAX_PATH];
    wchar_t work_dir[MAX_PATH];
    wchar_t cab_path[MAX_PATH];
    wchar_t installer_path[MAX_PATH];
    SfxFooter footer;
    LPWSTR *argv = NULL;
    int argc = 0;
    int extract_only = 0;
    int mkdir_result;
    int result = 1;

    (void)instance;
    (void)prev_instance;
    (void)cmd_line;
    (void)show_cmd;

    if (!GetModuleFileNameW(NULL, self_path, MAX_PATH)) {
        show_error(L"Could not locate this installer.");
        return 1;
    }
    if (!read_footer(self_path, &footer)) {
        show_error(L"This compressed installer is incomplete or damaged.");
        return 1;
    }

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 3 && _wcsicmp(argv[1], L"/extract-only") == 0) {
        _snwprintf(work_dir, MAX_PATH, L"%s", argv[2]);
        work_dir[MAX_PATH - 1] = L'\0';
        extract_only = 1;
    } else {
        if (!GetTempPathW(MAX_PATH, temp_root)) {
            show_error(L"Could not locate the temporary folder.");
            if (argv) LocalFree(argv);
            return 1;
        }
        _snwprintf(work_dir, MAX_PATH, L"%sErireInstaller_%lu_%lu", temp_root, (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
        work_dir[MAX_PATH - 1] = L'\0';
    }

    mkdir_result = SHCreateDirectoryExW(NULL, work_dir, NULL);
    if (mkdir_result != ERROR_SUCCESS && mkdir_result != ERROR_ALREADY_EXISTS && mkdir_result != ERROR_FILE_EXISTS) {
        show_error(L"Could not create the temporary extraction folder.");
        if (argv) LocalFree(argv);
        return 1;
    }

    join_path(cab_path, MAX_PATH, work_dir, L"payload.cab");
    join_path(installer_path, MAX_PATH, work_dir, L"ErireIDE-FirstStand-Setup.exe");

    if (!write_embedded_cab(self_path, cab_path, &footer)) {
        show_error(L"Could not unpack the compressed installer payload.");
        goto done;
    }
    if (!extract_cab(cab_path, work_dir)) {
        show_error(L"Could not extract the installer payload.");
        goto done;
    }
    DeleteFileW(cab_path);

    if (extract_only) {
        result = 0;
        goto done;
    }
    if (!run_installer(installer_path, work_dir)) {
        show_error(L"The installer could not be started.");
        goto done;
    }
    result = 0;

done:
    if (!extract_only) {
        cleanup_work_dir(work_dir, cab_path, installer_path);
    }
    if (argv) LocalFree(argv);
    return result;
}
