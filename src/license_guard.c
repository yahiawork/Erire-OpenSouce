#include "license_guard.h"

#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ER_LICENSE_MAX_PATH 4096

typedef struct ErLicensePaths {
    char module_path[ER_LICENSE_MAX_PATH];
    char install_dir[ER_LICENSE_MAX_PATH];
    char license_dir[ER_LICENSE_MAX_PATH];
    char secret_path[ER_LICENSE_MAX_PATH];
    char activation_path[ER_LICENSE_MAX_PATH];
    char studio_path[ER_LICENSE_MAX_PATH];
    char cli_path[ER_LICENSE_MAX_PATH];
} ErLicensePaths;

typedef struct ErLicenseSecret {
    char product_key[256];
    char google_account[512];
    char activation_url[1024];
    char api_key[512];
    char username[256];
    char studio_path[ER_LICENSE_MAX_PATH];
    char cli_path[ER_LICENSE_MAX_PATH];
    char studio_hash[65];
    char cli_hash[65];
    unsigned long long studio_size;
    unsigned long long cli_size;
    unsigned long long last_online_verify;
    unsigned long long vm_stamp;
} ErLicenseSecret;

typedef struct ErLicenseIntegrity {
    char username[256];
    char studio_hash[65];
    char cli_hash[65];
    unsigned long long studio_size;
    unsigned long long cli_size;
    unsigned long long vm_stamp;
} ErLicenseIntegrity;

static void er_copy(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst_size) return;
    src = src ? src : "";
    len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void er_set_error(char *error, size_t error_size, const char *message) {
    er_copy(error, error_size, message);
}

static void er_trim(char *text) {
    char *start;
    char *end;
    if (!text) return;
    start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
}

static void er_dirname(const char *path, char *out, size_t out_size) {
    const char *slash1 = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    const char *slash = slash1 > slash2 ? slash1 : slash2;
    size_t len;
    if (!slash) {
        er_copy(out, out_size, ".");
        return;
    }
    len = (size_t)(slash - path);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static void er_path_join(const char *a, const char *b, char *out, size_t out_size) {
    size_t len = strlen(a);
    if (len > 0 && (a[len - 1] == '\\' || a[len - 1] == '/')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s\\%s", a, b);
    }
}

static bool er_file_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static bool er_current_username(char *out, size_t out_size);

static bool er_debugger_present(void) {
    BOOL remote_debugger = FALSE;
    if (IsDebuggerPresent()) return true;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote_debugger);
    return remote_debugger ? true : false;
}

static unsigned long long er_validation_interval_seconds(void) {
    const char *value = getenv("ERIRE_VERIFY_INTERVAL_SECONDS");
    unsigned long long parsed = value && value[0] ? strtoull(value, NULL, 10) : 21600ULL;
    if (parsed < 300ULL) parsed = 300ULL;
    return parsed;
}

static unsigned long long er_fnv1a64(const char *text) {
    unsigned long long hash = 1469598103934665603ULL;
    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static unsigned long long er_rotl64(unsigned long long value, unsigned int shift) {
    shift &= 63U;
    if (shift == 0U) return value;
    return (value << shift) | (value >> (64U - shift));
}

static unsigned long long er_guard_vm_stamp(
    const char *username,
    const char *studio_hash,
    const char *cli_hash,
    unsigned long long studio_size,
    unsigned long long cli_size
) {
    static const unsigned char code[] = {
        1, 0, 5, 2, 1, 9, 3, 2, 17, 4, 3, 23, 5, 4, 29, 2, 0, 13, 4, 1, 31, 0
    };
    unsigned long long words[5];
    unsigned long long acc = 0xE1A1E1A1D06D5105ULL;
    size_t ip = 0;

    words[0] = er_fnv1a64(username);
    words[1] = er_fnv1a64(studio_hash);
    words[2] = er_fnv1a64(cli_hash);
    words[3] = studio_size;
    words[4] = cli_size;

    while (code[ip]) {
        unsigned char op = code[ip++];
        unsigned char index = code[ip++] % 5;
        unsigned char salt = code[ip++];
        if (op == 1) {
            acc ^= er_rotl64(words[index] + salt, salt);
        } else if (op == 2) {
            acc += er_rotl64(words[index] ^ 0x9E3779B97F4A7C15ULL, salt);
        } else if (op == 3) {
            acc *= 1099511628211ULL + salt;
            acc ^= words[index];
        } else if (op == 4) {
            acc = er_rotl64(acc ^ words[index], salt);
        } else if (op == 5) {
            acc ^= (acc >> 33);
            acc *= 0xff51afd7ed558ccdULL;
            acc ^= words[index];
        }
    }
    return acc ^ (acc >> 29);
}

static bool er_file_sha256_hex(const char *path, char out_hex[65], unsigned long long *out_size) {
    FILE *file = fopen(path, "rb");
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE digest[32];
    DWORD digest_size = sizeof(digest);
    unsigned char buffer[32768];
    size_t got;
    unsigned long long total = 0;
    static const char hex[] = "0123456789abcdef";
    int i;

    if (!file) return false;
    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        fclose(file);
        return false;
    }
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        fclose(file);
        return false;
    }
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        total += (unsigned long long)got;
        if (!CryptHashData(hash, buffer, (DWORD)got, 0)) {
            CryptDestroyHash(hash);
            CryptReleaseContext(provider, 0);
            fclose(file);
            return false;
        }
    }
    fclose(file);
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_size, 0) || digest_size != 32) {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        return false;
    }
    for (i = 0; i < 32; i++) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0x0f];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    out_hex[64] = '\0';
    if (out_size) *out_size = total;
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return true;
}

static bool er_collect_integrity(const ErLicensePaths *paths, ErLicenseIntegrity *integrity, char *error, size_t error_size) {
    memset(integrity, 0, sizeof(*integrity));
    er_current_username(integrity->username, sizeof(integrity->username));
    if (!er_file_sha256_hex(paths->studio_path, integrity->studio_hash, &integrity->studio_size)) {
        er_set_error(error, error_size, "Could not verify ErireStudio.exe integrity.");
        return false;
    }
    if (!er_file_sha256_hex(paths->cli_path, integrity->cli_hash, &integrity->cli_size)) {
        er_set_error(error, error_size, "Could not verify erire.exe integrity.");
        return false;
    }
    integrity->vm_stamp = er_guard_vm_stamp(
        integrity->username,
        integrity->studio_hash,
        integrity->cli_hash,
        integrity->studio_size,
        integrity->cli_size
    );
    return true;
}

static bool er_create_dir_recursive(const char *dir) {
    char temp[ER_LICENSE_MAX_PATH];
    char *p;
    er_copy(temp, sizeof(temp), dir);
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
        if (err != ERROR_ALREADY_EXISTS) return false;
    }
    return true;
}

static bool er_read_file(const char *path, unsigned char **out_data, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    long size;
    unsigned char *data;
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    rewind(file);
    data = (unsigned char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return false;
    }
    if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return false;
    }
    fclose(file);
    data[size] = '\0';
    *out_data = data;
    *out_size = (size_t)size;
    return true;
}

static bool er_write_file(const char *path, const void *data, size_t size) {
    FILE *file;
    char dir[ER_LICENSE_MAX_PATH];
    er_dirname(path, dir, sizeof(dir));
    if (!er_create_dir_recursive(dir)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (size > 0 && fwrite(data, 1, size, file) != size) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool er_current_username(char *out, size_t out_size) {
    DWORD size = (DWORD)out_size;
    if (!GetUserNameA(out, &size)) {
        er_copy(out, out_size, "UNKNOWN-USER");
        return false;
    }
    return true;
}

static bool er_machine_name(char *out, size_t out_size) {
    DWORD size = (DWORD)out_size;
    if (!GetComputerNameA(out, &size)) {
        er_copy(out, out_size, "UNKNOWN-PC");
        return false;
    }
    return true;
}

static bool er_license_paths(ErLicenseAppKind app_kind, ErLicensePaths *paths) {
    memset(paths, 0, sizeof(*paths));
    if (!GetModuleFileNameA(NULL, paths->module_path, sizeof(paths->module_path))) return false;
    er_dirname(paths->module_path, paths->install_dir, sizeof(paths->install_dir));
    er_path_join(paths->install_dir, "license", paths->license_dir, sizeof(paths->license_dir));
    er_path_join(paths->license_dir, "secret.bin", paths->secret_path, sizeof(paths->secret_path));
    er_path_join(paths->license_dir, "activation.json", paths->activation_path, sizeof(paths->activation_path));
    if (app_kind == ER_LICENSE_APP_STUDIO) {
        er_copy(paths->studio_path, sizeof(paths->studio_path), paths->module_path);
        er_path_join(paths->install_dir, "erire.exe", paths->cli_path, sizeof(paths->cli_path));
    } else {
        er_path_join(paths->install_dir, "ErireStudio.exe", paths->studio_path, sizeof(paths->studio_path));
        er_copy(paths->cli_path, sizeof(paths->cli_path), paths->module_path);
    }
    return true;
}

static bool er_dpapi_decrypt_file(const char *path, char **out_text) {
    unsigned char *encrypted = NULL;
    size_t encrypted_size = 0;
    DATA_BLOB in;
    DATA_BLOB out;
    char *text;
    if (!er_read_file(path, &encrypted, &encrypted_size)) return false;
    in.pbData = encrypted;
    in.cbData = (DWORD)encrypted_size;
    memset(&out, 0, sizeof(out));
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &out)) {
        free(encrypted);
        return false;
    }
    text = (char *)calloc((size_t)out.cbData + 1, 1);
    if (!text) {
        LocalFree(out.pbData);
        free(encrypted);
        return false;
    }
    memcpy(text, out.pbData, out.cbData);
    *out_text = text;
    LocalFree(out.pbData);
    free(encrypted);
    return true;
}

static bool er_dpapi_encrypt_file(const char *path, const char *text) {
    DATA_BLOB in;
    DATA_BLOB out;
    bool ok;
    in.pbData = (BYTE *)text;
    in.cbData = (DWORD)strlen(text);
    memset(&out, 0, sizeof(out));
    if (!CryptProtectData(&in, L"Erire Core license", NULL, NULL, NULL, 0, &out)) return false;
    ok = er_write_file(path, out.pbData, out.cbData);
    LocalFree(out.pbData);
    return ok;
}

static void er_secret_set(ErLicenseSecret *secret, const char *key, const char *value) {
    if (strcmp(key, "product_key") == 0) er_copy(secret->product_key, sizeof(secret->product_key), value);
    else if (strcmp(key, "account_email") == 0 || strcmp(key, "google_account") == 0) er_copy(secret->google_account, sizeof(secret->google_account), value);
    else if (strcmp(key, "activation_url") == 0) er_copy(secret->activation_url, sizeof(secret->activation_url), value);
    else if (strcmp(key, "api_key") == 0) er_copy(secret->api_key, sizeof(secret->api_key), value);
    else if (strcmp(key, "username") == 0) er_copy(secret->username, sizeof(secret->username), value);
    else if (strcmp(key, "studio_path") == 0) er_copy(secret->studio_path, sizeof(secret->studio_path), value);
    else if (strcmp(key, "cli_path") == 0) er_copy(secret->cli_path, sizeof(secret->cli_path), value);
    else if (strcmp(key, "studio_hash") == 0) er_copy(secret->studio_hash, sizeof(secret->studio_hash), value);
    else if (strcmp(key, "cli_hash") == 0) er_copy(secret->cli_hash, sizeof(secret->cli_hash), value);
    else if (strcmp(key, "studio_size") == 0) secret->studio_size = strtoull(value, NULL, 10);
    else if (strcmp(key, "cli_size") == 0) secret->cli_size = strtoull(value, NULL, 10);
    else if (strcmp(key, "last_online_verify") == 0) secret->last_online_verify = strtoull(value, NULL, 10);
    else if (strcmp(key, "vm_stamp") == 0) secret->vm_stamp = strtoull(value, NULL, 10);
}

static void er_parse_secret(const char *text, ErLicenseSecret *secret) {
    char *copy;
    char *line;
    memset(secret, 0, sizeof(*secret));
    copy = (char *)malloc(strlen(text) + 1);
    if (!copy) return;
    strcpy(copy, text);
    line = strtok(copy, "\n");
    while (line) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            er_trim(line);
            er_trim(eq + 1);
            er_secret_set(secret, line, eq + 1);
        }
        line = strtok(NULL, "\n");
    }
    free(copy);
}

static bool er_load_secret(const ErLicensePaths *paths, ErLicenseSecret *secret) {
    char *text = NULL;
    if (!er_dpapi_decrypt_file(paths->secret_path, &text)) return false;
    er_parse_secret(text, secret);
    free(text);
    return secret->product_key[0] && secret->google_account[0];
}

static bool er_save_secret(const ErLicensePaths *paths, const ErLicenseSecret *secret) {
    char text[12288];
    snprintf(text, sizeof(text),
        "product_key=%s\n"
        "google_account=%s\n"
        "activation_url=%s\n"
        "api_key=%s\n"
        "username=%s\n"
        "studio_path=%s\n"
        "cli_path=%s\n"
        "studio_hash=%s\n"
        "cli_hash=%s\n"
        "studio_size=%llu\n"
        "cli_size=%llu\n"
        "last_online_verify=%llu\n"
        "vm_stamp=%llu\n",
        secret->product_key,
        secret->google_account,
        secret->activation_url,
        secret->api_key,
        secret->username,
        secret->studio_path,
        secret->cli_path,
        secret->studio_hash,
        secret->cli_hash,
        secret->studio_size,
        secret->cli_size,
        secret->last_online_verify,
        secret->vm_stamp);
    return er_dpapi_encrypt_file(paths->secret_path, text);
}

static void er_json_escape(const char *src, char *dst, size_t dst_size) {
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

static bool er_utf8_to_wide(const char *src, wchar_t *dst, size_t dst_count) {
    int result = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int)dst_count);
    if (result <= 0) result = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dst_count);
    return result > 0;
}

static bool er_read_http_body(HINTERNET request, char **out_body) {
    DWORD size = 0;
    DWORD downloaded = 0;
    char *body = NULL;
    size_t body_len = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &size)) break;
        if (!size) break;
        body = (char *)realloc(body, body_len + size + 1);
        if (!body) return false;
        if (!WinHttpReadData(request, body + body_len, size, &downloaded)) break;
        body_len += downloaded;
        body[body_len] = '\0';
    } while (size > 0);
    if (!body) {
        body = (char *)calloc(1, 1);
        if (!body) return false;
    }
    *out_body = body;
    return true;
}

static void er_verify_url_from_activation_url(const char *activation_url, char *out, size_t out_size) {
    const char *fallback = "https://erire.pythonanywhere.com/api/verify";
    size_t len;
    er_copy(out, out_size, activation_url && activation_url[0] ? activation_url : fallback);
    len = strlen(out);
    if (len >= 13 && strcmp(out + len - 13, "/api/activate") == 0) {
        out[len - 13] = '\0';
        strncat(out, "/api/verify", out_size - strlen(out) - 1);
    } else if (len >= 9 && strcmp(out + len - 9, "/activate") == 0) {
        out[len - 9] = '\0';
        strncat(out, "/verify", out_size - strlen(out) - 1);
    }
}

static bool er_response_has_code(const char *response_body, const char *code) {
    char needle[128];
    if (!response_body || !response_body[0] || !code || !code[0]) return false;
    snprintf(needle, sizeof(needle), "\"code\":\"%s\"", code);
    return strstr(response_body, needle) != NULL;
}

static void er_format_http_license_error(DWORD status, const char *response_body, char *error, size_t error_size) {
    if (status == 409 && er_response_has_code(response_body, "not_activated")) {
        snprintf(error, error_size,
            "This product key is not activated yet.\n\n"
            "Activate the old product key, or enter a new product key and activate it.\n\n"
            "Run:\n"
            "  erire.exe --verify-key <product-key> <google-account>\n\n"
            "Then open Erire Studio again.");
        return;
    }

    snprintf(
        error,
        error_size,
        "License server rejected verification (HTTP %lu).%s%s",
        (unsigned long)status,
        response_body && response_body[0] ? "\n\n" : "",
        response_body ? response_body : ""
    );
}

static bool er_http_license_request(const char *url_text, const char *api_key, const char *product_key, const char *google_account, char **out_body, char *error, size_t error_size) {
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
    char headers[1200];
    wchar_t wheaders[1200];
    char *response_body = NULL;
    bool ok = false;

    if (out_body) *out_body = NULL;
    if (!er_utf8_to_wide(url_text, wurl, sizeof(wurl) / sizeof(wurl[0]))) {
        er_set_error(error, error_size, "Could not prepare activation URL.");
        return false;
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
        er_set_error(error, error_size, "Invalid activation URL.");
        return false;
    }

    er_machine_name(machine, sizeof(machine));
    er_json_escape(product_key, escaped_key, sizeof(escaped_key));
    er_json_escape(google_account, escaped_google, sizeof(escaped_google));
    er_json_escape(machine, escaped_machine, sizeof(escaped_machine));
    snprintf(payload, sizeof(payload), "{\"product_key\":\"%s\",\"machine_name\":\"%s\",\"google_account\":\"%s\"}", escaped_key, escaped_machine, escaped_google);

    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");
    if (api_key && api_key[0]) {
        strncat(headers, "X-Erire-Api-Key: ", sizeof(headers) - strlen(headers) - 1);
        strncat(headers, api_key, sizeof(headers) - strlen(headers) - 1);
        strncat(headers, "\r\n", sizeof(headers) - strlen(headers) - 1);
    }
    if (!er_utf8_to_wide(headers, wheaders, sizeof(wheaders) / sizeof(wheaders[0]))) {
        er_set_error(error, error_size, "Could not prepare activation headers.");
        return false;
    }

    session = WinHttpOpen(L"Erire-LicenseGuard/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto done;
    connect = WinHttpConnect(session, whost, url.nPort, 0);
    if (!connect) goto done;
    if (url.nScheme == INTERNET_SCHEME_HTTPS) flags |= WINHTTP_FLAG_SECURE;
    request = WinHttpOpenRequest(connect, L"POST", wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) goto done;
    if (!WinHttpSendRequest(request, wheaders, (DWORD)-1L, (LPVOID)payload, (DWORD)strlen(payload), (DWORD)strlen(payload), 0)) goto done;
    if (!WinHttpReceiveResponse(request, NULL)) goto done;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &status_size, NULL);
    er_read_http_body(request, &response_body);
    if (status >= 200 && status < 300) {
        ok = true;
        if (out_body) {
            *out_body = response_body;
            response_body = NULL;
        }
    } else {
        er_format_http_license_error(status, response_body, error, error_size);
    }

done:
    if (!ok && error && error[0] == '\0') {
        snprintf(error, error_size, "License verification failed. Windows error: %lu", (unsigned long)GetLastError());
    }
    free(response_body);
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static bool er_local_secret_matches(const ErLicensePaths *paths, const ErLicenseSecret *secret) {
    ErLicenseIntegrity integrity;
    unsigned long long now_value = (unsigned long long)time(NULL);
    char error_buffer[256] = {0};
    if (!er_collect_integrity(paths, &integrity, error_buffer, sizeof(error_buffer))) return false;
    return secret->product_key[0] &&
        secret->google_account[0] &&
        secret->username[0] &&
        _stricmp(secret->username, integrity.username) == 0 &&
        _stricmp(secret->studio_path, paths->studio_path) == 0 &&
        _stricmp(secret->cli_path, paths->cli_path) == 0 &&
        _stricmp(secret->studio_hash, integrity.studio_hash) == 0 &&
        _stricmp(secret->cli_hash, integrity.cli_hash) == 0 &&
        secret->studio_size == integrity.studio_size &&
        secret->cli_size == integrity.cli_size &&
        secret->vm_stamp == integrity.vm_stamp &&
        secret->last_online_verify > 0 &&
        now_value >= secret->last_online_verify &&
        (now_value - secret->last_online_verify) < er_validation_interval_seconds() &&
        er_file_exists(paths->studio_path) &&
        er_file_exists(paths->cli_path) &&
        er_file_exists(paths->activation_path);
}

static bool er_update_verified_secret(const ErLicensePaths *paths, ErLicenseSecret *secret, const char *activation_body) {
    ErLicenseIntegrity integrity;
    char error_buffer[256] = {0};
    if (!er_collect_integrity(paths, &integrity, error_buffer, sizeof(error_buffer))) return false;
    er_copy(secret->username, sizeof(secret->username), integrity.username);
    er_copy(secret->studio_path, sizeof(secret->studio_path), paths->studio_path);
    er_copy(secret->cli_path, sizeof(secret->cli_path), paths->cli_path);
    er_copy(secret->studio_hash, sizeof(secret->studio_hash), integrity.studio_hash);
    er_copy(secret->cli_hash, sizeof(secret->cli_hash), integrity.cli_hash);
    secret->studio_size = integrity.studio_size;
    secret->cli_size = integrity.cli_size;
    secret->vm_stamp = integrity.vm_stamp;
    secret->last_online_verify = (unsigned long long)time(NULL);
    if (!er_save_secret(paths, secret)) return false;
    if (activation_body && activation_body[0]) {
        return er_write_file(paths->activation_path, activation_body, strlen(activation_body));
    }
    return true;
}

bool er_license_guard_require(ErLicenseAppKind app_kind, char *error, size_t error_size) {
    ErLicensePaths paths;
    ErLicenseSecret secret;
    char verify_url[1200];
    char *activation_body = NULL;
    bool ok;

    if (er_debugger_present()) {
        er_set_error(error, error_size, "Debugging tools were detected. Close the debugger and start Erire again.");
        return false;
    }
    if (!er_license_paths(app_kind, &paths)) {
        er_set_error(error, error_size, "Could not locate Erire installation folder.");
        return false;
    }
    if (!er_load_secret(&paths, &secret)) {
        er_set_error(error, error_size,
            "Erire is not verified on this Windows account.\n\n"
            "Open Erire Studio from the installed folder, or run:\n"
            "  erire.exe --verify-key <product-key> <google-account>");
        return false;
    }
    if (er_local_secret_matches(&paths, &secret)) return true;

    er_verify_url_from_activation_url(secret.activation_url, verify_url, sizeof(verify_url));
    ok = er_http_license_request(verify_url, secret.api_key, secret.product_key, secret.google_account, &activation_body, error, error_size);
    if (!ok) {
        free(activation_body);
        return false;
    }
    if (!er_update_verified_secret(&paths, &secret, activation_body)) {
        er_set_error(error, error_size, "License was verified online, but Erire could not save the local verification state.");
        free(activation_body);
        return false;
    }
    free(activation_body);
    return true;
}

bool er_license_guard_verify_key(const char *product_key, const char *google_account, char *error, size_t error_size) {
    ErLicensePaths paths;
    ErLicenseSecret secret;
    char *activation_body = NULL;
    const char *env_url = getenv("ERIRE_ACTIVATION_URL");
    const char *env_token = getenv("ERIRE_ACTIVATION_TOKEN");
    const char *default_url = "https://erire.pythonanywhere.com/api/activate";

    if (er_debugger_present()) {
        er_set_error(error, error_size, "Debugging tools were detected. Close the debugger before product-key verification.");
        return false;
    }
    if (!product_key || !product_key[0] || !google_account || !google_account[0]) {
        er_set_error(error, error_size, "Product key and Google account are required.");
        return false;
    }
    if (!er_license_paths(ER_LICENSE_APP_CLI, &paths)) {
        er_set_error(error, error_size, "Could not locate Erire installation folder.");
        return false;
    }
    memset(&secret, 0, sizeof(secret));
    er_load_secret(&paths, &secret);
    er_copy(secret.product_key, sizeof(secret.product_key), product_key);
    er_copy(secret.google_account, sizeof(secret.google_account), google_account);
    if (!secret.activation_url[0]) er_copy(secret.activation_url, sizeof(secret.activation_url), env_url && env_url[0] ? env_url : default_url);
    if (!secret.api_key[0] && env_token && env_token[0]) er_copy(secret.api_key, sizeof(secret.api_key), env_token);

    if (!er_http_license_request(secret.activation_url, secret.api_key, secret.product_key, secret.google_account, &activation_body, error, error_size)) {
        free(activation_body);
        return false;
    }
    if (!er_update_verified_secret(&paths, &secret, activation_body)) {
        er_set_error(error, error_size, "Verified with the site, but could not save local license state.");
        free(activation_body);
        return false;
    }
    free(activation_body);
    return true;
}
