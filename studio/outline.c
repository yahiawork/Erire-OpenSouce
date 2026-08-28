#include "outline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "util.h"

static void studio_trim(char *text) {
    size_t start = 0;
    size_t end = strlen(text);

    while (text[start] && isspace((unsigned char) text[start])) {
        start += 1;
    }
    while (end > start && isspace((unsigned char) text[end - 1])) {
        end -= 1;
    }
    if (start > 0) {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static void studio_outline_emit(StudioJsonBuilder *builder, size_t *count, const char *name, const char *kind, int line) {
    if (*count > 0) {
        studio_json_builder_append(builder, ",");
    }
    studio_json_builder_append(builder, "{");
    studio_json_builder_append(builder, "\"name\":");
    studio_json_builder_append_escaped(builder, name);
    studio_json_builder_append(builder, ",\"kind\":");
    studio_json_builder_append_escaped(builder, kind);
    studio_json_builder_appendf(builder, ",\"line\":%d", line);
    studio_json_builder_append(builder, "}");
    *count += 1;
}

static void studio_outline_extract_python(StudioJsonBuilder *builder, const char *content, size_t *count) {
    char line[1024];
    int line_number = 1;
    const char *cursor = content;

    while (*cursor) {
        size_t length = 0;
        while (cursor[length] && cursor[length] != '\n' && length + 1 < sizeof(line)) {
            line[length] = cursor[length];
            length += 1;
        }
        line[length] = '\0';
        studio_trim(line);
        if (strncmp(line, "def ", 4) == 0) {
            char *paren = strchr(line + 4, '(');
            if (paren) *paren = '\0';
            studio_outline_emit(builder, count, line + 4, "function", line_number);
        } else if (strncmp(line, "class ", 6) == 0) {
            char *paren = strchr(line + 6, '(');
            char *colon = strchr(line + 6, ':');
            if (paren) *paren = '\0';
            if (colon) *colon = '\0';
            studio_outline_emit(builder, count, line + 6, "class", line_number);
        } else if (strncmp(line, "import ", 7) == 0 || strncmp(line, "from ", 5) == 0) {
            studio_outline_emit(builder, count, line, "import", line_number);
        }
        cursor += length;
        if (*cursor == '\n') {
            cursor += 1;
            line_number += 1;
        }
    }
}

static void studio_outline_extract_erire(StudioJsonBuilder *builder, const char *content, size_t *count) {
    char line[1024];
    int line_number = 1;
    const char *cursor = content;

    while (*cursor) {
        size_t length = 0;
        while (cursor[length] && cursor[length] != '\n' && length + 1 < sizeof(line)) {
            line[length] = cursor[length];
            length += 1;
        }
        line[length] = '\0';
        studio_trim(line);
        if (strncmp(line, "import python ", 14) == 0 || strncmp(line, "@imp from ", 10) == 0) {
            studio_outline_emit(builder, count, line, "import", line_number);
        } else if (strncmp(line, "page ", 5) == 0) {
            studio_outline_emit(builder, count, line + 5, "page", line_number);
        } else if (strncmp(line, "component ", 10) == 0) {
            studio_outline_emit(builder, count, line + 10, "component", line_number);
        } else if (strncmp(line, "screen.", 7) == 0) {
            studio_outline_emit(builder, count, line, "command", line_number);
        } else if (strncmp(line, "event.", 6) == 0 || strncmp(line, "var.set", 7) == 0) {
            studio_outline_emit(builder, count, line, "symbol", line_number);
        }
        cursor += length;
        if (*cursor == '\n') {
            cursor += 1;
            line_number += 1;
        }
    }
}

char *studio_outline_extract_json(const char *path, const char *content) {
    StudioJsonBuilder builder;
    size_t count = 0;
    const char *language = studio_language_from_path(path);

    studio_json_builder_init(&builder);
    studio_json_builder_append(&builder, "{\"path\":");
    studio_json_builder_append_escaped(&builder, path ? path : "");
    studio_json_builder_append(&builder, ",\"language\":");
    studio_json_builder_append_escaped(&builder, language);
    studio_json_builder_append(&builder, ",\"symbols\":[");
    if (strcmp(language, "python") == 0) {
        studio_outline_extract_python(&builder, content ? content : "", &count);
    } else {
        studio_outline_extract_erire(&builder, content ? content : "", &count);
    }
    studio_json_builder_append(&builder, "]}");
    return studio_json_builder_take(&builder);
}
