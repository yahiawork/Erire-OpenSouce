#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "console.h"
#include "fileio.h"
#include "frontend.h"
#include "ide_highlight.h"
#include "lexer.h"
#include "license_guard.h"
#include "module.h"
#include "packager.h"
#include "runtime.h"

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
static void erire_enable_ansi(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}
#endif

static void erire_print_usage(void) {
    puts("\033[1;34m╔══════════════════════════════════════════════════════════╗\033[0m");
    puts("\033[1;34m║\033[0m  \033[1;37mErire CLI\033[0m — Modern Native & GUI Language Runtime      \033[1;34m║\033[0m");
    puts("\033[1;34m║\033[0m  \033[90mCreated by Yahia Saad | Erire ©2026\033[0m                     \033[1;34m║\033[0m");
    puts("\033[1;34m╚══════════════════════════════════════════════════════════╝\033[0m");
    puts("\033[90mVersion 1.0.5 [x86_64-win32]\033[0m\n");
    puts("\033[1;33mUsage:\033[0m");
    puts("  \033[1;32merire\033[0m <file.er>                                  Run an Erire script");
    puts("  \033[1;32merire --live\033[0m <file.er>                           Run in Live Hot-Reload mode");
    puts("  \033[1;32merire --check\033[0m <file.er>                          Validate syntax & semantics");
    puts("  \033[1;32merire --new\033[0m <ProjectName>                        Scaffold a new project template");
    puts("  \033[1;32merire --install-highlight\033[0m                        Install IDE syntax highlighting (VS Code, Antigravity, Cursor)");
    puts("  \033[1;32merire build\033[0m [--onefile] [--icon=...] <file.er>   Compile to native Windows .exe");
    puts("");
    puts("\033[1;33mDiagnostics & Tools:\033[0m");
    puts("  \033[1merire --tokens\033[0m <file.er>                         Dump lexer tokens");
    puts("  \033[1merire --ast\033[0m <file.er>                            Dump Abstract Syntax Tree");
    puts("  \033[1merire --console\033[0m <profile>                        Launch terminal profile");
    puts("  \033[1merire --console-help\033[0m                             List available console profiles");
    puts("  \033[1merire --version\033[0m                                  Show version information");
    puts("");
    puts("\033[1;33mBuild Options:\033[0m");
    puts("  \033[36m--onefile\033[0m        Bundle .er, Python modules, assets & native helpers into a single .exe");
    puts("  \033[36m--icon=FILE\033[0m      Set custom executable icon (.ico or .png)");
    puts("  \033[36m--win_title=TXT\033[0m  Override default window title");
}


static int erire_open_console(const char *profile) {
    ErError error;
    er_error_clear(&error);

    if (!er_console_launch_profile(profile, &error)) {
        er_error_print(stderr, "console", &error);
        return 1;
    }

    printf("launched console profile: %s\n", profile);
    return 0;
}

static int erire_dump_tokens(const char *path) {
    ErModuleSource module;
    ErTokenArray tokens;
    ErError error;

    er_error_clear(&error);
    if (!er_module_load_entry(path, &module, &error)) {
        er_error_print(stderr, path, &error);
        return 1;
    }

    if (!er_lexer_tokenize(module.normalized_path, module.source, &tokens, &error)) {
        er_error_print(stderr, module.normalized_path, &error);
        er_module_source_free(&module);
        return 1;
    }

    er_lexer_dump(stdout, &tokens);
    er_token_array_free(&tokens);
    er_module_source_free(&module);
    return 0;
}

static int erire_dump_ast(const char *path) {
    ErFrontendUnit unit;
    ErError error;

    er_error_clear(&error);
    if (!er_frontend_load_file(path, &unit, &error)) {
        er_error_print(stderr, path, &error);
        return 1;
    }

    er_program_dump(stdout, unit.program);
    er_frontend_unit_free(&unit);
    return 0;
}

static int erire_check(const char *path) {
    ErError error;
    er_error_clear(&error);

    if (!er_runtime_check_file(path, &error)) {
        er_error_print(stderr, path, &error);
        return 1;
    }

    printf("\033[1;32m[OK] ok:\033[0m %s\n", path);
    return 0;
}


static int erire_run(const char *path) {
    ErError error;
    er_error_clear(&error);

    if (er_runtime_run_file(path, &error) != 0) {
        er_error_print(stderr, path, &error);
        return 1;
    }

    return 0;
}

static int erire_run_live(const char *path) {
    ErError error;
    er_error_clear(&error);

    if (er_runtime_run_file_live(path, 300u, &error) != 0) {
        er_error_print(stderr, path, &error);
        return 1;
    }

    return 0;
}

static bool erire_flag_value(const char *argument, const char *name, const char **out_value) {
    size_t name_length;

    if (!argument || !name || !out_value) {
        return false;
    }

    name_length = strlen(name);
    if (strncmp(argument, name, name_length) != 0) {
        return false;
    }
    if (argument[name_length] != '=') {
        return false;
    }

    *out_value = argument + name_length + 1;
    return true;
}

static int erire_build(
    const char *source_path,
    const char *output_path,
    const char *icon_override,
    const char *win_title_override,
    bool onefile
) {
    ErError error;
    ErPackagerBuildOptions options;
    char default_name[256];
    char final_output[1024];
    char out_dir[1024];

    er_error_clear(&error);

    if (!er_path_has_extension(source_path, ".er")) {
        fprintf(stderr, "\033[1;31merror\033[0m: build expects an .er source file\n");
        return 1;
    }
    if (!er_runtime_check_file(source_path, &error)) {
        er_error_print(stderr, source_path, &error);
        return 1;
    }

    if (output_path && output_path[0] != '\0') {
        strncpy(final_output, output_path, sizeof(final_output) - 1);
        final_output[sizeof(final_output) - 1] = '\0';
    } else {
        er_path_basename_without_extension(source_path, default_name, sizeof(default_name));
        if (!er_directory_create_recursive("dist", &error)) {
            er_error_print(stderr, "dist", &error);
            return 1;
        }
        snprintf(final_output, sizeof(final_output), "dist\\%s.exe", default_name);
    }

    er_path_dirname(final_output, out_dir, sizeof(out_dir));
    if (!er_directory_create_recursive(out_dir, &error)) {
        er_error_print(stderr, out_dir, &error);
        return 1;
    }

    memset(&options, 0, sizeof(options));
    options.icon_path_override = icon_override;
    options.win_title_override = win_title_override;
    options.onefile = onefile;

    if (!er_packager_build(source_path, final_output, &options, &error)) {
        er_error_print(stderr, final_output, &error);
        return 1;
    }

    printf("\033[1;32m[OK] built executable:\033[0m %s\n", final_output);
    printf("  \033[90mpackage:\033[0m %s executable (entry .er + bundled resources + Python helpers)\n", options.onefile ? "onefile" : "single-file");
    printf("  \033[1;36mstatus:\033[0m ready to run\n");
    return 0;
}

static int erire_new_project(const char *name) {
    ErError error;
    bool has_project_icon = false;
    char root[512];
    char assets[512];
    char assets_brand[512];
    char python_dir[512];
    char cpp_dir[512];
    char main_path[512];
    char backend_path[512];
    char app_json_path[512];
    char project_icon_path[512];
    char module_path[1024];
    char module_dir[1024];
    char bundled_icon_path[1024];
    const char *main_source =
        "screen.create[app;size;100;100;900;560]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#0f172a\"]\n"
        "screen.add[label.value[$hero_title].id[\"title\"].x[30].y[30].w[360].h[40].size[28].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$hero_subtitle].x[30].y[76].w[520].h[46].color[\"#94a3b8\"]]\n"
        "screen.add[card.text[\"Starter Card\\nCustom paint is now handled by Erire instead of default Win32 button/static controls.\"].x[30].y[140].w[360].h[130].padding[18].radius[18].bg[\"#111827\"].borderColor[\"#334155\"].color[\"#e5e7eb\"]]\n"
        "screen.add[text.value[$runtime_line].x[420].y[160].w[360].h[24].size[16].color[\"#7dd3fc\"]]\n"
        "screen.add[text.value[$math_line].x[420].y[194].w[360].h[22].size[14].color[\"#94a3b8\"]]\n"
        "screen.add[input.placeholder[\"Project name\"].id[\"project\"].x[30].y[292].w[260].h[50].padding[12].radius[14].bg[\"#111827\"].borderColor[\"#475569\"]]\n"
        "screen.add[button.text[\"Click Me\"].x[30].y[362].w[180].h[50].radius[14].padding[14].bg[\"#2563eb\"].onClick[\n"
        "    screen.setText[\"title\"; \"Button clicked\"]\n"
        "    screen.setText[\"project\"; \"Team Alpha\"]\n"
        "]]\n";
    const char *backend_source =
        "var.set[\"app_title\"; \"Welcome to Erire\"]\n"
        "var.set[\"hero_title\"; \"Hello from Erire\"]\n"
        "var.set[\"hero_subtitle\"; \"backend.er is loaded automatically for simple native backend logic.\"]\n"
        "var.set[\"runtime_line\"; text.concat[\"Platform: \"; text.upper[sys.platform[]]; \" | Host: \"; sys.hostname[]]]\n"
        "var.set[\"math_line\"; text.concat[\"Native math: 6 * 7 = \"; math.mul[6; 7]]]\n";
    char app_json[512];

    er_error_clear(&error);

    snprintf(root, sizeof(root), "%s", name);
    snprintf(assets, sizeof(assets), "%s\\assets", name);
    snprintf(assets_brand, sizeof(assets_brand), "%s\\assets\\brand", name);
    snprintf(python_dir, sizeof(python_dir), "%s\\python", name);
    snprintf(cpp_dir, sizeof(cpp_dir), "%s\\cpp", name);
    snprintf(main_path, sizeof(main_path), "%s\\main.er", name);
    snprintf(backend_path, sizeof(backend_path), "%s\\backend.er", name);
    snprintf(app_json_path, sizeof(app_json_path), "%s\\app.json", name);
    snprintf(project_icon_path, sizeof(project_icon_path), "%s\\assets\\brand\\app.ico", name);
    if (!er_directory_create_recursive(root, &error)) {
        er_error_print(stderr, root, &error);
        return 1;
    }
    if (!er_directory_create_recursive(assets, &error)) {
        er_error_print(stderr, assets, &error);
        return 1;
    }
    if (!er_directory_create_recursive(assets_brand, &error)) {
        er_error_print(stderr, assets_brand, &error);
        return 1;
    }
    if (!er_directory_create_recursive(python_dir, &error)) {
        er_error_print(stderr, python_dir, &error);
        return 1;
    }
    if (!er_directory_create_recursive(cpp_dir, &error)) {
        er_error_print(stderr, cpp_dir, &error);
        return 1;
    }
    if (!er_file_write_all(main_path, main_source, strlen(main_source), &error)) {
        er_error_print(stderr, main_path, &error);
        return 1;
    }
    if (!er_file_write_all(backend_path, backend_source, strlen(backend_source), &error)) {
        er_error_print(stderr, backend_path, &error);
        return 1;
    }
    er_error_clear(&error);
    if (er_get_current_module_path(module_path, sizeof(module_path), &error)) {
        er_path_dirname(module_path, module_dir, sizeof(module_dir));
        er_path_join(module_dir, "assets\\brand\\erire-logo.ico", bundled_icon_path, sizeof(bundled_icon_path));
        if (er_file_exists(bundled_icon_path)) {
            er_error_clear(&error);
            if (!er_file_copy(bundled_icon_path, project_icon_path, &error)) {
                er_error_print(stderr, project_icon_path, &error);
                return 1;
            }
            has_project_icon = true;
        }
    } else {
        er_error_clear(&error);
    }

    if (has_project_icon) {
        snprintf(app_json, sizeof(app_json),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"main.er\",\n"
            "  \"output\": \"dist\\\\%s.exe\",\n"
            "  \"icon\": \"assets\\\\brand\\\\app.ico\"\n"
            "}\n",
            name, name);
    } else {
        snprintf(app_json, sizeof(app_json),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"main.er\",\n"
            "  \"output\": \"dist\\\\%s.exe\"\n"
            "}\n",
            name, name);
    }

    if (!er_file_write_all(app_json_path, app_json, strlen(app_json), &error)) {
        er_error_print(stderr, app_json_path, &error);
        return 1;
    }

    // Auto-install IDE syntax highlighting for smooth out-of-the-box coding
    er_ide_highlight_install(false, NULL, &error);

    printf("\033[1;32m[OK] created project:\033[0m %s\n", root);
    printf("  \033[90mEntry point:\033[0m %s\\main.er\n", root);
    printf("  \033[1;36mRun:\033[0m erire %s\\main.er\n", root);
    printf("  \033[1;36mBuild:\033[0m erire build %s\\main.er\n", root);
    return 0;
}

static int erire_verify_key_from_console(int argc, char **argv) {
    char error[4096] = {0};
    if (argc < 4) {
        fprintf(stderr, "\033[1;31merror\033[0m: --verify-key expects <product-key> <google-account>\n");
        return 1;
    }
    if (!er_license_guard_verify_key(argv[2], argv[3], error, sizeof(error))) {
        fprintf(stderr, "\033[1;31mactivation failed:\033[0m %s\n", error[0] ? error : "unknown error");
        return 1;
    }
    puts("\033[1;32m[OK] ok:\033[0m product key verified and local Erire license state was saved.");
    return 0;
}


static int erire_require_license_for_cli(void) {
    char error[4096] = {0};
    if (er_license_guard_require(ER_LICENSE_APP_CLI, error, sizeof(error))) {
        return 0;
    }
    fprintf(stderr, "\033[1;31mErire CLI is locked.\033[0m\n%s\n", error[0] ? error : "Product key verification is required.");
    return 1;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    erire_enable_ansi();
#endif

    // Automatically check and install IDE syntax highlighting on first launch
    er_ide_highlight_auto_install_if_needed();

    if (argc < 2) {
        erire_print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "help") == 0) {
        erire_print_usage();
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "version") == 0) {
        puts("Erire 1.0.5 [x86_64-win32]");
        puts("Created by Yahia Saad | Erire ©2026");
        return 0;
    }
    if (strcmp(argv[1], "--install-highlight") == 0 || strcmp(argv[1], "install-highlight") == 0) {
        ErError error;
        er_error_clear(&error);
        if (!er_ide_highlight_install(true, stdout, &error)) {
            er_error_print(stderr, "install-highlight", &error);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "--verify-key") == 0) {
        return erire_verify_key_from_console(argc, argv);
    }

    if (strcmp(argv[1], "--tokens") == 0 && argc >= 3) {
        return erire_dump_tokens(argv[2]);
    }
    if (strcmp(argv[1], "--ast") == 0 && argc >= 3) {
        return erire_dump_ast(argv[2]);
    }
    if (strcmp(argv[1], "--check") == 0 && argc >= 3) {
        return erire_check(argv[2]);
    }
    if (strcmp(argv[1], "--build") == 0 || strcmp(argv[1], "build") == 0) {
        const char *source_path = NULL;
        const char *output_path = NULL;
        const char *icon_override = NULL;
        const char *win_title_override = NULL;
        bool onefile = false;
        int index;

        for (index = 2; index < argc; ++index) {
            const char *value = NULL;

            if (strcmp(argv[index], "--icon") == 0) {
                if (index + 1 >= argc) {
                    fprintf(stderr, "\033[1;31merror\033[0m: --icon expects a path\n");
                    return 1;
                }
                icon_override = argv[++index];
                continue;
            }
            if (strcmp(argv[index], "--win_title") == 0) {
                if (index + 1 >= argc) {
                    fprintf(stderr, "\033[1;31merror\033[0m: --win_title expects a value\n");
                    return 1;
                }
                win_title_override = argv[++index];
                continue;
            }
            if (strcmp(argv[index], "--onefile") == 0) {
                onefile = true;
                continue;
            }
            if (erire_flag_value(argv[index], "--icon", &value)) {
                icon_override = value;
                continue;
            }
            if (erire_flag_value(argv[index], "--win_title", &value)) {
                win_title_override = value;
                continue;
            }

            if (!source_path) {
                source_path = argv[index];
            } else if (!output_path) {
                output_path = argv[index];
            } else {
                fprintf(stderr, "\033[1;31merror\033[0m: unexpected build argument: %s\n", argv[index]);
                return 1;
            }
        }

        if (!source_path) {
            fprintf(stderr, "\033[1;31merror\033[0m: build requires an .er source file\n");
            return 1;
        }

        return erire_build(source_path, output_path, icon_override, win_title_override, onefile);
    }

    if ((strcmp(argv[1], "--new") == 0 || strcmp(argv[1], "new") == 0) && argc >= 3) {
        return erire_new_project(argv[2]);
    }

    if (erire_require_license_for_cli() != 0) {
        return 1;
    }

    if (strcmp(argv[1], "--live") == 0 && argc >= 3) {
        return erire_run_live(argv[2]);
    }
    if (strcmp(argv[1], "--run") == 0 && argc >= 3) {
        return erire_run(argv[2]);
    }
    if (strcmp(argv[1], "--console") == 0 && argc >= 3) {
        return erire_open_console(argv[2]);
    }
    if (strcmp(argv[1], "--console-help") == 0) {
        er_console_print_profiles(stdout);
        return 0;
    }
    if (argv[1][0] != '-') {
        return erire_run(argv[1]);
    }

    erire_print_usage();
    return 1;
}


