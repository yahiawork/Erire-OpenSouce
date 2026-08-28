#include "ide_highlight.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileio.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

static const char ER_EMBEDDED_PACKAGE_JSON[] =
"{\n"
"  \"name\": \"erire-language\",\n"
"  \"displayName\": \"Erire Language Support\",\n"
"  \"description\": \"Rich syntax highlighting and language configuration for the Erire programming language (.er files)\",\n"
"  \"version\": \"1.0.5\",\n"
"  \"publisher\": \"yahiasaad\",\n"
"  \"engines\": {\n"
"    \"vscode\": \"^1.60.0\"\n"
"  },\n"
"  \"categories\": [\n"
"    \"Programming Languages\"\n"
"  ],\n"
"  \"keywords\": [\n"
"    \"erire\",\n"
"    \"gui\",\n"
"    \"native\",\n"
"    \"electron\"\n"
"  ],\n"
"  \"contributes\": {\n"
"    \"languages\": [\n"
"      {\n"
"        \"id\": \"erire\",\n"
"        \"aliases\": [\n"
"          \"Erire\",\n"
"          \"erire\",\n"
"          \"er\"\n"
"        ],\n"
"        \"extensions\": [\n"
"          \".er\"\n"
"        ],\n"
"        \"configuration\": \"./language-configuration.json\"\n"
"      }\n"
"    ],\n"
"    \"grammars\": [\n"
"      {\n"
"        \"language\": \"erire\",\n"
"        \"scopeName\": \"source.erire\",\n"
"        \"path\": \"./syntaxes/erire.tmLanguage.json\"\n"
"      }\n"
"    ]\n"
"  }\n"
"}\n";

static const char ER_EMBEDDED_LANGUAGE_CONFIG_JSON[] =
"{\n"
"  \"comments\": {\n"
"    \"lineComment\": \"//\",\n"
"    \"blockComment\": [\"/*\", \"*/\"]\n"
"  },\n"
"  \"brackets\": [\n"
"    [\"[\", \"]\"],\n"
"    [\"(\", \")\"],\n"
"    [\"{\", \"}\"]\n"
"  ],\n"
"  \"autoClosingPairs\": [\n"
"    { \"open\": \"[\", \"close\": \"]\" },\n"
"    { \"open\": \"(\", \"close\": \")\" },\n"
"    { \"open\": \"{\", \"close\": \"}\" },\n"
"    { \"open\": \"\\\"\", \"close\": \"\\\"\", \"notIn\": [\"string\", \"comment\"] }\n"
"  ],\n"
"  \"surroundingPairs\": [\n"
"    [\"[\", \"]\"],\n"
"    [\"(\", \")\"],\n"
"    [\"{\", \"}\"],\n"
"    [\"\\\"\", \"\\\"\"]\n"
"  ],\n"
"  \"folding\": {\n"
"    \"markers\": {\n"
"      \"start\": \"^\\\\s*//\\\\s*#?region\\\\b\",\n"
"      \"end\": \"^\\\\s*//\\\\s*#?endregion\\\\b\"\n"
"    }\n"
"  },\n"
"  \"wordPattern\": \"(-?\\\\d*\\\\.\\\\d\\\\w*)|([^\\\\`\\\\~\\\\!\\\\@\\\\#\\\\%\\\\^\\\\&\\\\*\\\\(\\\\)\\\\-\\\\=\\\\+\\\\[\\\\{\\\\]\\\\}\\\\\\\\|\\\\;\\\\:\\\\\'\\\\\\\"\\\\,\\\\.\\\\<\\\\>\\\\/\\\\?\\\\s]+)\"\n"
"}\n";

static const char ER_EMBEDDED_TMLANGUAGE_JSON[] =
"{\n"
"  \"$schema\": \"https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json\",\n"
"  \"name\": \"Erire\",\n"
"  \"scopeName\": \"source.erire\",\n"
"  \"patterns\": [\n"
"    { \"include\": \"#comments\" },\n"
"    { \"include\": \"#strings\" },\n"
"    { \"include\": \"#numbers\" },\n"
"    { \"include\": \"#booleans\" },\n"
"    { \"include\": \"#keywords\" },\n"
"    { \"include\": \"#directives\" },\n"
"    { \"include\": \"#variables\" },\n"
"    { \"include\": \"#builtin-commands\" },\n"
"    { \"include\": \"#builtin-calls\" },\n"
"    { \"include\": \"#ui-elements\" },\n"
"    { \"include\": \"#properties\" },\n"
"    { \"include\": \"#events\" },\n"
"    { \"include\": \"#operators\" },\n"
"    { \"include\": \"#punctuation\" }\n"
"  ],\n"
"  \"repository\": {\n"
"    \"comments\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"comment.line.double-slash.erire\",\n"
"          \"match\": \"//.*$\"\n"
"        },\n"
"        {\n"
"          \"name\": \"comment.block.erire\",\n"
"          \"begin\": \"/\\\\*\",\n"
"          \"end\": \"\\\\*/\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"strings\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"string.quoted.double.erire\",\n"
"          \"begin\": \"\\\"\",\n"
"          \"end\": \"\\\"\",\n"
"          \"patterns\": [\n"
"            {\n"
"              \"name\": \"constant.character.escape.erire\",\n"
"              \"match\": \"\\\\\\\\(?:[\\\"\\\\\\\\/bfnrt]|u[0-9a-fA-F]{4})\"\n"
"            },\n"
"            {\n"
"              \"name\": \"variable.other.interpolated.erire\",\n"
"              \"match\": \"\\\\$[a-zA-Z_][a-zA-Z0-9_.]*\"\n"
"            }\n"
"          ]\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"numbers\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"constant.numeric.decimal.erire\",\n"
"          \"match\": \"\\\\b\\\\d+(?:\\\\.\\\\d+)?\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"constant.other.color.hex.erire\",\n"
"          \"match\": \"#[0-9a-fA-F]{3,8}\\\\b\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"booleans\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"constant.language.boolean.erire\",\n"
"          \"match\": \"\\\\b(true|false)\\\\b\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"keywords\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"keyword.control.import.erire\",\n"
"          \"match\": \"\\\\b(import|python|as|from)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"keyword.control.conditional.erire\",\n"
"          \"match\": \"\\\\b(if|else)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"keyword.control.loop.erire\",\n"
"          \"match\": \"\\\\b(while|for)\\\\b\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"directives\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"keyword.other.directive.erire\",\n"
"          \"match\": \"@imp\\\\b|@[a-zA-Z_][a-zA-Z0-9_]*\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"variables\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"variable.other.erire\",\n"
"          \"match\": \"\\\\$[a-zA-Z_][a-zA-Z0-9_]*(?:\\\\.[a-zA-Z_][a-zA-Z0-9_]*)*\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"builtin-commands\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"support.function.screen.erire\",\n"
"          \"match\": \"\\\\b(screen\\\\.(?:create|title|bg|add|show|setText))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.var.erire\",\n"
"          \"match\": \"\\\\b(var\\\\.set)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.timer.erire\",\n"
"          \"match\": \"\\\\b(timer\\\\.every)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.console.erire\",\n"
"          \"match\": \"\\\\b(console\\\\.open)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.media-cmd.erire\",\n"
"          \"match\": \"\\\\b(media\\\\.(?:play|pause|playPause|stop|next|previous|addFiles|openPlaylist|savePlaylist|clear|toggleMute|toggleShuffle|cycleRepeat|seek|seekRelative|setVolume|changeVolume))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.storage-cmd.erire\",\n"
"          \"match\": \"\\\\b(storage\\\\.write)\\\\b\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"builtin-calls\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"support.function.python.erire\",\n"
"          \"match\": \"\\\\b(py\\\\.call)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.text.erire\",\n"
"          \"match\": \"\\\\b(text\\\\.(?:upper|lower|title|length|contains|concat))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.math.erire\",\n"
"          \"match\": \"\\\\b(math\\\\.(?:add|sub|mul|div|min|max))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.sys.erire\",\n"
"          \"match\": \"\\\\b(sys\\\\.(?:cwd|hostname|platform))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.time.erire\",\n"
"          \"match\": \"\\\\b(time\\\\.now)\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.dialog.erire\",\n"
"          \"match\": \"\\\\b(dialog\\\\.(?:openFile|openFiles|saveFile))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.storage.erire\",\n"
"          \"match\": \"\\\\b(storage\\\\.(?:read|exists))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.runtime.erire\",\n"
"          \"match\": \"\\\\b(runtime\\\\.(?:liveEnabled|liveVersion|liveStatus|liveError|widgetCount|timerCount|hasVar|getVar|widgetExists))\\\\b\"\n"
"        },\n"
"        {\n"
"          \"name\": \"support.function.media-query.erire\",\n"
"          \"match\": \"\\\\b(media\\\\.(?:state|position|duration|volume|count|index|muted|shuffle|repeat|currentName|currentTitle|currentArtist|currentYear|currentBitrate|currentSampleRate|currentArt|positionText|remainingText|durationText|playlistText))\\\\b\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"ui-elements\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"entity.name.tag.ui-element.erire\",\n"
"          \"match\": \"(?<=\\\\[|\\\\b)(text|label|button|btn|card|panel|input|image|webview|box|slider|progress|checkbox|toggle|list|grid|separator|divider)(?=\\\\.)\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"events\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"entity.other.attribute-name.event.erire\",\n"
"          \"match\": \"\\\\.(onClick|onChange|onLoad|onHover|onLeave|onFocus|onBlur|onKey|onSubmit)(?=\\\\[)\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"properties\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"entity.other.attribute-name.property.erire\",\n"
"          \"match\": \"\\\\.([a-zA-Z_][a-zA-Z0-9_]*)(?=\\\\[)\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"operators\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"keyword.operator.comparison.erire\",\n"
"          \"match\": \"(==|!=|<=|>=|<|>)\"\n"
"        },\n"
"        {\n"
"          \"name\": \"keyword.operator.assignment.erire\",\n"
"          \"match\": \"=\"\n"
"        }\n"
"      ]\n"
"    },\n"
"    \"punctuation\": {\n"
"      \"patterns\": [\n"
"        {\n"
"          \"name\": \"punctuation.separator.delimiter.erire\",\n"
"          \"match\": \";\"\n"
"        },\n"
"        {\n"
"          \"name\": \"punctuation.separator.comma.erire\",\n"
"          \"match\": \",\"\n"
"        },\n"
"        {\n"
"          \"name\": \"punctuation.definition.bracket.square.erire\",\n"
"          \"match\": \"[\\\\[\\\\]]\"\n"
"        },\n"
"        {\n"
"          \"name\": \"punctuation.definition.bracket.round.erire\",\n"
"          \"match\": \"[()]\"\n"
"        },\n"
"        {\n"
"          \"name\": \"punctuation.accessor.dot.erire\",\n"
"          \"match\": \"\\\\.\"\n"
"        }\n"
"      ]\n"
"    }\n"
"  }\n"
"}\n";

static const char *IDE_EXTENSION_REL_PATHS[] = {
    ".vscode/extensions/erire-language",
    ".vscode-insiders/extensions/erire-language",
    ".cursor/extensions/erire-language",
    ".vscode-oss/extensions/erire-language",
    ".codium/extensions/erire-language",
    ".antigravity/extensions/erire-language",
    ".gemini/antigravity-ide/extensions/erire-language",
    ".windsurf/extensions/erire-language",
    ".positron/extensions/erire-language"
};

static bool er_ide_get_home_dir(char *out_path, size_t capacity) {
    if (!out_path || capacity == 0) {
        return false;
    }
#ifdef _WIN32
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile && userprofile[0] != '\0') {
        snprintf(out_path, capacity, "%s", userprofile);
        return true;
    }
    const char *homedrive = getenv("HOMEDRIVE");
    const char *homepath = getenv("HOMEPATH");
    if (homedrive && homepath) {
        snprintf(out_path, capacity, "%s%s", homedrive, homepath);
        return true;
    }
#else
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(out_path, capacity, "%s", home);
        return true;
    }
#endif
    return false;
}

static bool er_ide_write_extension_to_dir(const char *ext_dir, ErError *error) {
    char syntaxes_dir[1024];
    char pkg_path[1024];
    char cfg_path[1024];
    char tml_path[1024];

    if (!er_directory_create_recursive(ext_dir, error)) {
        return false;
    }

    snprintf(syntaxes_dir, sizeof(syntaxes_dir), "%s/syntaxes", ext_dir);
    if (!er_directory_create_recursive(syntaxes_dir, error)) {
        return false;
    }

    snprintf(pkg_path, sizeof(pkg_path), "%s/package.json", ext_dir);
    snprintf(cfg_path, sizeof(cfg_path), "%s/language-configuration.json", ext_dir);
    snprintf(tml_path, sizeof(tml_path), "%s/syntaxes/erire.tmLanguage.json", ext_dir);

    if (!er_file_write_all(pkg_path, ER_EMBEDDED_PACKAGE_JSON, strlen(ER_EMBEDDED_PACKAGE_JSON), error)) {
        return false;
    }

    if (!er_file_write_all(cfg_path, ER_EMBEDDED_LANGUAGE_CONFIG_JSON, strlen(ER_EMBEDDED_LANGUAGE_CONFIG_JSON), error)) {
        return false;
    }

    if (!er_file_write_all(tml_path, ER_EMBEDDED_TMLANGUAGE_JSON, strlen(ER_EMBEDDED_TMLANGUAGE_JSON), error)) {
        return false;
    }

    return true;
}

bool er_ide_highlight_install(bool verbose, FILE *out, ErError *error) {
    char home[1024];
    char ext_dir[1024];
    size_t i;
    int installed_count = 0;

    er_error_clear(error);

    if (!er_ide_get_home_dir(home, sizeof(home))) {
        er_error_set(error, 0, 0, "Could not determine user home directory");
        return false;
    }

    if (verbose && out) {
        fprintf(out, "\033[1;36m[Erire Highlighting]\033[0m Installing syntax highlighting for Electron IDEs...\n");
    }

    // Always install to primary .vscode and .antigravity extensions folders, and check others
    for (i = 0; i < sizeof(IDE_EXTENSION_REL_PATHS) / sizeof(IDE_EXTENSION_REL_PATHS[0]); ++i) {
        char parent_ide_dir[1024];
        const char *rel = IDE_EXTENSION_REL_PATHS[i];
        
        // Find parent directory of extensions (e.g. .vscode, .cursor, .antigravity)
        char temp_rel[256];
        snprintf(temp_rel, sizeof(temp_rel), "%s", rel);
        char *slash = strchr(temp_rel, '/');
        if (slash) {
            *slash = '\0';
        }
        snprintf(parent_ide_dir, sizeof(parent_ide_dir), "%s/%s", home, temp_rel);

        snprintf(ext_dir, sizeof(ext_dir), "%s/%s", home, rel);

        // We always install to .vscode and .antigravity, or any folder if the parent IDE folder exists
        bool is_primary = (i == 0 || i == 5 || i == 6);
        bool parent_exists = er_directory_exists(parent_ide_dir) || er_file_exists(parent_ide_dir);

        if (is_primary || parent_exists) {
            ErError sub_error;
            er_error_clear(&sub_error);
            if (er_ide_write_extension_to_dir(ext_dir, &sub_error)) {
                installed_count++;
                if (verbose && out) {
                    fprintf(out, "  \033[1;32m[OK]\033[0m Installed to: %s\n", ext_dir);
                }

            }
        }
    }

    // Also write marker in ~/.erire/highlight_v1_0_5.installed
    char marker_dir[1024];
    char marker_file[1024];
    snprintf(marker_dir, sizeof(marker_dir), "%s/.erire", home);
    er_directory_create_recursive(marker_dir, error);
    snprintf(marker_file, sizeof(marker_file), "%s/.erire/highlight_v1_0_5.installed", home);
    er_file_write_all(marker_file, "1.0.5", 5, error);

    if (installed_count == 0) {
        er_error_set(error, 0, 0, "No IDE extensions directory could be written");
        return false;
    }

    if (verbose && installed_count > 0 && out) {
        fprintf(out, "\033[1;32m[OK] Erire syntax highlighting is active for VS Code, Antigravity IDE, and Electron editors!\033[0m\n");
    }

    return true;
}

void er_ide_highlight_auto_install_if_needed(void) {
    char home[1024];
    char marker_file[1024];

    if (!er_ide_get_home_dir(home, sizeof(home))) {
        return;
    }

    snprintf(marker_file, sizeof(marker_file), "%s/.erire/highlight_v1_0_5.installed", home);
    if (!er_file_exists(marker_file)) {
        ErError err;
        er_error_clear(&err);
        er_ide_highlight_install(false, NULL, &err);
    }
}
