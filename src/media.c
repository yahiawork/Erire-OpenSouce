#define COBJMACROS

#include "media.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fileio.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include <shobjidl.h>
#endif

static char *er_media_dup(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *) malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void er_media_safe_assign(char **target, const char *text) {
    char *copy = er_media_dup(text ? text : "");
    if (!copy) {
        return;
    }
    free(*target);
    *target = copy;
}

static int er_media_clamp_int(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static char *er_media_format_duration_ms_internal(int duration_ms) {
    char buffer[32];
    int hours;
    int minutes;
    int seconds;
    int total_seconds;

    if (duration_ms < 0) {
        duration_ms = 0;
    }

    total_seconds = duration_ms / 1000;
    hours = total_seconds / 3600;
    minutes = (total_seconds % 3600) / 60;
    seconds = total_seconds % 60;

    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    }

    return er_media_dup(buffer);
}

static bool er_media_is_supported_extension(const char *path) {
    static const char *extensions[] = {
        ".mp3", ".wav", ".aac", ".wma", ".m4a", ".flac", ".ogg", ".mid", ".midi"
    };
    size_t i;

    if (!path) {
        return false;
    }

    for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        if (er_path_has_extension(path, extensions[i])) {
            return true;
        }
    }

    return false;
}

static void er_media_free_track(ErMediaTrack *track) {
    if (!track) {
        return;
    }

    free(track->path);
    free(track->name);
    free(track->title);
    free(track->artist);
    free(track->year);
    free(track->bitrate);
    free(track->sample_rate);
    free(track->art_path);
    memset(track, 0, sizeof(*track));
}

static const ErMediaTrack *er_media_current_track(const ErMediaPlayer *player) {
    if (!player || player->current_index < 0 || (size_t) player->current_index >= player->track_count) {
        return NULL;
    }
    return &player->tracks[player->current_index];
}

static ErMediaTrack *er_media_current_track_mut(ErMediaPlayer *player) {
    if (!player || player->current_index < 0 || (size_t) player->current_index >= player->track_count) {
        return NULL;
    }
    return &player->tracks[player->current_index];
}

static bool er_media_grow_tracks(ErMediaPlayer *player, ErError *error) {
    size_t new_capacity;
    ErMediaTrack *new_tracks;

    if (!player) {
        er_error_set(error, 0, 0, "Media player instance is required");
        return false;
    }

    if (player->track_count < player->track_capacity) {
        return true;
    }

    new_capacity = player->track_capacity == 0 ? 8 : player->track_capacity * 2;
    new_tracks = (ErMediaTrack *) realloc(player->tracks, new_capacity * sizeof(ErMediaTrack));
    if (!new_tracks) {
        er_error_set(error, 0, 0, "Out of memory while growing media playlist");
        return false;
    }

    player->tracks = new_tracks;
    player->track_capacity = new_capacity;
    return true;
}

static bool er_media_has_track_path(const ErMediaPlayer *player, const char *path) {
    size_t i;

    if (!player || !path) {
        return false;
    }

    for (i = 0; i < player->track_count; ++i) {
#ifdef _WIN32
        if (_stricmp(player->tracks[i].path ? player->tracks[i].path : "", path) == 0) {
#else
        if (strcmp(player->tracks[i].path ? player->tracks[i].path : "", path) == 0) {
#endif
            return true;
        }
    }

    return false;
}

static bool er_media_buffer_append(char **buffer, size_t *length, size_t *capacity, const char *text, ErError *error) {
    char *new_buffer;
    size_t needed;
    size_t text_length = text ? strlen(text) : 0;
    size_t new_capacity;

    needed = *length + text_length + 1;
    if (needed > *capacity) {
        new_capacity = *capacity == 0 ? 128 : *capacity;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        new_buffer = (char *) realloc(*buffer, new_capacity);
        if (!new_buffer) {
            er_error_set(error, 0, 0, "Out of memory while growing media text buffer");
            return false;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    if (text_length > 0) {
        memcpy(*buffer + *length, text, text_length);
        *length += text_length;
    }
    (*buffer)[*length] = '\0';
    return true;
}

static bool er_media_buffer_append_line(char **buffer, size_t *length, size_t *capacity, const char *text, ErError *error) {
    if (!er_media_buffer_append(buffer, length, capacity, text, error)) {
        return false;
    }
    return er_media_buffer_append(buffer, length, capacity, "\n", error);
}

#ifdef _WIN32
static bool er_media_send_mci_command(const char *command, char *buffer, size_t buffer_size, ErError *error) {
    MCIERROR code;
    char message[256];

    code = mciSendStringA(command, buffer, (UINT) buffer_size, NULL);
    if (code == 0) {
        return true;
    }

    message[0] = '\0';
    mciGetErrorStringA(code, message, (UINT) sizeof(message));
    er_error_set(error, 0, 0, "%s", message[0] != '\0' ? message : "Media engine command failed");
    return false;
}

static wchar_t *er_media_utf8_to_wide(const char *text) {
    int length;
    wchar_t *buffer;

    if (!text) {
        return NULL;
    }

    length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }

    buffer = (wchar_t *) calloc((size_t) length, sizeof(wchar_t));
    if (!buffer) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, length) <= 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

static char *er_media_wide_to_utf8(const wchar_t *text) {
    int length;
    char *buffer;

    if (!text) {
        return NULL;
    }

    length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (length <= 0) {
        return NULL;
    }

    buffer = (char *) malloc((size_t) length);
    if (!buffer) {
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, length, NULL, NULL) <= 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

static char *er_media_property_string(IPropertyStore *store, const PROPERTYKEY *key) {
    PROPVARIANT value;
    LPWSTR wide_text = NULL;
    char *utf8 = NULL;

    if (!store || !key) {
        return NULL;
    }

    PropVariantInit(&value);
    if (SUCCEEDED(IPropertyStore_GetValue(store, key, &value)) &&
        SUCCEEDED(PropVariantToStringAlloc(&value, &wide_text)) &&
        wide_text && wide_text[0] != L'\0') {
        utf8 = er_media_wide_to_utf8(wide_text);
    }

    if (wide_text) {
        CoTaskMemFree(wide_text);
    }
    PropVariantClear(&value);
    return utf8;
}

static unsigned long long er_media_property_u64(IPropertyStore *store, const PROPERTYKEY *key) {
    PROPVARIANT value;
    unsigned long long result = 0;

    if (!store || !key) {
        return 0;
    }

    PropVariantInit(&value);
    if (SUCCEEDED(IPropertyStore_GetValue(store, key, &value))) {
        ULONGLONG result_value = 0ULL;
        if (SUCCEEDED(PropVariantToUInt64(&value, &result_value))) {
            result = (unsigned long long) result_value;
        }
    }
    PropVariantClear(&value);
    return result;
}

static unsigned int er_media_property_u32(IPropertyStore *store, const PROPERTYKEY *key) {
    PROPVARIANT value;
    unsigned int result = 0;

    if (!store || !key) {
        return 0;
    }

    PropVariantInit(&value);
    if (SUCCEEDED(IPropertyStore_GetValue(store, key, &value))) {
        result = (unsigned int) PropVariantToUInt32WithDefault(&value, 0u);
    }
    PropVariantClear(&value);
    return result;
}
#endif

static void er_media_populate_track_defaults(ErMediaTrack *track, const char *path, const char *default_art_path) {
    char base_name[260];

    if (!track) {
        return;
    }

    memset(track, 0, sizeof(*track));
    er_path_basename_without_extension(path ? path : "", base_name, sizeof(base_name));
    track->path = er_media_dup(path ? path : "");
    track->name = er_media_dup(base_name[0] != '\0' ? base_name : "Track");
    track->title = er_media_dup(track->name ? track->name : "Track");
    track->artist = er_media_dup("-");
    track->year = er_media_dup("-");
    track->bitrate = er_media_dup("-");
    track->sample_rate = er_media_dup("-");
    track->art_path = er_media_dup(default_art_path ? default_art_path : "");
    track->duration_ms = 0;
}

static void er_media_load_track_metadata(ErMediaTrack *track) {
    if (!track) {
        return;
    }
#ifdef _WIN32
    {
        wchar_t *wide_path = er_media_utf8_to_wide(track->path);
        IPropertyStore *store = NULL;
        char *title = NULL;
        char *artist = NULL;
        unsigned int year = 0;
        unsigned int bitrate = 0;
        unsigned int sample_rate = 0;
        unsigned long long duration_100ns = 0;

        if (!wide_path) {
            return;
        }

        if (SUCCEEDED(SHGetPropertyStoreFromParsingName(
                wide_path,
                NULL,
                GPS_BESTEFFORT,
                &IID_IPropertyStore,
                (void **) &store
            )) && store) {
            title = er_media_property_string(store, &PKEY_Title);
            if (title && title[0] != '\0') {
                er_media_safe_assign(&track->title, title);
            }

            artist = er_media_property_string(store, &PKEY_Music_Artist);
            if ((!artist || artist[0] == '\0')) {
                free(artist);
                artist = er_media_property_string(store, &PKEY_Music_AlbumArtist);
            }
            if (artist && artist[0] != '\0') {
                er_media_safe_assign(&track->artist, artist);
            }

            year = er_media_property_u32(store, &PKEY_Media_Year);
            if (year > 0) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%u", year);
                er_media_safe_assign(&track->year, buffer);
            }

            bitrate = er_media_property_u32(store, &PKEY_Audio_EncodingBitrate);
            if (bitrate > 0) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%u kbps", bitrate / 1000u);
                er_media_safe_assign(&track->bitrate, buffer);
            }

            sample_rate = er_media_property_u32(store, &PKEY_Audio_SampleRate);
            if (sample_rate > 0) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%u Hz", sample_rate);
                er_media_safe_assign(&track->sample_rate, buffer);
            }

            duration_100ns = er_media_property_u64(store, &PKEY_Media_Duration);
            if (duration_100ns > 0ULL) {
                track->duration_ms = (int) (duration_100ns / 10000ULL);
            }

            free(title);
            free(artist);
            IPropertyStore_Release(store);
        }

        free(wide_path);
    }
#else
    (void) track;
#endif
}

static void er_media_close_current(ErMediaPlayer *player) {
#ifdef _WIN32
    if (player && player->current_open) {
        mciSendStringA("close erire_media", NULL, 0, NULL);
    }
#endif
    if (!player) {
        return;
    }
    player->current_open = false;
    player->opened_index = -1;
}

static int er_media_query_numeric(ErMediaPlayer *player, const char *command) {
#ifdef _WIN32
    char buffer[128];
    ErError error;

    if (!player || !player->current_open) {
        return 0;
    }

    er_error_clear(&error);
    buffer[0] = '\0';
    if (!er_media_send_mci_command(command, buffer, sizeof(buffer), &error)) {
        return 0;
    }

    return atoi(buffer);
#else
    (void) player;
    (void) command;
    return 0;
#endif
}

static const char *er_media_query_mode(ErMediaPlayer *player) {
#ifdef _WIN32
    static char buffer[64];
    ErError error;

    if (!player || !player->current_open) {
        return "stopped";
    }

    er_error_clear(&error);
    buffer[0] = '\0';
    if (!er_media_send_mci_command("status erire_media mode", buffer, sizeof(buffer), &error)) {
        return "stopped";
    }
    return buffer[0] != '\0' ? buffer : "stopped";
#else
    (void) player;
    return "stopped";
#endif
}

static void er_media_apply_output_volume(ErMediaPlayer *player) {
#ifdef _WIN32
    char command[128];
    int effective_volume;

    if (!player || !player->current_open) {
        return;
    }

    effective_volume = player->muted ? 0 : er_media_clamp_int(player->volume, 0, 100);
    snprintf(command, sizeof(command), "setaudio erire_media volume to %d", effective_volume * 10);
    mciSendStringA(command, NULL, 0, NULL);
#else
    (void) player;
#endif
}

static bool er_media_open_index(ErMediaPlayer *player, int index, bool autoplay, ErError *error) {
#ifdef _WIN32
    char command[2048];
    const char *path;

    if (!player || index < 0 || (size_t) index >= player->track_count) {
        return true;
    }

    path = player->tracks[index].path ? player->tracks[index].path : "";
    if (path[0] == '\0' || !er_file_exists(path)) {
        er_error_set(error, 0, 0, "Track file not found: %s", path[0] != '\0' ? path : "(empty path)");
        return false;
    }

    er_media_close_current(player);

    snprintf(command, sizeof(command), "open \"%s\" alias erire_media", path);
    if (!er_media_send_mci_command(command, NULL, 0, error)) {
        er_error_set(
            error,
            0,
            0,
            "Could not open track '%s'. The file may be missing or unsupported by the native Windows backend.",
            path
        );
        return false;
    }
    if (!er_media_send_mci_command("set erire_media time format milliseconds", NULL, 0, error)) {
        er_media_close_current(player);
        return false;
    }

    player->current_index = index;
    player->opened_index = index;
    player->current_open = true;
    player->manual_stop = false;
    er_media_apply_output_volume(player);

    if (autoplay) {
        if (!er_media_send_mci_command("play erire_media", NULL, 0, error)) {
            er_media_close_current(player);
            return false;
        }
        snprintf(player->last_mode, sizeof(player->last_mode), "%s", "playing");
    } else {
        snprintf(player->last_mode, sizeof(player->last_mode), "%s", "stopped");
    }

    if (player->tracks[index].duration_ms <= 0) {
        player->tracks[index].duration_ms = er_media_query_numeric(player, "status erire_media length");
    }

    return true;
#else
    (void) player;
    (void) index;
    (void) autoplay;
    er_error_set(error, 0, 0, "Erire media playback currently targets Windows first");
    return false;
#endif
}

static bool er_media_play_index(ErMediaPlayer *player, int index, ErError *error) {
    return er_media_open_index(player, index, true, error);
}

static bool er_media_choose_random_next(const ErMediaPlayer *player, int *out_index) {
    size_t tries;

    if (!player || !out_index || player->track_count == 0) {
        return false;
    }

    if (player->track_count == 1) {
        *out_index = 0;
        return true;
    }

    for (tries = 0; tries < 8; ++tries) {
        int candidate = rand() % (int) player->track_count;
        if (candidate != player->current_index) {
            *out_index = candidate;
            return true;
        }
    }

    *out_index = player->current_index == 0 ? 1 : 0;
    return true;
}

static bool er_media_advance_finished_track(ErMediaPlayer *player, ErError *error) {
    int next_index = player ? player->current_index : -1;

    if (!player || player->track_count == 0 || player->current_index < 0) {
        return true;
    }

    if (player->repeat_mode == ER_MEDIA_REPEAT_ONE) {
        return er_media_play_index(player, player->current_index, error);
    }

    if (player->shuffle) {
        if (!er_media_choose_random_next(player, &next_index)) {
            return true;
        }
        return er_media_play_index(player, next_index, error);
    }

    if ((size_t) (player->current_index + 1) < player->track_count) {
        return er_media_play_index(player, player->current_index + 1, error);
    }

    if (player->repeat_mode == ER_MEDIA_REPEAT_ALL) {
        return er_media_play_index(player, 0, error);
    }

    snprintf(player->last_mode, sizeof(player->last_mode), "%s", "stopped");
    player->manual_stop = false;
    return true;
}

void er_media_player_init(ErMediaPlayer *player, const char *source_dir, const char *default_art_path) {
    static bool seeded = false;

    if (!player) {
        return;
    }

    memset(player, 0, sizeof(*player));
    player->source_dir = er_media_dup(source_dir ? source_dir : ".");
    player->default_art_path = er_media_dup(default_art_path ? default_art_path : "");
    player->current_index = -1;
    player->opened_index = -1;
    player->volume = 100;
    player->volume_before_mute = 100;
    player->repeat_mode = ER_MEDIA_REPEAT_NONE;
    snprintf(player->last_mode, sizeof(player->last_mode), "%s", "stopped");

    if (!seeded) {
        srand((unsigned int) time(NULL));
        seeded = true;
    }
}

void er_media_player_destroy(ErMediaPlayer *player) {
    size_t i;

    if (!player) {
        return;
    }

    er_media_close_current(player);

    for (i = 0; i < player->track_count; ++i) {
        er_media_free_track(&player->tracks[i]);
    }
    free(player->tracks);
    free(player->source_dir);
    free(player->default_art_path);
    memset(player, 0, sizeof(*player));
}

bool er_media_player_add_paths(ErMediaPlayer *player, const char *paths_text, ErError *error) {
    char *copy;
    char *cursor;
    size_t old_count;
    int first_new_index = -1;
    bool was_active;

    if (!player || !paths_text || paths_text[0] == '\0') {
        return true;
    }

    old_count = player->track_count;
    was_active = strcmp(er_media_query_mode(player), "playing") == 0 || strcmp(er_media_query_mode(player), "paused") == 0;
    copy = er_media_dup(paths_text);
    if (!copy) {
        er_error_set(error, 0, 0, "Out of memory while adding media paths");
        return false;
    }

    cursor = copy;
    while (cursor && *cursor != '\0') {
        char *line = cursor;
        char *next_newline = strchr(cursor, '\n');
        char normalized[1024];
        size_t length;
        ErMediaTrack track;

        if (next_newline) {
            *next_newline = '\0';
            cursor = next_newline + 1;
        } else {
            cursor = NULL;
        }

        while (*line == ' ' || *line == '\t' || *line == '\r') {
            ++line;
        }
        length = strlen(line);
        while (length > 0 &&
               (line[length - 1] == '\r' || line[length - 1] == '\n' || line[length - 1] == ' ' || line[length - 1] == '\t')) {
            line[--length] = '\0';
        }
        if (line[0] == '\0' || !er_media_is_supported_extension(line)) {
            continue;
        }

#ifdef _WIN32
        if (!_fullpath(normalized, line, sizeof(normalized))) {
            continue;
        }
#else
        snprintf(normalized, sizeof(normalized), "%s", line);
#endif

        if (!er_file_exists(normalized)) {
            continue;
        }

        if (er_media_has_track_path(player, normalized)) {
            continue;
        }

        if (!er_media_grow_tracks(player, error)) {
            free(copy);
            return false;
        }

        er_media_populate_track_defaults(&track, normalized, player->default_art_path);
        er_media_load_track_metadata(&track);
        player->tracks[player->track_count++] = track;
        if (first_new_index < 0) {
            first_new_index = (int) player->track_count - 1;
        }
    }

    free(copy);

    if (player->track_count == 0) {
        return true;
    }

    if (player->current_index < 0) {
        player->current_index = 0;
    }

    if (player->track_count > old_count && !was_active && first_new_index >= 0) {
        player->current_index = first_new_index;
        return er_media_play_index(player, first_new_index, error);
    }

    return true;
}

bool er_media_player_open_playlist(ErMediaPlayer *player, const char *path, ErError *error) {
    char *data = NULL;
    size_t size = 0;
    bool ok;

    if (!path || path[0] == '\0') {
        return true;
    }

    if (!er_file_read_all(path, &data, &size, error)) {
        return false;
    }
    (void) size;

    ok = er_media_player_add_paths(player, data ? data : "", error);
    free(data);
    return ok;
}

bool er_media_player_save_playlist(const ErMediaPlayer *player, const char *path, ErError *error) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t i;
    bool ok;

    if (!player || !path || path[0] == '\0') {
        return true;
    }

    for (i = 0; i < player->track_count; ++i) {
        if (!er_media_buffer_append(&buffer, &length, &capacity, player->tracks[i].path ? player->tracks[i].path : "", error)) {
            free(buffer);
            return false;
        }
        if (i + 1 < player->track_count &&
            !er_media_buffer_append(&buffer, &length, &capacity, "\n", error)) {
            free(buffer);
            return false;
        }
    }

    ok = er_file_write_all(path, buffer ? buffer : "", length, error);
    free(buffer);
    return ok;
}

void er_media_player_clear(ErMediaPlayer *player) {
    size_t i;

    if (!player) {
        return;
    }

    er_media_close_current(player);
    for (i = 0; i < player->track_count; ++i) {
        er_media_free_track(&player->tracks[i]);
    }
    free(player->tracks);
    player->tracks = NULL;
    player->track_count = 0;
    player->track_capacity = 0;
    player->current_index = -1;
    player->opened_index = -1;
    player->manual_stop = false;
    player->current_open = false;
    snprintf(player->last_mode, sizeof(player->last_mode), "%s", "stopped");
}

bool er_media_player_play(ErMediaPlayer *player, ErError *error) {
#ifdef _WIN32
    const char *mode;

    if (!player || player->track_count == 0) {
        return true;
    }

    if (player->current_index < 0) {
        player->current_index = 0;
    }

    mode = er_media_query_mode(player);
    if (player->current_open && strcmp(mode, "paused") == 0) {
        if (!er_media_send_mci_command("resume erire_media", NULL, 0, error)) {
            return false;
        }
        player->manual_stop = false;
        snprintf(player->last_mode, sizeof(player->last_mode), "%s", "playing");
        return true;
    }

    if (player->current_open && strcmp(mode, "stopped") == 0 && player->opened_index == player->current_index) {
        if (!er_media_send_mci_command("play erire_media", NULL, 0, error)) {
            return false;
        }
        player->manual_stop = false;
        snprintf(player->last_mode, sizeof(player->last_mode), "%s", "playing");
        return true;
    }

    return er_media_play_index(player, player->current_index, error);
#else
    (void) player;
    er_error_set(error, 0, 0, "Erire media playback currently targets Windows first");
    return false;
#endif
}

bool er_media_player_pause(ErMediaPlayer *player, ErError *error) {
#ifdef _WIN32
    if (!player || !player->current_open) {
        return true;
    }
    if (strcmp(er_media_query_mode(player), "playing") != 0) {
        return true;
    }
    if (!er_media_send_mci_command("pause erire_media", NULL, 0, error)) {
        return false;
    }
    snprintf(player->last_mode, sizeof(player->last_mode), "%s", "paused");
    return true;
#else
    (void) player;
    er_error_set(error, 0, 0, "Erire media playback currently targets Windows first");
    return false;
#endif
}

bool er_media_player_play_pause(ErMediaPlayer *player, ErError *error) {
    const char *mode;

    if (!player || player->track_count == 0) {
        return true;
    }

    mode = er_media_query_mode(player);
    if (strcmp(mode, "playing") == 0) {
        return er_media_player_pause(player, error);
    }

    return er_media_player_play(player, error);
}

bool er_media_player_stop(ErMediaPlayer *player, ErError *error) {
#ifdef _WIN32
    if (!player || !player->current_open) {
        return true;
    }
    if (!er_media_send_mci_command("stop erire_media", NULL, 0, error)) {
        return false;
    }
    er_media_send_mci_command("seek erire_media to start", NULL, 0, error);
    player->manual_stop = true;
    snprintf(player->last_mode, sizeof(player->last_mode), "%s", "stopped");
    return true;
#else
    (void) player;
    er_error_set(error, 0, 0, "Erire media playback currently targets Windows first");
    return false;
#endif
}

bool er_media_player_next(ErMediaPlayer *player, ErError *error) {
    int next_index;

    if (!player || player->track_count == 0) {
        return true;
    }

    if (player->shuffle) {
        if (!er_media_choose_random_next(player, &next_index)) {
            return true;
        }
        return er_media_play_index(player, next_index, error);
    }

    next_index = player->current_index + 1;
    if ((size_t) next_index >= player->track_count) {
        if (player->repeat_mode == ER_MEDIA_REPEAT_ALL) {
            next_index = 0;
        } else {
            return true;
        }
    }

    return er_media_play_index(player, next_index, error);
}

bool er_media_player_previous(ErMediaPlayer *player, ErError *error) {
    int previous_index;

    if (!player || player->track_count == 0) {
        return true;
    }

    if (player->shuffle) {
        if (!er_media_choose_random_next(player, &previous_index)) {
            return true;
        }
        return er_media_play_index(player, previous_index, error);
    }

    previous_index = player->current_index - 1;
    if (previous_index < 0) {
        if (player->repeat_mode == ER_MEDIA_REPEAT_ALL) {
            previous_index = (int) player->track_count - 1;
        } else {
            return true;
        }
    }

    return er_media_play_index(player, previous_index, error);
}

bool er_media_player_seek(ErMediaPlayer *player, int position_ms, ErError *error) {
#ifdef _WIN32
    char command[128];
    const char *mode;
    int duration;

    if (!player || !player->current_open) {
        return true;
    }

    duration = er_media_player_duration_ms(player);
    position_ms = er_media_clamp_int(position_ms, 0, duration > 0 ? duration : position_ms);
    mode = er_media_query_mode(player);

    snprintf(command, sizeof(command), "seek erire_media to %d", position_ms);
    if (!er_media_send_mci_command(command, NULL, 0, error)) {
        return false;
    }

    if (strcmp(mode, "playing") == 0) {
        snprintf(command, sizeof(command), "play erire_media from %d", position_ms);
        if (!er_media_send_mci_command(command, NULL, 0, error)) {
            return false;
        }
        snprintf(player->last_mode, sizeof(player->last_mode), "%s", "playing");
    }

    return true;
#else
    (void) player;
    er_error_set(error, 0, 0, "Erire media playback currently targets Windows first");
    return false;
#endif
}

bool er_media_player_seek_relative(ErMediaPlayer *player, int delta_ms, ErError *error) {
    return er_media_player_seek(player, er_media_player_position_ms(player) + delta_ms, error);
}

void er_media_player_set_volume(ErMediaPlayer *player, int volume) {
    if (!player) {
        return;
    }

    player->volume = er_media_clamp_int(volume, 0, 100);
    if (!player->muted) {
        player->volume_before_mute = player->volume;
    }
    er_media_apply_output_volume(player);
}

void er_media_player_change_volume(ErMediaPlayer *player, int delta) {
    if (!player) {
        return;
    }
    er_media_player_set_volume(player, player->volume + delta);
}

void er_media_player_toggle_mute(ErMediaPlayer *player) {
    if (!player) {
        return;
    }

    if (player->muted) {
        player->muted = false;
        if (player->volume == 0 && player->volume_before_mute > 0) {
            player->volume = player->volume_before_mute;
        }
    } else {
        player->muted = true;
        if (player->volume > 0) {
            player->volume_before_mute = player->volume;
        }
    }

    er_media_apply_output_volume(player);
}

void er_media_player_toggle_shuffle(ErMediaPlayer *player) {
    if (!player) {
        return;
    }
    player->shuffle = !player->shuffle;
}

void er_media_player_cycle_repeat(ErMediaPlayer *player) {
    if (!player) {
        return;
    }
    if (player->repeat_mode == ER_MEDIA_REPEAT_NONE) {
        player->repeat_mode = ER_MEDIA_REPEAT_ALL;
    } else if (player->repeat_mode == ER_MEDIA_REPEAT_ALL) {
        player->repeat_mode = ER_MEDIA_REPEAT_ONE;
    } else {
        player->repeat_mode = ER_MEDIA_REPEAT_NONE;
    }
}

void er_media_player_sync(ErMediaPlayer *player) {
    const char *mode;
    ErError error;

    if (!player || player->track_count == 0 || !player->current_open) {
        return;
    }

    mode = er_media_query_mode(player);
    if ((strcmp(player->last_mode, "playing") == 0 || strcmp(player->last_mode, "paused") == 0) &&
        strcmp(mode, "stopped") == 0 &&
        !player->manual_stop) {
        er_error_clear(&error);
        if (!er_media_advance_finished_track(player, &error)) {
            er_media_close_current(player);
        }
        mode = er_media_query_mode(player);
    }

    snprintf(player->last_mode, sizeof(player->last_mode), "%s", mode);
}

size_t er_media_player_count(const ErMediaPlayer *player) {
    return player ? player->track_count : 0;
}

int er_media_player_current_index(const ErMediaPlayer *player) {
    return player ? player->current_index : -1;
}

const char *er_media_player_state(ErMediaPlayer *player) {
    er_media_player_sync(player);
    return er_media_query_mode(player);
}

int er_media_player_position_ms(ErMediaPlayer *player) {
    er_media_player_sync(player);
    return er_media_query_numeric(player, "status erire_media position");
}

int er_media_player_duration_ms(ErMediaPlayer *player) {
    ErMediaTrack *track;
    int duration;

    er_media_player_sync(player);
    duration = er_media_query_numeric(player, "status erire_media length");
    if (duration > 0) {
        track = er_media_current_track_mut(player);
        if (track) {
            track->duration_ms = duration;
        }
        return duration;
    }

    track = er_media_current_track_mut(player);
    return track ? track->duration_ms : 0;
}

int er_media_player_volume(const ErMediaPlayer *player) {
    return player ? player->volume : 0;
}

bool er_media_player_is_muted(const ErMediaPlayer *player) {
    return player ? player->muted : false;
}

bool er_media_player_shuffle_enabled(const ErMediaPlayer *player) {
    return player ? player->shuffle : false;
}

const char *er_media_player_repeat_mode_text(const ErMediaPlayer *player) {
    if (!player) {
        return "none";
    }
    if (player->repeat_mode == ER_MEDIA_REPEAT_ALL) {
        return "all";
    }
    if (player->repeat_mode == ER_MEDIA_REPEAT_ONE) {
        return "one";
    }
    return "none";
}

const char *er_media_player_current_name(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->name ? track->name : "";
}

const char *er_media_player_current_title(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->title ? track->title : "";
}

const char *er_media_player_current_artist(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->artist ? track->artist : "-";
}

const char *er_media_player_current_year(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->year ? track->year : "-";
}

const char *er_media_player_current_bitrate(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->bitrate ? track->bitrate : "-";
}

const char *er_media_player_current_sample_rate(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    return track && track->sample_rate ? track->sample_rate : "-";
}

const char *er_media_player_current_art(const ErMediaPlayer *player) {
    const ErMediaTrack *track = er_media_current_track(player);
    if (track && track->art_path && track->art_path[0] != '\0') {
        return track->art_path;
    }
    return player && player->default_art_path ? player->default_art_path : "";
}

char *er_media_player_position_text(ErMediaPlayer *player) {
    return er_media_format_duration_ms_internal(er_media_player_position_ms(player));
}

char *er_media_player_remaining_text(ErMediaPlayer *player) {
    int remaining = er_media_player_duration_ms(player) - er_media_player_position_ms(player);
    return er_media_format_duration_ms_internal(remaining > 0 ? remaining : 0);
}

char *er_media_player_duration_text(ErMediaPlayer *player) {
    return er_media_format_duration_ms_internal(er_media_player_duration_ms(player));
}

char *er_media_player_playlist_text(const ErMediaPlayer *player, ErError *error) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t i;

    if (!player || player->track_count == 0) {
        return er_media_dup("No tracks loaded yet.");
    }

    for (i = 0; i < player->track_count; ++i) {
        char line[512];
        const ErMediaTrack *track = &player->tracks[i];
        const char *title = track->title && track->title[0] != '\0' ? track->title : track->name;
        const char *artist = track->artist && track->artist[0] != '\0' ? track->artist : "-";
        char *duration_text = er_media_format_duration_ms_internal(track->duration_ms);

        snprintf(
            line,
            sizeof(line),
            "%s %02u. %s  [%s]  %s",
            (int) i == player->current_index ? ">>" : "  ",
            (unsigned int) (i + 1),
            title ? title : "Track",
            artist,
            duration_text ? duration_text : "--:--"
        );
        free(duration_text);

        if (!er_media_buffer_append_line(&buffer, &length, &capacity, line, error)) {
            free(buffer);
            return NULL;
        }
    }

    if (length > 0 && buffer) {
        buffer[length - 1] = '\0';
    }

    return buffer;
}
