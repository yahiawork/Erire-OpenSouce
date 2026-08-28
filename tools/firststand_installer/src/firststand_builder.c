#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firststand_format.h"

#define FS_MAX_PATH 4096

typedef struct Config {
    char app_name[256];
    char app_version[64];
    char publisher[256];
    char source_dir[FS_MAX_PATH];
    char output[FS_MAX_PATH];
    char install_dir[FS_MAX_PATH];
    char main_exe[FS_MAX_PATH];
    char icon[FS_MAX_PATH];
    char stub_source[FS_MAX_PATH];
    char stub_path[FS_MAX_PATH];
    char gcc[FS_MAX_PATH];
    char windres[FS_MAX_PATH];
    char bash[FS_MAX_PATH];
    char use_msys_bash[16];
    char require_key[16];
    char activation_url[1024];
    char api_key[512];
    char google_required[16];
    char desktop_shortcut[16];
    char start_menu_shortcut[16];
    char run_after_install[16];
} Config;

typedef struct FileEntry {
    char *full_path;
    char *rel_path;
    uint64_t size;
    uint64_t offset;
} FileEntry;

typedef struct FileList {
    FileEntry *items;
    size_t count;
    size_t cap;
} FileList;

typedef struct Buffer {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

static void quote_arg(const char *arg, char *out, size_t out_size);

static void die(const char *message) {
    fprintf(stderr, "FirstStand-Installer: %s\n", message);
    exit(1);
}

static char *xstrdup(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        die("out of memory");
    }
    memcpy(copy, value, len + 1);
    return copy;
}

static void trim_in_place(char *text) {
    char *start = text;
    char *end;

    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
}

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

static void set_default_config(Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->app_name, "Erire Core");
    strcpy(cfg->app_version, "1.0.4");
    strcpy(cfg->publisher, "FirstStandStudio");
    strcpy(cfg->output, "dist\\ErireCore-Setup.exe");
    strcpy(cfg->install_dir, "{localappdata}\\Programs\\{app_name}");
    strcpy(cfg->main_exe, "ErireStudio.exe");
    strcpy(cfg->stub_source, "src\\firststand_stub.c");
    strcpy(cfg->gcc, "gcc");
    strcpy(cfg->windres, "windres");
    strcpy(cfg->bash, "C:\\msys64\\usr\\bin\\bash.exe");
    strcpy(cfg->use_msys_bash, "true");
    strcpy(cfg->require_key, "true");
    strcpy(cfg->google_required, "true");
    strcpy(cfg->desktop_shortcut, "true");
    strcpy(cfg->start_menu_shortcut, "true");
    strcpy(cfg->run_after_install, "false");
}

static void copy_value(char *dst, size_t dst_size, const char *value) {
    size_t len;
    if (dst_size == 0) {
        return;
    }
    len = strlen(value);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, value, len);
    dst[len] = '\0';
}

static void assign_config(Config *cfg, const char *key, const char *value) {
    if (streq_ci(key, "app_name")) copy_value(cfg->app_name, sizeof(cfg->app_name), value);
    else if (streq_ci(key, "app_version")) copy_value(cfg->app_version, sizeof(cfg->app_version), value);
    else if (streq_ci(key, "publisher")) copy_value(cfg->publisher, sizeof(cfg->publisher), value);
    else if (streq_ci(key, "source_dir")) copy_value(cfg->source_dir, sizeof(cfg->source_dir), value);
    else if (streq_ci(key, "output")) copy_value(cfg->output, sizeof(cfg->output), value);
    else if (streq_ci(key, "install_dir")) copy_value(cfg->install_dir, sizeof(cfg->install_dir), value);
    else if (streq_ci(key, "main_exe")) copy_value(cfg->main_exe, sizeof(cfg->main_exe), value);
    else if (streq_ci(key, "icon")) copy_value(cfg->icon, sizeof(cfg->icon), value);
    else if (streq_ci(key, "stub_source")) copy_value(cfg->stub_source, sizeof(cfg->stub_source), value);
    else if (streq_ci(key, "stub_path")) copy_value(cfg->stub_path, sizeof(cfg->stub_path), value);
    else if (streq_ci(key, "gcc")) copy_value(cfg->gcc, sizeof(cfg->gcc), value);
    else if (streq_ci(key, "windres")) copy_value(cfg->windres, sizeof(cfg->windres), value);
    else if (streq_ci(key, "bash")) copy_value(cfg->bash, sizeof(cfg->bash), value);
    else if (streq_ci(key, "use_msys_bash")) copy_value(cfg->use_msys_bash, sizeof(cfg->use_msys_bash), value);
    else if (streq_ci(key, "require_key")) copy_value(cfg->require_key, sizeof(cfg->require_key), value);
    else if (streq_ci(key, "activation_url")) copy_value(cfg->activation_url, sizeof(cfg->activation_url), value);
    else if (streq_ci(key, "api_key")) copy_value(cfg->api_key, sizeof(cfg->api_key), value);
    else if (streq_ci(key, "google_required")) copy_value(cfg->google_required, sizeof(cfg->google_required), value);
    else if (streq_ci(key, "desktop_shortcut")) copy_value(cfg->desktop_shortcut, sizeof(cfg->desktop_shortcut), value);
    else if (streq_ci(key, "start_menu_shortcut")) copy_value(cfg->start_menu_shortcut, sizeof(cfg->start_menu_shortcut), value);
    else if (streq_ci(key, "run_after_install")) copy_value(cfg->run_after_install, sizeof(cfg->run_after_install), value);
}

static void read_config(const char *path, Config *cfg) {
    FILE *f = fopen(path, "rb");
    char line[8192];
    unsigned long line_no = 0;

    if (!f) {
        fprintf(stderr, "Could not open config: %s\n", path);
        exit(1);
    }

    while (fgets(line, sizeof(line), f)) {
        char *equals;
        line_no++;
        trim_in_place(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';' || line[0] == '[') {
            continue;
        }
        equals = strchr(line, '=');
        if (!equals) {
            fprintf(stderr, "Ignoring invalid config line %lu: %s\n", line_no, line);
            continue;
        }
        *equals = '\0';
        trim_in_place(line);
        trim_in_place(equals + 1);
        assign_config(cfg, line, equals + 1);
    }

    fclose(f);

    if (cfg->source_dir[0] == '\0') {
        die("source_dir is required");
    }
    if (truthy(cfg->require_key) && cfg->activation_url[0] == '\0') {
        die("activation_url is required when require_key=true");
    }
}

static void path_dirname(const char *path, char *out, size_t out_size) {
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

static void make_dirs_for_file(const char *file_path) {
    char dir[FS_MAX_PATH];
    char current[FS_MAX_PATH];
    char *p;

    path_dirname(file_path, dir, sizeof(dir));
    if (dir[0] == '\0' || strcmp(dir, ".") == 0) {
        return;
    }

    copy_value(current, sizeof(current), dir);
    for (p = current; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (strlen(current) > 0 && current[strlen(current) - 1] != ':') {
                CreateDirectoryA(current, NULL);
            }
            *p = saved;
        }
    }
    CreateDirectoryA(current, NULL);
}

static void join_path(const char *a, const char *b, char *out, size_t out_size) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '\\' || a[len - 1] == '/')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s\\%s", a, b);
    }
}

static void normalize_rel_path(char *path) {
    char *p;
    for (p = path; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
}

static uint64_t file_size_u64(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER size;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        fprintf(stderr, "Could not stat file: %s\n", path);
        exit(1);
    }
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart;
}

static void file_list_push(FileList *list, const char *full_path, const char *rel_path) {
    FileEntry *item;
    if (list->count == list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 64;
        FileEntry *new_items = (FileEntry *)realloc(list->items, new_cap * sizeof(FileEntry));
        if (!new_items) {
            die("out of memory");
        }
        list->items = new_items;
        list->cap = new_cap;
    }
    item = &list->items[list->count++];
    item->full_path = xstrdup(full_path);
    item->rel_path = xstrdup(rel_path);
    item->size = file_size_u64(full_path);
    item->offset = 0;
}

static void scan_dir(FileList *list, const char *root, const char *relative) {
    char search[FS_MAX_PATH];
    WIN32_FIND_DATAA data;
    HANDLE handle;

    if (relative[0]) {
        char base[FS_MAX_PATH];
        join_path(root, relative, base, sizeof(base));
        join_path(base, "*", search, sizeof(search));
    } else {
        join_path(root, "*", search, sizeof(search));
    }

    handle = FindFirstFileA(search, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not scan source directory: %s\n", search);
        exit(1);
    }

    do {
        char child_rel[FS_MAX_PATH];
        char child_full[FS_MAX_PATH];
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        if (relative[0]) {
            snprintf(child_rel, sizeof(child_rel), "%s\\%s", relative, data.cFileName);
        } else {
            copy_value(child_rel, sizeof(child_rel), data.cFileName);
        }
        join_path(root, child_rel, child_full, sizeof(child_full));
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_dir(list, root, child_rel);
        } else {
            normalize_rel_path(child_rel);
            file_list_push(list, child_full, child_rel);
        }
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
}

static void buffer_append(Buffer *buffer, const char *text) {
    size_t need = strlen(text);
    if (buffer->len + need + 1 > buffer->cap) {
        size_t new_cap = buffer->cap ? buffer->cap * 2 : 4096;
        while (new_cap < buffer->len + need + 1) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(buffer->data, new_cap);
        if (!new_data) {
            die("out of memory");
        }
        buffer->data = new_data;
        buffer->cap = new_cap;
    }
    memcpy(buffer->data + buffer->len, text, need);
    buffer->len += need;
    buffer->data[buffer->len] = '\0';
}

static void manifest_add(Buffer *buffer, const char *type, const char *a, const char *b, const char *c) {
    char line[8192];
    snprintf(line, sizeof(line), "%s|%s|%s|%s\n", type, a ? a : "", b ? b : "", c ? c : "");
    buffer_append(buffer, line);
}

static void build_manifest(const Config *cfg, FileList *files, Buffer *manifest) {
    size_t i;
    uint64_t offset = 0;
    char temp[64];

    manifest_add(manifest, "M", "app_name", cfg->app_name, "");
    manifest_add(manifest, "M", "app_version", cfg->app_version, "");
    manifest_add(manifest, "M", "publisher", cfg->publisher, "");
    manifest_add(manifest, "M", "install_dir", cfg->install_dir, "");
    manifest_add(manifest, "M", "main_exe", cfg->main_exe, "");
    manifest_add(manifest, "M", "require_key", truthy(cfg->require_key) ? "true" : "false", "");
    manifest_add(manifest, "M", "activation_url", cfg->activation_url, "");
    manifest_add(manifest, "M", "api_key", cfg->api_key, "");
    manifest_add(manifest, "M", "google_required", truthy(cfg->google_required) ? "true" : "false", "");
    manifest_add(manifest, "M", "desktop_shortcut", truthy(cfg->desktop_shortcut) ? "true" : "false", "");
    manifest_add(manifest, "M", "start_menu_shortcut", truthy(cfg->start_menu_shortcut) ? "true" : "false", "");
    manifest_add(manifest, "M", "run_after_install", truthy(cfg->run_after_install) ? "true" : "false", "");

    for (i = 0; i < files->count; i++) {
        files->items[i].offset = offset;
        snprintf(temp, sizeof(temp), "%llu", (unsigned long long)offset);
        {
            char size_text[64];
            snprintf(size_text, sizeof(size_text), "%llu", (unsigned long long)files->items[i].size);
            manifest_add(manifest, "F", files->items[i].rel_path, temp, size_text);
        }
        offset += files->items[i].size;
    }
}

static void copy_stream(FILE *in, FILE *out, uint64_t bytes) {
    char buffer[65536];
    while (bytes > 0) {
        size_t want = bytes > sizeof(buffer) ? sizeof(buffer) : (size_t)bytes;
        size_t got = fread(buffer, 1, want, in);
        if (got == 0) {
            die("unexpected end of file while copying");
        }
        if (fwrite(buffer, 1, got, out) != got) {
            die("could not write output");
        }
        bytes -= got;
    }
}

static void copy_file_to_output(const char *path, FILE *out) {
    FILE *in = fopen(path, "rb");
    uint64_t size;
    if (!in) {
        fprintf(stderr, "Could not open file: %s\n", path);
        exit(1);
    }
    size = file_size_u64(path);
    copy_stream(in, out, size);
    fclose(in);
}

static void rc_escape_path(const char *path, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;
    for (i = 0; path[i] && j + 1 < out_size; i++) {
        char ch = path[i] == '\\' ? '/' : path[i];
        out[j++] = ch;
    }
    out[j] = '\0';
}

static int run_command(const char *command) {
    printf("%s\n", command);
    return system(command);
}

static void to_msys_path(const char *path, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        if (j + 3 < out_size) {
            out[j++] = '/';
            out[j++] = (char)tolower((unsigned char)path[0]);
            path += 2;
        }
    }
    for (i = 0; path[i] && j + 1 < out_size; i++) {
        out[j++] = path[i] == '\\' ? '/' : path[i];
    }
    out[j] = '\0';
}

static void bash_quote(const char *arg, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;
    if (j < out_size) out[j++] = '\'';
    for (i = 0; arg[i] && j + 5 < out_size; i++) {
        if (arg[i] == '\'') {
            out[j++] = '\'';
            out[j++] = '\\';
            out[j++] = '\'';
            out[j++] = '\'';
        } else {
            out[j++] = arg[i];
        }
    }
    if (j < out_size) out[j++] = '\'';
    if (j < out_size) out[j] = '\0';
    else out[out_size - 1] = '\0';
}

static int run_bash_command(const Config *cfg, const char *inner_command) {
    char cwd[FS_MAX_PATH];
    char cwd_msys[FS_MAX_PATH];
    char q_cwd[FS_MAX_PATH + 8];
    char full_inner[FS_MAX_PATH * 5];
    char command[FS_MAX_PATH * 6];

    GetCurrentDirectoryA(sizeof(cwd), cwd);
    to_msys_path(cwd, cwd_msys, sizeof(cwd_msys));
    bash_quote(cwd_msys, q_cwd, sizeof(q_cwd));

    snprintf(
        full_inner,
        sizeof(full_inner),
        "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd %s && %s",
        q_cwd,
        inner_command
    );
    snprintf(command, sizeof(command), "%s -lc \"%s\"", cfg->bash, full_inner);
    return run_command(command);
}

static void quote_arg(const char *arg, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;
    if (j < out_size) out[j++] = '"';
    for (i = 0; arg[i] && j + 2 < out_size; i++) {
        if (arg[i] == '"') {
            out[j++] = '\\';
        }
        out[j++] = arg[i];
    }
    if (j < out_size) out[j++] = '"';
    if (j < out_size) out[j] = '\0';
    else out[out_size - 1] = '\0';
}

static int compile_stub(const Config *cfg, char *stub_out, size_t stub_out_size) {
    char temp_dir[FS_MAX_PATH];
    char rc_path[FS_MAX_PATH];
    char res_path[FS_MAX_PATH];
    char exe_path[FS_MAX_PATH];
    char icon_path[FS_MAX_PATH];
    char q_windres[FS_MAX_PATH + 8];
    char q_gcc[FS_MAX_PATH + 8];
    char q_rc[FS_MAX_PATH + 8];
    char q_res[FS_MAX_PATH + 8];
    char q_exe[FS_MAX_PATH + 8];
    char q_src[FS_MAX_PATH + 8];
    char command[FS_MAX_PATH * 4];
    FILE *rc;
    DWORD tick = GetTickCount();

    GetTempPathA(sizeof(temp_dir), temp_dir);
    snprintf(rc_path, sizeof(rc_path), "%sfirststand_%lu.rc", temp_dir, (unsigned long)tick);
    snprintf(res_path, sizeof(res_path), "%sfirststand_%lu_res.o", temp_dir, (unsigned long)tick);
    snprintf(exe_path, sizeof(exe_path), "%sfirststand_%lu_stub.exe", temp_dir, (unsigned long)tick);

    rc = fopen(rc_path, "wb");
    if (!rc) {
        return 0;
    }

    if (cfg->icon[0]) {
        rc_escape_path(cfg->icon, icon_path, sizeof(icon_path));
        fprintf(rc, "1 ICON \"%s\"\n", icon_path);
    }
    fprintf(rc, "1 VERSIONINFO\n");
    fprintf(rc, "FILEVERSION 1,0,0,0\n");
    fprintf(rc, "PRODUCTVERSION 1,0,0,0\n");
    fprintf(rc, "BEGIN\n");
    fprintf(rc, "  BLOCK \"StringFileInfo\"\n");
    fprintf(rc, "  BEGIN\n");
    fprintf(rc, "    BLOCK \"040904b0\"\n");
    fprintf(rc, "    BEGIN\n");
    fprintf(rc, "      VALUE \"CompanyName\", \"%s\"\n", cfg->publisher);
    fprintf(rc, "      VALUE \"FileDescription\", \"%s Installer\"\n", cfg->app_name);
    fprintf(rc, "      VALUE \"ProductName\", \"%s\"\n", cfg->app_name);
    fprintf(rc, "    END\n");
    fprintf(rc, "  END\n");
    fprintf(rc, "  BLOCK \"VarFileInfo\"\n");
    fprintf(rc, "  BEGIN\n");
    fprintf(rc, "    VALUE \"Translation\", 0x409, 1200\n");
    fprintf(rc, "  END\n");
    fprintf(rc, "END\n");
    fclose(rc);

    if (truthy(cfg->use_msys_bash)) {
        char msys_rc[FS_MAX_PATH], msys_res[FS_MAX_PATH], msys_exe[FS_MAX_PATH], msys_src[FS_MAX_PATH];
        to_msys_path(rc_path, msys_rc, sizeof(msys_rc));
        to_msys_path(res_path, msys_res, sizeof(msys_res));
        to_msys_path(exe_path, msys_exe, sizeof(msys_exe));
        to_msys_path(cfg->stub_source, msys_src, sizeof(msys_src));
        bash_quote(msys_rc, q_rc, sizeof(q_rc));
        bash_quote(msys_res, q_res, sizeof(q_res));
        bash_quote(msys_exe, q_exe, sizeof(q_exe));
        bash_quote(msys_src, q_src, sizeof(q_src));
        snprintf(command, sizeof(command), "windres -i %s -O coff -o %s", q_rc, q_res);
        if (run_bash_command(cfg, command) != 0) {
            remove(rc_path);
            return 0;
        }
        snprintf(
            command,
            sizeof(command),
            "gcc -std=c11 -O2 -Wall -Wextra -Wno-format-truncation -mwindows -o %s %s %s -luser32 -lgdi32 -lcomdlg32 -lshell32 -lole32 -ladvapi32 -lwinhttp -luuid -lcrypt32",
            q_exe,
            q_src,
            q_res
        );
        if (run_bash_command(cfg, command) != 0) {
            remove(rc_path);
            remove(res_path);
            return 0;
        }
    } else {
        quote_arg(cfg->windres, q_windres, sizeof(q_windres));
        quote_arg(cfg->gcc, q_gcc, sizeof(q_gcc));
        quote_arg(rc_path, q_rc, sizeof(q_rc));
        quote_arg(res_path, q_res, sizeof(q_res));
        quote_arg(exe_path, q_exe, sizeof(q_exe));
        quote_arg(cfg->stub_source, q_src, sizeof(q_src));

        snprintf(command, sizeof(command), "%s -i %s -O coff -o %s", q_windres, q_rc, q_res);
        if (run_command(command) != 0) {
            remove(rc_path);
            return 0;
        }

        snprintf(
            command,
            sizeof(command),
            "%s -std=c11 -O2 -Wall -Wextra -Wno-format-truncation -mwindows -o %s %s %s -luser32 -lgdi32 -lcomdlg32 -lshell32 -lole32 -ladvapi32 -lwinhttp -luuid -lcrypt32",
            q_gcc,
            q_exe,
            q_src,
            q_res
        );
        if (run_command(command) != 0) {
            remove(rc_path);
            remove(res_path);
            return 0;
        }
    }

    copy_value(stub_out, stub_out_size, exe_path);
    remove(rc_path);
    remove(res_path);
    return 1;
}

static void write_package(const Config *cfg, const FileList *files, const Buffer *manifest, const char *stub_exe) {
    FILE *out;
    FsFooter footer;
    uint64_t archive_offset;
    uint64_t archive_end;
    uint64_t manifest_size = (uint64_t)manifest->len;
    uint64_t file_count = (uint64_t)files->count;
    size_t i;

    make_dirs_for_file(cfg->output);

    out = fopen(cfg->output, "wb");
    if (!out) {
        fprintf(stderr, "Could not create output: %s\n", cfg->output);
        exit(1);
    }

    copy_file_to_output(stub_exe, out);
    archive_offset = (uint64_t)_ftelli64(out);

    fwrite(&manifest_size, sizeof(manifest_size), 1, out);
    fwrite(&file_count, sizeof(file_count), 1, out);
    fwrite(manifest->data, 1, manifest->len, out);

    for (i = 0; i < files->count; i++) {
        printf("Packing %s\n", files->items[i].rel_path);
        copy_file_to_output(files->items[i].full_path, out);
    }

    archive_end = (uint64_t)_ftelli64(out);
    memset(&footer, 0, sizeof(footer));
    memcpy(footer.magic, FS_MAGIC, strlen(FS_MAGIC));
    footer.archive_offset = archive_offset;
    footer.archive_size = archive_end - archive_offset;
    footer.version = FS_ARCHIVE_VERSION;
    footer.flags = 0;
    fwrite(&footer, sizeof(footer), 1, out);
    fclose(out);

    printf("\nCreated installer: %s\n", cfg->output);
    printf("Files: %llu\n", (unsigned long long)files->count);
}

static void free_file_list(FileList *list) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->items[i].full_path);
        free(list->items[i].rel_path);
    }
    free(list->items);
}

int main(int argc, char **argv) {
    Config cfg;
    FileList files = {0};
    Buffer manifest = {0};
    char stub_exe[FS_MAX_PATH] = {0};
    int compiled_stub = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: FirstStand-Installer.exe path\\to\\installer.ini\n");
        return 2;
    }

    set_default_config(&cfg);
    read_config(argv[1], &cfg);

    printf("FirstStand Installer Builder\n");
    printf("App: %s %s\n", cfg.app_name, cfg.app_version);
    printf("Source: %s\n", cfg.source_dir);

    scan_dir(&files, cfg.source_dir, "");
    if (files.count == 0) {
        die("source_dir has no files");
    }
    build_manifest(&cfg, &files, &manifest);

    if (cfg.stub_source[0]) {
        compiled_stub = compile_stub(&cfg, stub_exe, sizeof(stub_exe));
    }
    if (!compiled_stub) {
        if (!cfg.stub_path[0]) {
            die("could not compile installer stub and no stub_path fallback was configured");
        }
        copy_value(stub_exe, sizeof(stub_exe), cfg.stub_path);
    }

    write_package(&cfg, &files, &manifest, stub_exe);

    if (compiled_stub) {
        remove(stub_exe);
    }
    free(manifest.data);
    free_file_list(&files);
    return 0;
}
