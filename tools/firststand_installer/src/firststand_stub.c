#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <objbase.h>
#include <wincrypt.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firststand_format.h"

#define FS_MAX_PATH 4096
#define ID_EDIT_KEY 101
#define ID_EDIT_GOOGLE 102
#define ID_EDIT_DIR 103
#define ID_BUTTON_BROWSE 104
#define ID_BUTTON_INSTALL 105
#define ID_BUTTON_CANCEL 106
#define ID_PROGRESS 107
#define ID_STATUS 108
#define ID_CHECK_PATH 109
#define ID_HELP_BOX 110

typedef struct PackageConfig {
    char app_name[256];
    char app_version[64];
    char publisher[256];
    char install_dir[FS_MAX_PATH];
    char main_exe[FS_MAX_PATH];
    char require_key[16];
    char activation_url[1024];
    char api_key[512];
    char google_required[16];
    char desktop_shortcut[16];
    char start_menu_shortcut[16];
    char run_after_install[16];
} PackageConfig;

typedef struct FileEntry {
    char *rel_path;
    uint64_t offset;
    uint64_t size;
} FileEntry;

typedef struct Package {
    PackageConfig cfg;
    FileEntry *files;
    size_t file_count;
    uint64_t archive_offset;
    uint64_t manifest_size;
    uint64_t data_offset;
    char self_path[FS_MAX_PATH];
} Package;

typedef struct Ui {
    HWND hwnd;
    HWND key_edit;
    HWND google_edit;
    HWND dir_edit;
    HWND path_check;
    HWND install_button;
    HWND status_label;
    HWND progress;
    HFONT font;
    HFONT title_font;
    HFONT help_font;
    HBRUSH bg_brush;
    HBRUSH input_brush;
    HBRUSH transparent_brush;
} Ui;

static Package g_package;
static Ui g_ui;

static int streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int truthy(const char *value) {
    return streq_ci(value, "1") || streq_ci(value, "true") || streq_ci(value, "yes") || streq_ci(value, "on");
}

static void copy_value(char *dst, size_t dst_size, const char *value) {
    size_t len;
    if (!dst_size) return;
    value = value ? value : "";
    len = strlen(value);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, value, len);
    dst[len] = '\0';
}

static void set_status(const char *text) {
    if (g_ui.status_label) {
        SetWindowTextA(g_ui.status_label, text);
        UpdateWindow(g_ui.status_label);
    }
}

static void set_progress(int percent) {
    char text[32];
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    snprintf(text, sizeof(text), "%d%%", percent);
    if (g_ui.progress) {
        SetWindowTextA(g_ui.progress, text);
        UpdateWindow(g_ui.progress);
    }
}

static char *xstrdup(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, value, len + 1);
    return copy;
}

static void trim_in_place(char *text) {
    char *start = text;
    char *end;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
}

static void normalize_slashes(char *path) {
    char *p;
    for (p = path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
}

static void path_join(const char *a, const char *b, char *out, size_t out_size) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '\\' || a[len - 1] == '/')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s\\%s", a, b);
    }
}

static void replace_token(char *text, size_t text_size, const char *token, const char *value) {
    char buffer[FS_MAX_PATH];
    char *pos = strstr(text, token);
    if (!pos) return;
    *pos = '\0';
    snprintf(buffer, sizeof(buffer), "%s%s%s", text, value, pos + strlen(token));
    copy_value(text, text_size, buffer);
}

static void expand_install_dir(char *path, size_t path_size) {
    char folder[MAX_PATH] = {0};
    replace_token(path, path_size, "{app_name}", g_package.cfg.app_name);
    if (strstr(path, "{localappdata}")) {
        SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder);
        replace_token(path, path_size, "{localappdata}", folder);
    }
    if (strstr(path, "{appdata}")) {
        SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder);
        replace_token(path, path_size, "{appdata}", folder);
    }
    if (strstr(path, "{pf}")) {
        SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, folder);
        replace_token(path, path_size, "{pf}", folder);
    }
}

static int create_dir_recursive(const char *dir) {
    char temp[FS_MAX_PATH];
    char *p;
    copy_value(temp, sizeof(temp), dir);
    for (p = temp; *p; p++) {
        if (*p == '\\' || *p == '/') {
            char saved = *p;
            *p = '\0';
            if (strlen(temp) > 0 && temp[strlen(temp) - 1] != ':') {
                CreateDirectoryA(temp, NULL);
            }
            *p = saved;
        }
    }
    if (!CreateDirectoryA(temp, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            return 0;
        }
    }
    return 1;
}

static void dirname_of(const char *path, char *out, size_t out_size) {
    const char *slash1 = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    const char *slash = slash1 > slash2 ? slash1 : slash2;
    size_t len;
    if (!slash) {
        copy_value(out, out_size, ".");
        return;
    }
    len = (size_t)(slash - path);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static int create_parent_dirs(const char *file_path) {
    char dir[FS_MAX_PATH];
    dirname_of(file_path, dir, sizeof(dir));
    return create_dir_recursive(dir);
}

static int read_exact(FILE *f, void *data, size_t size) {
    return fread(data, 1, size, f) == size;
}

static void set_default_package_config(PackageConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->app_name, "FirstStand App");
    strcpy(cfg->app_version, "1.0.4");
    strcpy(cfg->publisher, "FirstStandStudio");
    strcpy(cfg->install_dir, "{localappdata}\\Programs\\{app_name}");
    strcpy(cfg->require_key, "false");
    strcpy(cfg->google_required, "false");
    strcpy(cfg->desktop_shortcut, "true");
    strcpy(cfg->start_menu_shortcut, "true");
    strcpy(cfg->run_after_install, "false");
}

static void assign_manifest_value(PackageConfig *cfg, const char *key, const char *value) {
    if (streq_ci(key, "app_name")) copy_value(cfg->app_name, sizeof(cfg->app_name), value);
    else if (streq_ci(key, "app_version")) copy_value(cfg->app_version, sizeof(cfg->app_version), value);
    else if (streq_ci(key, "publisher")) copy_value(cfg->publisher, sizeof(cfg->publisher), value);
    else if (streq_ci(key, "install_dir")) copy_value(cfg->install_dir, sizeof(cfg->install_dir), value);
    else if (streq_ci(key, "main_exe")) copy_value(cfg->main_exe, sizeof(cfg->main_exe), value);
    else if (streq_ci(key, "require_key")) copy_value(cfg->require_key, sizeof(cfg->require_key), value);
    else if (streq_ci(key, "activation_url")) copy_value(cfg->activation_url, sizeof(cfg->activation_url), value);
    else if (streq_ci(key, "api_key")) copy_value(cfg->api_key, sizeof(cfg->api_key), value);
    else if (streq_ci(key, "google_required")) copy_value(cfg->google_required, sizeof(cfg->google_required), value);
    else if (streq_ci(key, "desktop_shortcut")) copy_value(cfg->desktop_shortcut, sizeof(cfg->desktop_shortcut), value);
    else if (streq_ci(key, "start_menu_shortcut")) copy_value(cfg->start_menu_shortcut, sizeof(cfg->start_menu_shortcut), value);
    else if (streq_ci(key, "run_after_install")) copy_value(cfg->run_after_install, sizeof(cfg->run_after_install), value);
}

static int add_file_entry(Package *pkg, const char *rel_path, uint64_t offset, uint64_t size) {
    FileEntry *items = (FileEntry *)realloc(pkg->files, (pkg->file_count + 1) * sizeof(FileEntry));
    if (!items) return 0;
    pkg->files = items;
    pkg->files[pkg->file_count].rel_path = xstrdup(rel_path);
    pkg->files[pkg->file_count].offset = offset;
    pkg->files[pkg->file_count].size = size;
    pkg->file_count++;
    return 1;
}

static int parse_manifest(Package *pkg, char *manifest) {
    char *context = NULL;
    char *line = strtok_s(manifest, "\n", &context);
    while (line) {
        char *type;
        char *a;
        char *b;
        char *c;
        trim_in_place(line);
        if (line[0]) {
            type = line;
            a = strchr(type, '|');
            if (!a) return 0;
            *a++ = '\0';
            b = strchr(a, '|');
            if (!b) return 0;
            *b++ = '\0';
            c = strchr(b, '|');
            if (!c) return 0;
            *c++ = '\0';

            if (strcmp(type, "M") == 0) {
                assign_manifest_value(&pkg->cfg, a, b);
            } else if (strcmp(type, "F") == 0) {
                if (!add_file_entry(pkg, a, _strtoui64(b, NULL, 10), _strtoui64(c, NULL, 10))) {
                    return 0;
                }
            }
        }
        line = strtok_s(NULL, "\n", &context);
    }
    return 1;
}

static int load_package(Package *pkg) {
    FILE *f;
    FsFooter footer;
    uint64_t manifest_size;
    uint64_t file_count;
    char *manifest;

    memset(pkg, 0, sizeof(*pkg));
    set_default_package_config(&pkg->cfg);
    GetModuleFileNameA(NULL, pkg->self_path, sizeof(pkg->self_path));

    f = fopen(pkg->self_path, "rb");
    if (!f) return 0;
    _fseeki64(f, -(int64_t)sizeof(FsFooter), SEEK_END);
    if (!read_exact(f, &footer, sizeof(footer))) {
        fclose(f);
        return 0;
    }
    if (memcmp(footer.magic, FS_MAGIC, strlen(FS_MAGIC)) != 0 || footer.version != FS_ARCHIVE_VERSION) {
        fclose(f);
        return 0;
    }

    pkg->archive_offset = footer.archive_offset;
    _fseeki64(f, (int64_t)footer.archive_offset, SEEK_SET);
    if (!read_exact(f, &manifest_size, sizeof(manifest_size)) || !read_exact(f, &file_count, sizeof(file_count))) {
        fclose(f);
        return 0;
    }
    manifest = (char *)calloc((size_t)manifest_size + 1, 1);
    if (!manifest) {
        fclose(f);
        return 0;
    }
    if (!read_exact(f, manifest, (size_t)manifest_size)) {
        free(manifest);
        fclose(f);
        return 0;
    }
    pkg->manifest_size = manifest_size;
    pkg->data_offset = footer.archive_offset + sizeof(uint64_t) + sizeof(uint64_t) + manifest_size;
    if (!parse_manifest(pkg, manifest)) {
        free(manifest);
        fclose(f);
        return 0;
    }
    free(manifest);
    fclose(f);
    (void)file_count;
    return 1;
}

static void json_escape(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    while (*src && j + 2 < dst_size) {
        unsigned char ch = (unsigned char)*src++;
        if (ch == '"' || ch == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)ch;
        } else if (ch == '\n' || ch == '\r' || ch == '\t') {
            dst[j++] = ' ';
        } else {
            dst[j++] = (char)ch;
        }
    }
    dst[j] = '\0';
}

static int get_machine_name(char *out, size_t out_size) {
    DWORD size = (DWORD)out_size;
    if (!GetComputerNameA(out, &size)) {
        copy_value(out, out_size, "UNKNOWN-PC");
        return 0;
    }
    return 1;
}

static int utf8_to_wide(const char *src, wchar_t *dst, size_t dst_count) {
    int result = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int)dst_count);
    if (result <= 0) {
        result = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dst_count);
    }
    return result > 0;
}

static int append_response_body(HINTERNET request, char **out_body) {
    DWORD size = 0;
    DWORD downloaded = 0;
    char *body = NULL;
    size_t body_len = 0;

    do {
        if (!WinHttpQueryDataAvailable(request, &size)) break;
        if (size == 0) break;
        body = (char *)realloc(body, body_len + size + 1);
        if (!body) return 0;
        if (!WinHttpReadData(request, body + body_len, size, &downloaded)) break;
        body_len += downloaded;
        body[body_len] = '\0';
    } while (size > 0);

    *out_body = body ? body : xstrdup("");
    return 1;
}

static int activate_license(const char *product_key, const char *google_account, char **activation_body, char *error, size_t error_size) {
    URL_COMPONENTS url;
    wchar_t wurl[2048];
    wchar_t whost[512];
    wchar_t wpath[1024];
    HINTERNET session = NULL;
    HINTERNET connect = NULL;
    HINTERNET request = NULL;
    DWORD flags = 0;
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    char machine[256];
    char escaped_key[256];
    char escaped_google[512];
    char escaped_machine[256];
    char payload[2048];
    char headers[1024];
    wchar_t wheaders[1024];
    char *response_body = NULL;
    int ok = 0;
    if (activation_body) {
        *activation_body = NULL;
    }

    if (!utf8_to_wide(g_package.cfg.activation_url, wurl, sizeof(wurl) / sizeof(wurl[0]))) {
        copy_value(error, error_size, "Could not prepare activation URL.");
        return 0;
    }

    memset(&url, 0, sizeof(url));
    memset(whost, 0, sizeof(whost));
    memset(wpath, 0, sizeof(wpath));
    url.dwStructSize = sizeof(url);
    url.lpszHostName = whost;
    url.dwHostNameLength = sizeof(whost) / sizeof(whost[0]);
    url.lpszUrlPath = wpath;
    url.dwUrlPathLength = sizeof(wpath) / sizeof(wpath[0]);

    if (!WinHttpCrackUrl(wurl, 0, 0, &url)) {
        copy_value(error, error_size, "Invalid activation URL.");
        return 0;
    }

    get_machine_name(machine, sizeof(machine));
    json_escape(product_key, escaped_key, sizeof(escaped_key));
    json_escape(google_account, escaped_google, sizeof(escaped_google));
    json_escape(machine, escaped_machine, sizeof(escaped_machine));
    snprintf(
        payload,
        sizeof(payload),
        "{\"product_key\":\"%s\",\"machine_name\":\"%s\",\"google_account\":\"%s\"}",
        escaped_key,
        escaped_machine,
        escaped_google
    );

    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");
    if (g_package.cfg.api_key[0]) {
        strncat(headers, "X-Erire-Api-Key: ", sizeof(headers) - strlen(headers) - 1);
        strncat(headers, g_package.cfg.api_key, sizeof(headers) - strlen(headers) - 1);
        strncat(headers, "\r\n", sizeof(headers) - strlen(headers) - 1);
    }
    if (!utf8_to_wide(headers, wheaders, sizeof(wheaders) / sizeof(wheaders[0]))) {
        copy_value(error, error_size, "Could not prepare activation headers.");
        return 0;
    }

    session = WinHttpOpen(L"FirstStand-Installer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto done;
    connect = WinHttpConnect(session, whost, url.nPort, 0);
    if (!connect) goto done;
    if (url.nScheme == INTERNET_SCHEME_HTTPS) {
        flags |= WINHTTP_FLAG_SECURE;
    }
    request = WinHttpOpenRequest(connect, L"POST", wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) goto done;

    if (!WinHttpSendRequest(
            request,
            wheaders,
            (DWORD)-1L,
            (LPVOID)payload,
            (DWORD)strlen(payload),
            (DWORD)strlen(payload),
            0)) {
        goto done;
    }
    if (!WinHttpReceiveResponse(request, NULL)) goto done;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &status_size, NULL);
    append_response_body(request, &response_body);
    if (status >= 200 && status < 300) {
        ok = 1;
        if (activation_body) {
            *activation_body = response_body;
            response_body = NULL;
        }
    } else {
        snprintf(error, error_size, "Activation failed. Server returned HTTP %lu.%s%s", (unsigned long)status, response_body && response_body[0] ? "\n\n" : "", response_body ? response_body : "");
    }

done:
    if (!ok && error[0] == '\0') {
        snprintf(error, error_size, "Activation request failed. Windows error: %lu", (unsigned long)GetLastError());
    }
    free(response_body);
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static int write_activation_body(const char *install_dir, const char *body) {
    char license_dir[FS_MAX_PATH];
    char proof_path[FS_MAX_PATH];
    FILE *out;

    if (!body || !body[0]) {
        return 1;
    }
    path_join(install_dir, "license", license_dir, sizeof(license_dir));
    if (!create_dir_recursive(license_dir)) {
        return 0;
    }
    path_join(license_dir, "activation.json", proof_path, sizeof(proof_path));
    out = fopen(proof_path, "wb");
    if (!out) {
        return 0;
    }
    fwrite(body, 1, strlen(body), out);
    fwrite("\n", 1, 1, out);
    fclose(out);
    return 1;
}

static int current_username(char *out, size_t out_size) {
    DWORD size = (DWORD)out_size;
    if (!GetUserNameA(out, &size)) {
        copy_value(out, out_size, "UNKNOWN-USER");
        return 0;
    }
    return 1;
}

static int write_protected_file(const char *path, const char *text) {
    DATA_BLOB in;
    DATA_BLOB out;
    FILE *file;
    int ok = 0;

    in.pbData = (BYTE *)text;
    in.cbData = (DWORD)strlen(text);
    memset(&out, 0, sizeof(out));
    if (!CryptProtectData(&in, L"Erire Core license", NULL, NULL, NULL, 0, &out)) {
        return 0;
    }
    create_parent_dirs(path);
    file = fopen(path, "wb");
    if (file) {
        ok = fwrite(out.pbData, 1, out.cbData, file) == out.cbData;
        fclose(file);
    }
    LocalFree(out.pbData);
    return ok;
}

static int write_local_license_secret(const char *install_dir, const char *product_key, const char *google_account) {
    char license_dir[FS_MAX_PATH];
    char secret_path[FS_MAX_PATH];
    char studio_path[FS_MAX_PATH];
    char cli_path[FS_MAX_PATH];
    char username[256];
    char secret_text[12288];

    path_join(install_dir, "license", license_dir, sizeof(license_dir));
    path_join(license_dir, "secret.bin", secret_path, sizeof(secret_path));
    path_join(install_dir, "ErireStudio.exe", studio_path, sizeof(studio_path));
    path_join(install_dir, "erire.exe", cli_path, sizeof(cli_path));
    current_username(username, sizeof(username));

    snprintf(
        secret_text,
        sizeof(secret_text),
        "product_key=%s\n"
        "google_account=%s\n"
        "activation_url=%s\n"
        "api_key=%s\n"
        "username=%s\n"
        "studio_path=%s\n"
        "cli_path=%s\n",
        product_key,
        google_account,
        g_package.cfg.activation_url,
        g_package.cfg.api_key,
        username,
        studio_path,
        cli_path
    );
    return write_protected_file(secret_path, secret_text);
}

static int extract_files(const char *install_dir) {
    FILE *self = fopen(g_package.self_path, "rb");
    char buffer[65536];
    size_t i;
    if (!self) return 0;

    for (i = 0; i < g_package.file_count; i++) {
        FileEntry *entry = &g_package.files[i];
        char rel[FS_MAX_PATH];
        char out_path[FS_MAX_PATH];
        FILE *out;
        uint64_t remaining = entry->size;

        copy_value(rel, sizeof(rel), entry->rel_path);
        normalize_slashes(rel);
        path_join(install_dir, rel, out_path, sizeof(out_path));

        set_status(rel);
        set_progress((int)((i * 100) / (g_package.file_count ? g_package.file_count : 1)));
        if (!create_parent_dirs(out_path)) {
            fclose(self);
            return 0;
        }

        out = fopen(out_path, "wb");
        if (!out) {
            fclose(self);
            return 0;
        }

        _fseeki64(self, (int64_t)(g_package.data_offset + entry->offset), SEEK_SET);
        while (remaining > 0) {
            size_t want = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
            size_t got = fread(buffer, 1, want, self);
            if (got == 0) {
                fclose(out);
                fclose(self);
                return 0;
            }
            if (fwrite(buffer, 1, got, out) != got) {
                fclose(out);
                fclose(self);
                return 0;
            }
            remaining -= got;
        }
        fclose(out);
    }
    fclose(self);
    set_progress(100);
    return 1;
}

static int create_shortcut(const char *shortcut_path, const char *target_path, const char *working_dir) {
    IShellLinkA *link = NULL;
    IPersistFile *persist = NULL;
    wchar_t wide_shortcut[FS_MAX_PATH];
    HRESULT hr;
    int ok = 0;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&link);
    if (FAILED(hr)) goto done;
    link->lpVtbl->SetPath(link, target_path);
    link->lpVtbl->SetWorkingDirectory(link, working_dir);
    link->lpVtbl->SetDescription(link, g_package.cfg.app_name);
    hr = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&persist);
    if (FAILED(hr)) goto done;
    MultiByteToWideChar(CP_ACP, 0, shortcut_path, -1, wide_shortcut, FS_MAX_PATH);
    create_parent_dirs(shortcut_path);
    hr = persist->lpVtbl->Save(persist, wide_shortcut, TRUE);
    ok = SUCCEEDED(hr);

done:
    if (persist) persist->lpVtbl->Release(persist);
    if (link) link->lpVtbl->Release(link);
    CoUninitialize();
    return ok;
}

static void create_requested_shortcuts(const char *install_dir) {
    char target[FS_MAX_PATH];
    char desktop[MAX_PATH];
    char start_menu[MAX_PATH];
    char shortcut[FS_MAX_PATH];

    if (!g_package.cfg.main_exe[0]) return;
    path_join(install_dir, g_package.cfg.main_exe, target, sizeof(target));

    if (truthy(g_package.cfg.desktop_shortcut)) {
        if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop) == S_OK) {
            snprintf(shortcut, sizeof(shortcut), "%s\\%s.lnk", desktop, g_package.cfg.app_name);
            create_shortcut(shortcut, target, install_dir);
        }
    }
    if (truthy(g_package.cfg.start_menu_shortcut)) {
        if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, start_menu) == S_OK) {
            snprintf(shortcut, sizeof(shortcut), "%s\\%s\\%s.lnk", start_menu, g_package.cfg.publisher, g_package.cfg.app_name);
            create_shortcut(shortcut, target, install_dir);
        }
    }
}

static void run_installed_app(const char *install_dir) {
    char target[FS_MAX_PATH];
    if (!truthy(g_package.cfg.run_after_install) || !g_package.cfg.main_exe[0]) return;
    path_join(install_dir, g_package.cfg.main_exe, target, sizeof(target));
    ShellExecuteA(NULL, "open", target, NULL, install_dir, SW_SHOWNORMAL);
}

static void set_user_environment_value(const char *name, const char *value) {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Environment", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(key, name, 0, REG_SZ, (const BYTE *)value, (DWORD)strlen(value) + 1);
        RegCloseKey(key);
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    }
}

static void strip_trailing_slashes(char *path) {
    size_t len = strlen(path);
    while (len > 3 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        path[--len] = '\0';
    }
}

static void normalize_path_segment(char *path) {
    size_t len;
    trim_in_place(path);
    len = strlen(path);
    if (len >= 2 && path[0] == '"' && path[len - 1] == '"') {
        memmove(path, path + 1, len - 2);
        path[len - 2] = '\0';
    }
    normalize_slashes(path);
    strip_trailing_slashes(path);
    trim_in_place(path);
}

static int path_segment_matches(const char *segment, size_t segment_len, const char *install_dir) {
    char left[FS_MAX_PATH];
    char right[FS_MAX_PATH];
    if (segment_len >= sizeof(left)) return 0;
    memcpy(left, segment, segment_len);
    left[segment_len] = '\0';
    copy_value(right, sizeof(right), install_dir);
    normalize_path_segment(left);
    normalize_path_segment(right);
    return left[0] && _stricmp(left, right) == 0;
}

static int user_path_contains(const char *path_value, const char *install_dir) {
    const char *start = path_value;
    const char *p = path_value;
    while (1) {
        if (*p == ';' || *p == '\0') {
            if (path_segment_matches(start, (size_t)(p - start), install_dir)) {
                return 1;
            }
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    return 0;
}

static int add_install_dir_to_user_path(const char *install_dir) {
    HKEY key;
    LONG result;
    DWORD type = REG_EXPAND_SZ;
    DWORD size = 0;
    const char *value_name = "Path";
    char *current = NULL;
    char *updated = NULL;
    size_t current_len;
    size_t install_len;
    int needs_separator;
    int ok = 0;

    result = RegCreateKeyExA(HKEY_CURRENT_USER, "Environment", 0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL);
    if (result != ERROR_SUCCESS) {
        return 0;
    }

    result = RegQueryValueExA(key, value_name, 0, &type, NULL, &size);
    if (result == ERROR_FILE_NOT_FOUND) {
        value_name = "PATH";
        result = RegQueryValueExA(key, value_name, 0, &type, NULL, &size);
    }

    if (result == ERROR_FILE_NOT_FOUND) {
        current = xstrdup("");
        type = REG_EXPAND_SZ;
    } else if (result == ERROR_SUCCESS) {
        current = (char *)calloc((size_t)size + 2, 1);
        if (current) {
            if (RegQueryValueExA(key, value_name, 0, &type, (BYTE *)current, &size) != ERROR_SUCCESS) {
                free(current);
                current = NULL;
            } else {
                current[size] = '\0';
            }
        }
        if (type != REG_SZ && type != REG_EXPAND_SZ) {
            type = REG_EXPAND_SZ;
        }
    }

    if (!current) {
        RegCloseKey(key);
        return 0;
    }

    if (user_path_contains(current, install_dir)) {
        ok = 1;
    } else {
        current_len = strlen(current);
        install_len = strlen(install_dir);
        needs_separator = current_len > 0 && current[current_len - 1] != ';';
        updated = (char *)malloc(current_len + (needs_separator ? 1 : 0) + install_len + 1);
        if (updated) {
            memcpy(updated, current, current_len);
            if (needs_separator) {
                updated[current_len++] = ';';
            }
            memcpy(updated + current_len, install_dir, install_len + 1);
            ok = RegSetValueExA(key, value_name, 0, type, (const BYTE *)updated, (DWORD)strlen(updated) + 1) == ERROR_SUCCESS;
        }
    }

    free(updated);
    free(current);
    RegCloseKey(key);
    if (ok) {
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    }
    return ok;
}

static int write_install_environment(const char *install_dir, int add_to_path) {
    set_user_environment_value("ERIRE_IDE_HOME", install_dir);
    set_user_environment_value("ERIRE_STUDIO_HOME", install_dir);
    if (add_to_path && !add_install_dir_to_user_path(install_dir)) {
        return 0;
    }
    return 1;
}

static void browse_install_dir(void) {
    BROWSEINFOA bi;
    LPITEMIDLIST pidl;
    char selected[FS_MAX_PATH];
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_ui.hwnd;
    bi.lpszTitle = "Select installation folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        if (SHGetPathFromIDListA(pidl, selected)) {
            SetWindowTextA(g_ui.dir_edit, selected);
        }
        CoTaskMemFree(pidl);
    }
}

static void do_install(void) {
    char product_key[256] = {0};
    char google_account[512] = {0};
    char install_dir[FS_MAX_PATH] = {0};
    char error[4096] = {0};
    char success_message[512] = {0};
    char *activation_body = NULL;
    int add_to_path = 0;

    EnableWindow(g_ui.install_button, FALSE);
    GetWindowTextA(g_ui.key_edit, product_key, sizeof(product_key));
    GetWindowTextA(g_ui.google_edit, google_account, sizeof(google_account));
    GetWindowTextA(g_ui.dir_edit, install_dir, sizeof(install_dir));
    trim_in_place(product_key);
    trim_in_place(google_account);
    trim_in_place(install_dir);
    add_to_path = g_ui.path_check && SendMessageA(g_ui.path_check, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (truthy(g_package.cfg.require_key)) {
        if (!product_key[0]) {
            MessageBoxA(g_ui.hwnd, "Product key is required.", "FirstStand Installer", MB_ICONERROR);
            EnableWindow(g_ui.install_button, TRUE);
            return;
        }
        if (truthy(g_package.cfg.google_required) && !google_account[0]) {
            MessageBoxA(g_ui.hwnd, "Google account is required.", "FirstStand Installer", MB_ICONERROR);
            EnableWindow(g_ui.install_button, TRUE);
            return;
        }
        set_status("Activating product key...");
        if (!activate_license(product_key, google_account, &activation_body, error, sizeof(error))) {
            MessageBoxA(g_ui.hwnd, error, "Activation failed", MB_ICONERROR);
            EnableWindow(g_ui.install_button, TRUE);
            return;
        }
    }

    if (!install_dir[0]) {
        free(activation_body);
        MessageBoxA(g_ui.hwnd, "Installation folder is required.", "FirstStand Installer", MB_ICONERROR);
        EnableWindow(g_ui.install_button, TRUE);
        return;
    }

    set_status("Installing files...");
    if (!create_dir_recursive(install_dir) || !extract_files(install_dir)) {
        free(activation_body);
        MessageBoxA(g_ui.hwnd, "Could not install files. Try another folder or run as administrator.", "Install failed", MB_ICONERROR);
        EnableWindow(g_ui.install_button, TRUE);
        return;
    }
    if (activation_body) {
        set_status("Saving signed license proof...");
        if (!write_activation_body(install_dir, activation_body)) {
            free(activation_body);
            MessageBoxA(g_ui.hwnd, "Installed files, but could not save the activation proof.", "Install warning", MB_ICONWARNING);
            EnableWindow(g_ui.install_button, TRUE);
            return;
        }
        if (!write_local_license_secret(install_dir, product_key, google_account)) {
            free(activation_body);
            MessageBoxA(g_ui.hwnd, "Installed files, but could not save the protected local license secret.", "Install warning", MB_ICONWARNING);
            EnableWindow(g_ui.install_button, TRUE);
            return;
        }
        free(activation_body);
        activation_body = NULL;
    }
    set_status("Creating shortcuts...");
    create_requested_shortcuts(install_dir);
    set_status("Saving environment variables...");
    if (!write_install_environment(install_dir, add_to_path)) {
        MessageBoxA(g_ui.hwnd, "Installed files, but could not add the install folder to Path.", "Install warning", MB_ICONWARNING);
    }
    run_installed_app(install_dir);
    set_status("Installation complete.");
    snprintf(
        success_message,
        sizeof(success_message),
        "Installation completed successfully.\n\nEnvironment variables added:\nERIRE_IDE_HOME\nERIRE_STUDIO_HOME%s",
        add_to_path ? "\n\nInstall folder was added to user Path." : ""
    );
    MessageBoxA(g_ui.hwnd, success_message, "FirstStand Installer", MB_ICONINFORMATION);
    DestroyWindow(g_ui.hwnd);
}

typedef struct AccentPolicyCompat {
    int accent_state;
    int accent_flags;
    DWORD gradient_color;
    int animation_id;
} AccentPolicyCompat;

typedef struct WindowCompositionAttribDataCompat {
    int attrib;
    PVOID data;
    SIZE_T size;
} WindowCompositionAttribDataCompat;

typedef BOOL (WINAPI *SetWindowCompositionAttributeFn)(HWND, WindowCompositionAttribDataCompat *);

static void apply_window_acrylic(HWND hwnd) {
    (void)hwnd;
}

static COLORREF blend_color(COLORREF from, COLORREF to, int step, int steps) {
    int r = GetRValue(from) + (GetRValue(to) - GetRValue(from)) * step / steps;
    int g = GetGValue(from) + (GetGValue(to) - GetGValue(from)) * step / steps;
    int b = GetBValue(from) + (GetBValue(to) - GetBValue(from)) * step / steps;
    return RGB(r, g, b);
}

static void draw_filled_ellipse(HDC hdc, int left, int top, int right, int bottom, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    if (!brush) return;
    old_brush = SelectObject(hdc, brush);
    old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, left, top, right, bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
}

static void paint_installer_background(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    int y;
    HBRUSH card_brush;
    HPEN card_pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rc);

    for (y = 0; y < rc.bottom; y += 3) {
        RECT strip;
        HBRUSH brush;
        COLORREF color = blend_color(RGB(240, 246, 255), RGB(248, 250, 252), y, rc.bottom ? rc.bottom : 1);
        strip.left = 0;
        strip.top = y;
        strip.right = rc.right;
        strip.bottom = y + 3;
        brush = CreateSolidBrush(color);
        if (brush) {
            FillRect(hdc, &strip, brush);
            DeleteObject(brush);
        }
    }

    draw_filled_ellipse(hdc, -130, -120, 220, 180, RGB(214, 233, 255));
    draw_filled_ellipse(hdc, 450, -100, 760, 210, RGB(224, 238, 255));
    draw_filled_ellipse(hdc, 310, 350, 750, 650, RGB(223, 242, 236));

    card_brush = CreateSolidBrush(RGB(255, 255, 255));
    card_pen = CreatePen(PS_SOLID, 1, RGB(206, 216, 228));
    if (card_brush && card_pen) {
        old_brush = SelectObject(hdc, card_brush);
        old_pen = SelectObject(hdc, card_pen);
        RoundRect(hdc, 18, 18, rc.right - 18, rc.bottom - 18, 16, 16);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
    }
    if (card_brush) DeleteObject(card_brush);
    if (card_pen) DeleteObject(card_pen);

    {
        RECT accent = {30, 100, rc.right - 30, 104};
        HBRUSH brush = CreateSolidBrush(RGB(37, 99, 235));
        if (brush) {
            FillRect(hdc, &accent, brush);
            DeleteObject(brush);
        }
    }

    EndPaint(hwnd, &ps);
}

static HWND add_control(HWND parent, const char *class_name, const char *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND hwnd = CreateWindowExA(
        0,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL
    );
    if (g_ui.font) {
        SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_ui.font, TRUE);
    }
    return hwnd;
}

static void add_command_help_box(HWND hwnd, int x, int y, int w, int h) {
    static const char help_text[] =
        "erire <file.er>\r\n"
        "erire --run <file.er>\r\n"
        "erire --live <file.er>\r\n"
        "erire --check <file.er>\r\n"
        "erire build [--icon=logo.png] [--win_title=\"My App\"] <file.er> [output.exe]\r\n"
        "erire --build [--onefile] [--icon=logo.png] [--win_title=\"My App\"] <file.er> [output.exe]\r\n"
        "erire --new <ProjectName>\r\n"
        "erire --verify-key <product-key> <google-account>\r\n"
        "erire --console <profile>\r\n"
        "erire --console-help\r\n"
        "erire --tokens <file.er>\r\n"
        "erire --ast <file.er>";
    HWND box = add_control(
        hwnd,
        "EDIT",
        help_text,
        WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        x,
        y,
        w,
        h,
        ID_HELP_BOX
    );
    if (g_ui.help_font) {
        SendMessageA(box, WM_SETFONT, (WPARAM)g_ui.help_font, TRUE);
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    switch (msg) {
        case WM_CREATE: {
            char title[512];
            char install_dir[FS_MAX_PATH];
            int y = 30;
            g_ui.hwnd = hwnd;
            apply_window_acrylic(hwnd);
            g_ui.font = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_ui.title_font = CreateFontA(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_ui.help_font = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
            g_ui.bg_brush = CreateSolidBrush(RGB(255, 255, 255));
            g_ui.input_brush = CreateSolidBrush(RGB(255, 255, 255));
            g_ui.transparent_brush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);

            snprintf(title, sizeof(title), "%s Setup", g_package.cfg.app_name);
            HWND title_label = add_control(hwnd, "STATIC", title, SS_LEFT, 34, y, 500, 36, 0);
            SendMessageA(title_label, WM_SETFONT, (WPARAM)g_ui.title_font, TRUE);
            add_control(hwnd, "STATIC", "by Yahia Saad", SS_RIGHT, 596, y + 7, 150, 24, 0);
            y += 42;
            add_control(hwnd, "STATIC", "Official one-file installer with product key activation", SS_LEFT, 34, y, 712, 24, 0);
            y += 46;
            add_control(hwnd, "STATIC", "Install folder", SS_LEFT, 34, y, 180, 22, 0);
            y += 24;
            copy_value(install_dir, sizeof(install_dir), g_package.cfg.install_dir);
            expand_install_dir(install_dir, sizeof(install_dir));
            g_ui.dir_edit = add_control(hwnd, "EDIT", install_dir, WS_BORDER | ES_AUTOHSCROLL, 34, y, 560, 30, ID_EDIT_DIR);
            add_control(hwnd, "BUTTON", "Browse", BS_PUSHBUTTON, 606, y, 140, 30, ID_BUTTON_BROWSE);
            y += 50;

            add_control(hwnd, "STATIC", "Product key", SS_LEFT, 34, y, 180, 22, 0);
            y += 24;
            g_ui.key_edit = add_control(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 34, y, 712, 30, ID_EDIT_KEY);
            y += 50;

            add_control(hwnd, "STATIC", "Google account", SS_LEFT, 34, y, 180, 22, 0);
            y += 24;
            g_ui.google_edit = add_control(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 34, y, 712, 30, ID_EDIT_GOOGLE);
            y += 44;

            g_ui.path_check = add_control(hwnd, "BUTTON", "Add install folder to user Path", BS_AUTOCHECKBOX, 34, y, 712, 26, ID_CHECK_PATH);
            SendMessageA(g_ui.path_check, BM_SETCHECK, BST_CHECKED, 0);
            y += 38;

            add_control(hwnd, "STATIC", "Erire command help", SS_LEFT, 34, y, 240, 22, 0);
            y += 24;
            add_command_help_box(hwnd, 34, y, 712, 178);
            y += 194;

            if (!truthy(g_package.cfg.require_key)) {
                EnableWindow(g_ui.key_edit, FALSE);
                EnableWindow(g_ui.google_edit, FALSE);
            }

            g_ui.status_label = add_control(hwnd, "STATIC", "Ready.", SS_LEFT, 34, y, 712, 24, ID_STATUS);
            y += 28;
            g_ui.progress = add_control(hwnd, "STATIC", "0%", SS_CENTER | WS_BORDER, 34, y, 712, 26, ID_PROGRESS);

            g_ui.install_button = add_control(hwnd, "BUTTON", "Install", BS_DEFPUSHBUTTON, 526, 656, 100, 34, ID_BUTTON_INSTALL);
            add_control(hwnd, "BUTTON", "Cancel", BS_PUSHBUTTON, 646, 656, 100, 34, ID_BUTTON_CANCEL);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint_installer_background(hwnd);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(30, 41, 59));
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(255, 255, 255));
            return (LRESULT)g_ui.bg_brush;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(18, 24, 38));
            SetBkColor(hdc, RGB(255, 255, 255));
            return (LRESULT)g_ui.input_brush;
        }
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(30, 41, 59));
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(255, 255, 255));
            return (LRESULT)g_ui.bg_brush;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case ID_BUTTON_BROWSE:
                    browse_install_dir();
                    return 0;
                case ID_BUTTON_INSTALL:
                    do_install();
                    return 0;
                case ID_BUTTON_CANCEL:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        case WM_DESTROY:
            if (g_ui.font) DeleteObject(g_ui.font);
            if (g_ui.title_font) DeleteObject(g_ui.title_font);
            if (g_ui.help_font) DeleteObject(g_ui.help_font);
            if (g_ui.bg_brush) DeleteObject(g_ui.bg_brush);
            if (g_ui.input_brush) DeleteObject(g_ui.input_brush);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    char title[512];

    (void)prev_instance;
    (void)cmd_line;

    if (!load_package(&g_package)) {
        MessageBoxA(NULL, "This installer package is invalid or incomplete.", "FirstStand Installer", MB_ICONERROR);
        return 1;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "FirstStandInstallerWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(1));
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);

    snprintf(title, sizeof(title), "%s Setup", g_package.cfg.app_name);
    hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        790,
        745,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, show_cmd);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
