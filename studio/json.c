#include "json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool studio_json_builder_reserve(StudioJsonBuilder *builder, size_t extra) {
    char *next;
    size_t next_capacity;

    if (builder->length + extra + 1 <= builder->capacity) {
        return true;
    }

    next_capacity = builder->capacity == 0 ? 512 : builder->capacity * 2;
    while (next_capacity < builder->length + extra + 1) {
        next_capacity *= 2;
    }

    next = (char *) realloc(builder->data, next_capacity);
    if (!next) {
        return false;
    }
    builder->data = next;
    builder->capacity = next_capacity;
    return true;
}

void studio_json_builder_init(StudioJsonBuilder *builder) {
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

void studio_json_builder_free(StudioJsonBuilder *builder) {
    if (!builder) {
        return;
    }
    free(builder->data);
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

bool studio_json_builder_append(StudioJsonBuilder *builder, const char *text) {
    size_t text_length = strlen(text);

    if (!studio_json_builder_reserve(builder, text_length)) {
        return false;
    }
    memcpy(builder->data + builder->length, text, text_length);
    builder->length += text_length;
    builder->data[builder->length] = '\0';
    return true;
}

bool studio_json_builder_appendf(StudioJsonBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list copy;
    int needed;

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0 || !studio_json_builder_reserve(builder, (size_t) needed)) {
        va_end(args);
        return false;
    }
    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    builder->length += (size_t) needed;
    va_end(args);
    return true;
}

bool studio_json_builder_append_escaped(StudioJsonBuilder *builder, const char *text) {
    size_t i;

    if (!studio_json_builder_append(builder, "\"")) {
        return false;
    }
    for (i = 0; text && text[i] != '\0'; ++i) {
        char chunk[7];
        switch (text[i]) {
            case '\\': if (!studio_json_builder_append(builder, "\\\\")) return false; break;
            case '"': if (!studio_json_builder_append(builder, "\\\"")) return false; break;
            case '\r': if (!studio_json_builder_append(builder, "\\r")) return false; break;
            case '\n': if (!studio_json_builder_append(builder, "\\n")) return false; break;
            case '\t': if (!studio_json_builder_append(builder, "\\t")) return false; break;
            default:
                if ((unsigned char) text[i] < 32) {
                    snprintf(chunk, sizeof(chunk), "\\u%04x", (unsigned char) text[i]);
                    if (!studio_json_builder_append(builder, chunk)) {
                        return false;
                    }
                } else {
                    chunk[0] = text[i];
                    chunk[1] = '\0';
                    if (!studio_json_builder_append(builder, chunk)) {
                        return false;
                    }
                }
        }
    }
    return studio_json_builder_append(builder, "\"");
}

char *studio_json_builder_take(StudioJsonBuilder *builder) {
    char *result = builder->data;
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    return result;
}

static const char *studio_json_find_key(const char *json, const char *key) {
    char needle[128];
    size_t needle_length;
    const char *cursor;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    needle_length = strlen(needle);
    cursor = json;

    while ((cursor = strstr(cursor, needle)) != NULL) {
        cursor += needle_length;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            ++cursor;
        }
        if (*cursor == ':') {
            ++cursor;
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
            return cursor;
        }
    }
    return NULL;
}

bool studio_json_get_string(const char *json, const char *key, char *out, size_t out_capacity) {
    const char *value = studio_json_find_key(json, key);
    size_t index = 0;

    if (!value || *value != '"' || out_capacity == 0) {
        return false;
    }
    ++value;
    while (*value && *value != '"' && index + 1 < out_capacity) {
        if (*value == '\\') {
            ++value;
            if (*value == 'n') out[index++] = '\n';
            else if (*value == 'r') out[index++] = '\r';
            else if (*value == 't') out[index++] = '\t';
            else if (*value == 'u') {
                value += 4;
                out[index++] = '?';
            } else {
                out[index++] = *value;
            }
        } else {
            out[index++] = *value;
        }
        ++value;
    }
    out[index] = '\0';
    return true;
}

bool studio_json_get_bool(const char *json, const char *key, bool *out_value) {
    const char *value = studio_json_find_key(json, key);

    if (!value || !out_value) {
        return false;
    }
    if (strncmp(value, "true", 4) == 0) {
        *out_value = true;
        return true;
    }
    if (strncmp(value, "false", 5) == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

bool studio_json_get_int(const char *json, const char *key, int *out_value) {
    const char *value = studio_json_find_key(json, key);

    if (!value || !out_value) {
        return false;
    }
    *out_value = atoi(value);
    return true;
}
