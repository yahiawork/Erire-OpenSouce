#include "templates.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fileio.h"
#include "util.h"

#define STUDIO_TEMPLATE_BUFFER 32768

static void studio_templates_slug(const char *source, char *out, size_t out_capacity) {
    size_t in_index = 0;
    size_t out_index = 0;

    while (source[in_index] && out_index + 1 < out_capacity) {
        char ch = source[in_index++];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            out[out_index++] = ch;
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            out[out_index++] = '_';
        }
    }
    out[out_index] = '\0';
    if (out[0] == '\0') {
        snprintf(out, out_capacity, "app");
    }
}

static bool studio_templates_is(const char *left, const char *right) {
    if (!left || !right) {
        return false;
    }
    return _stricmp(left, right) == 0;
}

static bool studio_write_text_file(const char *path, const char *content, ErError *error) {
    return er_file_write_all(path, content, strlen(content), error);
}

static bool studio_write_project_file(const char *root, const char *relative_path, const char *content, ErError *error) {
    char path[STUDIO_MAX_PATH];

    studio_join_path(root, relative_path, path, sizeof(path));
    return studio_write_text_file(path, content, error);
}

static bool studio_write_project_file_format(const char *root, const char *relative_path, ErError *error, const char *fmt, ...) {
    char buffer[STUDIO_TEMPLATE_BUFFER];
    int written;
    va_list args;

    va_start(args, fmt);
    written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (written < 0 || written >= (int) sizeof(buffer)) {
        er_error_set(error, 0, 0, "Template content is too large for %s", relative_path ? relative_path : "file");
        return false;
    }
    return studio_write_project_file(root, relative_path, buffer, error);
}

static bool studio_templates_create_scaffold(const char *root, ErError *error) {
    static const char *directories[] = {
        "src",
        "pages",
        "components",
        "assets",
        "python",
        "build",
        "tests"
    };
    size_t index;
    char path[STUDIO_MAX_PATH];

    if (!er_directory_create_recursive(root, error)) {
        return false;
    }
    for (index = 0; index < sizeof(directories) / sizeof(directories[0]); ++index) {
        studio_join_path(root, directories[index], path, sizeof(path));
        if (!er_directory_create_recursive(path, error)) {
            return false;
        }
    }
    return true;
}

static bool studio_templates_write_app_json(
    const char *root,
    const char *project_name,
    const char *slug,
    const char *template_name,
    const char *description,
    ErError *error
) {
    return studio_write_project_file_format(
        root,
        "app.json",
        error,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"entry\": \"src/main.er\",\n"
        "  \"python\": \"python/app_bridge.py\",\n"
        "  \"output\": \"build/%s.exe\",\n"
        "  \"type\": \"desktop-ui\",\n"
        "  \"template\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"version\": \"0.1.0\"\n"
        "}\n",
        project_name,
        slug,
        template_name,
        description
    );
}

static bool studio_templates_write_default_python_bridge(const char *root, const char *project_name, const char *template_name, ErError *error) {
    return studio_write_project_file_format(
        root,
        "python\\app_bridge.py",
        error,
        "from pathlib import Path\n"
        "\n"
        "PROJECT_ROOT = Path(__file__).resolve().parent.parent\n"
        "TEMPLATE_NAME = \"%s\"\n"
        "\n"
        "def ping() -> dict:\n"
        "    return {\n"
        "        \"message\": \"Python bridge connected\",\n"
        "        \"project\": \"%s\",\n"
        "        \"template\": TEMPLATE_NAME,\n"
        "        \"root\": str(PROJECT_ROOT),\n"
        "    }\n"
        "\n"
        "def list_assets() -> list[str]:\n"
        "    assets_dir = PROJECT_ROOT / \"assets\"\n"
        "    if not assets_dir.exists():\n"
        "        return []\n"
        "    return sorted(str(path.relative_to(PROJECT_ROOT)) for path in assets_dir.rglob(\"*\") if path.is_file())\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    print(ping())\n",
        template_name,
        project_name
    );
}

static bool studio_templates_write_python_tool_bridge(const char *root, const char *project_name, ErError *error) {
    return studio_write_project_file_format(
        root,
        "python\\app_bridge.py",
        error,
        "from pathlib import Path\n"
        "\n"
        "PROJECT_ROOT = Path(__file__).resolve().parent.parent\n"
        "\n"
        "def ping() -> dict:\n"
        "    return {\n"
        "        \"message\": \"Python bridge is ready for %s\",\n"
        "        \"root\": str(PROJECT_ROOT),\n"
        "    }\n"
        "\n"
        "def asset_count() -> dict:\n"
        "    assets_dir = PROJECT_ROOT / \"assets\"\n"
        "    count = 0\n"
        "    if assets_dir.exists():\n"
        "        count = sum(1 for path in assets_dir.rglob(\"*\") if path.is_file())\n"
        "    return {\n"
        "        \"message\": f\"Assets discovered: {count}\",\n"
        "        \"count\": count,\n"
        "    }\n"
        "\n"
        "def workspace_summary() -> dict:\n"
        "    files = sum(1 for path in PROJECT_ROOT.rglob(\"*\") if path.is_file())\n"
        "    return {\n"
        "        \"message\": f\"Workspace files indexed: {files}\",\n"
        "        \"files\": files,\n"
        "    }\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    print(ping())\n",
        project_name
    );
}

static bool studio_templates_write_default(const char *root, const char *project_name, ErError *error) {
    if (!studio_write_project_file_format(
        root,
        "src\\main.er",
        error,
        "screen.create[app; size; 120; 120; 1120; 720]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#0f172a\"]\n"
        "screen.add[label.value[$hero_title].id[\"hero\"].x[44].y[36].w[520].h[38].size[28].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$hero_copy].x[44].y[84].w[620].h[48].size[15].color[\"#94a3b8\"]]\n"
        "screen.add[card.text[\"Erire Studio starter project\\nDesktop UI layout, build pipeline, and Python bridge are already wired.\"].x[44].y[156].w[420].h[150].padding[18].radius[18].bg[\"#111827\"].borderColor[\"#334155\"].color[\"#e5e7eb\"]]\n"
        "screen.add[text.value[$python_hint].x[520].y[188].w[480].h[24].size[16].color[\"#7dd3fc\"]]\n"
        "screen.add[text.value[$status_line].id[\"status_line\"].x[520].y[226].w[480].h[22].size[14].color[\"#94a3b8\"]]\n"
        "screen.add[input.placeholder[\"Project name\"].id[\"project_name\"].x[44].y[342].w[260].h[48].padding[12].radius[14].bg[\"#111827\"].borderColor[\"#475569\"]]\n"
        "screen.add[button.text[\"Run starter action\"].x[44].y[410].w[200].h[48].radius[14].padding[12].bg[\"#2563eb\"].onClick[\n"
        "    screen.setText[\"hero\"; \"Studio action fired\"]\n"
        "    screen.setText[\"project_name\"; \"%s\"]\n"
        "    screen.setText[\"status_line\"; \"Starter action completed\"]\n"
        "]]\n",
        project_name
    )) return false;

    if (!studio_write_project_file_format(
        root,
        "src\\backend.er",
        error,
        "var.set[\"app_title\"; \"%s\"]\n"
        "var.set[\"hero_title\"; \"Build desktop UI with Erire\"]\n"
        "var.set[\"hero_copy\"; \"This starter is shaped for real app work: src/, pages/, components/, assets/, python/, and build/ are ready from the first run.\"]\n"
        "var.set[\"python_hint\"; \"Python bridge file: python/app_bridge.py\"]\n"
        "var.set[\"status_line\"; text.concat[\"Project root ready for \"; \"%s\"; \".\"]]\n",
        project_name,
        project_name
    )) return false;

    if (!studio_templates_write_default_python_bridge(root, project_name, "default", error)) return false;
    if (!studio_write_project_file(root, "pages\\home.er", "var.set[\"page_title\"; \"Home\"]\nvar.set[\"page_copy\"; \"Edit this page and copy its values into your main workflow as your app grows.\"]\n", error)) return false;
    if (!studio_write_project_file(root, "components\\welcome_card.er", "var.set[\"component_name\"; \"welcome_card\"]\nvar.set[\"component_role\"; \"Starter component placeholder\"]\n", error)) return false;
    if (!studio_write_project_file(root, "assets\\README.md", "Drop images, icons, fonts, and packaged resources in this folder.\n", error)) return false;
    return studio_write_project_file_format(
        root,
        "README.md",
        error,
        "# %s\n\nTemplate: Desktop UI Starter\n\nUse this project when you want a solid default Erire desktop app with a Python bridge already wired in.\n",
        project_name
    );
}

static bool studio_templates_write_blank(const char *root, const char *project_name, ErError *error) {
    if (!studio_write_project_file(
        root,
        "src\\main.er",
        "screen.create[app; size; 160; 160; 980; 640]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#0f172a\"]\n"
        "screen.add[label.value[$app_title].x[44].y[44].w[420].h[38].size[28].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$subtitle].x[44].y[92].w[620].h[24].size[15].color[\"#94a3b8\"]]\n"
        "screen.add[text.value[$status_text].id[\"status_text\"].x[44].y[160].w[540].h[24].size[14].color[\"#7dd3fc\"]]\n"
        "screen.add[button.text[\"Primary action\"].x[44].y[214].w[180].h[46].radius[14].padding[12].bg[\"#2563eb\"].onClick[\n"
        "    screen.setText[\"status_text\"; \"Blank window action fired\"]\n"
        "]]\n",
        error
    )) return false;

    if (!studio_write_project_file_format(
        root,
        "src\\backend.er",
        error,
        "var.set[\"app_title\"; \"%s\"]\n"
        "var.set[\"subtitle\"; \"A minimal Erire window template with just enough structure to start fast.\"]\n"
        "var.set[\"status_text\"; \"Ready\"]\n",
        project_name
    )) return false;

    if (!studio_templates_write_default_python_bridge(root, project_name, "blank", error)) return false;
    return studio_write_project_file_format(
        root,
        "README.md",
        error,
        "# %s\n\nTemplate: Blank Window\n\nUse this when you want the smallest useful Erire desktop app and plan to build the layout yourself.\n",
        project_name
    );
}

static bool studio_templates_write_workspace(const char *root, const char *project_name, ErError *error) {
    if (!studio_write_project_file(
        root,
        "src\\main.er",
        "screen.create[app; size; 96; 96; 1280; 760]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#0b1220\"]\n"
        "screen.add[box.value[\"\"].x[24].y[24].w[280].h[680].bg[\"#101827\"].bg2[\"#0d1727\"].borderColor[\"#22314a\"].radius[24]]\n"
        "screen.add[text.value[$sidebar_title].x[48].y[46].w[200].h[24].size[16].color[\"#7dd3fc\"]]\n"
        "screen.add[text.value[$sidebar_hint].x[48].y[76].w[220].h[40].size[13].color[\"#94a3b8\"]]\n"
        "screen.add[button.text[\"Overview\"].x[48].y[136].w[200].h[42].radius[12].padding[10].bg[\"#1d4ed8\"].onClick[\n"
        "    screen.setText[\"content_title\"; \"Overview\"]\n"
        "    screen.setText[\"content_body\"; $overview_body]\n"
        "]]\n"
        "screen.add[button.text[\"Tasks\"].x[48].y[188].w[200].h[42].radius[12].padding[10].bg[\"#0f766e\"].onClick[\n"
        "    screen.setText[\"content_title\"; \"Tasks\"]\n"
        "    screen.setText[\"content_body\"; $tasks_body]\n"
        "]]\n"
        "screen.add[button.text[\"Logs\"].x[48].y[240].w[200].h[42].radius[12].padding[10].bg[\"#7c3aed\"].onClick[\n"
        "    screen.setText[\"content_title\"; \"Logs\"]\n"
        "    screen.setText[\"content_body\"; $logs_body]\n"
        "]]\n"
        "screen.add[box.value[\"\"].x[332].y[24].w[924].h[680].bg[\"#0f172a\"].bg2[\"#0c1322\"].borderColor[\"#253449\"].radius[24]]\n"
        "screen.add[label.value[$default_title].id[\"content_title\"].x[368].y[52].w[320].h[36].size[28].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$default_body].id[\"content_body\"].x[368].y[106].w[760].h[110].size[15].color[\"#9fb0c8\"]]\n"
        "screen.add[text.value[$footer_text].id[\"footer_text\"].x[368].y[252].w[520].h[22].size[14].color[\"#7dd3fc\"]]\n"
        "screen.add[button.text[\"Update Status\"].x[368].y[300].w[180].h[44].radius[12].padding[10].bg[\"#2563eb\"].onClick[\n"
        "    screen.setText[\"footer_text\"; \"Workspace status updated\"]\n"
        "]]\n",
        error
    )) return false;

    if (!studio_write_project_file_format(
        root,
        "src\\backend.er",
        error,
        "var.set[\"app_title\"; \"%s\"]\n"
        "var.set[\"sidebar_title\"; \"Workspace\"]\n"
        "var.set[\"sidebar_hint\"; \"Use the left rail for sections and turn this into your own tool-style app.\"]\n"
        "var.set[\"default_title\"; \"Overview\"]\n"
        "var.set[\"default_body\"; \"This template is shaped for productivity tools, internal apps, inspectors, and admin-style workflows without the visual clutter.\"]\n"
        "var.set[\"overview_body\"; \"Overview page selected. Replace this text with live project information or a dashboard summary.\"]\n"
        "var.set[\"tasks_body\"; \"Tasks page selected. Track jobs, commands, queues, or user workflows here.\"]\n"
        "var.set[\"logs_body\"; \"Logs page selected. Stream process output, telemetry, or diagnostics into this area.\"]\n"
        "var.set[\"footer_text\"; \"Workspace ready\"]\n",
        project_name
    )) return false;

    if (!studio_templates_write_default_python_bridge(root, project_name, "workspace", error)) return false;
    if (!studio_write_project_file(root, "pages\\overview.er", "var.set[\"page_name\"; \"overview\"]\nvar.set[\"page_note\"; \"Overview page placeholder\"]\n", error)) return false;
    if (!studio_write_project_file(root, "pages\\logs.er", "var.set[\"page_name\"; \"logs\"]\nvar.set[\"page_note\"; \"Logs page placeholder\"]\n", error)) return false;
    if (!studio_write_project_file(root, "components\\sidebar.er", "var.set[\"component_name\"; \"sidebar\"]\nvar.set[\"component_role\"; \"Workspace navigation placeholder\"]\n", error)) return false;
    return studio_write_project_file_format(
        root,
        "README.md",
        error,
        "# %s\n\nTemplate: Workspace Tool\n\nUse this template for internal tools, data editors, inspectors, and structured desktop workflows.\n",
        project_name
    );
}

static bool studio_templates_write_python_tool(const char *root, const char *project_name, ErError *error) {
    if (!studio_write_project_file(
        root,
        "src\\main.er",
        "var.set[\"bridge_state\"; py.call[\"app_bridge.ping\"]]\n"
        "screen.create[app; size; 120; 120; 1120; 720]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#0e1525\"]\n"
        "screen.add[label.value[$hero_title].x[44].y[34].w[460].h[36].size[28].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$hero_copy].x[44].y[82].w[640].h[44].size[15].color[\"#9fb0c8\"]]\n"
        "screen.add[text.value[$bridge_state.message].id[\"bridge_message\"].x[44].y[156].w[560].h[24].size[14].color[\"#7dd3fc\"]]\n"
        "screen.add[text.value[$bridge_state.root].id[\"bridge_root\"].x[44].y[188].w[840].h[24].size[13].color[\"#94a3b8\"]]\n"
        "screen.add[text.value[\"Assets discovered: 0\"].id[\"asset_count\"].x[44].y[220].w[320].h[24].size[14].color[\"#f6c177\"]]\n"
        "screen.add[text.value[\"Workspace files indexed: 0\"].id[\"workspace_summary\"].x[44].y[252].w[360].h[24].size[14].color[\"#c792ea\"]]\n"
        "screen.add[button.text[\"Ping Python\"].x[44].y[314].w[170].h[44].radius[12].padding[10].bg[\"#2563eb\"].onClick[\n"
        "    var.set[\"ping_result\"; py.call[\"app_bridge.ping\"]]\n"
        "    screen.setText[\"bridge_message\"; $ping_result.message]\n"
        "    screen.setText[\"bridge_root\"; $ping_result.root]\n"
        "]]\n"
        "screen.add[button.text[\"Count Assets\"].x[228].y[314].w[170].h[44].radius[12].padding[10].bg[\"#0f766e\"].onClick[\n"
        "    var.set[\"asset_result\"; py.call[\"app_bridge.asset_count\"]]\n"
        "    screen.setText[\"asset_count\"; $asset_result.message]\n"
        "]]\n"
        "screen.add[button.text[\"Workspace Summary\"].x[412].y[314].w[190].h[44].radius[12].padding[10].bg[\"#7c3aed\"].onClick[\n"
        "    var.set[\"workspace_result\"; py.call[\"app_bridge.workspace_summary\"]]\n"
        "    screen.setText[\"workspace_summary\"; $workspace_result.message]\n"
        "]]\n",
        error
    )) return false;

    if (!studio_write_project_file_format(
        root,
        "src\\backend.er",
        error,
        "var.set[\"app_title\"; \"%s\"]\n"
        "var.set[\"hero_title\"; \"Python bridge tool\"]\n"
        "var.set[\"hero_copy\"; \"This starter is tuned for apps where Erire drives the UI and Python handles filesystem, data processing, or automation tasks.\"]\n",
        project_name
    )) return false;

    if (!studio_templates_write_python_tool_bridge(root, project_name, error)) return false;
    if (!studio_write_project_file(root, "python\\tasks.py", "def normalize_name(name: str) -> str:\n    return name.strip().lower().replace(\" \", \"_\")\n", error)) return false;
    if (!studio_write_project_file(root, "pages\\bridge_console.er", "var.set[\"page_name\"; \"bridge_console\"]\nvar.set[\"page_note\"; \"Use this page for bridge-driven UI sections.\"]\n", error)) return false;
    return studio_write_project_file_format(
        root,
        "README.md",
        error,
        "# %s\n\nTemplate: Python Bridge Tool\n\nUse this when Python is part of the core workflow and you want the UI to trigger helper scripts and display the returned data.\n",
        project_name
    );
}

static bool studio_templates_write_showcase(const char *root, const char *project_name, ErError *error) {
    if (!studio_write_project_file(
        root,
        "src\\main.er",
        "screen.create[app; size; 88; 88; 1240; 760]\n"
        "screen.title[$app_title]\n"
        "screen.bg[\"#08111f\"]\n"
        "screen.add[label.value[$hero_title].id[\"hero_title\"].x[44].y[40].w[600].h[40].size[30].color[\"#f8fafc\"]]\n"
        "screen.add[text.value[$hero_copy].x[44].y[92].w[760].h[46].size[15].color[\"#a5b4cc\"]]\n"
        "screen.add[card.text[$card_one].x[44].y[182].w[344].h[176].padding[18].radius[20].bg[\"#111827\"].borderColor[\"#2b374a\"].color[\"#e5e7eb\"]]\n"
        "screen.add[card.text[$card_two].x[406].y[182].w[344].h[176].padding[18].radius[20].bg[\"#111827\"].borderColor[\"#2b374a\"].color[\"#e5e7eb\"]]\n"
        "screen.add[card.text[$card_three].x[768].y[182].w[344].h[176].padding[18].radius[20].bg[\"#111827\"].borderColor[\"#2b374a\"].color[\"#e5e7eb\"]]\n"
        "screen.add[text.value[$footer_line].id[\"footer_line\"].x[44].y[402].w[680].h[24].size[14].color[\"#7dd3fc\"]]\n"
        "screen.add[button.text[\"Cycle highlight\"].x[44].y[454].w[180].h[46].radius[14].padding[12].bg[\"#2563eb\"].onClick[\n"
        "    screen.setText[\"hero_title\"; \"Showcase action fired\"]\n"
        "    screen.setText[\"footer_line\"; \"Highlight cycled\"]\n"
        "]]\n",
        error
    )) return false;

    if (!studio_write_project_file_format(
        root,
        "src\\backend.er",
        error,
        "var.set[\"app_title\"; \"%s\"]\n"
        "var.set[\"hero_title\"; \"Present your desktop app clearly\"]\n"
        "var.set[\"hero_copy\"; \"Use this template for demos, prototypes, landing-style previews, and polished internal showcases that still stay lightweight.\"]\n"
        "var.set[\"card_one\"; \"Feature card one\\n\\nShow the core value of your app, the workflow it improves, and the important UI state. \"]\n"
        "var.set[\"card_two\"; \"Feature card two\\n\\nPoint to automation, Python integration, or build tooling that makes the desktop app useful in practice. \"]\n"
        "var.set[\"card_three\"; \"Feature card three\\n\\nKeep room for metrics, release notes, previews, or screenshots linked from assets/. \"]\n"
        "var.set[\"footer_line\"; \"Showcase ready\"]\n",
        project_name
    )) return false;

    if (!studio_templates_write_default_python_bridge(root, project_name, "showcase", error)) return false;
    if (!studio_write_project_file(root, "components\\feature_card.er", "var.set[\"component_name\"; \"feature_card\"]\nvar.set[\"component_role\"; \"Showcase card placeholder\"]\n", error)) return false;
    if (!studio_write_project_file(root, "assets\\README.md", "Put screenshots, icons, product shots, and media assets here for the showcase template.\n", error)) return false;
    return studio_write_project_file_format(
        root,
        "README.md",
        error,
        "# %s\n\nTemplate: Showcase Demo\n\nUse this when you want a cleaner presentation-style Erire app for demos, previews, and polished prototypes.\n",
        project_name
    );
}

bool studio_templates_create_project(
    const char *project_name,
    const char *location,
    const char *template_name,
    char *out_root,
    size_t out_capacity,
    ErError *error
) {
    char root[STUDIO_MAX_PATH];
    char slug[128];
    const char *selected_template = template_name && template_name[0] != '\0' ? template_name : "default";
    const char *description = "Desktop Erire UI project";

    if (!project_name || !location || project_name[0] == '\0' || location[0] == '\0') {
        er_error_set(error, 0, 0, "Project name and location are required");
        return false;
    }

    studio_templates_slug(project_name, slug, sizeof(slug));
    studio_join_path(location, project_name, root, sizeof(root));

    if (studio_templates_is(selected_template, "blank")) {
        description = "Minimal blank Erire window";
    } else if (studio_templates_is(selected_template, "workspace")) {
        description = "Structured workspace tool";
    } else if (studio_templates_is(selected_template, "python-tool")) {
        description = "Python bridge focused desktop tool";
    } else if (studio_templates_is(selected_template, "showcase")) {
        description = "Presentation and showcase starter";
    } else {
        selected_template = "default";
        description = "Balanced desktop UI starter";
    }

    if (!studio_templates_create_scaffold(root, error)) return false;
    if (!studio_templates_write_app_json(root, project_name, slug, selected_template, description, error)) return false;

    if (studio_templates_is(selected_template, "blank")) {
        if (!studio_templates_write_blank(root, project_name, error)) return false;
    } else if (studio_templates_is(selected_template, "workspace")) {
        if (!studio_templates_write_workspace(root, project_name, error)) return false;
    } else if (studio_templates_is(selected_template, "python-tool")) {
        if (!studio_templates_write_python_tool(root, project_name, error)) return false;
    } else if (studio_templates_is(selected_template, "showcase")) {
        if (!studio_templates_write_showcase(root, project_name, error)) return false;
    } else {
        if (!studio_templates_write_default(root, project_name, error)) return false;
    }

    strncpy(out_root, root, out_capacity - 1);
    out_root[out_capacity - 1] = '\0';
    return true;
}
