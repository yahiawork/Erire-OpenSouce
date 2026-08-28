#ifndef ERIRE_UI_H
#define ERIRE_UI_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum ErUiNodeKind {
    ER_UI_NODE_TEXT,
    ER_UI_NODE_BUTTON,
    ER_UI_NODE_INPUT,
    ER_UI_NODE_IMAGE,
    ER_UI_NODE_WEBVIEW,
    ER_UI_NODE_BOX
} ErUiNodeKind;

typedef enum ErUiTextAlign {
    ER_UI_TEXT_ALIGN_DEFAULT = -1,
    ER_UI_TEXT_ALIGN_START,
    ER_UI_TEXT_ALIGN_CENTER,
    ER_UI_TEXT_ALIGN_END
} ErUiTextAlign;

typedef enum ErUiSurfaceStyle {
    ER_UI_SURFACE_STYLE_DEFAULT,
    ER_UI_SURFACE_STYLE_BOX,
    ER_UI_SURFACE_STYLE_CARD,
    ER_UI_SURFACE_STYLE_PANEL
} ErUiSurfaceStyle;

typedef enum ErUiImageFit {
    ER_UI_IMAGE_FIT_CONTAIN,
    ER_UI_IMAGE_FIT_COVER,
    ER_UI_IMAGE_FIT_STRETCH,
    ER_UI_IMAGE_FIT_CENTER
} ErUiImageFit;

typedef enum ErUiAnimationKind {
    ER_UI_ANIMATION_NONE,
    ER_UI_ANIMATION_PRESET_FLOAT,
    ER_UI_ANIMATION_PRESET_DRIFT,
    ER_UI_ANIMATION_PRESET_PULSE,
    ER_UI_ANIMATION_OSCILLATE,
    ER_UI_ANIMATION_KEYFRAMES
} ErUiAnimationKind;

typedef enum ErUiAnimationProperty {
    ER_UI_ANIMATION_PROPERTY_NONE,
    ER_UI_ANIMATION_PROPERTY_X,
    ER_UI_ANIMATION_PROPERTY_Y,
    ER_UI_ANIMATION_PROPERTY_W,
    ER_UI_ANIMATION_PROPERTY_H,
    ER_UI_ANIMATION_PROPERTY_SHADOW
} ErUiAnimationProperty;

typedef struct ErUiTheme {
    unsigned int window_bg_rgb;
    unsigned int text_rgb;
    unsigned int muted_text_rgb;
    unsigned int surface_rgb;
    unsigned int surface_hover_rgb;
    unsigned int surface_active_rgb;
    unsigned int surface_border_rgb;
    unsigned int accent_rgb;
    unsigned int accent_hover_rgb;
    unsigned int accent_active_rgb;
    unsigned int accent_text_rgb;
    unsigned int input_bg_rgb;
    unsigned int input_border_rgb;
    unsigned int focus_rgb;
    unsigned int shadow_rgb;
    unsigned int highlight_rgb;
} ErUiTheme;

struct ErUiApp;
struct ErUiNode;
#ifdef _WIN32
struct ErUiPlatformWebView;
#endif

typedef void (*ErUiNodeCallback)(struct ErUiApp *app, struct ErUiNode *node, void *user_data);
typedef void (*ErUiTimerCallback)(struct ErUiApp *app, unsigned int timer_id, void *user_data);

typedef struct ErUiTimer {
    unsigned int timer_id;
    unsigned int os_timer_id;
    unsigned int interval_ms;
    ErUiTimerCallback callback;
    void *user_data;
} ErUiTimer;

typedef struct ErUiShortcut {
    unsigned int virtual_key;
    bool ctrl;
    bool alt;
    bool shift;
    char *target_id;
} ErUiShortcut;

typedef struct ErUiNodeSpec {
    ErUiNodeKind kind;
    const char *id;
    const char *page;
    const char *text;
    const char *hint;
    const char *preset;
    const char *asset_path;
    const char *icon_path;
    int x;
    int y;
    int w;
    int h;
    int font_size;
    int icon_size;
    int padding;
    int border_width;
    int border_radius;
    ErUiSurfaceStyle surface_style;
    ErUiTextAlign text_align;
    ErUiImageFit image_fit;
    bool multiline;
    bool read_only;
    bool has_text_color;
    unsigned int text_color_rgb;
    bool has_bg_color;
    unsigned int bg_color_rgb;
    bool has_bg_alt_color;
    unsigned int bg_alt_color_rgb;
    bool has_border_color;
    unsigned int border_color_rgb;
    int shadow_size;
    bool has_shadow_color;
    unsigned int shadow_color_rgb;
    ErUiNodeCallback on_click;
    void *on_click_user_data;
    ErUiNodeCallback on_change;
    void *on_change_user_data;
} ErUiNodeSpec;

typedef struct ErUiNode {
#ifdef _WIN32
    HWND hwnd;
    struct ErUiPlatformWebView *webview;
#endif
    int control_id;
    ErUiNodeKind kind;
    char *id;
    char *page;
    char *text;
    char *preset;
    char *asset_path;
    char *icon_path;
    int base_x;
    int base_y;
    int base_w;
    int base_h;
    int base_font_size;
    int base_icon_size;
    int base_padding;
    int base_border_width;
    int base_border_radius;
    int base_shadow_size;
    int x;
    int y;
    int w;
    int h;
    int font_size;
    int icon_size;
    int padding;
    int border_width;
    int border_radius;
    ErUiSurfaceStyle surface_style;
    ErUiTextAlign text_align;
    ErUiImageFit image_fit;
    bool multiline;
    bool read_only;
    ErUiAnimationKind animation_kind;
    ErUiAnimationProperty animation_property;
    bool visible;
    bool custom_drawn;
    bool hovered;
    bool pressed;
    bool focused;
    bool focusable;
    bool animation_active;
    bool animation_loop;
    bool has_text_color;
    unsigned int text_color_rgb;
    bool has_bg_color;
    unsigned int bg_color_rgb;
    bool has_bg_alt_color;
    unsigned int bg_alt_color_rgb;
    bool has_border_color;
    unsigned int border_color_rgb;
    int shadow_size;
    bool has_shadow_color;
    unsigned int shadow_color_rgb;
    int animation_duration_ms;
    unsigned long animation_start_tick;
    int animation_origin_x;
    int animation_origin_y;
    int animation_origin_w;
    int animation_origin_h;
    int animation_origin_shadow;
    double animation_amplitude;
    double animation_key_base;
    size_t animation_keyframe_count;
    double animation_keyframe_times[12];
    double animation_keyframe_values[12];
    ErUiNodeCallback on_click;
    void *on_click_user_data;
    ErUiNodeCallback on_change;
    void *on_change_user_data;
#ifdef _WIN32
    HFONT font;
    HBRUSH brush;
    HBITMAP image_bitmap;
    HICON icon_handle;
    int image_pixel_width;
    int image_pixel_height;
#endif
} ErUiNode;

typedef struct ErUiApp {
#ifdef _WIN32
    HINSTANCE instance;
    HWND hwnd;
    HBRUSH background_brush;
    HCURSOR arrow_cursor;
    HCURSOR hand_cursor;
    HCURSOR ibeam_cursor;
    HICON window_icon_small;
    HICON window_icon_large;
    bool ole_initialized;
#endif
    char *title;
    char *icon_path;
    int x;
    int y;
    int w;
    int h;
    int client_w;
    int client_h;
    int design_client_w;
    int design_client_h;
    bool resizable;
    int last_mouse_x;
    int last_mouse_y;
    int last_click_x;
    int last_click_y;
    unsigned int background_rgb;
    ErUiTheme theme;
    ErUiNode *nodes;
    size_t node_count;
    size_t node_capacity;
    int next_control_id;
    char *current_page;
    int hovered_control_id;
    int pressed_control_id;
    int focused_control_id;
    bool tracking_mouse;
    ErUiTimer *timers;
    size_t timer_count;
    size_t timer_capacity;
    ErUiShortcut *shortcuts;
    size_t shortcut_count;
    size_t shortcut_capacity;
    unsigned int next_timer_os_id;
} ErUiApp;

bool er_ui_app_init(
    ErUiApp *app,
    const char *title,
    int x,
    int y,
    int w,
    int h,
    bool resizable,
    ErError *error
);
void er_ui_app_destroy(ErUiApp *app);
void er_ui_app_set_title(ErUiApp *app, const char *title);
bool er_ui_app_set_icon(ErUiApp *app, const char *path, ErError *error);
void er_ui_app_set_background(ErUiApp *app, unsigned int rgb);
ErUiNode *er_ui_app_find_node(ErUiApp *app, const char *id);
ErUiNode *er_ui_app_add_node(ErUiApp *app, const ErUiNodeSpec *spec, ErError *error);
bool er_ui_app_remove_node(ErUiApp *app, const char *id, ErError *error);
void er_ui_app_show_page(ErUiApp *app, const char *page);
bool er_ui_app_set_text(ErUiApp *app, const char *id, const char *text);
bool er_ui_app_set_image(ErUiApp *app, const char *id, const char *path, ErError *error);
bool er_ui_app_set_bounds(ErUiApp *app, const char *id, int x, int y, int w, int h, ErError *error);
bool er_ui_app_set_timer(
    ErUiApp *app,
    unsigned int timer_id,
    unsigned int interval_ms,
    ErUiTimerCallback callback,
    void *user_data,
    ErError *error
);
void er_ui_app_clear_timer(ErUiApp *app, unsigned int timer_id);
bool er_ui_app_add_shortcut(ErUiApp *app, const char *combo, const char *target_id, ErError *error);
bool er_ui_app_webview_navigate(ErUiApp *app, const char *id, const char *url);
bool er_ui_app_webview_back(ErUiApp *app, const char *id);
bool er_ui_app_webview_forward(ErUiApp *app, const char *id);
bool er_ui_app_webview_reload(ErUiApp *app, const char *id);
bool er_ui_app_webview_run_script(ErUiApp *app, const char *id, const char *script);
bool er_ui_app_animation_play(
    ErUiApp *app,
    const char *id,
    const char *preset,
    int duration_ms,
    bool loop,
    ErError *error
);
bool er_ui_app_animation_oscillate(
    ErUiApp *app,
    const char *id,
    const char *property,
    double amplitude,
    int duration_ms,
    bool loop,
    ErError *error
);
bool er_ui_app_animation_keyframes(
    ErUiApp *app,
    const char *id,
    const char *property,
    const char *frames_text,
    int duration_ms,
    bool loop,
    ErError *error
);
bool er_ui_app_animation_stop(ErUiApp *app, const char *id, ErError *error);
char *er_ui_node_dup_text(ErUiNode *node);
int er_ui_app_run(ErUiApp *app);

#endif
