#include "frontend.h"

#include <stdlib.h>
#include <string.h>

#include "fileio.h"
#include "parser.h"
#include "semantic.h"

static int er_frontend_ascii_casecmp(const char *left, const char *right) {
    unsigned char left_char;
    unsigned char right_char;

    if (!left) {
        left = "";
    }
    if (!right) {
        right = "";
    }

    while (*left != '\0' || *right != '\0') {
        left_char = (unsigned char) *left;
        right_char = (unsigned char) *right;

        if (left_char >= 'A' && left_char <= 'Z') {
            left_char = (unsigned char) (left_char - 'A' + 'a');
        }
        if (right_char >= 'A' && right_char <= 'Z') {
            right_char = (unsigned char) (right_char - 'A' + 'a');
        }

        if (left_char != right_char) {
            return (int) left_char - (int) right_char;
        }

        if (*left != '\0') {
            ++left;
        }
        if (*right != '\0') {
            ++right;
        }
    }

    return 0;
}

static bool er_frontend_parse_only(
    const char *source_name,
    const char *source,
    ErProgram **out_program,
    ErError *error
) {
    return er_parse_program(source_name, source, out_program, error);
}

static bool er_frontend_parse_and_analyze(
    const char *source_name,
    const char *source,
    ErProgram **out_program,
    ErError *error
) {
    if (!er_frontend_parse_only(source_name, source, out_program, error)) {
        return false;
    }

    if (!er_semantic_analyze_program(source_name, *out_program, error)) {
        er_program_free(*out_program);
        *out_program = NULL;
        return false;
    }

    return true;
}

static bool er_frontend_is_backend_module_path(const char *path) {
    const char *base = path;
    const char *slash = strrchr(path ? path : "", '\\');
    const char *slash2 = strrchr(path ? path : "", '/');

    if (slash && slash[1] != '\0') {
        base = slash + 1;
    }
    if (slash2 && slash2[1] != '\0' && slash2 + 1 > base) {
        base = slash2 + 1;
    }

    return er_frontend_ascii_casecmp(base, "backend.er") == 0;
}

static bool er_frontend_program_append(ErProgram *dest, ErProgram *src, ErError *error) {
    size_t i;

    if (!dest || !src) {
        er_error_set(error, 0, 0, "Program append requires valid source and destination programs");
        return false;
    }

    for (i = 0; i < src->statements.count; ++i) {
        if (!er_statement_array_push(&dest->statements, src->statements.items[i])) {
            er_error_set(error, 0, 0, "Out of memory while merging parsed programs");
            return false;
        }
        src->statements.items[i] = NULL;
    }

    free(src->statements.items);
    src->statements.items = NULL;
    src->statements.count = 0;
    src->statements.capacity = 0;
    return true;
}

bool er_frontend_load_file(const char *entry_path, ErFrontendUnit *out_unit, ErError *error) {
    const ErModuleSource *entry_module;
    ErProgram *backend_program = NULL;
    ErProgram *entry_program = NULL;
    ErProgram *combined_program = NULL;
    size_t i;

    memset(out_unit, 0, sizeof(*out_unit));

    if (!er_module_graph_build_entry(entry_path, &out_unit->graph, error)) {
        return false;
    }

    entry_module = &out_unit->graph.modules[out_unit->graph.entry_index];

    for (i = 0; i < out_unit->graph.count; ++i) {
        if (i == out_unit->graph.entry_index) {
            continue;
        }
        if (er_frontend_is_backend_module_path(out_unit->graph.modules[i].normalized_path)) {
            if (!er_frontend_parse_only(
                    out_unit->graph.modules[i].normalized_path,
                    out_unit->graph.modules[i].source,
                    &backend_program,
                    error
                )) {
                er_frontend_unit_free(out_unit);
                return false;
            }
            break;
        }
    }

    if (!er_frontend_parse_only(entry_module->normalized_path, entry_module->source, &entry_program, error)) {
        er_frontend_unit_free(out_unit);
        return false;
    }

    combined_program = er_program_create();
    if (!combined_program) {
        er_program_free(backend_program);
        er_program_free(entry_program);
        er_frontend_unit_free(out_unit);
        er_error_set(error, 0, 0, "Out of memory while creating combined frontend program");
        return false;
    }

    if (backend_program && !er_frontend_program_append(combined_program, backend_program, error)) {
        er_program_free(backend_program);
        er_program_free(entry_program);
        er_program_free(combined_program);
        er_frontend_unit_free(out_unit);
        return false;
    }

    if (!er_frontend_program_append(combined_program, entry_program, error)) {
        er_program_free(backend_program);
        er_program_free(entry_program);
        er_program_free(combined_program);
        er_frontend_unit_free(out_unit);
        return false;
    }

    er_program_free(backend_program);
    er_program_free(entry_program);

    if (!er_semantic_analyze_program(entry_module->normalized_path, combined_program, error)) {
        er_program_free(combined_program);
        er_frontend_unit_free(out_unit);
        return false;
    }

    out_unit->program = combined_program;
    return true;
}

bool er_frontend_load_source(const char *source_name, const char *source, ErFrontendUnit *out_unit, ErError *error) {
    memset(out_unit, 0, sizeof(*out_unit));

    if (!er_frontend_parse_and_analyze(source_name, source, &out_unit->program, error)) {
        er_frontend_unit_free(out_unit);
        return false;
    }

    return true;
}

void er_frontend_unit_free(ErFrontendUnit *unit) {
    if (!unit) {
        return;
    }

    er_program_free(unit->program);
    unit->program = NULL;
    er_module_graph_free(&unit->graph);
}
