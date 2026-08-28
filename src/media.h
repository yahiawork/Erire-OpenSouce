#ifndef ERIRE_MEDIA_H
#define ERIRE_MEDIA_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

typedef enum ErMediaRepeatMode {
    ER_MEDIA_REPEAT_NONE,
    ER_MEDIA_REPEAT_ALL,
    ER_MEDIA_REPEAT_ONE
} ErMediaRepeatMode;

typedef struct ErMediaTrack {
    char *path;
    char *name;
    char *title;
    char *artist;
    char *year;
    char *bitrate;
    char *sample_rate;
    char *art_path;
    int duration_ms;
} ErMediaTrack;

typedef struct ErMediaPlayer {
    char *source_dir;
    char *default_art_path;
    ErMediaTrack *tracks;
    size_t track_count;
    size_t track_capacity;
    int current_index;
    int opened_index;
    int volume;
    int volume_before_mute;
    bool muted;
    bool shuffle;
    bool manual_stop;
    bool current_open;
    ErMediaRepeatMode repeat_mode;
    char last_mode[24];
} ErMediaPlayer;

void er_media_player_init(ErMediaPlayer *player, const char *source_dir, const char *default_art_path);
void er_media_player_destroy(ErMediaPlayer *player);

bool er_media_player_add_paths(ErMediaPlayer *player, const char *paths_text, ErError *error);
bool er_media_player_open_playlist(ErMediaPlayer *player, const char *path, ErError *error);
bool er_media_player_save_playlist(const ErMediaPlayer *player, const char *path, ErError *error);
void er_media_player_clear(ErMediaPlayer *player);

bool er_media_player_play(ErMediaPlayer *player, ErError *error);
bool er_media_player_pause(ErMediaPlayer *player, ErError *error);
bool er_media_player_play_pause(ErMediaPlayer *player, ErError *error);
bool er_media_player_stop(ErMediaPlayer *player, ErError *error);
bool er_media_player_next(ErMediaPlayer *player, ErError *error);
bool er_media_player_previous(ErMediaPlayer *player, ErError *error);
bool er_media_player_seek(ErMediaPlayer *player, int position_ms, ErError *error);
bool er_media_player_seek_relative(ErMediaPlayer *player, int delta_ms, ErError *error);
void er_media_player_set_volume(ErMediaPlayer *player, int volume);
void er_media_player_change_volume(ErMediaPlayer *player, int delta);
void er_media_player_toggle_mute(ErMediaPlayer *player);
void er_media_player_toggle_shuffle(ErMediaPlayer *player);
void er_media_player_cycle_repeat(ErMediaPlayer *player);
void er_media_player_sync(ErMediaPlayer *player);

size_t er_media_player_count(const ErMediaPlayer *player);
int er_media_player_current_index(const ErMediaPlayer *player);
const char *er_media_player_state(ErMediaPlayer *player);
int er_media_player_position_ms(ErMediaPlayer *player);
int er_media_player_duration_ms(ErMediaPlayer *player);
int er_media_player_volume(const ErMediaPlayer *player);
bool er_media_player_is_muted(const ErMediaPlayer *player);
bool er_media_player_shuffle_enabled(const ErMediaPlayer *player);
const char *er_media_player_repeat_mode_text(const ErMediaPlayer *player);
const char *er_media_player_current_name(const ErMediaPlayer *player);
const char *er_media_player_current_title(const ErMediaPlayer *player);
const char *er_media_player_current_artist(const ErMediaPlayer *player);
const char *er_media_player_current_year(const ErMediaPlayer *player);
const char *er_media_player_current_bitrate(const ErMediaPlayer *player);
const char *er_media_player_current_sample_rate(const ErMediaPlayer *player);
const char *er_media_player_current_art(const ErMediaPlayer *player);
char *er_media_player_position_text(ErMediaPlayer *player);
char *er_media_player_remaining_text(ErMediaPlayer *player);
char *er_media_player_duration_text(ErMediaPlayer *player);
char *er_media_player_playlist_text(const ErMediaPlayer *player, ErError *error);

#endif
