#ifndef ERIRE_STUDIO_JSON_H
#define ERIRE_STUDIO_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef struct StudioJsonBuilder {
    char *data;
    size_t length;
    size_t capacity;
} StudioJsonBuilder;

void studio_json_builder_init(StudioJsonBuilder *builder);
void studio_json_builder_free(StudioJsonBuilder *builder);
bool studio_json_builder_append(StudioJsonBuilder *builder, const char *text);
bool studio_json_builder_appendf(StudioJsonBuilder *builder, const char *fmt, ...);
bool studio_json_builder_append_escaped(StudioJsonBuilder *builder, const char *text);
char *studio_json_builder_take(StudioJsonBuilder *builder);

bool studio_json_get_string(const char *json, const char *key, char *out, size_t out_capacity);
bool studio_json_get_bool(const char *json, const char *key, bool *out_value);
bool studio_json_get_int(const char *json, const char *key, int *out_value);

#endif
